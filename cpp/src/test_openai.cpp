#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

#include "liboai.h"

namespace {

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

} // namespace

int main()
{
    const std::string endpoint = get_env_or_empty("AI_ENDPOINT");
    const std::string deployment_name = get_env_or_empty("DEPLOYMENT_NAME");
    const std::string api_key = get_env_or_empty("AI_API_KEY");

    if (api_key.empty()) {
        std::cerr << "Error: AI_API_KEY environment variable is not set" << std::endl;
        return 1;
    }

    if (endpoint.empty()) {
        std::cerr << "Error: AI_ENDPOINT environment variable is not set" << std::endl;
        return 1;
    }

    if (deployment_name.empty()) {
        std::cerr << "Error: DEPLOYMENT_NAME environment variable is not set" << std::endl;
        return 1;
    }

    try {
        liboai::OpenAI openai(endpoint);
        liboai::Conversation conversation({"Hello, what is the capital of France?"});
        liboai::Response response;

        const std::string resource_name = extract_azure_resource_name(endpoint);
        if (!resource_name.empty()) {
            const std::string api_version = [&]() {
                const std::string version_from_endpoint = extract_api_version(endpoint);
                if (!version_from_endpoint.empty()) {
                    return version_from_endpoint;
                }

                return get_env_or_empty("AI_API_VERSION");
            }();

            if (api_version.empty()) {
                std::cerr << "Error: AI_API_VERSION must be set when AI_ENDPOINT does not include api-version=..." << std::endl;
                return 1;
            }

            if (!openai.auth.SetAzureKey(api_key)) {
                std::cerr << "Error: failed to configure Azure API key" << std::endl;
                return 1;
            }

            response = openai.Azure->create_chat_completion(
                resource_name,
                deployment_name,
                api_version,
                conversation
            );
        } else {
            if (!openai.auth.SetKey(api_key)) {
                std::cerr << "Error: failed to configure API key" << std::endl;
                return 1;
            }

            response = openai.ChatCompletion->create(deployment_name, conversation);
        }

        std::string content = conversation.GetLastResponse();
        if (content.empty()) {
            if (!conversation.Update(response)) {
                std::cerr << "Error: failed to parse the assistant response" << std::endl;
                return 1;
            }

            content = conversation.GetLastResponse();
        }

        if (content.empty()) {
            std::cerr << "Error: no assistant message was returned" << std::endl;
            return 1;
        }

        std::cout << content << std::endl;
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }
}