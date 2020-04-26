#pragma once

#include <Helium/Runtime/Value.hpp>

#include <functional>
#include <memory>
#include <span.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace Helium
{
    using tcb::span;

    static const unsigned Helium_object_magic = 0x13377131;

    static const size_t EXTERNALS_MAX = INT16_MAX;  // TODO: limited by what?
    static const size_t LOCALS_MAX = INT16_MAX;     // TODO: limited by what?

    typedef uint16_t Opcode_t;
    typedef uint16_t FunctionIndex_t;
    typedef uint16_t LocalIndex_t;

    static constexpr LocalIndex_t LOCAL_THIS = 0;

    namespace Opcodes
    {
        enum Opcode
        {
            nop,

            // Flow control
            args,       // push an argument count on the call stack
            call_func,  // call a function resolved at compile time
            call_var,   //* call a function referenced by a variable on the stack
            call_ext,   // call an external function
            invoke,         // pop obj; invokeWithSelf obj.{STRING}
            jmp,        //* jump to a location in code
            jmp_true,   // conditional jump
            jmp_false,  // conditional jump
            ret,        // return from a function

            // Arithmetics
            op_add,
            op_div,
            op_mod,
            op_mul,
            neg,
            op_sub,

            // Comparison
            eq, grtr, grtrEq, less, lessEq, neq,

            // Logic operations
            land, lnot, lor,

            // Stack
            pushnil,    // push nil
            pushc_b,    // push boolean literal
            pushc_f,    // push const float
            pushc_i,    // push integer literal
            pushc_s,    // push const string
            pushc_func, // push const function reference
            pushglobal,
            drop,           // pop (discard)
            dup,            // push stack[top]
            dup1,           // push stack[top - 1]

            // Locals
            getLocal,       // push local (int)
            setLocal,       // pop local[INTEGER]

            // Indexed access
            getIndexed,     // pop index; pop range; push range[index]
            setIndexed,     // pop index; pop range; pop range[index]

            // Property access
            getProperty,    // pop obj; push obj.{STRING}
            setMember,      // pop obj; pop obj.{STRING}

            // Misc
            assert,     //
            new_list,   // create a list from top values on the stack
            new_obj,    // create a new empty object
            op_switch,  // optimized switch
            throw_var,  // throw an exception

#ifdef HELIUM_MIXED_MODE
            push_i,             // push.i r1

            pop_i,              // pop.i r1

            mov_i,              // mov.i r1, r2
            add_i,              // add.i r1, r2, r3
#endif

            numValidOpcodes,

            // Special (valid only during compilation)
            unknownCall, unknownPush
        };
    }

    // Exception handler definition
    struct Eh
    {
        CodeAddr_t start;
        CodeAddr_t length;
        CodeAddr_t handler;
    };

    struct SwitchTable
    {
        std::vector<ValueRef> cases;
        std::vector<CodeAddr_t> handlers;       // size: cases.size() + 1 -- the last one is the 'else' handler
    };

    enum class OperandType
    {
        codeAddress,
        functionIndex,
        integer,
        localIndex,
        none,
        real,
        string,
        switchTable,

#ifdef HELIUM_MIXED_MODE
        reg1,
        reg2,
        reg3,
#endif
    };

    struct InstructionDesc
    {
        Opcode_t opcode;
        const char* name;
        OperandType operandType;
        int numPop, numPush;

        static const InstructionDesc* getByOpcode(Opcode_t opcode);
    };

    struct InstructionOrigin
    {
        std::shared_ptr<std::string> unit, function;
        int line;

        InstructionOrigin() : line( -2 )
        {
        }

        InstructionOrigin( const InstructionOrigin* other )
                : line( other->line )
        {
            unit = other->unit;
            function = other->function;
        }
    };

    using ReadCallback = std::function<size_t(span<std::byte>)>;
    using WriteCallback = std::function<size_t(span<const std::byte>)>;

    struct Instruction
    {
        // <8-byte header
        Opcode_t opcode;          // keep
        uint16_t mode = 0;      // keep
        InstructionOrigin* origin = nullptr;    // move into another structure for cache reasons

        // 8-byte union
        int64_t integer = 0;                    // keep in union
        Real_t realValue;
        size_t switchTableIndex;
        CodeAddr_t codeAddr;
        CodeAddr_t functionIndex;
        size_t stringIndex;                     // TODO: in loaded code, resolve to direct pointer
        struct { uint16_t r0, r1, r2, r3; } reg;

        Instruction() = default;
        Instruction( const Instruction& other );
        ~Instruction();

        static bool needsRelocation( Opcode_t opcode );
    };

    // TODO: track SourceSpan ?
    struct ScriptFunction
    {
        enum class ArgumentListType {
            explicit_,
            //variadic,
        };

        static constexpr std::string_view MAIN_FUNCTION_NAME = ".main";

        std::string name;
        bool exported;      // maybe_deprecated

        ArgumentListType argumentListType;
        size_t numExplicitArguments;

        CodeAddr_t start, length;

        std::vector<Eh> exceptionHandlers;
    };

    struct Module
    {
        std::vector<std::string> dependencies;
        std::vector<ScriptFunction> functions;
        std::vector<Instruction*> code;         // aaaaaaaaaaaaaaaaaa

        std::vector<std::vector<uint8_t>> stringPool;
        std::vector<std::shared_ptr<SwitchTable>> switchTables;

        ~Module();
    };
}
