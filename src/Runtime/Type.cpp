#include <Helium/Compiler/Type.hpp>

namespace Helium
{
    static Type builtinTypeInt{ BuiltinType::int_, "Int" };

    Type* Type::builtinInt() {
        return &builtinTypeInt;
    }

    const char* Type::toString(Type* maybeType) {
        if (maybeType)
            return maybeType->name;
        else
            return "dynamic";
    }
}
