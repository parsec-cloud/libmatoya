# Requirements

First, [build `libmatoya`](https://github.com/matoya/libmatoya/wiki/Building). The following compiler commands assume that your working directory is this example directory and the `libmatoya` static library resides in the `../bin` directory.

## Windows

The Windows build requires you to install the [Build Tools for Visual Studio](https://visualstudio.microsoft.com/downloads/), using one of those two ways:
* From the Visual Studio installer if the IDE is already installed on your system.
* By downloading only the minimal package (under `Tools for Visual Studio`) if you don't want to install the full IDE.

Once installed, open a `Native Tools Command Prompt` (`x64`, `x86` or `arm64` depending on which architecture you want). From a regular command prompt, you can also run `vcvars64.bat` or `vcvars86.bat`, usually located in `<visual_studio_install_folder>\Community\VC\Auxiliary\Build\`.

## Apple

Apple devices requires the `Xcode Command Line Tools`. Those can be installed using the following command:
```bash
xcode-select --install
```

## Android

The Android build requires some initial setup:

* *Windows only*: Install and setup a WSL1 distribution (we recommend `Ubuntu-20.04`).
* Install the dependencies (make sure no other JDK version is installed):
    ```bash
    sudo apt install make build-essential openjdk-11-jre-headless
    ```
* Extract the [Android NDK](https://developer.android.com/ndk/downloads) in `~/android-ndk-<version>`.
* Extract the [Android Platform Tools](https://developer.android.com/studio/releases/platform-tools) in `~/android-home/platform-tools`.
* Extract the [Android Command Tools](https://developer.android.com/studio#command-tools) in `~/android-home/cmdline-tools`.
* Accept the SDK licenses: 
    ```bash
    $HOME/android-home/cmdline-tools/bin/sdkmanager --licenses --sdk_root=$HOME/android-home
    ```
* *Windows only*: `adb.exe` must be started on the host to let the connected devices be visible inside WSL.

## WASM

* *Windows only*: Install and setup a WSL1 distribution (we recommend `Ubuntu-20.04`).
* Extract the [WASI SDK](https://github.com/WebAssembly/wasi-sdk/releases) in `~/wasi-sdk-<version>`.

# Build & Run

Change the `name` parameter in the commands below to compile other examples.

```bash
# Windows
nmake name=0-minimal
./example.exe

# Linux
make linux=1 name=0-minimal
./example

# Apple
make apple=1 name=0-minimal
./example

# Android
make android=1 name=0-minimal
make run android=1 name=0-minimal

# Wasm
make wasm=1 name=0-minimal
python3 -m http.server 8000 -d web
```

# Notes

* As of now, WSL2 does not allow communication with ADB for Android debugging.
* As WASM does not currently support threading, `2-threaded` will not work on the web platform.
