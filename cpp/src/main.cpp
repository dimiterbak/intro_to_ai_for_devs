#include <iostream>
#include <cstdlib>
#include <string>

#include "liboai.h"

int main()
{
    const std::string project_name = "intro_to_ai_for_devs cpp";
    liboai::OpenAI openai;

    std::cout << "Hello from " << project_name << "!" << std::endl;

    const char* api_key = std::getenv("OPENAI_API_KEY");
    if (api_key == nullptr || api_key[0] == '\0') {
        std::cout << "liboai is linked and ready. Set OPENAI_API_KEY to make API calls." << std::endl;
        return 0;
    }

    if (!openai.auth.SetKey(api_key)) {
        std::cerr << "OPENAI_API_KEY is set, but liboai rejected it." << std::endl;
        return 1;
    }

    std::cout << "liboai is linked and OPENAI_API_KEY is configured." << std::endl;

    return 0;
}