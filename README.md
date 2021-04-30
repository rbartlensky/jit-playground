## jit-playground

Playing around with interpreters, and JIT compilation.

### Goals

Write a JIT compiler for a subset of lisp in C.

TODO:
* Implement more language features
* Compile bytecode to x86_64 and execute when a particular path is hot.

### Running

```shell
make
./build/lsp examples/fib.lsp
```
