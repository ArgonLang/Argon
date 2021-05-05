# The Argon Programming Language
This is the main repository for Argon language. It contains interpreter and builtins libraries.

# What's Argon
Argon is an interpreted multi-paradigm programming language. Its syntax is influenced by Python, Go and Rust, it is neat, simple to use and easily extensible. 

# Installing from source ⚙️
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
  
3. Build

  ```sh
  cmake .
  make
  ```
  
When completed, Argon and related libraries will placed in bin/ directory.
N.B: At present there are no installation steps.
