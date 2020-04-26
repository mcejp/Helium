#pragma once

#include <Helium/Assert.hpp>
#include <Helium/Compiler/Compiler.hpp>
#include <Helium/Memory/PoolPtr.hpp>
#include <Helium/Runtime/Value.hpp>

#include <stx/string_view.hpp>
#include <vector>

namespace Helium
{
    class LinearAllocator;

    using AstAllocator = LinearAllocator;

    struct AstNodeBase {
        const SourceSpan span;

        explicit AstNodeBase(SourceSpan span) : span{span} {}
        virtual ~AstNodeBase() {}

        virtual SourceSpan getFullSpan() const { helium_assert(false); return span; }
    };

    // ============================================================================================================= //
    //  EXPRESSIONS
    // ============================================================================================================= //

    class AstNodeExpression : public AstNodeBase {
    public:
        enum class Type {
            binaryExpr,
            call,
            function,
            identifier,
            indexed,
            list,
            literal,
            property,
            unaryExpr,
        };

        AstNodeExpression(Type type, SourceSpan span) : AstNodeBase(span), type(type) {
        }

        //virtual void print( int indent = 0 );

        const Type type;
    };

    class AstNodeBinaryExpr : public AstNodeExpression
    {
    public:
        enum class Type {
            add, subtract,
            divide, modulo, multiply,
            and_, or_,
            equals, greater, greaterEq, less, lessEq, notEquals
        };

        AstNodeBinaryExpr(Type type, pool_ptr<AstNodeExpression>&& left, pool_ptr<AstNodeExpression>&& right, SourceSpan span)
                : AstNodeExpression(AstNodeExpression::Type::binaryExpr, span), binaryExprType(type), left(std::move(left)), right(std::move(right)) {
        }

        SourceSpan getFullSpan() const override { return SourceSpan::union_(left->getFullSpan(), right->getFullSpan()); }

        const AstNodeExpression* getLeft() const { return left.get(); }
        pool_ptr<AstNodeExpression> getLeft2() const { return left; }
        const AstNodeExpression* getRight() const { return right.get(); }
        pool_ptr<AstNodeExpression> getRight2() const { return right; }

        stx::string_view getSymbol() const {
            using stx::string_view;
            switch (binaryExprType) {
                case Type::add:         return "+";
                case Type::and_:        return "&&";
                case Type::divide:      return "/";
                case Type::equals:      return "==";
                case Type::greater:     return ">";
                case Type::greaterEq:   return ">=";
                case Type::less:        return "<";
                case Type::lessEq:      return "<=";
                case Type::modulo:      return "%";
                case Type::multiply:    return "*";
                case Type::notEquals:   return "!=";
                case Type::or_:         return "||";
                case Type::subtract:    return "-";
            }
        }

        const Type binaryExprType;

    private:
        const pool_ptr<AstNodeExpression> left, right;
    };

    class AstNodeList : public AstNodeExpression
    {
    public:
        explicit AstNodeList(SourceSpan span) : AstNodeExpression(AstNodeExpression::Type::list, span) {
        }

        SourceSpan getFullSpan() const override;

        void emplace_back(pool_ptr<AstNodeExpression>&& item) {
            items.emplace_back(std::move(item));
        }

        const auto& getItems() const { return items; }

    private:
        std::vector<pool_ptr<AstNodeExpression>> items;
    };

    class AstNodeCall : public AstNodeExpression
    {
    public:
        AstNodeCall(pool_ptr<AstNodeExpression>&& callable, pool_ptr<AstNodeList>&& arguments, SourceSpan span, bool blindCall)
                : AstNodeExpression(AstNodeExpression::Type::call, span), callable(std::move(callable)), arguments(std::move(arguments)), blindCall{blindCall} {
        }

        auto getArguments() const { return arguments.get(); }
        auto getCallable() const { return callable.get(); }

        SourceSpan getFullSpan() const override { return SourceSpan::union_(callable->getFullSpan(), arguments->getFullSpan()); }

        bool isBlindCall() const { return blindCall; }

    private:
        const pool_ptr<AstNodeExpression> callable;
        const pool_ptr<AstNodeList> arguments;
        const bool blindCall;
    };

    class AstNodeExprIndexed : public AstNodeExpression
    {
    public:
        AstNodeExprIndexed(pool_ptr<AstNodeExpression>&& range, pool_ptr<AstNodeExpression>&& index, SourceSpan span)
                : AstNodeExpression(AstNodeExpression::Type::indexed, span), range(std::move(range)), index(std::move(index)) {
        }

