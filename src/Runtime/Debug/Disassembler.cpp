#include <Helium/Assert.hpp>
#include <Helium/Runtime/Debug/Disassembler.hpp>

#include <stx/optional.hpp>
#include <sstream>

#include <fmt/format.h>

namespace Helium
{
    using fmt::format;
    using stx::optional;
    using std::string;

    static ScriptFunction const* tryGetFunctionAtPC(Script const& script, CodeAddr_t pc) {
        for (size_t i = 0; i < script.functions.size(); i++) {
            if (pc >= script.functions[i].start && pc < script.functions[i].start + script.functions[i].length)
                return &script.functions[i];
        }

        return {};
    }

    static std::string variableAsString( Value var )
    {
        switch ( var.type )
        {
            case ValueType::nil:
                return "nil";

            case ValueType::integer:
                return std::to_string(var.integerValue);

            case ValueType::real:
                return std::to_string(var.realValue);

            case ValueType::string:
                return std::string("'") + var.string->text + "'";

            default:
                helium_assert(false);
        }
    }

    void Disassembler::disassemble(Script const& script, LineSink const& output)
    {
        output(format("; {} functions in module", script.functions.size()));

        for (size_t i = 0; i < script.functions.size(); i++) {
            output(format("def `{}` at {:04X}h length {:04X}h\t; {:3d}, exported: {:5}, {} explicit arguments, {} exception handlers",
                               script.functions[i].name, script.functions[i].start, script.functions[i].length,
                               i, script.functions[i].exported, script.functions[i].numExplicitArguments, script.functions[i].exceptionHandlers.size()
                               ));
        }

        output("");
        output(format("; {} dependencies", script.dependencies.size()));

        for (size_t i = 0; i < script.dependencies.size(); i++) {
            output(format("import `{}`\t; {:3d}", script.dependencies[i], i));
        }

        output("");

        for ( size_t pc = 0; pc < script.code.size(); pc++ )
        {
            Instruction* current = script.code[pc];

            auto maybeFunc = tryGetFunctionAtPC(script, pc);

            char buffer[100];

            if (maybeFunc && pc == maybeFunc->start) {
                auto& func = *maybeFunc;

                std::stringstream ss;
                ss << "; def `" << func.name << "`(";

                switch (func.argumentListType) {
                case ScriptFunction::ArgumentListType::explicit_: {
                    for (size_t i = 0; i < func.numExplicitArguments; i++) {
                        if (i > 0)
                            ss << ", ";

                        ss << "arg" << i;
                    }
                    break;
                }
                }

                ss << ")";
                output(ss.str());

                for (size_t i = 0; i < func.exceptionHandlers.size(); i++) {
                    const auto& eh = func.exceptionHandlers[i];

                    snprintf(buffer, sizeof(buffer), "; eh %zu: <%04X; %04X) => %04X", i, eh.start, eh.start + eh.length, eh.handler);
                    output(buffer);
                }
            }

            std::stringstream ss;
            ss << format("{:04x}\t{:02x}\t", pc, current->opcode);

            auto desc = InstructionDesc::getByOpcode(current->opcode);

            if (!desc) {
                ss << format("Unknown instruction opcode {:02x}h!", current->opcode);
                output( ss.str() );
                continue;
            }

            ss << desc->name;

            switch ( desc->operandType ) {
                case OperandType::none:
                    break;

                case OperandType::codeAddress:
                    snprintf( buffer, sizeof( buffer ), " %04Xh", static_cast<unsigned int>(current->codeAddr) );
                    ss << buffer;
                    break;

                case OperandType::functionIndex:
                    helium_assert_userdata(current->functionIndex < script.functions.size());

                    snprintf( buffer, sizeof( buffer ), " %04Xh\t; `%s`", static_cast<unsigned int>(current->functionIndex),
                              script.functions[current->functionIndex].name.c_str());
                    ss << buffer;
                    break;

                case OperandType::integer:
                    ss << " " << current->integer;
                    break;

                case OperandType::localIndex:
                    ss << " " << current->integer;

                    if (current->integer == LOCAL_THIS) {
                        ss << "\t; `this`";
                    }
                    break;

                case OperandType::real:
                    ss << " " << current->realValue;
                    break;

#ifdef HELIUM_MIXED_MODE
                case OperandType::reg1:
                    snprintf(buffer, sizeof(buffer), " #%d", current->reg.r0);
                    ss << buffer;
                    break;

                case OperandType::reg2:
                    snprintf(buffer, sizeof(buffer), " #%d, #%d", current->reg.r0, current->reg.r1);
                    ss << buffer;
                    break;

                case OperandType::reg3:
                    snprintf(buffer, sizeof(buffer), " #%d, #%d, #%d", current->reg.r0, current->reg.r1, current->reg.r2);
                    ss << buffer;
                    break;
#endif

                case OperandType::string: {
                    ss << " " << current->stringIndex << "\t; '";
                    const auto& string = script.stringPool[current->stringIndex];
                    ss << std::string_view(reinterpret_cast<const char*>(string.data()), string.size());
                    ss << "'";
                    break;
                }

                case OperandType::switchTable:
                {
                    output(ss.str());
                    std::stringstream().swap(ss);

                    auto switchTable = script.switchTables[current->switchTableIndex];

                    for ( unsigned i = 0; i < switchTable->handlers.size(); i++ )
                    {
                        ss << "              " ;

                        if ( i < switchTable->cases.size() )
                            ss << "case " << variableAsString( switchTable->cases[i] ).c_str();
                        else
                            ss << "default";

                        snprintf( buffer, sizeof( buffer ), "%04Xh", switchTable->handlers[i] );
                        ss << " : " << buffer;
                        output(ss.str());
                        std::stringstream().swap(ss);
                    }

                    break;
                }
            }

            output(ss.str());
        }

        output("");

        output(format("; {} strings", script.stringPool.size()));

        for (size_t i = 0; i < script.stringPool.size(); i++) {
            const auto& string = script.stringPool[i];
            auto sv = std::string_view(reinterpret_cast<const char*>(string.data()), string.size());

            output(format("string '{}'\t; {:3d}", sv, i));
        }
    }
}
