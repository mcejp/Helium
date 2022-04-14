#set(CMAKE_VERBOSE_MAKEFILE ON)
cmake_minimum_required(VERSION 3.11)
project(Helium)
set(CMAKE_CXX_STANDARD 17)

add_subdirectory(dependencies/fmt EXCLUDE_FROM_ALL)

set(ENABLE_FORMAL OFF CACHE BOOL "Enable experimental Formal extensions")
set(SANITIZE OFF CACHE BOOL "Enable -fsanitize=address")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

set(HEADER_FILES
        include/Helium/Assert.hpp
        include/Helium/Compiler/Ast.hpp
        include/Helium/Compiler/BytecodeCompiler.hpp
        include/Helium/Compiler/Compiler.hpp
        include/Helium/Compiler/Lexer.hpp
        include/Helium/Compiler/Optimizer.hpp
        include/Helium/Compiler/Parser.hpp
        include/Helium/Compiler/Type.hpp
        include/Helium/Config.hpp
        include/Helium/Memory/LinearAllocator.hpp
        include/Helium/Memory/PoolPtr.hpp
        include/Helium/Platform/ScriptContainer.hpp
        include/Helium/Runtime/Debug/Disassembler.hpp
        include/Helium/Runtime/Debug/GcTrace.hpp
        include/Helium/Runtime/Debug/ValueTrace.hpp
        include/Helium/Runtime/ActivationContext.hpp
        include/Helium/Runtime/BindingHelpers.hpp
        include/Helium/Runtime/Code.hpp
        include/Helium/Runtime/Hash.hpp
        include/Helium/Runtime/InlineStack.hpp
        include/Helium/Runtime/Metaclass.hpp
        include/Helium/Runtime/NativeListFunctions.hpp
        include/Helium/Runtime/RuntimeFunctions.hpp
        include/Helium/Runtime/NativeObjectFunctions.hpp
        include/Helium/Runtime/NativeStringFunctions.hpp
        include/Helium/Runtime/Value.hpp
        include/Helium/Runtime/VM.hpp
        include/stx/any.hpp
        include/stx/optional.hpp
        include/stx/string_view.hpp
        include/stx/variant.hpp
        )

if (ENABLE_FORMAL)
    list(APPEND HEADER_FILES
            include/Helium/Formal/FormalAnalyzer.hpp
            include/Helium/Formal/Theorem.hpp
            include/Helium/Formal/TheoremOstream.hpp
            src/Formal/AnalysisContext.hpp
            )
endif()

set(SOURCE_FILES
        src/Compiler/BytecodeCompiler.cpp
        src/Compiler/Code.cpp
        src/Compiler/Compiler.cpp
        src/Compiler/Lexer.cpp
        src/Compiler/P3.cpp
        src/Memory/LinearAllocator.cpp
        src/Platform/ScriptContainer.cpp
        src/Runtime/Debug/Disassembler.cpp
        src/Runtime/Debug/GcTrace.cpp
        src/Runtime/Debug/ValueTrace.cpp
        src/Runtime/ActivationContext.cpp
        src/Runtime/BindingHelpers.cpp
        src/Runtime/BuiltinFunctions.cpp
        src/Runtime/Hash.cpp
        src/Runtime/InstructionDesc.cpp
        src/Runtime/NativeListFunctions.cpp
        src/Runtime/RuntimeFunctions.cpp
        src/Runtime/NativeObjectFunctions.cpp
        src/Runtime/NativeStringFunctions.cpp
        src/Runtime/Type.cpp
        src/Runtime/VM.cpp
        src/Runtime/Value.cpp
        src/Optional/Optimizer.cpp

        dependencies/xxHash/xxhash.c
        )

if (ENABLE_FORMAL)
    list(APPEND SOURCE_FILES
            src/Formal/AnalysisContext.cpp
            src/Formal/FormalAnalyzer.cpp
            src/Formal/Theorem.cpp
            src/Formal/TheoremOstream.cpp
            )
endif()

add_library(Helium ${SOURCE_FILES} ${HEADER_FILES})

target_include_directories(Helium PUBLIC include)
target_link_libraries(Helium PRIVATE fmt::fmt)

if (CMAKE_COMPILER_IS_GNUCXX)
    target_compile_options(Helium PRIVATE "-Werror=switch" "-Werror=old-style-cast"
            "-Wextra" "-Woverloaded-virtual" "-Wsign-promo")

    if (SANITIZE)
        set(SANITIZE_OPTIONS -fsanitize=address -fsanitize=leak -fsanitize=undefined)
        target_compile_options(Helium PUBLIC ${SANITIZE_OPTIONS})
        target_link_options(Helium PUBLIC ${SANITIZE_OPTIONS})
    endif()
endif()

if (ENABLE_FORMAL)
    target_compile_definitions(Helium PUBLIC HELIUM_ENABLE_FORMAL=1)
endif()

# xxHash
target_include_directories(Helium PUBLIC dependencies/xxHash)

# ZLIB
find_package(ZLIB)
if (ZLIB_FOUND)
  target_compile_definitions(Helium PUBLIC HELIUM_WITH_ZLIB=1)
  target_include_directories(Helium PUBLIC ${ZLIB_INCLUDE_DIRS})
  target_link_libraries(Helium PRIVATE ${ZLIB_LIBRARIES})
endif()

set(EXECUTABLE_SOURCE_FILES src/Helium.cpp)

# SDL2
if (NOT EMSCRIPTEN)
    find_package(SDL2)
    find_package(SDL2_ttf)

    if (SDL2_FOUND)
        list(APPEND EXECUTABLE_SOURCE_FILES src/Platform/SDL2.cpp)
    endif()

    if (SDL2_TTF_FOUND)
        list(APPEND EXECUTABLE_SOURCE_FILES src/Platform/SDL2_ttf.cpp)
    endif()
endif()

if (WIN32)
    # TODO: some explanation
    target_compile_definitions(Helium PUBLIC NOMINMAX)
    target_link_libraries(Helium PRIVATE psapi)
endif()

add_executable(HeliumExe ${EXECUTABLE_SOURCE_FILES})
target_link_libraries(HeliumExe Helium)

# SDL2
if (NOT EMSCRIPTEN)
    if (SDL2_FOUND)
        target_compile_definitions(HeliumExe PUBLIC HELIUM_WITH_SDL2=1)
        target_include_directories(HeliumExe SYSTEM PRIVATE "${SDL2_INCLUDE_DIR}")
        target_link_libraries(HeliumExe "${SDL2_LIBRARY}")
    endif()

    if (SDL2_TTF_FOUND)
        target_compile_definitions(HeliumExe PUBLIC HELIUM_WITH_SDL2_TTF=1)
        target_include_directories(HeliumExe SYSTEM PRIVATE "${SDL2_TTF_INCLUDE_DIR}")
        target_link_libraries(HeliumExe "${SDL2_TTF_LIBRARY}")
    endif()
endif()

enable_testing()
add_test(NAME tests
         COMMAND HeliumExe testrunner.he
         WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/tests)