        SourceSpan getFullSpan() const override { return SourceSpan::union_(range->getFullSpan(), index->getFullSpan()); }

        const pool_ptr<AstNodeExpression> range, index;
    };

    class AstNodeIdent : public AstNodeExpression
    {
    public:
        enum class Namespace {
            unknown,            // plain identifier, scope unknown (default)
            local,              // identifier forced to be a local variable (e.g. "foo = local bar;")
        };

        AstNodeIdent(std::string&& name, Namespace ns, SourceSpan span)
                : AstNodeExpression(AstNodeExpression::Type::identifier, span),
                  name(std::move(name)),
                  ns(ns) {
        }

        SourceSpan getFullSpan() const override { return span; }

        const std::string name;
        const Namespace ns;
    };

    class AstNodeLiteral : public AstNodeExpression {
    public:
        enum class Type {
            boolean,
            integer,
            nil,
            object,
            real,
            string,
        };

        AstNodeLiteral(Type type, SourceSpan span)
                : AstNodeExpression( AstNodeExpression::Type::literal, span), literalType(type) {
        }

        SourceSpan getFullSpan() const override { return span; }

        const Type literalType;
    };

    class AstNodeLiteralBoolean : public AstNodeLiteral
    {
    public:
        AstNodeLiteralBoolean(bool value, SourceSpan span)
                : AstNodeLiteral(AstNodeLiteral::Type::boolean, span), value(value) {
        }

        const bool value;
    };

    class AstNodeLiteralInteger : public AstNodeLiteral
    {
    public:
        AstNodeLiteralInteger(Int_t value, SourceSpan span)
                : AstNodeLiteral(AstNodeLiteral::Type::integer, span), value(value) {
        }

        const Int_t value;
    };

    class AstNodeLiteralObject : public AstNodeLiteral
    {
    public:
        explicit AstNodeLiteralObject(SourceSpan span)
                : AstNodeLiteral(AstNodeLiteral::Type::object, span) {
        }

        void addProperty(std::string&& propertyName, pool_ptr<AstNodeExpression>&& expression) {
            properties.emplace_back(std::move(propertyName), std::move(expression));
        }

        const auto& getProperties() const { return properties; }

    private:
        // not a map, accessed mostly in order
        std::vector<std::pair<std::string, pool_ptr<AstNodeExpression>>> properties;
    };

    class AstNodeLiteralReal : public AstNodeLiteral
    {
    public:
        AstNodeLiteralReal(Real_t value, SourceSpan span)
                : AstNodeLiteral(AstNodeLiteral::Type::real, span), value(value) {
        }

        const Real_t value;
    };

    class AstNodeLiteralString : public AstNodeLiteral
    {
    public:
        AstNodeLiteralString(std::string&& text, SourceSpan span)
                : AstNodeLiteral(AstNodeLiteral::Type::string, span), text(std::move(text)) {
        }

        const std::string text;
    };

    class AstNodeProperty : public AstNodeExpression {
    public:
        // TODO: we should enforce that the identifier is not annotated (`local foo`)
        AstNodeProperty(pool_ptr<AstNodeExpression>&& object, pool_ptr<AstNodeIdent>&& propertyName, SourceSpan span)
                : AstNodeExpression(AstNodeExpression::Type::property, span), object(std::move(object)), propertyName(std::move(propertyName)) {
        }

        SourceSpan getFullSpan() const override { return SourceSpan::union_(object->getFullSpan(), propertyName->getFullSpan()); }

        const pool_ptr<AstNodeExpression> object;
        const pool_ptr<AstNodeIdent> propertyName;
    };

    class AstNodeUnaryExpr : public AstNodeExpression
    {
    public:
        enum class Type {
            negation, not_,
        };

        AstNodeUnaryExpr(Type type, pool_ptr<AstNodeExpression>&& right, SourceSpan span)
                : AstNodeExpression(AstNodeExpression::Type::unaryExpr, span), type(type), right(std::move(right)) {
        }

        SourceSpan getFullSpan() const override { return SourceSpan::union_(span, right->getFullSpan()); }

        stx::string_view getSymbol() const {
            switch (type) {
                case Type::negation:    return "-";
                case Type::not_:        return "!";
            }
        }

