![](https://img.shields.io/badge/version-0.3.0--alpha-red)
![https://www.apache.org/licenses/LICENSE-2.0](https://img.shields.io/badge/license-apache--2.0-blue)
[![Maintainability Rating](https://sonarcloud.io/api/project_badges/measure?project=ArgonLang_Argon&metric=sqale_rating)](https://sonarcloud.io/summary/new_code?id=ArgonLang_Argon)
[![Bugs](https://sonarcloud.io/api/project_badges/measure?project=ArgonLang_Argon&metric=bugs)](https://sonarcloud.io/summary/new_code?id=ArgonLang_Argon)
[![Lines of Code](https://sonarcloud.io/api/project_badges/measure?project=ArgonLang_Argon&metric=ncloc)](https://sonarcloud.io/summary/new_code?id=ArgonLang_Argon)

# The Argon Programming Language
This is the main repository for Argon language. It contains interpreter and builtins libraries.

# What's Argon
Argon is an interpreted multi-paradigm programming language. Its syntax is influenced by Python, Go and Rust, it is neat, simple to use and easily extensible. 

# 🚀 Quick start
The wiki is under development, you can view the current version here: [wiki](https://github.com/jacopodl/Argon/wiki)

# 🛠️ Installing from source

| Platform / Architecture  | x86 | x86_64 | ARM | Apple silicon |
|--------------------------|-----|--------|-----|---------------|
| Windows (7, 8, 10, ...)  | ✓   | ✓      | ??? | ???           |
| Linux                    | ✓   | ✓      | ✓   | ???           |
| Mac OS                   | NA  | ✓      | NA  | ✓             |

The Argon build system uses a simple cmake file to build the interpreter.

## Building on Unix-like os

1. Make sure you have installed the following dependences:
  * cmake >= 3.7
  * g++ or clang++
  * GNU make
  * git

2. Clone the sources with git:

  ```sh
  git clone https://github.com/ArgonLang/Argon
  cd argon
  ```
  
3. Build:

  ```sh
  cmake .
  make
  ```
  
When completed, Argon and related libraries will placed in `bin/` directory.

N.B: At present there are no installation steps.

## Building on Windows
Building Argon on Windows environment requires Visual Studio, or the Microsoft Build Tools. 
You can download and install the tool of your choice from the [Microsoft Visual Studio](https://www.visualstudio.com/downloads/) page.

1. Install Visual Studio / Microsoft Build Tools.
2. From Windows start menu, open **Developer Command Prompt** and go to the Argon root folder:
```sh
    cd %path_of_Argon_folder%
```
3. Execute `build.bat` to start compilation.

If you preferred to install Visual Studio, you can import the CMake project directly into visual studio. 
To do this, open Visual Studio and press **continue without code** in the welcome window that opens before the IDE. 
From IDE menu, select File > Open > CMake... and open the CMakeLists.txt file, ignore the CMake Overview Page that might open 
and from the top bar select Argon.exe as start up item, select the configuration of interest (Debug / Release) and start the compilation 
through the play button(or press F5 key).

⚠️ Tested with Microsoft Visual Studio 2019.

# ‼️ Notes
Argon is under development, so many features are not yet active or available, a list (certainly not exhaustive) of the missing features is the following:
* ~~No support for interactive mode.~~
* ~~Lack of most of the built-in functionality for basic DataType (e.g. bytes::find, bytestream::split, ...).~~
* ~~Garbage collector not enabled (currently the memory is managed only by the ARC).~~
* ~~Limited I/O support.~~
* ~~No multithreading support.~~
* No debugging support.
* Currently, all test cases are disabled ~~and out of date~~.

# 🤝 Contributing
There is currently no guide available to contribute to the development, but if you are interested you can contribute in several ways:
* Argon core (C++)
* Argon builtins libraries (C++ / Argon)
* Writing documentation

# License
Argon is primarily distributed under the terms of the Apache License (Version 2.0). 

See [LICENSE](LICENSE)


