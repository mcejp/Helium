#pragma once

namespace Helium
{
    enum class BuiltinType {
        none,
        int_,
    };

    class Type {
        public:
            bool isInt() const { return builtinType == BuiltinType::int_; }

            static Type* builtinInt();
            static const char* toString(Type* maybeType);

        //private:
            BuiltinType builtinType;
            const char* name;
    };

    constexpr inline bool isInt(const Type* type) { return type != nullptr && type->builtinType == BuiltinType::int_; }
}
