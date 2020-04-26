#include <Helium/Formal/Theorem.hpp>

#include <Helium/Compiler/Ast.hpp>
#include <Helium/Memory/LinearAllocator.hpp>

namespace Helium {

static bool expressionEqualsToOrContains(const AstNodeExpression& haystack, const AstNodeExpression& needle);

bool expressionEqualsTo(const AstNodeExpression& a, const AstNodeExpression& b) {
    if (a.type != b.type)
        return false;

    switch (a.type) {
    case AstNodeExpression::Type::binaryExpr: {
        auto& a_binaryExpr = static_cast<const AstNodeBinaryExpr&>(a);
        auto& b_binaryExpr = static_cast<const AstNodeBinaryExpr&>(b);

        return a_binaryExpr.binaryExprType == b_binaryExpr.binaryExprType
                && expressionEqualsTo(*a_binaryExpr.getLeft(), *b_binaryExpr.getLeft())
                && expressionEqualsTo(*a_binaryExpr.getRight(), *b_binaryExpr.getRight());
    }

    case AstNodeExpression::Type::call: {
        auto& a_call = static_cast<const AstNodeCall&>(a);
        auto& b_call = static_cast<const AstNodeCall&>(b);

        return a_call.isBlindCall() == b_call.isBlindCall()
                && expressionEqualsTo(*a_call.getArguments(), *b_call.getArguments())
                && expressionEqualsTo(*a_call.getCallable(), *b_call.getCallable());
    }

    case AstNodeExpression::Type::identifier: {
        auto& a_ident = static_cast<const AstNodeIdent&>(a);
        auto& b_ident = static_cast<const AstNodeIdent&>(b);

        helium_assert(a_ident.ns == b_ident.ns);

        return a_ident.name == b_ident.name;
    }

        case AstNodeExpression::Type::literal: {
            auto& a_literal = static_cast<const AstNodeLiteral&>(a);
            auto& b_literal = static_cast<const AstNodeLiteral&>(b);

            if (a_literal.literalType != b_literal.literalType)
                return false;

            switch (a_literal.literalType) {
                case AstNodeLiteral::Type::boolean:
                    return static_cast<const AstNodeLiteralBoolean&>(a_literal).value
                           == static_cast<const AstNodeLiteralBoolean&>(b_literal).value;

                case AstNodeLiteral::Type::integer:
                    return static_cast<const AstNodeLiteralInteger&>(a_literal).value
                           == static_cast<const AstNodeLiteralInteger&>(b_literal).value;

                case AstNodeLiteral::Type::nil:
                    return true;

                case AstNodeLiteral::Type::object:
                    // TODO: best approach?
                    return false;

                case AstNodeLiteral::Type::real:
                    return static_cast<const AstNodeLiteralReal&>(a_literal).value
                           == static_cast<const AstNodeLiteralReal&>(b_literal).value;

                case AstNodeLiteral::Type::string:
                    return static_cast<const AstNodeLiteralString&>(a_literal).text
                           == static_cast<const AstNodeLiteralString&>(b_literal).text;
            }

            // actually never reached
            return false;
        }

    default:
        // FIXME: remove this to catch incomplete switch at compile time
        helium_assert(false);
        return false;
    }
}

static bool expressionContains(const AstNodeExpression& haystack, const AstNodeExpression& needle) {
    switch (haystack.type) {
    case AstNodeExpression::Type::binaryExpr: {
        auto& binaryExpr = static_cast<const AstNodeBinaryExpr&>(haystack);
        return expressionEqualsToOrContains(*binaryExpr.getLeft(), needle)
                || expressionEqualsToOrContains(*binaryExpr.getRight(), needle);
    }

    case AstNodeExpression::Type::identifier:
    case AstNodeExpression::Type::literal:
        return false;

    default:
        // FIXME: remove this to catch incomplete switch at compile time
        helium_assert(false);
        return false;
    }
}

static bool expressionEqualsToOrContains(const AstNodeExpression& haystack, const AstNodeExpression& needle) {
    return expressionEqualsTo(haystack, needle) || expressionContains(haystack, needle);
}

static pool_ptr<AstNodeExpression> replaceSubExpression(pool_ptr<AstNodeExpression> haystack,
                                                        const AstNodeExpression& needle,
                                                        pool_ptr<AstNodeExpression> replacement,
                                                        AstAllocator& allocator) {
    if (expressionEqualsTo(*haystack, needle))
        return replacement;

    switch (haystack->type) {
    case AstNodeExpression::Type::binaryExpr: {
        auto& binaryExpr = static_cast<const AstNodeBinaryExpr&>(*haystack);

        return allocator.make_pooled<AstNodeBinaryExpr>(binaryExpr.binaryExprType,
                                                        replaceSubExpression(binaryExpr.getLeft2(), needle, replacement, allocator),
                                                        replaceSubExpression(binaryExpr.getRight2(), needle, replacement, allocator),
                                                        binaryExpr.span);
    }

    case AstNodeExpression::Type::identifier:
    case AstNodeExpression::Type::literal:
        return haystack;

    default:
        // FIXME: remove this to catch incomplete switch at compile time
        helium_assert(false);
        return {};
    }
}

bool TheoremExpressionIsTrue::contains(const AstNodeExpression& expr) const {
    return expressionEqualsToOrContains(*this->expr, expr);
}

pool_ptr<Theorem> TheoremExpressionIsTrue::replace(const AstNodeExpression& needle,
                                                   pool_ptr<AstNodeExpression> replacement,
                                                   TheoremAllocator& allocator) const {
    return allocator.make_pooled<TheoremExpressionIsTrue>(
            replaceSubExpression(this->expr, needle, replacement, allocator));
}

}
