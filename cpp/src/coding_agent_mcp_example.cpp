#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include <curl/curl.h>
#include <nlohmann/json.hpp>

namespace {

std::atomic<bool> interrupted = false;

constexpr const char* kSystemPrompt =
    "You are a helpful coding assistant with access to file system tools. "
    "You can read files, write files, see directory structures, execute bash commands, "
    "and search in files. Use these tools to help with coding tasks and file management.";

struct ToolCall {
    std::string id;
    std::string function_name;
    nlohmann::json arguments;
};

struct ModelResponse {
    std::string content;
    nlohmann::json assistant_message;
    std::vector<ToolCall> tool_calls;
};

struct HttpResult {
    long status_code = 0;
    std::string body;
};

struct CommandResult {
    std::string stdout_output;
    std::string stderr_output;
    int return_code = 0;
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

std::string base64_encode(const std::string& input)
{
    static constexpr char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string output;
    output.reserve(((input.size() + 2) / 3) * 4);

    int accumulator = 0;
    int bit_count = -6;
    for (unsigned char ch : input) {
        accumulator = (accumulator << 8) + ch;
        bit_count += 8;
        while (bit_count >= 0) {
            output.push_back(table[(accumulator >> bit_count) & 0x3F]);
            bit_count -= 6;
        }
    }

    if (bit_count > -6) {
        output.push_back(table[((accumulator << 8) >> (bit_count + 8)) & 0x3F]);
    }
    while (output.size() % 4 != 0) {
        output.push_back('=');
    }

    return output;
}

class ScopedEnvVar {
public:
    ScopedEnvVar(std::string name, std::string value)
        : name_(std::move(name)),
          had_previous_(std::getenv(name_.c_str()) != nullptr),
          previous_value_(get_env_or_empty(name_.c_str()))
    {
        set(value);
    }

    ~ScopedEnvVar()
    {
        if (had_previous_) {
            set(previous_value_);
            return;
        }
        unset();
    }

private:
    void set(const std::string& value)
    {
#ifdef _WIN32
        _putenv_s(name_.c_str(), value.c_str());
#else
        setenv(name_.c_str(), value.c_str(), 1);
#endif
    }

    void unset()
    {
#ifdef _WIN32
        _putenv_s(name_.c_str(), "");
#else
        unsetenv(name_.c_str());
#endif
    }

