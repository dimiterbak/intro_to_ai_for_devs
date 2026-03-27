# Introduction to AI for C++ Devs

This folder contains a standalone C++17 starter project built with CMake.
It fetches `liboai` and `nlohmann/json` during configure time and links against a
locally installed `curl` implementation.

## Prerequisites

Make sure you have the following installed:

- A C++17-capable compiler
  - macOS: Apple Clang from Xcode Command Line Tools or LLVM Clang
  - Linux: GCC 8+ or Clang 7+
  - Windows: Visual Studio 2019+ or a recent MinGW toolchain
- CMake 3.21 or newer
- `curl` development files available to CMake
  - macOS: the system `curl` is usually sufficient; if CMake cannot find it, install `curl` with Homebrew
  - Linux: install your distro's `libcurl` development package
  - Windows: prefer `vcpkg` for `curl`

Verify your tools:

```bash
cmake --version
c++ --version
```

If CMake cannot find `curl` on macOS, install it with Homebrew:

```bash
brew install curl
```

## 1. Navigate to the project directory

```bash
cd /Users/dimitarbakardzhiev/git/intro_to_ai_for_devs/cpp
```

## 2. Configure the build

Create an out-of-source build directory:

```bash
cmake -S . -B build
```
### 2.1. Install C++17 library for the OpenAI API

On the first configure, CMake will download `liboai` and `nlohmann/json`.

## 3. Build the project

```bash
cmake --build build
```
## 4. Export your environment variables

### Windows PowerShell:
For persistent environment variables (requires restart of terminal):
```
setx AI_API_KEY "your_api_key_here"
setx AI_ENDPOINT "your_endpoint_key_here"
setx DEPLOYMENT_NAME "your_model_name-here"
```

### Linux / macOS:
```
export AI_API_KEY="your_api_key_here"
export AI_ENDPOINT="your_endpoint_key_here"
export DEPLOYMENT_NAME="your_model_name-here"
```

## 5. Run a simple test

```bash
./build/intro_to_ai_cpp
```

To enable real OpenAI API calls later, export your API key before running:

```bash
export OPENAI_API_KEY="your-api-key"
```

On Windows with a multi-config generator, build and run the Debug configuration:

```powershell
cmake -S . -B build
cmake --build build --config Debug
.\build\Debug\intro_to_ai_cpp.exe
```

## Project layout

- `CMakeLists.txt` - project configuration and dependency fetching
- `src/main.cpp` - application entry point
- `build/` - generated build output (created locally)

## Next steps

- Replace the starter output with actual `liboai` API calls
- Add more source files under `src/`
- Add headers under `include/` if the project grows
- Add tests with CTest or a framework such as Catch2 or GoogleTest