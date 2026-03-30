#include <algorithm>
#include <atomic>
#include <cmath>
#include <csignal>
#include <cstdlib>
#include <curl/curl.h>
#include <exception>
#include <functional>
#include <iostream>
#include <map>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "liboai.h"

namespace {

std::atomic<bool> interrupted = false;

constexpr const char* kSystemPrompt = R"(You run in a loop of Thought, Action, PAUSE, Observation.
At the end of the loop you output an Answer
Use Thought to describe your thoughts about the question you have been asked.
Use Action to run one of the actions available to you - then return PAUSE.
Observation will be the result of running those actions.

Your available actions are:

calculate:
e.g. calculate: 4 * 7 / 3
Runs a calculation and returns the number - uses C++ style floating point syntax if necessary

wikipedia:
e.g. wikipedia: Django
Returns a summary from searching Wikipedia

count_letters:
e.g. count_letters: letter r in word strawberry
Counts how many times a specific letter appears in a word (case-insensitive)

Always look things up on Wikipedia if you have the opportunity to do so.

Example session:

Question: What is the capital of France?
Thought: I should look up France on Wikipedia
Action: wikipedia: France
PAUSE

You will be called again with this:

Observation: France is a country. The capital is Paris.

You then output:

Answer: The capital of France is Paris)";

struct Message {
    std::string role;
    std::string content;
};

std::string get_env_or_empty(const char* name)
{
    const char* value = std::getenv(name);
    return value == nullptr ? std::string() : std::string(value);
}

std::string extract_azure_resource_name(const std::string& endpoint)
{
    static const std::string suffix = ".openai.azure.com";

    std::size_t host_start = endpoint.find("://");
    host_start = host_start == std::string::npos ? 0 : host_start + 3;

    const std::size_t host_end = endpoint.find_first_of("/?", host_start);
    const std::string host = endpoint.substr(host_start, host_end - host_start);

    if (host.size() <= suffix.size()) {
        return {};
    }

    if (host.compare(host.size() - suffix.size(), suffix.size(), suffix) != 0) {
        return {};
    }

    return host.substr(0, host.size() - suffix.size());
}

std::string extract_api_version(const std::string& endpoint)
{
    static const std::string key = "api-version=";

    const std::size_t query_pos = endpoint.find('?');
    if (query_pos == std::string::npos) {
        return {};
    }

    const std::size_t value_pos = endpoint.find(key, query_pos + 1);
    if (value_pos == std::string::npos) {
        return {};
    }

    const std::size_t start = value_pos + key.size();
    const std::size_t end = endpoint.find('&', start);
    return endpoint.substr(start, end - start);
}

std::string trim_copy(const std::string& value)
{
    const std::size_t first = value.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) {
        return {};
    }

    const std::size_t last = value.find_last_not_of(" \t\n\r");
    return value.substr(first, last - first + 1);
}

std::string to_lower_copy(std::string value)
{
    for (char& ch : value) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }

    return value;
}

void ensure_required_environment()
{
    const std::string endpoint = get_env_or_empty("AI_ENDPOINT");
    const std::string deployment_name = get_env_or_empty("DEPLOYMENT_NAME");
    const std::string api_key = get_env_or_empty("AI_API_KEY");

    if (api_key.empty()) {
        throw std::runtime_error("AI_API_KEY environment variable is not set");
    }

    if (endpoint.empty()) {
        throw std::runtime_error("AI_ENDPOINT environment variable is not set");
    }

    if (deployment_name.empty()) {
        throw std::runtime_error("DEPLOYMENT_NAME environment variable is not set");
    }

    if (!extract_azure_resource_name(endpoint).empty()) {
        const std::string api_version = extract_api_version(endpoint).empty()
            ? get_env_or_empty("AI_API_VERSION")
            : extract_api_version(endpoint);

        if (api_version.empty()) {
            throw std::runtime_error(
                "AI_API_VERSION must be set when AI_ENDPOINT does not include api-version=..."
            );
        }
    }
}

void handle_sigint(int)
{
    interrupted.store(true);
    static constexpr char newline = '\n';
    ::write(STDOUT_FILENO, &newline, 1);
}

void install_signal_handler()
{
    struct sigaction action {};
    action.sa_handler = handle_sigint;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    if (sigaction(SIGINT, &action, nullptr) != 0) {
        throw std::system_error(errno, std::generic_category(), "Failed to install SIGINT handler");
    }
}

