# SableWASM: A static compiler for WebAssembly

SableWASM is a work-in-progress static compiler for WebAssembly with WASI extension. This project is heavily inspired by
Lucet, Wasmer and other WebAssembly runtimes. The compiler compile WebAssembly modules into native shared libraries with
the help of LLVM framework. SableWASM also comes with a runtime loader with minimal WASI features (Perhaps will be
completed in the future?).

- Lucet (BytecodeAlliance): https://github.com/bytecodealliance/lucet
- Wasmer: https://wasmer.io/
- Wasmtime: https://github.com/bytecodealliance/wasmtime
