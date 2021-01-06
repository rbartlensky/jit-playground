## jit-playground

Playing around with interpreters, and JIT compilation.

### Goals

Write a JIT compiler for a subset of lisp in C.

TODO:
* Implement more language features
* Trace execution, and create traces of bytecode
* Compile bytecode to x86_64 and execute when the particular path is hot.

### Running

```shell
make
./build/lsp example.lsp
```