        const Type type;
        const pool_ptr<AstNodeExpression> right;
    };

    // ============================================================================================================= //
    //  STATEMENTS
    // ============================================================================================================= //

    class AstNodeStatement : public AstNodeBase {
    public:
        enum class Type {
            assert,
            assignment,
            block,
            expression,
            for_,
            forRange,
            if_,
            infer,
            return_,
            switch_,
            throw_,
            tryCatch,
            while_,
        };

        AstNodeStatement(Type type, SourceSpan span) : AstNodeBase(span), type(type) {
        }

        const Type type;
    };

    class AstNodeAssert : public AstNodeStatement
    {
    public:
        AstNodeAssert(pool_ptr<AstNodeExpression>&& expr, std::string&& exprString, SourceSpan span)
                : AstNodeStatement(AstNodeStatement::Type::assert, span),
                  expr(std::move(expr)), exprString{std::move(exprString)} {
        }

        const AstNodeExpression* getExpression() const { return expr.get(); }
        const std::string& getExpressionString() const { return exprString; }

    private:
        const pool_ptr<AstNodeExpression> expr;
        const std::string exprString;
    };

    class AstNodeAssignment : public AstNodeStatement
    {
    public:
        AstNodeAssignment(pool_ptr<AstNodeExpression>&& target, pool_ptr<AstNodeExpression>&& expression, SourceSpan span)
                : AstNodeStatement(AstNodeStatement::Type::assignment, span), target(std::move(target)), expression(std::move(expression)) {
        }

        SourceSpan getFullSpan() const override { return SourceSpan::union_(target->getFullSpan(), expression->getFullSpan()); }

        const AstNodeExpression* getExpression() const { return expression.get(); }
        pool_ptr<AstNodeExpression> getExpression2() const { return expression; }
        const AstNodeExpression* getTarget() const { return target.get(); }
        pool_ptr<AstNodeExpression> getTarget2() const { return target; }

    private:
        const pool_ptr<AstNodeExpression> target, expression;
    };

    class AstNodeBlock : public AstNodeStatement {
    public:
        explicit AstNodeBlock(SourceSpan span)
                : AstNodeStatement(AstNodeStatement::Type::block, span) {
        }

        void emplace_back(pool_ptr<AstNodeStatement>&& statement) {
            statements.emplace_back(std::move(statement));
        }

        SourceSpan getFullSpan() const override;

        const auto& getStatements() const { return statements; }

    private:
        std::vector<pool_ptr<AstNodeStatement>> statements;
    };

    class AstNodeForClassic : public AstNodeStatement
    {
    public:
        AstNodeForClassic(pool_ptr<AstNodeStatement>&& init, pool_ptr<AstNodeExpression>&& expr,
                          pool_ptr<AstNodeStatement>&& update, pool_ptr<AstNodeBlock>&& body, SourceSpan span)
                : AstNodeStatement(AstNodeStatement::Type::for_, span),
                  init{std::move(init)}, expr(std::move(expr)), update{std::move(update)}, body{std::move(body)} {
        }

        const AstNodeBlock* getBody() const { return body.get(); }
        const AstNodeExpression* getExpression() const { return expr.get(); }
        const AstNodeStatement* getInitStatement() const { return init.get(); }
        const AstNodeStatement* getUpdateStatement() const { return update.get(); }

    private:
        const pool_ptr<AstNodeExpression> expr;
        const pool_ptr<AstNodeStatement> init, update;
        const pool_ptr<AstNodeBlock> body;
    };

    class AstNodeForRange : public AstNodeStatement
    {
    public:
        AstNodeForRange(std::string&& variableName, pool_ptr<AstNodeExpression>&& range, pool_ptr<AstNodeBlock>&& block, SourceSpan span)
                : AstNodeStatement(AstNodeStatement::Type::forRange, span),
                  variableName{std::move(variableName)}, range(std::move(range)), block{std::move(block)} {
        }

        const AstNodeBlock* getBlock() const { return block.get(); }
        const AstNodeExpression* getRange() const { return range.get(); }
        const auto& getVariableName() const { return variableName; }

    private:
        const std::string variableName;
        const pool_ptr<AstNodeExpression> range;
        const pool_ptr<AstNodeBlock> block;
    };

