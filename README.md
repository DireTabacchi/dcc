# DCC - Dire C Compiler

DCC is a C compiler written following the book *Writing a C Compiler* by Nora Sandler. The compiler currently only supports x86-64 Linux systems.

## Prerequisites

To build the compiler, you will need the following tools installed:
- Bash
- GCC
- Make

The build script will expect that `gcc`, `bash`, and `make` are on your command path or in your environment.

## Getting Started

1. Ensure that the prerequisites are installed and executable.
2. Clone the repository from one of the sources: `https://github.com/DireTabacchi/dcc` or `https://codeberg.org/DireTabacchi/dcc`.
   ```
   $ git clone <repo-source>
   ```
3. Navigate to the `dcc` directory.
4. Run the build script: `./build.sh release`.
After building, `dcc` can be found in the `release` directory.
Alternatively, running `./build.sh debug` create a debug build, which can be found in the `debug` directory.

## Roadmap

DCC development is following along with the *Writing a C Compiler* book, and as such supports all features included up to the current chapter of the book.

As of now, DCC can compile to x86-64 all features up through chapter 9, functions. The current chapter is 10, *File-Scope Declarations and Storage Specifiers*. As of current, DCC can handle as far as semantic analysis of file-scope declarations and storage specifiers.

## Credit

The test source files found in the `tests` directory are, except for a spare few, taken from Nora Sandler's test suite for the C compiler. The test suite can be found at https://github.com/nlsandler/writing-a-c-compiler-tests. All credit for the tests from the suite go to Nora Sandler.