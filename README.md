<p align="center">
  <img alt="Argon Logo" height="250px" src="https://raw.githubusercontent.com/ArgonLang/argon-web/main/static/img/logo.svg">
</p>

<p align="center">
    <a href="https://img.shields.io/badge/version-0.4.2--alpha-red">
      <img src="https://img.shields.io/badge/version-0.4.2--alpha-red" alt="Version 0.4.2-alpha">
    </a>
    <a href="https://www.apache.org/licenses/LICENSE-2.0">
      <img src="https://img.shields.io/badge/license-apache--2.0-blue" alt="Apache License 2.0">
    </a>
    <a href="https://sonarcloud.io/summary/new_code?id=ArgonLang_Argon">
      <img src="https://sonarcloud.io/api/project_badges/measure?project=ArgonLang_Argon&metric=sqale_rating" alt="Maintainability Rating">
    </a>
    <a href="https://sonarcloud.io/summary/new_code?id=ArgonLang_Argon">
      <img src="https://sonarcloud.io/api/project_badges/measure?project=ArgonLang_Argon&metric=bugs" alt="Bugs">
    </a>
    <a href="https://sonarcloud.io/summary/new_code?id=ArgonLang_Argon">
      <img src="https://sonarcloud.io/api/project_badges/measure?project=ArgonLang_Argon&metric=ncloc" alt="Lines of Code">
    </a>
</p>

# The Argon Programming Language
This is the main repository for Argon language. It contains interpreter and builtins libraries.

# What's Argon
Argon is an interpreted multi-paradigm programming language. Its syntax is influenced by many modern languages and aims to be elegant, clean and simple to use. 

```js
import "enum"
import "io"

let NOBLE_GAS = ["Helium", "Neon", "Argon", "Krypton", "Xenon"]

var group_by_name_length = enum.group_by(len)

NOBLE_GAS
    |> group_by_name_length
    |> io.print

/* {6: [Helium], 4: [Neon], 5: [Argon, Xenon], 7: [Krypton]} */
```

# 🚀 Quick start
The wiki is under development, you can view the current version here: [wiki](https://www.arlang.io/docs/intro)

If you are looking for examples, you can find them here: [examples](https://github.com/ArgonLang/Argon/tree/master/example)

A good way to start could also be to take a look at Argon's built-in modules [here](https://github.com/ArgonLang/Argon/tree/master/arlib)

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
Argon is under active development, so many features are not yet active or available, a list (certainly not exhaustive) of the missing features is the following:
* No debugging support.
* Currently, all test cases are disabled ~~and out of date~~.

# 🤝 Contributing
If you're interested, there are several ways you can contribute:

- **Argon Core**: Help develop the Argon core (C++).
- **Improving the Argon Standard Library**: Enhance the functionality of the [Argon Standard Library](https://github.com/argonlang/arlib).
- **Writing/Improving Documentation**: Contribute by writing or improving documentation.
- **Writing Examples**: Create examples to demonstrate Argon's capabilities.
- **Sharing and Promotion**: Spread the word about the project and make it more widely known.

For technical details, refer to the [contributing guidelines](CONTRIBUTING.md).

# License
Argon is primarily distributed under the terms of the Apache License (Version 2.0). 

See [LICENSE](LICENSE)


