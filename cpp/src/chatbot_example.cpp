#include <atomic>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#include "liboai.h"

namespace {

std::atomic<bool> interrupted = false;

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

std::string join_lines(const std::vector<std::string>& lines)
{
    std::ostringstream stream;

    for (std::size_t index = 0; index < lines.size(); ++index) {
        if (index > 0) {
            stream << '\n';
        }
        stream << lines[index];
    }

    return stream.str();
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
}

void install_signal_handler()
{
    if (std::signal(SIGINT, handle_sigint) == SIG_ERR) {
        throw std::runtime_error("Failed to install SIGINT handler");
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

void show_examples()
{
    std::cout << std::string(80, '=') << '\n';
    std::cout << "CHATBOT EXAMPLES - Simple Q&A\n";
    std::cout << std::string(80, '=') << '\n';

    const std::vector<std::string> examples = {
        "What states does Ohio share borders with?",
        "Calculate the square root of 256.",
        "How many r's are in strawbery?",
        "Who was the first president of the United States?"
    };

    for (std::size_t index = 0; index < examples.size(); ++index) {
        std::cout << "\n--- Example " << index + 1 << " ---\n";
        std::cout << examples[index] << '\n';
    }
}

std::string query(const std::string& question, ChatBot& bot)
{
    return bot(question);
}

void interactive_loop()
{
    std::cout << std::string(80, '=') << '\n';
    std::cout << "INTERACTIVE CHATBOT - Have a conversation!\n";
    std::cout << "Type your questions and press Enter. Use Ctrl+C to exit.\n";
    std::cout << "Tips:\n";
    std::cout << "- Single-line: type and press Enter.\n";
    std::cout << "- Multi-line: type /ml and press Enter, then paste lines; finish with /end (or ---) on its own line.\n";
    std::cout << "- Exit: type /exit or /quit.\n";
    std::cout << std::string(80, '=') << '\n';

    ChatBot bot;

    while (!interrupted.load()) {
        std::cout << "\nYour question: ";
        std::cout.flush();

        std::string first;
        if (!std::getline(std::cin, first)) {
            if (interrupted.load()) {
                std::cin.clear();
            }
            break;
        }

        first = trim_copy(first);
        std::string user_query;

        if (to_lower_copy(first) == "/ml") {
            std::cout << "Enter multi-line input. Finish with /end or --- on a line by itself.\n";
            std::vector<std::string> lines;
            std::string line;

            while (!interrupted.load() && std::getline(std::cin, line)) {
                const std::string trimmed = trim_copy(line);
                if (trimmed == "/end" || trimmed == "---") {
                    break;
                }

                lines.push_back(line);
            }

            user_query = trim_copy(join_lines(lines));
        } else {
            user_query = first;
        }

        const std::string lowered_query = to_lower_copy(user_query);
        if (lowered_query == "/exit" || lowered_query == "/quit") {
            std::cout << "Exiting...\n";
            return;
        }

        if (!user_query.empty()) {
            const std::string result = query(user_query, bot);
            std::cout << "Answer: " << result << '\n';
        } else if (!interrupted.load()) {
            std::cout << "Please enter a question.\n";
        }
    }

    std::cout << "\n\nGoodbye! Thanks for chatting!\n";
}

} // namespace

int main()
{
    try {
        install_signal_handler();
        ensure_required_environment();
        show_examples();
        interactive_loop();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }
}