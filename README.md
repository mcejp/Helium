# Helium Programming Language

A little glossary:

- A *list* of *items*: `(1, 2, 3)`
- An *object* has *members* (probably should be *properties*, *attributes*, *fields*, or something)
- All *functions* go into *modules* (previously: units), including the fictitious *main function*

## How to build

Simple! Using CMake:

    mkdir cmake-build-debug
    cd cmake-build-debug
    cmake ..
    cmake --build .

## Debug mode

- automatic disassembly is enabled by default (into _.helium_disassembly_)
- full value tracing is enabled (`HELIUM_TRACE_VALUES`, output _.helium_value_trace_)
- GC tracing is enabled (`HELIUM_TRACE_GC`, output _.helium_gc.log_)

## Notes

- Garbage collection per https://researcher.watson.ibm.com/researcher/files/us-bacon/Bacon01Concurrent.pdf (like PHP 5.3)

## TODO

- some examples of usage
- strings mutable or not?
- all compile-time options