    class AstNodeIf : public AstNodeStatement
    {
    public:
        AstNodeIf(pool_ptr<AstNodeExpression>&& expr, pool_ptr<AstNodeBlock>&& block, pool_ptr<AstNodeBlock>&& elseBlock, SourceSpan span)
                : AstNodeStatement(AstNodeStatement::Type::if_, span),
                  expr(std::move(expr)), block{std::move(block)}, elseBlock{std::move(elseBlock)} {
        }

        const AstNodeBlock* getBlock() const { return block.get(); }
        const AstNodeBlock* getElseBlock() const { return elseBlock.get(); }
        const AstNodeExpression* getExpression() const { return expr.get(); }

    private:
        const pool_ptr<AstNodeExpression> expr;
        const pool_ptr<AstNodeBlock> block, elseBlock;
    };

    class AstNodeInfer : public AstNodeStatement
    {
    public:
        AstNodeInfer(pool_ptr<AstNodeExpression>&& expr, SourceSpan span)
                : AstNodeStatement(AstNodeStatement::Type::infer, span),
                  expr(std::move(expr)) {
        }

        const AstNodeExpression* getExpression() const { return expr.get(); }

    private:
        const pool_ptr<AstNodeExpression> expr;
    };

    class AstNodeReturn : public AstNodeStatement
    {
    public:
        AstNodeReturn(pool_ptr<AstNodeExpression>&& expr, SourceSpan span)
                : AstNodeStatement(AstNodeStatement::Type::return_, span),
                  expr{std::move(expr)} {
        }

        const AstNodeExpression* getExpression() const { return expr.get(); }

    private:
        const pool_ptr<AstNodeExpression> expr;
    };

    class AstNodeStatementExpression : public AstNodeStatement
    {
    public:
        AstNodeStatementExpression(pool_ptr<AstNodeExpression>&& expr, SourceSpan span)
                : AstNodeStatement(AstNodeStatement::Type::expression, span),
                  expr{std::move(expr)} {
        }

        const AstNodeExpression* getExpression() const { return expr.get(); }

    private:
        const pool_ptr<AstNodeExpression> expr;
    };

    class AstNodeSwitch : public AstNodeStatement {
    public:
        AstNodeSwitch(pool_ptr<AstNodeExpression>&& expression, SourceSpan span)
                : AstNodeStatement(AstNodeStatement::Type::switch_, span), expression(std::move(expression)) {
        }

        void addCase(pool_ptr<AstNodeList>&& values, pool_ptr<AstNodeBlock>&& body) {
            cases.emplace_back(std::move(values), std::move(body));
        }

        const auto& getCases() const { return cases; }
        const auto getDefaultHandler() const { return defaultHandler.get(); }
        const auto getExpression() const { return expression.get(); }

        bool hasDefaultHandler() const { return defaultHandler.operator bool(); }

        void setDefaultHandler(pool_ptr<AstNodeBlock>&& defaultHandler) {
            this->defaultHandler = std::move(defaultHandler);
        }

    private:
        pool_ptr<AstNodeExpression> expression;
        pool_ptr<AstNodeBlock> defaultHandler;
        std::vector<std::pair<pool_ptr<AstNodeList>, pool_ptr<AstNodeBlock>>> cases;
    };

    class AstNodeThrow : public AstNodeStatement {
    public:
        AstNodeThrow(pool_ptr<AstNodeExpression>&& expr, SourceSpan span)
                : AstNodeStatement(AstNodeStatement::Type::throw_, span),
                  expr{std::move(expr)} {
        }

        const AstNodeExpression* getExpression() const { return expr.get(); }

    private:
        const pool_ptr<AstNodeExpression> expr;
    };

    class AstNodeTryCatch : public AstNodeStatement
    {
    public:
        AstNodeTryCatch(pool_ptr<AstNodeBlock>&& tryBlock,
                        pool_ptr<AstNodeBlock>&& catchBlock,
                        std::string&& caughtVariableName,
                        SourceSpan span)
                : AstNodeStatement(AstNodeStatement::Type::tryCatch, span),
                  tryBlock{std::move(tryBlock)}, catchBlock{std::move(catchBlock)}, caughtVariableName{std::move(caughtVariableName)} {
        }

        const auto& getCaughtVariableName() const { return caughtVariableName; }
        const AstNodeBlock* getCatchBlock() const { return catchBlock.get(); }
        const AstNodeBlock* getTryBlock() const { return tryBlock.get(); }

