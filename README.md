![](https://img.shields.io/badge/version-0.1.0--alpha-red)
![https://www.apache.org/licenses/LICENSE-2.0](https://img.shields.io/badge/license-apache--2.0-blue)

# The Argon Programming Language
This is the main repository for Argon language. It contains interpreter and builtins libraries.

# What's Argon
Argon is an interpreted multi-paradigm programming language. Its syntax is influenced by Python, Go and Rust, it is neat, simple to use and easily extensible. 

# 🚀 Quick start
The wiki is under development, you can view the current version here: [wiki](https://github.com/jacopodl/Argon/wiki)

# 🛠️ Installing from source

| Platform / Architecture  | x86 | x86_64 | ARM | Apple silicon |
|--------------------------|-----|--------|-----|---------------|
| Windows (7, 8, 10, ...)  | ??? | ???    | ??? | ???           |
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
  git clone https://github.com/jacopodl/argon
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
Coming soon...

# 🤝 Contributing
There is currently no guide available to contribute to the development, but if you are interested you can contribute in several ways:
* Argon core (C++)
* Argon builtins libraries (C++ / Argon)
* Writing documentation

# License
Argon is primarily distributed under the terms of the Apache License (Version 2.0). 

See [LICENSE](LICENSE)


