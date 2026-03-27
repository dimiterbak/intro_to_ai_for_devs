cmake_minimum_required(VERSION 3.21)

project(intro_to_ai_for_devs_cpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

find_package(CURL REQUIRED)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.12.0
)

FetchContent_Declare(
    liboai
    GIT_REPOSITORY https://github.com/jasonduncan/liboai.git
    GIT_TAG 23f46da
)

FetchContent_MakeAvailable(nlohmann_json liboai)

get_target_property(OAI_INTERFACE_INCLUDE_DIRS oai INTERFACE_INCLUDE_DIRECTORIES)
if(OAI_INTERFACE_INCLUDE_DIRS)
    set_target_properties(oai PROPERTIES
        INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${OAI_INTERFACE_INCLUDE_DIRS}"
    )
endif()

add_executable(intro_to_ai_cpp
    src/main.cpp
)

target_compile_features(intro_to_ai_cpp PRIVATE cxx_std_17)
target_link_libraries(intro_to_ai_cpp PRIVATE oai)

if(MSVC)
    target_compile_options(intro_to_ai_cpp PRIVATE /W4 /permissive-)
else()
    target_compile_options(intro_to_ai_cpp PRIVATE -Wall -Wextra -Wpedantic)
endif()