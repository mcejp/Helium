#include <Helium/Formal/TheoremOstream.hpp>
#include <Helium/Formal/Theorem.hpp>

#include <Helium/Compiler/Ast.hpp>

namespace Helium {

std::ostream& operator<<(std::ostream& stream, const AstNodeExpression& expression) {
    switch (expression.type) {
    case AstNodeExpression::Type::binaryExpr: {
        auto& binaryExpr = static_cast<const AstNodeBinaryExpr&>(expression);

        stream << *binaryExpr.getLeft() << " " << binaryExpr.getSymbol() << " " << *binaryExpr.getRight();
        break;
    }

    case AstNodeExpression::Type::identifier: {
        auto& ident = static_cast<const AstNodeIdent&>(expression);

        // TODO: handle ident.ns
        helium_assert(ident.ns == AstNodeIdent::Namespace::unknown);

        stream << ident.name;
        break;
    }

    case AstNodeExpression::Type::literal: {
        auto& literal = static_cast<const AstNodeLiteral&>(expression);

        switch (literal.literalType) {
            case AstNodeLiteral::Type::boolean: stream << static_cast<const AstNodeLiteralBoolean&>(literal).value; break;
            case AstNodeLiteral::Type::integer: stream << static_cast<const AstNodeLiteralInteger&>(literal).value; break;
            case AstNodeLiteral::Type::nul: stream << "nul"; break;
            case AstNodeLiteral::Type::object: helium_assert(false); /* FIXME */ break;
            case AstNodeLiteral::Type::real: stream << static_cast<const AstNodeLiteralReal&>(literal).value; break;
            case AstNodeLiteral::Type::string: stream << static_cast<const AstNodeLiteralString&>(literal).text; break;
        }
        break;
    }

    default:
        // FIXME: remove this to catch incomplete switch at compile time
        helium_assert(false);
    }

    return stream;
}

std::ostream& operator<<(std::ostream& stream, const Theorem& theorem) {
    switch (theorem.getType()) {
    case Theorem::Type::expressionIsTrue: {
        auto& theoremExpressionIsTrue = static_cast<const TheoremExpressionIsTrue&>(theorem);

        stream << theoremExpressionIsTrue.getExpression();
        break;
    }
    }

    return stream;
}

}
