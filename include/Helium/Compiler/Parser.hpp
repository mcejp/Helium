#pragma once

#include <Helium/Compiler/Ast.hpp>
#include <Helium/Compiler/Compiler.hpp>
#include <Helium/Compiler/Lexer.hpp>
#include <Helium/Memory/LinearAllocator.hpp>

#include <memory>

namespace Helium
{
    class Parser
    {
        public:
            Parser(Compiler* compiler, Lexer* lexer, LinearAllocator& astAllocator);
            ~Parser();

        pool_ptr<AstNodeScript> parseScript();

        private:
            Token* current();
            std::string line();          // TODO: deprecate and remove

            Token* accept( TokenType tt );
            pool_ptr<AstNodeClass> classDefinition();

            pool_ptr<AstNodeExpression>      parseAddSubExpression();
            pool_ptr<AstNodeExpression>      parseAtomicExpression(bool mustBeAssignable);
            pool_ptr<AstNodeBlock>           parseBlock();
            pool_ptr<AstNodeExpression>      parseCmpExpression();
            pool_ptr<AstNodeExpression>      parseConstantExpression();
            pool_ptr<AstNodeDeclList>        parseDeclList();
            pool_ptr<AstNodeDeclList>        parseEnclosedDeclList();
            pool_ptr<AstNodeList>            parseEnclosedList();
            pool_ptr<AstNodeExpression>      parseExpression();
            pool_ptr<AstNodeFunction>        parseFunction(bool mustBeAnonymous);
            pool_ptr<AstNodeIdent>           parseIdentifier();
            pool_ptr<AstNodeList>            parseList(SourceSpan span);
            pool_ptr<AstNodeExpression>      parseLogExpression();
            pool_ptr<AstNodeExpression>      parsePlusMinusNotExpression();
            pool_ptr<AstNodeExpression>      parseMulDivModExpression();
            pool_ptr<AstNodeExpression>      parseSubscriptExpression(bool mustBeAssignable);
            pool_ptr<AstNodeStatement>       parseStatement(bool terminated);
            pool_ptr<AstNodeTypeName>        parseTypeName();

            template<typename _Tp, typename... _Args>
            inline auto make_pooled(_Args&&... __args)
            {
                return astAllocator.make_pooled<_Tp>(std::forward<_Args>(__args)...);
            }

            [[noreturn]] void syntaxError(const char* message);

        Compiler* compiler;
        Lexer* lexer;
        LinearAllocator& astAllocator;

        unsigned anonymousFunctions;    // TODO: deprecate and remove (not Parser's job to track this)
        unsigned latestLine;            // TODO: deprecate and remove

        Token* currentToken;
        bool currentTokenValid;
    };
}
