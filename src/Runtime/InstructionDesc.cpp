#include <Helium/Assert.hpp>
#include <Helium/Runtime/Code.hpp>

namespace Helium {

static const InstructionDesc descs[] = {
    {Opcodes::nop,          "nop",          OperandType::none,         0, 0},

    {Opcodes::args,         "args",         OperandType::integer},          // FIXME: force range
    {Opcodes::call_func,    "call_func",    OperandType::functionIndex},
    {Opcodes::call_var,     "call_var",     OperandType::none},
    {Opcodes::call_ext,     "call_ext",     OperandType::integer},
    {Opcodes::invoke,       "invoke",       OperandType::string,        1, 1},
    {Opcodes::jmp,          "jmp",          OperandType::codeAddress},
    {Opcodes::jmp_true,     "jmp.true",     OperandType::codeAddress},
    {Opcodes::jmp_false,    "jmp.false",    OperandType::codeAddress},
    {Opcodes::ret,          "ret",          OperandType::none},
    {Opcodes::op_switch,    "switch",       OperandType::switchTable},
    {Opcodes::throw_var,    "throw_var",    OperandType::none},

    {Opcodes::op_add,       "add",          OperandType::none,          2, 1},
    {Opcodes::op_div,       "div",          OperandType::none,          2, 1},
    {Opcodes::op_mod,       "mod",          OperandType::none,          2, 1},
    {Opcodes::op_mul,       "mul",          OperandType::none,          2, 1},
    {Opcodes::neg,          "neg",          OperandType::none,          1, 1},
    {Opcodes::op_sub,       "sub",          OperandType::none,          2, 1},

    {Opcodes::eq,           "eq",           OperandType::none},
    {Opcodes::grtr,         "grtr",         OperandType::none},
    {Opcodes::grtrEq,       "grtreq",       OperandType::none},
    {Opcodes::less,         "less",         OperandType::none},
    {Opcodes::lessEq,       "lesseq",       OperandType::none},
    {Opcodes::neq,          "neq",          OperandType::none},

    {Opcodes::land,         "land",         OperandType::none},
    {Opcodes::lnot,         "lnot",         OperandType::none},
    {Opcodes::lor,          "lor",          OperandType::none},
    //{Opcodes::lxor,         "lxor",          OperandType::none},

    {Opcodes::pushnil,      "pushnil",      OperandType::none},
    {Opcodes::pushc_b,      "pushc.b",      OperandType::integer},
    {Opcodes::pushc_f,      "pushc.f",      OperandType::real},
    {Opcodes::pushc_i,      "pushc.i",      OperandType::integer},
    {Opcodes::pushc_s,      "pushc.s",      OperandType::string},
    {Opcodes::pushc_func,   "pushc.func",   OperandType::functionIndex},
    {Opcodes::pushglobal,   "pushglobal",   OperandType::none},

    {Opcodes::drop,         "drop",         OperandType::none},
    {Opcodes::dup,          "dup",          OperandType::none},
    {Opcodes::dup1,         "dup1",         OperandType::none},
    {Opcodes::getLocal,     "getLocal",     OperandType::localIndex},
    {Opcodes::setLocal,     "setLocal",     OperandType::localIndex},

    {Opcodes::getIndexed,   "getIndexed",   OperandType::none},
    {Opcodes::setIndexed,   "setIndexed",   OperandType::none},

    {Opcodes::getProperty,  "getProperty",  OperandType::string},
    {Opcodes::setMember,    "setMember",    OperandType::string},

    {Opcodes::assert,       "assert",       OperandType::string},
    {Opcodes::new_list,     "new.list",     OperandType::integer},
    {Opcodes::new_obj,      "new.obj",      OperandType::none},
};

const InstructionDesc* InstructionDesc::getByOpcode(Opcode_t opcode) {
    helium_assert(opcode < Opcodes::numValidOpcodes);

    auto desc = &descs[opcode];
    helium_assert_debug(desc->opcode == opcode);
    return desc;
}

}
