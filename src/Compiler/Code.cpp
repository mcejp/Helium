#include <Helium/Config.hpp>
#include <Helium/Runtime/Code.hpp>

#if HELIUM_TRACE_VALUES
#include <Helium/Runtime/Debug/ValueTrace.hpp>
#endif

namespace Helium
{
    const unsigned short HeliumObject_magic = 0x2211;

#if 0
    static bool serializeVariable(stream, Variable var) {
        stream->writeLE<uint8_t>(var.type);

        switch (var.type)
        {
        case ValueType::list:
            stream->writeLE<uint32_t>(var.list->length);

            for (size_t i = 0; i < var.list->length; i++)
                serializeVariable(stream, var.list->items[i]);

            break;

        case ValueType::integer:
            stream->writeLE<int64_t>(var.integerValue);
            break;

        case ValueType::nil:
            break;

        case ValueType::real:
            stream->write(&var.realValue, sizeof(var.realValue));
            break;

        case ValueType::object:
            stream->writeLE<uint16_t>(var.object->numMembers);

            for (size_t i = 0; i < var.object->numMembers; i++)
            {
                stream->writeString(var.object->members[i].key);
                serializeVariable(stream, var.object->members[i].value);
            }

            break;

        case ValueType::string:
            stream->writeString(var.string->text);
            break;
        }

        return true;
    }
#endif

#if 0
    bool deserializeVariable(stream, Variable* varRef_out) {
        uint8_t type;
        
        if (!stream->readLE(&type))
            return false;

        switch (type)
        {
        case ValueType::integer: {
            int64_t integerValue;

            if (!stream->readLE(&integerValue))
                return false;

            *varRef_out = Variable::newInteger(integerValue);
            return true;
        }

        case ValueType::nil:
            *varRef_out = Variable::newNul();
            return true;

        case ValueType::real: {
            double realValue;

            if (!stream->read(&realValue, sizeof(realValue)))
                return false;

            *varRef_out = Variable::newReal(realValue);
            return true;
        }

        case ValueType::object: {
            uint16_t numMembers;

            if (!stream->readLE(&numMembers))
                return false;

            AutoVariable object = Variable::newObject(nullptr);

            for (uint16_t i = 0; i < numMembers; i++) {
                name = stream->readString();
                Variable nextRef;
                
                if (!deserializeVariable(stream, &nextRef))
                    return false;

                object.setMember(name, nextRef);
            }

            *varRef_out = object.detach();
            return true;
        }

        case ValueType::string: {
            value = stream->readString();
            *varRef_out = Variable::newString(value, value.getNumBytes());
            return true;
        }
        }
    }
#endif

    Instruction::Instruction( const Instruction& other )
            : opcode( other.opcode ),
              codeAddr( other.codeAddr ),
              functionIndex( other.functionIndex ),
              integer( other.integer ),
              realValue( other.realValue ),
              stringIndex( other.stringIndex ),
              switchTableIndex( other.switchTableIndex )
    {
        origin = other.origin ? new InstructionOrigin( other.origin ) : 0;
    }

    Instruction::~Instruction()
    {
        delete origin;
    }

    bool Instruction::needsRelocation( Opcode_t opcode )
    {
        return opcode == Opcodes::jmp
               || opcode == Opcodes::jmp_true
               || opcode == Opcodes::jmp_false;
    }

    Module::~Module()
    {
#if HELIUM_TRACE_VALUES
        ValueTraceCtx tracking_ctx("~Script");
#endif

        switchTables.clear();

        code.clear();
    }
}