    private:
        const pool_ptr<AstNodeBlock> tryBlock, catchBlock;
        const std::string caughtVariableName;
    };

    class AstNodeWhile : public AstNodeStatement
    {
    public:
        AstNodeWhile(pool_ptr<AstNodeExpression>&& expr, pool_ptr<AstNodeBlock>&& block, SourceSpan span)
                : AstNodeStatement(AstNodeStatement::Type::while_, span),
                  expr(std::move(expr)), block{std::move(block)} {
        }

        const AstNodeBlock* getBlock() const { return block.get(); }
        const AstNodeExpression* getExpression() const { return expr.get(); }

    private:
        const pool_ptr<AstNodeExpression> expr;
        const pool_ptr<AstNodeBlock> block;
    };

    // ============================================================================================================= //
    //  UNSORTED
    // ============================================================================================================= //

    class AstNodeTypeName : public AstNodeBase
    {
        public:
            AstNodeTypeName(std::string&& name, bool optional, SourceSpan span) : AstNodeBase(span),
                                                                                  name(std::move(name)),
                                                                                  optional(optional) {
            }

            const std::string name;
            const bool optional;
    };

    class AstNodeDeclList : public AstNodeBase
    {
        public:
            struct Decl {
                std::string name;
                pool_ptr<AstNodeTypeName> type;
            };

            explicit AstNodeDeclList(SourceSpan span) : AstNodeBase(span) {
            }

            void emplace_back(std::string&& name, pool_ptr<AstNodeTypeName>&& type) {
                decls.emplace_back(Decl{ std::move(name), std::move(type) });
            }

            std::vector<Decl> decls;
    };

    class AstNodeFunction : public AstNodeExpression
    {
        public:
            AstNodeFunction(std::string&& name, SourceSpan span, pool_ptr<AstNodeDeclList>&& arguments, pool_ptr<AstNodeBlock>&& body)
                    : AstNodeExpression(AstNodeExpression::Type::function, span),
                      name(std::move(name)), arguments{std::move(arguments)}, body{std::move(body)} {
                this->isPrivate = false;
            }

            const std::string& getName() const { return name; }
            const AstNodeDeclList* getArguments() const { return arguments.get(); }
            const AstNodeBlock* getBody() const { return body.get(); }

            pool_ptr<AstNodeBlock> moveBody() { return std::move(body); }
            void setBody(pool_ptr<AstNodeBlock>&& body) { this->body = std::move(body); }

            [[deprecated]] pool_ptr<AstNodeExpression> moveParentConstructor() { return std::move(parentConstructor); }

            [[deprecated]] void setParentConstructor(pool_ptr<AstNodeExpression>&& parentConstructor) {
                this->parentConstructor = std::move(parentConstructor);
            }

        private:
            const std::string name;
            pool_ptr<AstNodeDeclList> arguments;
            pool_ptr<AstNodeBlock> body;

            pool_ptr<AstNodeExpression> parentConstructor;

        public:
            bool isPrivate;     // TODO: ???
    };

    class AstNodeClass : public AstNodeBase
    {
        public:
            struct MemberVariableDecl {
                std::string name;
                pool_ptr<AstNodeExpression> initialValue;
                SourceSpan origin;
            };

            AstNodeClass(std::string&& name, SourceSpan span) : AstNodeBase(span), name(std::move(name)) {}

            void addMethod(pool_ptr<AstNodeFunction>&& method) {
                this->methods.emplace_back(std::move(method));
            }

            const std::string name;
            std::vector<pool_ptr<AstNodeFunction>> methods;
            std::vector<MemberVariableDecl> memberVariables;
    };

    class AstNodeScript : public AstNodeBase
    {
        public:
            AstNodeScript() : AstNodeBase(SourceSpan {}) {
            }

            void addClass(pool_ptr<AstNodeClass>&& class_) {
                this->classes.emplace_back(std::move(class_));
            }

            void addFunction(pool_ptr<AstNodeFunction>&& function) {
                this->functions.emplace_back(std::move(function));
            }

            void setMainFunction(pool_ptr<AstNodeFunction>&& mainFunction) {
                this->mainFunction = std::move(mainFunction);
            }

            pool_ptr<AstNodeFunction> mainFunction;
            std::vector<pool_ptr<AstNodeClass>> classes;
            std::vector<pool_ptr<AstNodeFunction>> functions;
    };
}