    std::string name_;
    bool had_previous_;
    std::string previous_value_;
};

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

size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    std::string* buffer = static_cast<std::string*>(userdata);
    buffer->append(ptr, size * nmemb);
    return size * nmemb;
}

std::string strip_query_fragment(const std::string& value)
{
    const std::size_t pos = value.find_first_of("?#");
    return pos == std::string::npos ? value : value.substr(0, pos);
}

std::string trim_trailing_slash(std::string value)
{
    while (!value.empty() && value.back() == '/') {
        value.pop_back();
    }
    return value;
}

bool ends_with(std::string_view value, std::string_view suffix)
{
    return value.size() >= suffix.size()
        && value.substr(value.size() - suffix.size()) == suffix;
}

std::string extract_origin(const std::string& endpoint)
{
    std::size_t host_start = endpoint.find("://");
    host_start = host_start == std::string::npos ? 0 : host_start + 3;

    const std::size_t host_end = endpoint.find_first_of("/?", host_start);
    return host_end == std::string::npos ? endpoint : endpoint.substr(0, host_end);
}

std::string url_encode(const std::string& value)
{
    CURL* curl = curl_easy_init();
    if (curl == nullptr) {
        throw std::runtime_error("Failed to initialize CURL for URL encoding");
    }

    char* escaped = curl_easy_escape(curl, value.c_str(), static_cast<int>(value.size()));
    if (escaped == nullptr) {
        curl_easy_cleanup(curl);
        throw std::runtime_error("Failed to URL encode value");
    }

    const std::string encoded = escaped;
    curl_free(escaped);
    curl_easy_cleanup(curl);
    return encoded;
}

std::string resolve_chat_completions_url(
    const std::string& endpoint,
    const std::string& deployment_name,
    const std::string& api_version,
    bool is_azure
)
{
    const std::string stripped_endpoint = strip_query_fragment(endpoint);
    if (!is_azure) {
        const std::string base = trim_trailing_slash(stripped_endpoint);
        if (ends_with(base, "/chat/completions")) {
            return base;
        }
        return base + "/chat/completions";
    }

    if (stripped_endpoint.find("/chat/completions") != std::string::npos) {
        return endpoint;
    }

    return extract_origin(endpoint)
        + "/openai/deployments/"
        + url_encode(deployment_name)
        + "/chat/completions?api-version="
        + url_encode(api_version);
}

std::filesystem::path find_project_root(std::filesystem::path start)
{
    start = std::filesystem::absolute(start);
    if (!std::filesystem::exists(start)) {
        return std::filesystem::current_path();
    }

    std::filesystem::path current = start;
    while (true) {
        if (std::filesystem::exists(current / "intro_to_ai_for_devs.sln")) {
            return current;
        }

        if (std::filesystem::exists(current / ".git")
            && std::filesystem::exists(current / "cpp")
            && std::filesystem::exists(current / "python")) {
            return current;
        }

        if (!current.has_parent_path() || current.parent_path() == current) {
            return start;
        }
        current = current.parent_path();
    }
}

bool path_is_within_root(const std::filesystem::path& root, const std::filesystem::path& candidate)
{
    const std::filesystem::path normalized_root = root.lexically_normal();
    const std::filesystem::path normalized_candidate = candidate.lexically_normal();
    const std::filesystem::path relative = normalized_candidate.lexically_relative(normalized_root);
    return !relative.empty() && *relative.begin() != "..";
}

std::filesystem::path resolve_relative_path(const std::filesystem::path& root, const std::string& relative)
{
    if (relative.empty()) {
        throw std::runtime_error("filepath must not be empty");
    }

    const std::filesystem::path candidate = (root / std::filesystem::path(relative)).lexically_normal();
    if (!path_is_within_root(root, candidate) && candidate != root.lexically_normal()) {
        throw std::runtime_error("Path must stay within the project directory");
    }

    return candidate;
}

std::string read_text_file(const std::filesystem::path& path)
{
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Failed to open file: " + path.string());
    }

    std::ostringstream stream;
    stream << input.rdbuf();
    return stream.str();
}

std::string json_to_display_string(const nlohmann::json& value)
{
    if (value.is_string()) {
        return value.get<std::string>();
    }

    return value.dump(2);
}

