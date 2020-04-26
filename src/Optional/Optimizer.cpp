#include <Helium/Compiler/Optimizer.hpp>

namespace Helium
{
    using std::string;

    static bool isReferenced( Script* script, unsigned index )
    {
        auto& code = script->code;

        for ( size_t i = 0; i < code.size(); i++ )
        {
            Instruction* current = code[i];
            Opcode_t op = current->opcode;

            if ( Instruction::needsRelocation( op ) && current->codeAddr == index )
            {
                //printf( "referenced from %i\n", i );
                return true;
            }
            else if ( op == Opcodes::op_switch )
            {
                auto switchTable = script->switchTables[current->switchTableIndex];

                for ( unsigned i = 0; i < switchTable->handlers.size(); i++ )
                    if ( switchTable->handlers[i] == index )
                        return true;
            }
        }

        return false;
    }

    /*static Variable merge( Variable left, Variable right, Opcodes::Opcode op, InstructionOrigin* origin )
    {
#define caseFor( operation_ )\
            case Opcodes::op_##operation_:\
                return left.operation_( right, origin );

        switch ( op )
        {
            caseFor( add )
            caseFor( div )
            caseFor( mod )
            caseFor( mul )
            caseFor( sub )

            default:
                throw Variable::newException( "InternalOptimizationError", ( String )"No #merge rule defined for opcode " + ( int )op, origin );
        }
    }*/

    static void removeInstruction( Script* script, unsigned index )
    {
        auto& code = script->code;

        delete code[index];
        code.erase(code.begin() + index);

        for ( size_t i = 0; i < code.size(); i++ )
        {
            Instruction* current = code[i];
            Opcode_t op = current->opcode;

            if ( Instruction::needsRelocation( op ) && current->codeAddr > index )
                current->codeAddr--;
            else if ( op == Opcodes::op_switch )
            {
                auto switchTable = script->switchTables[current->switchTableIndex];

                for ( unsigned i = 0; i < switchTable->handlers.size(); i++ )
                    if ( switchTable->handlers[i] > index )
                        switchTable->handlers[i]--;
            }
        }

        // Fixup functions
        for (auto& func : script->functions) {
            if (index < func.start)
                func.start--;

            if (index >= func.start + func.length)
                continue;

            // Fixup exception handlers where needed
            for (auto& eh : func.exceptionHandlers) {
                if (index < eh.start)
                    eh.start--;
                else if (index < eh.start + eh.length)
                    eh.length--;

                if (index < eh.handler)
                    eh.handler--;
            }
        }
    }

    void Optimizer::optimize( Script* script )
    {
        for ( size_t i = 0; i < script->code.size(); i++ )
        {
            // ALWAYS remove instructions in the REVERSE ORDER!!!

            Instruction* current = script->code[i];
            Instruction* next = ( i + 1 < script->code.size() ? script->code[i + 1] : nullptr );
            Instruction* next2 = ( i + 2 < script->code.size() ? script->code[i + 2] : nullptr );

            // pushc - drop
            // description:     Pushes a constant and drops it again immediately
            // appears in:      After certains optimizations
            // general rule:    pushc; drop => --
            if ( next != nullptr
                    && (current->opcode == Opcodes::pushc_f || current->opcode == Opcodes::pushc_func || current->opcode == Opcodes::pushc_i || current->opcode == Opcodes::pushc_s)
                    && next->opcode == Opcodes::drop && !isReferenced( script, i + 1 ) )
            {
                removeInstruction( script, i + 1 );
                removeInstruction( script, i );

                i = std::max<int>( 0, i - 3 );
                continue;
            }
        }
    }

    void Optimizer::optimizeWithStatistics( Script* script )
    {
        unsigned lengthBefore = script->code.size();
        Helium::Optimizer::optimize( script );
        unsigned lengthAfter = script->code.size();

        int lengthDiff = lengthBefore - lengthAfter;

        if ( lengthDiff > 0 )
            printf( "%i instructions (%i %%) optimized out.\n\n", lengthDiff, lengthDiff * 100 / lengthBefore );
    }
}
