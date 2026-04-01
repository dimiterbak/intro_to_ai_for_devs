# Introduction to AI for C++ Devs

This folder contains a standalone C++17 starter project built with CMake.
It fetches `liboai` and `nlohmann/json` during configure time and links against a
locally installed `curl` implementation.

## Prerequisites

Make sure you have the following installed:

- A C++17-capable compiler
  - macOS: Apple Clang from Xcode Command Line Tools or LLVM Clang
  - Linux: GCC 8+ or Clang 7+
  - Windows: Visual Studio 2019+ (or newer) with C++ workload, or a recent MinGW toolchain
- CMake 3.21 or newer
- `curl` development files available to CMake
  - macOS: the system `curl` is usually sufficient; if CMake cannot find it, install `curl` with Homebrew
  - Linux: install your distro's `libcurl` development package
  - Windows: prefer `vcpkg` for `curl`

On Windows, run CMake from **Developer PowerShell for Visual Studio** (or a shell where your chosen toolchain is already on `PATH`).

Verify your tools:

```bash
cmake --version
c++ --version
```

If CMake cannot find `curl` on macOS, install it with Homebrew:

```bash
brew install curl
```

## Windows: Set Up Visual Studio C++ Toolchain

Use this section if CMake cannot find `cl`, `nmake`, or Visual Studio generators.

### Install Build Tools

1. Install **Visual Studio Build Tools** (or full Visual Studio).
2. Select workload **Desktop development with C++**.
3. In optional components, make sure these are selected:
  - **MSVC Build Tools for x64/x86**
  - **Windows 11 SDK** (or latest available Windows SDK)
  - **C++ CMake tools for Windows**
  - **vcpkg package manager**

### Open the correct shell

Open one of these from Start Menu:
- **Developer PowerShell for Visual Studio 2026**
- **x64 Native Tools Command Prompt for VS 2026**

Do not use a plain PowerShell window for first-time setup unless compiler tools are already on `PATH`.

### Verify toolchain availability

Run:

```powershell
where cmake
where cl
where nmake
cmake --help | findstr "Visual Studio"
```

You should see paths for `cl`/`nmake` and at least one Visual Studio generator.

### Use VS-integrated vcpkg toolchain

If you installed the Visual Studio `vcpkg package manager` component, this toolchain file should exist:

```text
C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\vcpkg\scripts\buildsystems\vcpkg.cmake
```

Example configure command:

```powershell
$toolchain = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\vcpkg\scripts\buildsystems\vcpkg.cmake"
cmake -S . -B build_vs2026 -G "Visual Studio 18 2026" -A x64 "-DCMAKE_TOOLCHAIN_FILE=$toolchain"
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

On Windows, specify a generator explicitly to avoid CMake defaulting to `NMake Makefiles` in shells where `nmake` is unavailable:

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
```

If you prefer Ninja and have it installed:

```powershell
cmake -S . -B build -G Ninja
```

### 2.1. Install C++17 library for the OpenAI API

On the first configure, CMake will download `liboai` and `nlohmann/json`.

## 3. Build the project

```bash
cmake --build build
```

On Windows with a multi-config generator, build the Debug configuration:

```powershell
cmake --build build_vs2026 --config Debug
```

## Windows troubleshooting

If configure fails with generator or compiler errors, verify your tools are available in the current shell:

```powershell
where cmake
where cl
where nmake
where ninja
```

Tips:
- If `cl` or `nmake` are missing, open **Developer PowerShell for Visual Studio**.
- If using Ninja, ensure `ninja` is installed and on `PATH`.
- Remove the stale build directory and reconfigure with an explicit generator:

```powershell
Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
```

## 4. Export your environment variables

### Windows PowerShell:
For persistent environment variables (requires restart of terminal):
```
setx AI_API_KEY "your_api_key_here"
setx AI_ENDPOINT "your_endpoint_key_here"
setx DEPLOYMENT_NAME "your_model_name-here"
setx AI_API_VERSION "2024-02-15-preview"
```

### Linux / macOS:
```
export AI_API_KEY="your_api_key_here"
export AI_ENDPOINT="your_endpoint_key_here"
export DEPLOYMENT_NAME="your_model_name-here"
export AI_API_VERSION="2024-02-15-preview"
```

#### MacOS export permanently:
Create .bash_profile file:
```
touch ~/.bash_profile
```

Next, open it with the following command:
```
open -a TextEdit.app ~/.bash_profile

export AI_API_KEY="your_api_key_here"
export AI_ENDPOINT="your_endpoint_key_here"
export DEPLOYMENT_NAME="your_model_name-here"
export AI_API_VERSION="2024-02-15-preview"

```

#### MacOS 10.15 Catalina and Newer export permanently:
Create .zshenv file:
```
touch ~/.zshenv
```

Next, open it with the following command:
```
open -a TextEdit.app ~/.zshenv

export AI_API_KEY="your_api_key_here"
export AI_ENDPOINT="your_endpoint_key_here"
export DEPLOYMENT_NAME="your_model_name-here"
export AI_API_VERSION="2024-02-15-preview"

```

If `AI_ENDPOINT` already contains an `api-version=...` query parameter, `AI_API_VERSION` is not required.

## 5. Run a simple test

```bash
./build/test_openai
```

On Windows with a multi-config generator, run the Debug configuration:

```powershell
.\build\Debug\test_openai.exe
```