class ChatBot {
public:
    explicit ChatBot(std::string system = "")
        : system_(std::move(system)),
          endpoint_(get_env_or_empty("AI_ENDPOINT")),
          deployment_name_(get_env_or_empty("DEPLOYMENT_NAME")),
          api_key_(get_env_or_empty("AI_API_KEY")),
          resource_name_(extract_azure_resource_name(endpoint_))
    {
        if (!system_.empty()) {
            if (!conversation_.SetSystemData(system_)) {
                throw std::runtime_error("Failed to initialize the system message");
            }

            messages_.push_back({"system", system_});
        }
    }

    std::string operator()(const std::string& message)
    {
        messages_.push_back({"user", message});
        if (!conversation_.AddUserData(message)) {
            throw std::runtime_error("Failed to append the user message to the conversation");
        }

        std::string result = execute();
        messages_.push_back({"assistant", result});
        return result;
    }

private:
    std::string execute()
    {
        ++number_of_prompts_sent_;
        std::cout << "\n-- Prompt " << number_of_prompts_sent_ << " --\n";
        std::cout << "\n=== FULL PROMPT SENT TO MODEL ===\n";
        for (const Message& message : messages_) {
            std::cout << "  " << message.content << "\n\n";
        }

        liboai::Response response = execute_request();
        std::string content = conversation_.GetLastResponse();
        if (content.empty()) {
            if (!conversation_.Update(response)) {
                throw std::runtime_error("Failed to parse the assistant response");
            }

            content = conversation_.GetLastResponse();
        }

        if (content.empty()) {
            throw std::runtime_error("No assistant message was returned");
        }

        std::cout << "=== END OF PROMPT ===\n\n";
        return content;
    }

    liboai::Response execute_request()
    {
        liboai::OpenAI openai(endpoint_);

        if (!resource_name_.empty()) {
            const std::string api_version = extract_api_version(endpoint_).empty()
                ? get_env_or_empty("AI_API_VERSION")
                : extract_api_version(endpoint_);

            if (!openai.auth.SetAzureKey(api_key_)) {
                throw std::runtime_error("Failed to configure Azure API key");
            }

            return openai.Azure->create_chat_completion(
                resource_name_,
                deployment_name_,
                api_version,
                conversation_
            );
        }

        if (!openai.auth.SetKey(api_key_)) {
            throw std::runtime_error("Failed to configure API key");
        }

        return openai.ChatCompletion->create(deployment_name_, conversation_);
    }

    int number_of_prompts_sent_ = 0;
    std::string system_;
    std::vector<Message> messages_;
    liboai::Conversation conversation_;
    std::string endpoint_;
    std::string deployment_name_;
    std::string api_key_;
    std::string resource_name_;
};

class ExpressionParser {
public:
    explicit ExpressionParser(std::string expression)
        : expression_(std::move(expression))
    {
    }

    double parse()
    {
        position_ = 0;
        const double value = parse_expression();
        skip_whitespace();
        if (position_ != expression_.size()) {
            throw std::runtime_error("Unexpected token in expression");
        }
        return value;
    }

private:
    double parse_expression()
    {
        double value = parse_term();

        while (true) {
            skip_whitespace();
            if (consume('+')) {
                value += parse_term();
                continue;
            }
            if (consume('-')) {
                value -= parse_term();
                continue;
            }
            return value;
        }
    }

    double parse_term()
    {
        double value = parse_factor();

        while (true) {
            skip_whitespace();
            if (consume('*')) {
                value *= parse_factor();
                continue;
            }
            if (consume('/')) {
                const double divisor = parse_factor();
                if (divisor == 0.0) {
                    throw std::runtime_error("Division by zero");
                }
                value /= divisor;
                continue;
            }
            return value;
        }
    }

    double parse_factor()
    {
        skip_whitespace();
        if (consume('+')) {
            return parse_factor();
        }
        if (consume('-')) {
            return -parse_factor();
        }

        if (consume('(')) {
            const double value = parse_expression();
            skip_whitespace();
            if (!consume(')')) {
                throw std::runtime_error("Expected closing parenthesis");
            }
            return value;
        }

        if (position_ < expression_.size() && is_alpha(expression_[position_])) {
            return parse_identifier();
        }

        return parse_number();
    }

