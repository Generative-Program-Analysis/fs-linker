This helper program preprocesses LLVM IR programs for symbolic execution.
The preprocess steps are configurable and include (1) linking with the POSIX
file system or the uClibc library, and (2) optimizations/transformation that
makes program more amenable for symbolic execution.

The implementation are extracted from [KLEE](http://klee.github.io/) with minor
changes and redistributed under [the same license](LICENSE.TXT) as KLEE.

