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

## Spec

### Built-in types

- boolean
- integer
- real
- string
- list
- object
- nativeFunction
- scriptFunction
- internal
- nil

### Implicit conversion rules

- integer to boolean, real
- real to boolean (TODO remove)
- string to boolean (true, unless empty)
- list to boolean (true)
- object to boolean (true)
- nativeFunction to boolean (true)
- scriptFunction to boolean (true)
- internal to boolean (true)
- nil to boolean (false)

## Notes

- Garbage collection per https://researcher.watson.ibm.com/researcher/files/us-bacon/Bacon01Concurrent.pdf (like PHP 5.3)

## TODO

- some examples of usage
- strings mutable or not?
- all compile-time options
