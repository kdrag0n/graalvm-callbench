# GraalVM callbench

This is a program to measure the speed of simple time syscalls and vDSO calls, as well as basic file I/O on procfs using both mmap and traditional POSIX I/O syscalls.

Unlike my original [C version](https://github.com/kdrag0n/callbench), this is an experiment with GraalVM polyglot programs. The C core is the same, but compiled to LLVM bitcode for [GraalVM](https://www.graalvm.org/) to run. The command-line interface has been replaced with a JavaScript frontend that calls the shared C core for benchmarking.

## Usage

To compile the C backend:

```bash
make
```

To run the command-line frontend:

```bash
$ js --polyglot --jvm launch.js
Benchmarking clock_gettime...
Benchmarking file I/O...

clock_gettime:
    syscall: 359 ns
    libc: 121 ns

read file:
    mmap: 9182 ns
    read: 4424 ns
```
