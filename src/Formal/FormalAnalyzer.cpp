#include "AnalysisContext.hpp"

#include <Helium/Compiler/Ast.hpp>
#include <Helium/Formal/FormalAnalyzer.hpp>

#include <map>

namespace Helium {

/*static pool_ptr<AstNodeExpression> copyExpression(const AstNodeExpression* expr, AstAllocator& allocator) {
    switch (expr->type) {
    case AstNodeExpression::Type::literal: {
        auto literal = static_cast<const AstNodeLiteral*>(expr);
        switch (literal->literalType) {
            case AstNodeLiteral::Type::boolean:
                return allocator.make_pooled<AstNodeLiteralBoolean>(
                        static_cast<const AstNodeLiteralBoolean*>(literal)->value, literal->span);

            case AstNodeLiteral::Type::integer:
                return allocator.make_pooled<AstNodeLiteralInteger>(
                        static_cast<const AstNodeLiteralInteger*>(literal)->value, literal->span);

            case AstNodeLiteral::Type::nul:
                return allocator.make_pooled<AstNodeLiteral>(AstNodeLiteral::Type::nul, literal->span);

            case AstNodeLiteral::Type::object:
                helium_assert(literal->literalType != AstNodeLiteral::Type::object);    // FIXME
                return {};

            case AstNodeLiteral::Type::real:
                return allocator.make_pooled<AstNodeLiteralReal>(
                        static_cast<const AstNodeLiteralReal*>(literal)->value, literal->span);

            case AstNodeLiteral::Type::string:
                return allocator.make_pooled<AstNodeLiteralString>(
                        std::string(static_cast<const AstNodeLiteralString*>(literal)->text), literal->span);
        }
        break;
    }

    default:
        // FIXME: remove this to catch incomplete switch at compile time
        helium_assert(false);
        return {};
    }
}*/

class InferrenceRulePattern {
public:
    InferrenceRulePattern(pool_ptr<AstNodeExpression>&& premise, pool_ptr<AstNodeExpression>&& conclusion)
            : premise(premise), conclusion(conclusion) {
    }

    virtual bool doesHoldForVariables(const std::map<AstNodeExpression*, AstNodeExpression*>& boundVariables) {
        return true;
    }

    // Strategy: pattern-match premise & conclusion
    //  verify doesHold
    bool doesFollowFrom(const AstNodeExpression& premise, const AstNodeExpression& conclusion);

private:
    bool doesFollowFrom(const AstNodeExpression& premise, const AstNodeExpression& conclusion, std::map<AstNodeExpression*, AstNodeExpression*>& boundVariables);

    pool_ptr<AstNodeExpression> premise, conclusion;
};

bool InferrenceRulePattern::doesFollowFrom(const AstNodeExpression& premise, const AstNodeExpression& conclusion) {
    std::map<AstNodeExpression*, AstNodeExpression*> boundVariables;

    if (doesFollowFrom(premise, conclusion, boundVariables) && doesHoldForVariables(boundVariables))
        return true;
    else
        return false;
}

bool InferrenceRulePattern::doesFollowFrom(const AstNodeExpression& premise, const AstNodeExpression& conclusion,
                                           std::map<AstNodeExpression*, AstNodeExpression*>& boundVariables) {
    return false;
}

FormalAnalyzer::FormalAnalyzer(FormalDiagnosticReceiver* diagnosticReceiver) : diagnosticReceiver(diagnosticReceiver) {
}

void FormalAnalyzer::verifyBlock(AnalysisContext& parentContext, const AstNodeBlock* block) {
    //AnalysisContext context{parentContext};
    //auto span = block->getFullSpan();
    //cout << "Context " << context.getId() << " for block @ lines " << span.start.line << "-" << span.end.line << std::endl;
    auto& context = parentContext;

    auto& thA = context.getTheoremAllocator();

    for (auto& statement_ : block->getStatements()) {
        auto statement = statement_.get();
        // Each statement might introduce new theorems and invalidate others

        auto start = context.currentPoint();

        switch (statement->type) {
            case AstNodeStatement::Type::assert: {
                auto assert = static_cast<const AstNodeAssert*>(statement);
                auto expr = assert->getExpression();

                // We must demonstrate that `expr` is true
                bool found = false;

                for (auto theorem : context.assumptionsValidAt(start)) {
                    if (theorem->getType() == Theorem::Type::expressionIsTrue) {
                        auto* theoremExpressionIsTrue = static_cast<const TheoremExpressionIsTrue*>(theorem);

                        if (expressionEqualsTo(*expr,
                                               theoremExpressionIsTrue->getExpression())) {
                            // TODO: log how demonstrated
                            found = true;
                            break;
                        }
                    }
                }

                if (!found) {
                    diagnosticReceiver->error(assert->span, "Failed to demonstrate that assertion `" + assert->getExpressionString() + "` holds true");
                }

                break;
            }

            case AstNodeStatement::Type::assignment: {
                auto assignment = static_cast<const AstNodeAssignment*>(statement);
                auto target = assignment->getTarget();
                auto expr = assignment->getExpression();

                auto afterAssign = context.nextPoint();

                // Invalidate assumptions about `target`
                for (auto theorem : context.assumptionsValidAt(start)) {
                    if (theorem->contains(*target)) {
                        constexpr bool reassignmentPermissible = false;

                        if (reassignmentPermissible)
                            //context.invalidateAssumption(theorem);
                            ;
                        else
                            abort();
                    }
                }

                // Establish that `target` == `expr`

                auto equals = allocator.make_pooled<AstNodeBinaryExpr>(AstNodeBinaryExpr::Type::equals,
                        assignment->getTarget2(), assignment->getExpression2(), assignment->span);

                context.assumeAt(afterAssign, thA.make_pooled<TheoremExpressionIsTrue>(equals),
                                 "assignment.equivalence");

                // Establish new assumptions about `target`
                for (auto theorem : context.assumptionsValidAt(start)) {
                    if (!theorem->contains(*expr))
                        continue;

                    auto newTheorem = theorem->replace(*expr, assignment->getTarget2(), thA);

                    context.assumeAt(afterAssign, std::move(newTheorem), "assignment.transitive");
                }

                break;
            }

            case AstNodeStatement::Type::block: {
                auto blockNode = static_cast<const AstNodeBlock*>(statement);
                verifyBlock(context, blockNode);
                break;
            }

            case AstNodeStatement::Type::infer: {
                auto assert = static_cast<const AstNodeInfer*>(statement);
                auto expr = assert->getExpression();

                // Attempt to infer that `expr` is true

                constexpr bool isExplicit = true;

                if (!isExplicit) {
                    ...
                }

                break;
            }

            //case AstNodeStatement::Type::return_:
            //    break;

            default:
                helium_assert(statement->type != statement->type);
        }
    }
}

void FormalAnalyzer::verifyFunction(const AnalysisContext& parentContext, const AstNodeFunction* function) {
    AnalysisContext context(parentContext);

    // TODO: arguments

    this->verifyBlock(context, function->getBody());
}

void FormalAnalyzer::verifyScript(const AstNodeScript* script) {
    AnalysisContext nullContext;

    verifyFunction(nullContext, script->mainFunction.get());

    for (auto& function : script->functions) {
        verifyFunction(nullContext, function.get());
    }
}

}