    double parse_identifier()
    {
        const std::size_t start = position_;
        while (position_ < expression_.size() && (is_alpha(expression_[position_]) || is_digit(expression_[position_]))) {
            ++position_;
        }

        const std::string identifier = expression_.substr(start, position_ - start);
        skip_whitespace();

        if (identifier == "pi") {
            return std::acos(-1.0);
        }

        if (identifier == "e") {
            return std::exp(1.0);
        }

        if (!consume('(')) {
            throw std::runtime_error("Expected function call syntax");
        }

        const double first = parse_expression();
        skip_whitespace();
        if (consume(',')) {
            const double second = parse_expression();
            skip_whitespace();
            if (!consume(')')) {
                throw std::runtime_error("Expected closing parenthesis");
            }

            if (identifier == "pow") {
                return std::pow(first, second);
            }

            throw std::runtime_error("Unsupported two-argument function: " + identifier);
        }

        if (!consume(')')) {
            throw std::runtime_error("Expected closing parenthesis");
        }

        if (identifier == "sqrt") {
            if (first < 0.0) {
                throw std::runtime_error("sqrt requires a non-negative input");
            }
            return std::sqrt(first);
        }
        if (identifier == "abs") {
            return std::fabs(first);
        }

        throw std::runtime_error("Unsupported function: " + identifier);
    }

    double parse_number()
    {
        skip_whitespace();
        const std::size_t start = position_;
        bool seen_digit = false;

        while (position_ < expression_.size()) {
            const char ch = expression_[position_];
            if (is_digit(ch)) {
                seen_digit = true;
                ++position_;
                continue;
            }
            if (ch == '.') {
                ++position_;
                continue;
            }
            break;
        }

        if (!seen_digit) {
            throw std::runtime_error("Expected number");
        }

        return std::stod(expression_.substr(start, position_ - start));
    }

    void skip_whitespace()
    {
        while (position_ < expression_.size()) {
            const char ch = expression_[position_];
            if (ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r') {
                break;
            }
            ++position_;
        }
    }

    bool consume(char expected)
    {
        if (position_ < expression_.size() && expression_[position_] == expected) {
            ++position_;
            return true;
        }
        return false;
    }

    static bool is_alpha(char ch)
    {
        return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_';
    }

    static bool is_digit(char ch)
    {
        return ch >= '0' && ch <= '9';
    }

    std::string expression_;
    std::size_t position_ = 0;
};

size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    std::string* buffer = static_cast<std::string*>(userdata);
    buffer->append(ptr, size * nmemb);
    return size * nmemb;
}

std::string html_decode(std::string value)
{
    static const std::vector<std::pair<std::string, std::string>> replacements = {
        {"&quot;", "\""},
        {"&#39;", "'"},
        {"&amp;", "&"},
        {"&lt;", "<"},
        {"&gt;", ">"}
    };

    for (const auto& [encoded, decoded] : replacements) {
        std::size_t position = 0;
        while ((position = value.find(encoded, position)) != std::string::npos) {
            value.replace(position, encoded.size(), decoded);
            position += decoded.size();
        }
    }

    return value;
}

std::string strip_html_tags(const std::string& value)
{
    return std::regex_replace(value, std::regex("<[^>]+>"), "");
}

std::string wikipedia(const std::string& query)
{
    CURL* curl = curl_easy_init();
    if (curl == nullptr) {
        return "Wikipedia error: failed to initialize CURL";
    }

    std::string response_body;
    char* escaped_query = curl_easy_escape(curl, query.c_str(), static_cast<int>(query.size()));
    if (escaped_query == nullptr) {
        curl_easy_cleanup(curl);
        return "Wikipedia error: failed to encode query";
    }

    const std::string url = std::string("https://en.wikipedia.org/w/api.php")
        + "?action=query&list=search&format=json&utf8=1&srsearch="
        + escaped_query;
    curl_free(escaped_query);

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "intro-to-ai-react-example/1.0");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    const CURLcode result = curl_easy_perform(curl);
    long status_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
    curl_easy_cleanup(curl);

    if (result != CURLE_OK) {
        return std::string("Wikipedia error: ") + curl_easy_strerror(result);
    }

    if (status_code < 200 || status_code >= 300) {
        return "Wikipedia request failed with HTTP status " + std::to_string(status_code);
    }

    try {
        const nlohmann::json json = nlohmann::json::parse(response_body);
        const auto search_iter = json.find("query");
        if (search_iter == json.end()) {
            return "No results";
        }

        const auto results_iter = search_iter->find("search");
        if (results_iter == search_iter->end() || !results_iter->is_array() || results_iter->empty()) {
            return "No results";
        }

        const std::string snippet = (*results_iter)[0].value("snippet", "No results");
        return trim_copy(html_decode(strip_html_tags(snippet)));
    } catch (const std::exception& ex) {
        return std::string("Wikipedia parse error: ") + ex.what();
    }
}

std::string calculate(const std::string& expression)
{
    try {
        std::ostringstream output;
        output << ExpressionParser(expression).parse();
        return output.str();
    } catch (const std::exception& ex) {
        return std::string("Error: ") + ex.what();
    }
}