HttpResult post_json(
    const std::string& url,
    const nlohmann::json& payload,
    const std::string& api_key,
    bool is_azure
)
{
    CURL* curl = curl_easy_init();
    if (curl == nullptr) {
        throw std::runtime_error("Failed to initialize CURL");
    }

    std::string response_body;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    const std::string auth_header = is_azure
        ? "api-key: " + api_key
        : "Authorization: Bearer " + api_key;
    headers = curl_slist_append(headers, auth_header.c_str());

    const std::string payload_string = payload.dump();
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload_string.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(payload_string.size()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    const CURLcode result = curl_easy_perform(curl);
    long status_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (result != CURLE_OK) {
        throw std::runtime_error(std::string("HTTP request failed: ") + curl_easy_strerror(result));
    }

    return {status_code, response_body};
}

class CodingAgentTools {
public:
    explicit CodingAgentTools(std::filesystem::path project_dir)
        : project_dir_(std::move(project_dir))
    {
    }

    nlohmann::json read_file(const std::string& filepath) const
    {
        return read_text_file(resolve_relative_path(project_dir_, filepath));
    }

    nlohmann::json write_file(const std::string& filepath, const std::string& content) const
    {
        const std::filesystem::path abs_path = resolve_relative_path(project_dir_, filepath);
        std::filesystem::create_directories(abs_path.parent_path());

        std::ofstream output(abs_path);
        if (!output) {
            throw std::runtime_error("Failed to open file for writing: " + abs_path.string());
        }

        output << content;
        return "Successfully wrote to " + filepath;
    }

    nlohmann::json see_file_tree(const std::string& root_dir = ".") const
    {
        const std::filesystem::path abs_root = resolve_relative_path(project_dir_, root_dir);
        if (!std::filesystem::exists(abs_root)) {
            throw std::runtime_error("Directory does not exist: " + root_dir);
        }

        std::vector<std::string> tree;
        std::filesystem::recursive_directory_iterator it(abs_root), end;
        while (it != end) {
            const std::filesystem::path path = it->path();
            const std::string name = path.filename().string();

            if (it->is_directory() && kSkipDirs.count(name) > 0) {
                it.disable_recursion_pending();
                ++it;
                continue;
            }

            tree.push_back(std::filesystem::relative(path, project_dir_).generic_string());
            ++it;
        }

        std::sort(tree.begin(), tree.end());
        return tree;
    }

    nlohmann::json execute_bash_command(const std::string& command, const std::optional<std::string>& cwd) const
    {
        if (command.find("runserver") != std::string::npos) {
            return {
                {"stdout", ""},
                {"stderr", "Error: Running the Django development server (runserver) is not allowed through this tool."},
                {"returncode", 1}
            };
        }

        const std::filesystem::path abs_cwd = cwd.has_value()
            ? resolve_relative_path(project_dir_, *cwd)
            : project_dir_;
        const CommandResult result = run_command(command, abs_cwd);
        return {
            {"stdout", result.stdout_output},
            {"stderr", result.stderr_output},
            {"returncode", result.return_code}
        };
    }

    nlohmann::json search_in_files(const std::string& pattern, const std::string& root_dir = ".") const
    {
        const std::filesystem::path abs_root = resolve_relative_path(project_dir_, root_dir);
        std::vector<nlohmann::json> matches;

        std::filesystem::recursive_directory_iterator it(abs_root), end;
        while (it != end) {
            const std::filesystem::path path = it->path();
            const std::string name = path.filename().string();

            if (it->is_directory() && kSkipDirs.count(name) > 0) {
                it.disable_recursion_pending();
                ++it;
                continue;
            }

            if (!it->is_regular_file()) {
                ++it;
                continue;
            }

            std::ifstream input(path);
            if (!input) {
                ++it;
                continue;
            }

            std::string line;
            int line_number = 0;
            while (std::getline(input, line)) {
                ++line_number;
                if (line.find(pattern) != std::string::npos) {
                    matches.push_back({
                        {"path", std::filesystem::relative(path, project_dir_).generic_string()},
                        {"line", line_number},
                        {"content", trim_copy(line)}
                    });
                }
            }

            ++it;
        }

        std::sort(
            matches.begin(),
            matches.end(),
            [](const nlohmann::json& left, const nlohmann::json& right) {
                if (left.at("path") == right.at("path")) {
                    return left.at("line").get<int>() < right.at("line").get<int>();
                }
                return left.at("path").get<std::string>() < right.at("path").get<std::string>();
            }
        );

        return matches;
    }

    nlohmann::json sequentialthinking(const nlohmann::json& arguments) const
    {
        const std::filesystem::path bridge_path = project_dir_ / "node" / "tools" / "mcp_seqthink_bridge.mjs";
        if (!std::filesystem::exists(bridge_path)) {
            return "MCP bridge script not found at node/tools/mcp_seqthink_bridge.mjs.";
        }

        const std::string encoded_args = base64_encode(arguments.dump());
        const std::string disable_logging = get_env_or_empty("DISABLE_THOUGHT_LOGGING").empty()
            ? "false"
            : get_env_or_empty("DISABLE_THOUGHT_LOGGING");

        ScopedEnvVar scoped_args("MCP_SEQ_ARGS_B64", encoded_args);
        ScopedEnvVar scoped_logging("DISABLE_THOUGHT_LOGGING", disable_logging);

        const std::string command = "node \"" + bridge_path.string() + "\"";
        const CommandResult result = run_command(command, project_dir_);

        const std::string stdout_text = trim_copy(result.stdout_output);
        if (stdout_text.empty()) {
            const std::string stderr_text = trim_copy(result.stderr_output);
            if (!stderr_text.empty()) {
                return "Error calling sequentialthinking via MCP: " + stderr_text;
            }
            return "Error calling sequentialthinking via MCP: no output from MCP bridge.";
        }

        try {
            const nlohmann::json response = nlohmann::json::parse(stdout_text);
            if (response.value("ok", false)) {
                if (response.contains("text") && response.at("text").is_string()) {
                    return response.at("text").get<std::string>();
                }
                if (response.contains("content")) {
                    return response.at("content");
                }
                return response;
            }

            if (response.contains("error") && response.at("error").is_string()) {
                return "Error calling sequentialthinking via MCP: "
                    + response.at("error").get<std::string>();
            }
            return "Error calling sequentialthinking via MCP: unknown bridge error.";
        } catch (const std::exception& ex) {
            return "Error parsing MCP bridge response: " + std::string(ex.what());
        }
    }

private:
    static CommandResult run_command(const std::string& command, const std::filesystem::path& cwd)
    {
        const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto thread_id_hash = std::hash<std::thread::id>{}(std::this_thread::get_id());
        const std::string unique = std::to_string(now) + "-" + std::to_string(thread_id_hash);
        const std::filesystem::path stdout_template = std::filesystem::temp_directory_path() / ("coding-agent-stdout-" + unique + ".txt");
        const std::filesystem::path stderr_template = std::filesystem::temp_directory_path() / ("coding-agent-stderr-" + unique + ".txt");

        const std::filesystem::path previous_cwd = std::filesystem::current_path();
        std::filesystem::current_path(cwd);

        std::string command_with_redirect = command
            + " > \""
            + stdout_template.string()
            + "\" 2> \""
            + stderr_template.string()
            + "\"";

        const int status = std::system(command_with_redirect.c_str());
        std::filesystem::current_path(previous_cwd);

        CommandResult result;
        if (std::filesystem::exists(stdout_template)) {
            result.stdout_output = read_text_file(stdout_template);
        }
        if (std::filesystem::exists(stderr_template)) {
            result.stderr_output = read_text_file(stderr_template);
        }
        result.return_code = status;

        std::filesystem::remove(stdout_template);
        std::filesystem::remove(stderr_template);
        return result;
    }

    inline static const std::set<std::string> kSkipDirs = {
        ".venv",
        "__pycache__",
        ".git",
        ".pytest_cache",
        ".mypy_cache",
        ".coverage",
        "node_modules",
        ".DS_Store"
    };

    std::filesystem::path project_dir_;
};

class ChatBot {
public:
    explicit ChatBot(std::string system = "", std::filesystem::path project_path = find_project_root(std::filesystem::current_path()))
        : project_path_(std::move(project_path)),
          agent_tools_(project_path_),
          system_(std::move(system)),
          endpoint_(get_env_or_empty("AI_ENDPOINT")),
          deployment_name_(get_env_or_empty("DEPLOYMENT_NAME")),
          api_key_(get_env_or_empty("AI_API_KEY")),
          resource_name_(extract_azure_resource_name(endpoint_)),
          api_version_(extract_api_version(endpoint_).empty() ? get_env_or_empty("AI_API_VERSION") : extract_api_version(endpoint_)),
          request_url_(resolve_chat_completions_url(endpoint_, deployment_name_, api_version_, !resource_name_.empty()))
    {
        if (!system_.empty()) {
            messages_.push_back({{"role", "system"}, {"content", system_}});
        }
    }

    ModelResponse operator()(const std::string& message)
    {
        messages_.push_back({{"role", "user"}, {"content", message}});
        return execute();
    }

    ModelResponse continue_conversation()
    {
        return execute();
    }

    void append_message(nlohmann::json message)
    {
        messages_.push_back(std::move(message));
    }

    nlohmann::json execute_tool(const std::string& tool_name, const nlohmann::json& arguments)
    {
        try {
            if (tool_name == "read_file") {
                return agent_tools_.read_file(arguments.at("filepath").get<std::string>());
            }
            if (tool_name == "write_file") {
                return agent_tools_.write_file(
                    arguments.at("filepath").get<std::string>(),
                    arguments.at("content").get<std::string>()
                );
            }
            if (tool_name == "see_file_tree") {
                return agent_tools_.see_file_tree(arguments.value("root_dir", "."));
            }
            if (tool_name == "execute_bash_command") {
                if (arguments.contains("cwd") && !arguments.at("cwd").is_null()) {
                    return agent_tools_.execute_bash_command(
                        arguments.at("command").get<std::string>(),
                        arguments.at("cwd").get<std::string>()
                    );
                }
                return agent_tools_.execute_bash_command(
                    arguments.at("command").get<std::string>(),
                    std::nullopt
                );
            }
            if (tool_name == "search_in_files") {
                return agent_tools_.search_in_files(
                    arguments.at("pattern").get<std::string>(),
                    arguments.value("root_dir", ".")
                );
            }
            if (tool_name == "sequentialthinking") {
                return agent_tools_.sequentialthinking(arguments);
            }

            return "Unknown tool: " + tool_name;
        } catch (const std::exception& ex) {
            return std::string("Error executing ") + tool_name + ": " + ex.what();
        }
    }

private:
    ModelResponse execute()
    {
        ++number_of_prompts_sent_;
        std::cout << "\n-- Prompt " << number_of_prompts_sent_ << " --\n";
        std::cout << "\n=== FULL PROMPT SENT TO MODEL ===\n";
        for (const nlohmann::json& message : messages_) {
            if (message.contains("content") && !message.at("content").is_null()) {
                std::cout << "  " << message.at("content").get<std::string>() << "\n\n";
            } else if (message.contains("tool_calls")) {
                std::cout << "  [assistant requested tool calls]\n\n";
            } else {
                std::cout << "  \n\n";
            }
        }

        const HttpResult response = request_completion();
        std::cout << "=== END OF PROMPT ===\n\n";

        nlohmann::json json;
        try {
            json = nlohmann::json::parse(response.body);
        } catch (const std::exception& ex) {
            throw std::runtime_error(std::string("Failed to parse model response: ") + ex.what());
        }

        if (response.status_code < 200 || response.status_code >= 300) {
            const std::string message = json.contains("error") ? json.at("error").dump() : response.body;
            throw std::runtime_error(
                "Model request failed with HTTP status " + std::to_string(response.status_code) + ": " + message
            );
        }

        if (!json.contains("choices") || !json.at("choices").is_array() || json.at("choices").empty()) {
            throw std::runtime_error("Model response did not include any choices");
        }

        const nlohmann::json& message = json.at("choices").at(0).at("message");
        ModelResponse parsed;
        parsed.assistant_message = message;
        if (message.contains("content") && !message.at("content").is_null()) {
            parsed.content = message.at("content").get<std::string>();
        }

        if (message.contains("tool_calls") && message.at("tool_calls").is_array()) {
            for (const nlohmann::json& tool_call : message.at("tool_calls")) {
                ToolCall call;
                call.id = tool_call.value("id", "");
                call.function_name = tool_call.at("function").value("name", "");

                const std::string argument_string = tool_call.at("function").value("arguments", "{}");
                try {
                    call.arguments = nlohmann::json::parse(argument_string);
                } catch (const std::exception& ex) {
                    throw std::runtime_error(
                        "Failed to parse tool arguments for " + call.function_name + ": " + ex.what()
                    );
                }
                parsed.tool_calls.push_back(std::move(call));
            }
        }

        return parsed;
    }

    HttpResult request_completion() const
    {
        nlohmann::json payload;
        payload["messages"] = messages_;
        payload["tools"] = get_tool_definitions();

        if (resource_name_.empty()) {
            payload["model"] = deployment_name_;
        }

        return post_json(request_url_, payload, api_key_, !resource_name_.empty());
    }

    static nlohmann::json get_tool_definitions()
    {
        return nlohmann::json::array({
            {
                {"type", "function"},
                {"function", {
                    {"name", "read_file"},
                    {"description", "Read and return the contents of a file at the given relative filepath"},
                    {"parameters", {
                        {"type", "object"},
                        {"properties", {
                            {"filepath", {
                                {"type", "string"},
                                {"description", "Path to the file, relative to the project directory"}
                            }}
                        }},
                        {"required", nlohmann::json::array({"filepath"})}
                    }}
                }}
            },
            {
                {"type", "function"},
                {"function", {
                    {"name", "write_file"},
                    {"description", "Write content to a file at the given relative filepath, creating directories as needed"},
                    {"parameters", {
                        {"type", "object"},
                        {"properties", {
                            {"filepath", {
                                {"type", "string"},
                                {"description", "Path to the file, relative to the project directory"}
                            }},
                            {"content", {
                                {"type", "string"},
                                {"description", "Content to write to the file"}
                            }}
                        }},
                        {"required", nlohmann::json::array({"filepath", "content"})}
                    }}
                }}
            },
            {
                {"type", "function"},
                {"function", {
                    {"name", "see_file_tree"},
                    {"description", "Return a list of all files and directories under the given root directory"},
                    {"parameters", {
                        {"type", "object"},
                        {"properties", {
                            {"root_dir", {
                                {"type", "string"},
                                {"description", "Root directory to list from, relative to the project directory"},
                                {"default", "."}
                            }}
                        }},
                        {"required", nlohmann::json::array()}
                    }}
                }}
            },
            {
                {"type", "function"},
                {"function", {
                    {"name", "execute_bash_command"},
                    {"description", "Execute a bash command in the shell and return its output, error, and exit code"},
                    {"parameters", {
                        {"type", "object"},
                        {"properties", {
                            {"command", {
                                {"type", "string"},
                                {"description", "The bash command to execute"}
                            }},
                            {"cwd", {
                                {"type", "string"},
                                {"description", "Working directory to run the command in, relative to the project directory"}
                            }}
                        }},
                        {"required", nlohmann::json::array({"command"})}
                    }}
                }}
            },
            {
                {"type", "function"},
                {"function", {
                    {"name", "search_in_files"},
                    {"description", "Search for a pattern in all files under the given root directory"},
                    {"parameters", {
                        {"type", "object"},
                        {"properties", {
                            {"pattern", {
                                {"type", "string"},
                                {"description", "Pattern to search for in files"}
                            }},
                            {"root_dir", {
                                {"type", "string"},
                                {"description", "Root directory to search from, relative to the project directory"},
                                {"default", "."}
                            }}
                        }},
                        {"required", nlohmann::json::array({"pattern"})}
                    }}
                }}
            },
            {
                {"type", "function"},
                {"function", {
                    {"name", "sequentialthinking"},
                    {"description", "Facilitates a detailed, step-by-step thinking process for problem-solving and analysis using an MCP server."},
                    {"parameters", {
                        {"type", "object"},
                        {"properties", {
                            {"thought", {{"type", "string"}, {"description", "Your current thinking step."}}},
                            {"nextThoughtNeeded", {{"type", "boolean"}, {"description", "Whether another thought step is needed."}}},
                            {"thoughtNumber", {{"type", "integer"}, {"description", "Current thought number (1-based)."}}},
                            {"totalThoughts", {{"type", "integer"}, {"description", "Estimated total thoughts needed."}}},
                            {"isRevision", {{"type", "boolean"}, {"description", "Whether this revises previous thinking."}}},
                            {"revisesThought", {{"type", "integer"}, {"description", "Which thought is being reconsidered."}}},
                            {"branchFromThought", {{"type", "integer"}, {"description", "Branching point thought number."}}},
                            {"branchId", {{"type", "string"}, {"description", "Branch identifier."}}},
                            {"needsMoreThoughts", {{"type", "boolean"}, {"description", "If reaching end but realizing more thoughts are needed."}}}
                        }},
                        {"required", nlohmann::json::array({"thought", "nextThoughtNeeded", "thoughtNumber", "totalThoughts"})}
                    }}
                }}
            }
        });
    }

    int number_of_prompts_sent_ = 0;
    std::filesystem::path project_path_;
    CodingAgentTools agent_tools_;
    std::string system_;
    nlohmann::json messages_ = nlohmann::json::array();
    std::string endpoint_;
    std::string deployment_name_;
    std::string api_key_;
    std::string resource_name_;
    std::string api_version_;
    std::string request_url_;
};

void show_examples()
{
    std::cout << std::string(80, '=') << '\n';
    std::cout << "CHATBOT EXAMPLES - Tool-Enabled Coding Agent\n";
    std::cout << std::string(80, '=') << '\n';

    const std::vector<std::string> examples = {
        "Show me the file structure of this project",
        "Read the README.md file",
        "Search for any Python files that contain 'OpenAI'",
        "What states does Ohio share borders with?",
        "Create a simple hello world Python script in a new file called hello.py"
    };

    for (std::size_t index = 0; index < examples.size(); ++index) {
        std::cout << "\n--- Example " << index + 1 << " ---\n";
        std::cout << examples[index] << '\n';
    }
}

std::string query(const std::string& question, ChatBot& bot)
{
    ModelResponse response_message = bot(question);

    constexpr int max_tool_cycles = 8;
    int cycles = 0;
    std::vector<std::string> tool_results_accum;

    while (true) {
        if (!response_message.tool_calls.empty()) {
            ++cycles;
            if (cycles > max_tool_cycles) {
                break;
            }

            nlohmann::json assistant_message = {
                {"role", "assistant"},
                {"content", response_message.content.empty() ? nlohmann::json(nullptr) : nlohmann::json(response_message.content)},
                {"tool_calls", response_message.assistant_message.at("tool_calls")}
            };
            bot.append_message(std::move(assistant_message));

            for (const ToolCall& tool_call : response_message.tool_calls) {
                std::cout << "[tool] Executing tool: " << tool_call.function_name
                          << " with args: " << tool_call.arguments.dump() << '\n';

                const nlohmann::json tool_result = bot.execute_tool(tool_call.function_name, tool_call.arguments);
                const std::string display = json_to_display_string(tool_result);
                tool_results_accum.push_back(display);

                bot.append_message({
                    {"role", "tool"},
                    {"content", display},
                    {"tool_call_id", tool_call.id}
                });
            }

            response_message = bot.continue_conversation();
            continue;
        }

        std::string content = response_message.content;
        if (content.empty() && !tool_results_accum.empty()) {
            content = "No final model answer was returned. Here's the latest tool result:\n" + tool_results_accum.back();
        }

        bot.append_message({{"role", "assistant"}, {"content", content}});
        return content;
    }

    if (!tool_results_accum.empty()) {
        const std::string content =
            "Stopped after reaching the maximum number of tool cycles. Latest tool result:\n"
            + tool_results_accum.back();
        bot.append_message({{"role", "assistant"}, {"content", content}});
        return content;
    }

    return "Stopped without receiving a final model answer.";
}

std::string read_multiline_query()
{
    std::cout << "Enter multi-line input. Finish with /end or --- on a line by itself.\n";

    std::vector<std::string> lines;
    while (!interrupted.load()) {
        std::string line;
        if (!std::getline(std::cin, line)) {
            break;
        }
        if (line == "/end" || line == "---") {
            break;
        }
        lines.push_back(line);
    }

    std::ostringstream stream;
    for (std::size_t index = 0; index < lines.size(); ++index) {
        if (index > 0) {
            stream << '\n';
        }
        stream << lines[index];
    }
    return trim_copy(stream.str());
}

void interactive_loop()
{
    std::cout << std::string(80, '=') << '\n';
    std::cout << "INTERACTIVE CODING AGENT - Have a conversation!\n";
    std::cout << "Type your questions and press Enter. Use Ctrl+C to exit.\n";
    std::cout << "I can help with file operations, code analysis, and general questions.\n\n";
    std::cout << "Tips:\n";
    std::cout << "- Single-line: type and press Enter.\n";
    std::cout << "- Multi-line: type /ml and press Enter, then paste lines; finish with /end (or ---) on its own line.\n";
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

        if (lowered_query == "/ml") {
            user_query = read_multiline_query();
        }

        if (lowered_query == "/exit" || lowered_query == "/quit") {
            std::cout << "Exiting...\n";
            break;
        }

        if (!user_query.empty()) {
            const std::string result = query(user_query, bot);
            std::cout << "Answer: " << result << '\n';
        } else {
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