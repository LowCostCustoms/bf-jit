# BF-JIT

A poorly-documented Brainfuck language virtual machine, which uses [MIR](https://github.com/vnmakarov/mir) JIT compiler
backend.

## Compilation

This application needs you to have C++20 compatible compiler, and [fmt](https://github.com/fmtlib/fmt) library installed
on your system. The following shell script could be used as a reference on how to compile the project.

```shell
$ mkdir -p .build
$ cd .build
$ cmake -G"Unix Makefiles" ..
$ make -j9 bfjit
```

## Usage

`bfjit [--heap-size <HEAP_SIZE>] <FILE_PATH>`

### FILE_PATH

The path to a file containing brainfuck code.

### HEAP_SIZE

Determines the size of heap, in bytes, available to the brainfuck application. 

## Caveats

This project aims no particular goal except than amusing its owner. Any commercial use is discouraged and safety of the
software is not guaranteed.