std::string count_letters(const std::string& query)
{
    const std::string normalized_query = to_lower_copy(trim_copy(query));
    static const std::vector<std::regex> patterns = {
        std::regex(R"(^letter\s+(\w)\s+in\s+word\s+(\w+)$)"),
        std::regex(R"(^letter\s+(\w)\s+in\s+(\w+)$)"),
        std::regex(R"(^(\w)\s+in\s+word\s+(\w+)$)"),
        std::regex(R"(^(\w)\s+in\s+(\w+)$)")
    };

    for (const std::regex& pattern : patterns) {
        std::smatch match;
        if (!std::regex_match(normalized_query, match, pattern)) {
            continue;
        }

        const std::string letter = match[1].str();
        const std::string word = match[2].str();
        const auto count = static_cast<int>(std::count(word.begin(), word.end(), letter[0]));
        return "The letter '" + letter + "' appears " + std::to_string(count)
            + " time(s) in the word '" + word + "'";
    }

    return "Could not parse the query '" + normalized_query
        + "'. Please use format like 'letter r in word strawberry' or 'r in strawberry'";
}

const std::map<std::string, std::function<std::string(const std::string&)>> known_actions = {
    {"calculate", calculate},
    {"wikipedia", wikipedia},
    {"count_letters", count_letters}
};

std::vector<std::pair<std::string, std::string>> parse_actions(const std::string& content)
{
    static const std::regex action_regex(R"(^Action: (\w+): (.*)$)");

    std::vector<std::pair<std::string, std::string>> actions;
    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) {
        std::smatch match;
        if (std::regex_match(line, match, action_regex)) {
            actions.emplace_back(match[1].str(), trim_copy(match[2].str()));
        }
    }

    return actions;
}

void query(const std::string& question, ChatBot& bot, int max_turns = 5)
{
    std::cout << "\nQuestion: " << question << '\n';
    int turn = 0;
    std::string next_prompt = question;

    while (!interrupted.load() && turn < max_turns) {
        const std::string result = bot(next_prompt);
        std::cout << result << '\n';

        const auto actions = parse_actions(result);
        if (actions.empty()) {
            return;
        }

        const std::string action_name = to_lower_copy(actions.front().first);
        const std::string& action_input = actions.front().second;
        const auto action_iter = known_actions.find(action_name);
        if (action_iter == known_actions.end()) {
            throw std::runtime_error("Unknown action: " + action_name + ": " + action_input);
        }

        std::cout << " -- running " << action_name << ' ' << action_input << '\n';
        const std::string observation = action_iter->second(action_input);
        std::cout << "Observation: " << observation << '\n';
        next_prompt = "Observation: " + observation;
        ++turn;
    }
}

void show_examples()
{
    std::cout << std::string(80, '=') << '\n';
    std::cout << "QUERY EXAMPLES - Demonstrating ReAct (Reason + Act) Pattern\n";
    std::cout << std::string(80, '=') << '\n';

    const std::vector<std::string> examples = {
        "What states does Ohio share borders with?",
        "Calculate the square root of 256.",
        "Who was the first president of the United States?",
        "How many r's are in strawberry?"
    };

    for (std::size_t index = 0; index < examples.size(); ++index) {
        std::cout << "\n--- Example " << index + 1 << " ---\n";
        std::cout << examples[index] << '\n';
    }
}

void interactive_loop()
{
    std::cout << std::string(80, '=') << '\n';
    std::cout << "INTERACTIVE MODE - Ask your own questions!\n";
    std::cout << "Type your questions and press Enter. Use Ctrl+C, /exit, or /quit to exit.\n";
    std::cout << std::string(80, '=') << '\n';

    ChatBot bot(kSystemPrompt);

    while (!interrupted.load()) {
        std::cout << "\nYour question: ";
        std::cout.flush();

        std::string user_query;
        if (!std::getline(std::cin, user_query)) {
            if (interrupted.load()) {
                std::cin.clear();
            }
            break;
        }

        user_query = trim_copy(user_query);
        const std::string lowered_query = to_lower_copy(user_query);
        if (lowered_query == "/exit" || lowered_query == "/quit" || lowered_query == "exit") {
            break;
        }

        if (!user_query.empty()) {
            query(user_query, bot);
        } else {
            std::cout << "Please enter a question.\n";
        }
    }

    std::cout << "\n\nGoodbye! Thanks for using the ReAct agent.\n";
}

} // namespace

int main()
{
    try {
        install_signal_handler();
        curl_global_init(CURL_GLOBAL_DEFAULT);
        ensure_required_environment();
        show_examples();
        interactive_loop();
        curl_global_cleanup();
        return 0;
    } catch (const std::exception& ex) {
        curl_global_cleanup();
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }
}