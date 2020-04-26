#ifndef HELIUM_FORMAL_THEOREM_HPP
#define HELIUM_FORMAL_THEOREM_HPP

#include <Helium/Memory/PoolPtr.hpp>

#include <memory>

#include <stx/optional.hpp>

namespace Helium {

class AstNodeExpression;
class LinearAllocator;

using TheoremAllocator = LinearAllocator;

struct SemanticPoint {
    size_t index;
};

class Theorem {
public:
    enum class Type {
        expressionIsTrue,
    };

    explicit Theorem(Type type) : type(type) {}
    virtual ~Theorem() {}

    virtual bool contains(const AstNodeExpression& expr) const = 0;
    virtual pool_ptr<Theorem> replace(const AstNodeExpression& needle,
                                      pool_ptr<AstNodeExpression> replacement,
                                      TheoremAllocator& allocator) const = 0;

    auto getType() const { return type; }

    //bool isValid() const { return !validUntil; }

private:
    Type type;
};

class TheoremExpressionIsTrue : public Theorem {
public:
    explicit TheoremExpressionIsTrue(pool_ptr<AstNodeExpression>&& expr)
            : Theorem(Theorem::Type::expressionIsTrue), expr(std::move(expr)) {
    }

    bool contains(const AstNodeExpression& expr) const override;
    pool_ptr<Theorem> replace(const AstNodeExpression& needle,
                              pool_ptr<AstNodeExpression> replacement,
                              TheoremAllocator& allocator) const override;

    const AstNodeExpression& getExpression() const { return *expr; }

private:
    pool_ptr<AstNodeExpression> expr;
};

bool expressionEqualsTo(const AstNodeExpression& a, const AstNodeExpression& b);

}

#endif //HELIUM_FORMAL_THEOREM_HPP
