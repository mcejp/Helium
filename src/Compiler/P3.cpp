#include <Helium/Compiler/Parser.hpp>

#include <memory>
#include <string>

// TODO: this->currentToken shouldn't be directly accessed by parser rules

namespace Helium
{
    using std::move;
    using std::string;

    Parser::Parser( Compiler* compiler, Lexer* lexer, LinearAllocator& astAllocator )
            : compiler( compiler ), lexer( lexer ), astAllocator(astAllocator),
              anonymousFunctions( 0 ), latestLine( 0 ),
              currentToken( NULL ), currentTokenValid( false )
    {
    }

    Parser::~Parser()
    {
        if ( currentToken )
            delete currentToken;
    }

    Token* Parser::accept( TokenType tt )
    {
        if ( current() && currentToken->type == tt )
        {
            currentTokenValid = false;
            return currentToken;
        }

        return 0;
    }

    pool_ptr<AstNodeExpression> Parser::parseAddSubExpression()
    {
        auto top = parseMulDivModExpression();

        if ( !top )
            return nullptr;

        while ( true )
        {
            if ( Token* tok = accept( Token_plus ) ) {
                auto span = tok->span;
                auto right = parseMulDivModExpression();

                if (!right)
                    syntaxError("Expected expression after '+'");

                top = make_pooled<AstNodeBinaryExpr>(AstNodeBinaryExpr::Type::add, move(top), move(right), span);
            }
            else if ( Token* tok = accept( Token_minus ) ) {
                auto span = tok->span;
                auto right = parseMulDivModExpression();

                if (!right)
                    syntaxError("Expected expression after '-'");

                top = make_pooled<AstNodeBinaryExpr>(AstNodeBinaryExpr::Type::subtract, move(top), move(right), span);
            }
            else
                break;
        }

        return top;
    }

    pool_ptr<AstNodeExpression> Parser::parseAtomicExpression(bool mustBeAssignable) {
        if (!mustBeAssignable) {
            auto expr = parseConstantExpression();

            if (expr)
                return expr;
        }

        if ( !mustBeAssignable && ( accept( Token_construction ) || accept( Token_leftBrace ) ) )
        {
            auto objectLiteral = make_pooled<AstNodeLiteralObject>(currentToken->span);

            while ( !accept( Token_rightBrace ) )
            {
                auto ident = parseIdentifier();

                if (!ident)
                    syntaxError("expected member name or ')'");

                if (!accept( Token_colon ))
                    syntaxError("expected ':' after member name");

                auto expression = parseExpression();

                if (!expression)
                    syntaxError("expected expression after ':'");

                objectLiteral->addProperty(string(ident->name), move(expression));

                if ( accept( Token_rightBrace ) )
                    break;

                if ( !accept( Token_comma ) )
                    syntaxError(" expected ',' or '}'");
            }

			return objectLiteral;
        }
		/*else if ( accept( Token_global ) )
        {
            std::unique_ptr<Branch> member(new Branch( AstNodeType::member, latestLine ));

            member->left = new AstNodeIdent("_global", AstNodeIdent::Namespace::local, latestLine);

            if ( !( member->right = ident() ) )
				syntaxError("Expected identifier after '@' ");

            return member.release();
        }*/
		else if ( accept( Token_local ) )
        {
            Token* tok;

            if ( !( tok = accept( Token_identifier ) ) ) {
                syntaxError("Expected identifier after 'local'");
                return nullptr;
            }

            return make_pooled<AstNodeIdent>( tok->text.c_str(), AstNodeIdent::Namespace::local, tok->span );
        }
        else if ( Token* tok = accept( Token_myMember ) )
        {
            auto span = tok->span;
            auto ident = parseIdentifier();

            if ( !ident )
                syntaxError("Expected identifier after '$'");

            return make_pooled<AstNodeProperty>(nullptr, move(ident), span );
        }
        else if (!mustBeAssignable) {
            auto function = parseFunction(true);

            if (function)
                return function;

            auto ident = parseIdentifier();

            if (ident)
                return ident;
        }

        if ( !mustBeAssignable )
            return parseEnclosedList();
        else
            syntaxError("Expected assignable expression");
    }

    pool_ptr<AstNodeBlock> Parser::parseBlock()
    {
        if ( accept( Token_leftBrace ) )
        {
            // TODO: better span
            auto block_ = make_pooled<AstNodeBlock>( currentToken->span );

            pool_ptr<AstNodeBlock> next;

            while ( ( next = parseBlock() ) )
                block_->emplace_back( move(next) );

            if ( !accept( Token_rightBrace ) )
                syntaxError("#block: Expected '}' in block");

            return block_;
        }
        else
        {
            Token* nextToken = current();

            if ( !nextToken )
                return 0;

            unsigned blockIndent = nextToken->indent;

            pool_ptr<AstNodeBlock> block;

            while ( nextToken && nextToken->indent >= blockIndent )
            {
                auto next = parseStatement(true);

                if ( next )
                {
                    if ( !block )
                        // TODO: better span
                        block = make_pooled<AstNodeBlock>( next->span );

                    block->emplace_back( move( next ) );
                }
                else
                    break;

                nextToken = current();

                if ( nextToken && nextToken->indent == 0 ) {
                    // We've been called directly from parseScript
                    // The reason to return here is because functions and classes are handled on top-level
                    // (encountering a function token would confuse us)
                    break;
                }
            }

            return block;
        }
    }

    pool_ptr<AstNodeClass> Parser::classDefinition()
    {
        Token* name;

        if ( !accept( Token_class ) )
            return 0;

        name = accept( Token_identifier );

        if ( !name )
            syntaxError("Expected class name following 'class'");

        // TODO: better span
        auto me = make_pooled<AstNodeClass>(name->text.c_str(), name->span);

        if ( !accept( Token_leftBrace ) )
            syntaxError("Expected '{' following class name");

        while ( accept( Token_member ) )
        {
            do
            {

                pool_ptr<AstNodeIdent> name(parseIdentifier());

			    if ( !name ) {
                    syntaxError("Expected comma-separated member names after 'member' keyword");
                    break;
                }

                pool_ptr<AstNodeExpression> initialValue;

                if ( accept( Token_assignation ) )
                {
                    initialValue = parseExpression();

                    if (!initialValue) {
                        syntaxError("Expected expression after '='");
                        break;
                    }
                }

                me->memberVariables.emplace_back(AstNodeClass::MemberVariableDecl{ name->name, move(initialValue), name->span });
            }
            while ( accept( Token_comma ) );

            if ( !accept( Token_semicolon ) )
                syntaxError("Expected ',' or ';'");
        }

        while ( true )
        {
            name = accept( Token_identifier );

            if ( !name )
                break;

            auto span = name->span;
            auto name_ = name->text;

            auto args = parseEnclosedDeclList();

            if (!args) {
                syntaxError("Expected argument list");
                continue;
            }

            auto body = parseBlock();

            if (!body)
                syntaxError("Expected function body");

            // TODO: better span
            auto method = make_pooled<AstNodeFunction>( name_.c_str(), span, move(args), move(body) );

            me->addMethod( move(method) );
        }

        if ( !accept( Token_rightBrace ) )
            syntaxError("Expected method name or '}'");

        return me;
    }

        #define compareRule( branchType_, error_ ) {\
                auto span = tok->span;\
                auto right = parseAddSubExpression();\
                if ( !right )\
                    syntaxError( error_ );\
\
                top = make_pooled<AstNodeBinaryExpr>( AstNodeBinaryExpr::Type::branchType_, move(top), move(right), span );\
            }

    pool_ptr<AstNodeExpression> Parser::parseCmpExpression()
    {
        auto top = parseAddSubExpression();

        if ( !top )
            return 0;

        while ( true )
        {
            if ( Token* tok = accept( Token_equals ) )
                compareRule( equals, "Expected #addExpression after binary '=='" )
            else if ( Token* tok = accept( Token_greater ) )
                compareRule( greater, "Expected #addExpression after binary '>'" )
            else if ( Token* tok = accept( Token_greaterEquals ) )
                compareRule( greaterEq, "Expected #addExpression after binary '>='" )
            else if ( Token* tok = accept( Token_less ) )
                compareRule( less, "Expected #addExpression after binary '<'" )
            else if ( Token* tok = accept( Token_lessEquals ) )
                compareRule( lessEq, "Expected #addExpression after binary '<='" )
            else if ( Token* tok = accept( Token_notEquals ) )
                compareRule( notEquals, "Expected #addExpression after binary '!='" )
            else
                break;
        }

        return top;
    }

    pool_ptr<AstNodeExpression> Parser::parseConstantExpression()
    {
        Token* tok;

        if ( ( tok = accept( Token_nul ) ) )
            return make_pooled<AstNodeLiteral>( AstNodeLiteral::Type::nul, tok->span );
        else if ( ( tok = accept( Token_integer ) ) )
        {
            return make_pooled<AstNodeLiteralInteger>(tok->integerValue, tok->span);
        }
        else if ( ( tok = accept( Token_literalBool ) ) )
        {
            return make_pooled<AstNodeLiteralBoolean>(tok->boolValue, tok->span);
        }
        else if ( ( tok = accept( Token_number ) ) )
        {
            return make_pooled<AstNodeLiteralReal>(tok->realValue, tok->span);
        }
        else if ( ( tok = accept( Token_string ) ) )
        {
            return make_pooled<AstNodeLiteralString>(tok->text.c_str(), tok->span);
        }
        else
            return nullptr;
    }

    Token* Parser::current()
    {
        if ( !currentTokenValid )
        {
            delete currentToken;
            currentToken = 0;
            currentTokenValid = true;
        }

        if ( !currentToken )
        {
            if ( ( currentToken = lexer->readToken() ) ) {
                latestLine = currentToken->span.start.line;
                //currentToken->print();
            }
        }

        return currentToken;
    }

    pool_ptr<AstNodeExpression> Parser::parseExpression() {
        return parseLogExpression();
    }

    std::string Parser::line()
    {
        return string(" (") + compiler->getCurrentUnit() + ":" + std::to_string(latestLine) + ")";
    }

    #define logicalRule( branchType_, error_ ) {\
                auto span = tok->span;\
                auto right = parseCmpExpression();\
                if ( !right )\
                    syntaxError(error_);\
\
                top = make_pooled<AstNodeBinaryExpr>( AstNodeBinaryExpr::Type::branchType_, move(top), move(right), span );\
            }

    pool_ptr<AstNodeExpression> Parser::parseLogExpression()
    {
        auto top = parseCmpExpression();

        if ( !top )
            return 0;

        while ( true )
        {
            if ( Token* tok = accept( Token_and ) )
                logicalRule( and_, "Expected #cmpExpression after binary '&&'" )
            else if ( Token* tok = accept( Token_or ) )
                logicalRule( or_, "Expected #cmpExpression after binary '||'" )
            //else if ( accept( Token_xor ) )
            //    logicalRule( AstNodeType::xor_, "Expected #cmpExpression after binary '^^'" )
            else
                break;
        }

        return top;
    }

    #define mulRule( branchType_, error_ ) {\
                auto span = tok->span;\
                auto right = parsePlusMinusNotExpression();\
                if ( !right )\
                    syntaxError(error_);\
\
                top = make_pooled<AstNodeBinaryExpr>( AstNodeBinaryExpr::Type::branchType_, move(top), move(right), span );\
            }

    pool_ptr<AstNodeExpression> Parser::parseMulDivModExpression()
    {
        auto top = parsePlusMinusNotExpression();

        if ( !top )
            return 0;

        while ( true )
        {
            if ( Token* tok = accept( Token_times ) )
                mulRule( multiply, "Expected #memberExpression after binary '*'" )
            else if ( Token* tok = accept( Token_dividedBy ) )
                mulRule( divide, "Expected #memberExpression after binary '/'" )
            else if ( Token* tok = accept( Token_modulo ) )
                mulRule( modulo, "Expected #memberExpression after binary '%'" )
            else
                break;
        }

        return top;
    }

    pool_ptr<AstNodeExpression> Parser::parseSubscriptExpression(bool mustBeAssignable)
    {
        // TODO: mustBeAssignable is idiotic and only shows that some things here should be moved into atomicExpression

        auto top = parseAtomicExpression(false);

        if ( !top )
            return 0;

        while ( true )
        {
            pool_ptr<AstNodeList> arguments;

            if ( Token* tok = accept( Token_leftSquareBrace ) )
            {
                auto span = tok->span;
                auto index = parseExpression();

                if ( !index )
                    syntaxError("Expected expression after '['");

                if ( !accept( Token_rightSquareBrace ) )
                    syntaxError("Expected ']'");

                top = make_pooled<AstNodeExprIndexed>(move(top), move(index), span);
            }
            else if ( Token* tok = accept( Token_period ) )
            {
                auto span = tok->span;
                auto ident = parseIdentifier();

                if ( !ident )
                    syntaxError("Expected identifier after '.'");

                top = make_pooled<AstNodeProperty>(move(top), move(ident), span );
            }
            else if ( !mustBeAssignable && accept( Token_try ) )
            {
                auto span = currentToken->span;
                auto arguments = parseEnclosedList();

                if ( !arguments )
                    syntaxError("Expected argument list after blind-call 'try'");

                top = make_pooled<AstNodeCall>(move(top), move(arguments), span, true);
            }
            else if ( !mustBeAssignable && ( arguments = parseEnclosedList() ) )
            {
                top = make_pooled<AstNodeCall>(move(top), move(arguments), arguments->span, false);
            }
            else
                break;
        }

        return top;
    }

    pool_ptr<AstNodeDeclList> Parser::parseDeclList()
    {
        // FIXME: span
        auto node = make_pooled<AstNodeDeclList>(SourceSpan {});

        do
        {
            auto ident = accept(Token_identifier);

            if (!ident)
                break;

            string name = ident->text.c_str();

            pool_ptr<AstNodeTypeName> typeName;

            if (accept(Token_colon)) {
                typeName = parseTypeName();

                if (!typeName) {
                    syntaxError("Expected type name after ':'");
                    break;
                }
            }

            node->emplace_back(move(name), move(typeName));
        } while (accept(Token_comma));

        return node;
    }

    pool_ptr<AstNodeDeclList> Parser::parseEnclosedDeclList()
    {
        if (!accept(Token_leftParen))
            return nullptr;

        auto declList = parseDeclList();

        if (!declList) {
            syntaxError("Expected argument list");
            return nullptr;
        }

        if (!accept(Token_rightParen))
            syntaxError("Expected ')'");

        return declList;
    }

    pool_ptr<AstNodeList> Parser::parseEnclosedList()
    {
        Token* tok;

        if ( !( tok = accept( Token_leftParen ) ) )
            return nullptr;

        auto list = parseList(tok->span);

        if ( !accept( Token_rightParen ) )
            syntaxError("Expected ',' or ')' ");

        return list;
    }

    pool_ptr<AstNodeFunction> Parser::parseFunction(bool mustBeAnonymous)
    {
        if (!accept(Token_function))
            return nullptr;

        Token* name = accept(Token_identifier);

        if (name && mustBeAnonymous) {
            syntaxError("Expected anonymous function");
            name = nullptr;
        }

        // BytecodeCompiler relies on anonymous functions having unique names for funcptr resolution
        string name_ = name ? name->text.c_str() : ".anon_" + std::to_string(anonymousFunctions++);

        auto args = parseEnclosedDeclList();

        if (!args) {
            syntaxError("Expected argument list");
            return nullptr;
        }

        auto body = parseBlock();

        if (!body)
            syntaxError("Expected function body");

        // TODO: better span
        auto function = make_pooled<AstNodeFunction>(
                move(name_), SourceSpan::union_(args->span, body->span),
                move(args), move(body));

        return function;
    }

    pool_ptr<AstNodeIdent> Parser::parseIdentifier()
    {
        Token* tok = accept(Token_identifier);

        if (!tok)
            return nullptr;

        return make_pooled<AstNodeIdent>(tok->text.c_str(), AstNodeIdent::Namespace::unknown, tok->span);
    }

    pool_ptr<AstNodeList> Parser::parseList(SourceSpan span) {
        auto list = make_pooled<AstNodeList>(span);

        do {
            auto item = parseExpression();

            if (!item)
                break;

            list->emplace_back(move(item));
        }
        while (accept(Token_comma));

        return list;
    }

    pool_ptr<AstNodeScript> Parser::parseScript()
    {
        // TODO: better spans in the following block
        auto script = make_pooled<AstNodeScript>();

        auto body = make_pooled<AstNodeBlock>(script->span);

        while (true)
        {
            auto function = parseFunction(false);

            if (function) {
                script->addFunction(move(function));
                continue;
            }

            auto class_ = classDefinition();

            if (class_) {
                script->addClass(move(class_));
                continue;
            }

            auto next = parseBlock();

            if (next) {
                body->emplace_back(move(next));
                continue;
            }

            if (current())
                syntaxError("Expected a class, function or statement");

            break;
        }

        auto mainFunction = make_pooled<AstNodeFunction>("(main)", script->span,
                                                         make_pooled<AstNodeDeclList>(script->span),
                                                              move(body));

        script->setMainFunction(move(mainFunction));
        return script;
    }

    pool_ptr<AstNodeTypeName> Parser::parseTypeName()
    {
        Token* tok = accept(Token_identifier);

        if (!tok)
            return nullptr;

        auto span = tok->span;
        string typeName = tok->text;

        bool optional = !!accept(Token_questionMark);

        return make_pooled<AstNodeTypeName>(move(typeName), optional, span);
    }

    pool_ptr<AstNodeExpression> Parser::parsePlusMinusNotExpression()
    {
        accept( Token_plus );

        if ( Token* tok = accept( Token_minus ) )
        {
            auto span = tok->span;
            auto right = parsePlusMinusNotExpression();

            if (!right)
                syntaxError("Expected expression after '-'");

            return make_pooled<AstNodeUnaryExpr>(AstNodeUnaryExpr::Type::negation, move(right), span);
        }
        else if ( Token* tok = accept( Token_not ) )
        {
            auto span = tok->span;
            auto right = parsePlusMinusNotExpression();

            if (!right)
                syntaxError("Expected expression after '!'");

            return make_pooled<AstNodeUnaryExpr>(AstNodeUnaryExpr::Type::not_, move(right), span);
        }

        return parseSubscriptExpression(false);
    }

    pool_ptr<AstNodeStatement> Parser::parseStatement(bool terminated)
    {
        if ( Token* tok = accept( Token_assert ) )
        {
            auto span = tok->span;
            auto expr = parseExpression();

            if (!expr) {
                syntaxError("Expected expression following 'assert'");
                return nullptr;
            }

            accept( Token_semicolon );

            return make_pooled<AstNodeAssert>(move(expr), lexer->getTextOfSpan(expr->getFullSpan()), span);
        }
        else if ( Token* tok = accept( Token_for ) )
        {
            auto span = tok->span;

            auto initStatement = parseStatement(false);

			if ( !initStatement )
                syntaxError("#for: expected init-statement right after 'for' ");

            if ( !accept( Token_comma ) )
                syntaxError("#for: expected ',' after init-statement ");

            auto updateStatement = parseStatement(false);

            if ( !updateStatement )
                syntaxError("#for: expected loop-statement after init-statement ");

            if ( !accept( Token_while ) )
                syntaxError("#for: expected 'while' after loop-statement ");

            auto expr = parseExpression();

            if ( !expr )
                syntaxError("#for: expected condition expression after init statement ");

            auto block = parseBlock();

            if ( !block )
                syntaxError("#for: expected block ");

            return make_pooled<AstNodeForClassic>(move(initStatement), move(expr), move(updateStatement), move(block), span);
        }
        else if ( ( tok = accept( Token_if ) ) )
        {
            auto span = tok->span;

            unsigned indent = tok->usedFixedIndent ? tok->indent - 1 : tok->indent;

            auto expr = parseExpression();

            if (!expr)
                syntaxError("expected expression after 'if'");

            auto block = parseBlock();

            if (!block)
                syntaxError("expected block after 'if <expression>'");

            pool_ptr<AstNodeBlock> elseBlock;

            // This is a hack actually and doesnt work well
            // TODO: FIX!!
            // Update: FIXME why is this a hack and why doesn't it work?
            if ( current() && current()->indent >= indent && current()->type == Token_else )
            //if ( accept( Token_else ) )
            {
                accept( Token_else );

                lexer->setFixedIndent( indent + 1 );
                elseBlock = parseBlock();

                if ( !elseBlock )
                    syntaxError("expected block after 'else' ");
            }

            return make_pooled<AstNodeIf>(move(expr), move(block), move(elseBlock), span);
        }
        else if ( Token* tok = accept( Token_infer ) )
        {
            auto span = tok->span;
            auto expr = parseExpression();

            if (!expr) {
                syntaxError("Expected expression following 'infer'");
                return nullptr;
            }

            accept( Token_semicolon );

            return make_pooled<AstNodeInfer>(move(expr), span);
        }
		else if ( accept( Token_iterate ) )
        {
            auto variable = parseIdentifier();

            if ( !variable )
                syntaxError("#iterate: expected iterator right after 'iterate' ");

			if ( Token* tok = accept( Token_in ) )
            {
                auto span = tok->span;

                // Iteration over a list

                auto range = parseSubscriptExpression(false);

                if ( !range )
				    syntaxError("expected iterable expression after 'in'");

                /*if ( accept( Token_as ) )
                {
                    Branch* iter;

                    if ( !( iter = parseSubscriptExpression(false).release() ) )
				        syntaxError("#iterate: Expected index iterator after 'as' ");

                    me->addChild( iter );
                }*/

                auto block = parseBlock();

                if ( !block )
                    syntaxError("#iterate: expected block ");

                return make_pooled<AstNodeForRange>(string(variable->name), move(range), move(block), span);
            }
            else
            {
                syntaxError("expected 'in'");

#if 0
                // BASIC-like FOR

                unique_ptr<Branch> next;
                // TODO: better span
                unique_ptr<Branch> me = std::make_unique<Branch>(AstNodeType::iterate, currentToken->span);

                if ( !( me->right = list( true, 2, 3 ) ) )
                    syntaxError("#iterate: expected 'in' or ( initial, final[, step] ) after iterator ");

                me->isListIteration = false;

                next.reset( block() );

                if ( !next )
                    syntaxError("#iterate: expected block ");

                me->addChild( next.release() );

                return me.release();
#endif
            }
        }
        else if ( Token* tok = accept( Token_return ) )
        {
            auto span = tok->span;

            auto expression = parseExpression();

            if ( !expression )
                expression = make_pooled<AstNodeLiteral>(AstNodeLiteral::Type::nul, span);

            accept( Token_semicolon );

            return make_pooled<AstNodeReturn>(move(expression), span);
        }
        else if ( Token* tok = accept( Token_switch ) )
        {
            auto span = tok->span;

            auto expression = parseExpression();

            if ( !expression )
                syntaxError("expected expression following 'switch' keyword");

            auto switch_ = make_pooled<AstNodeSwitch>( move(expression), span );

            bool isBraced = ( accept( Token_leftBrace ) != 0 );

            Token* nextToken = 0;
            unsigned blockIndent = 0;

            if ( !isBraced )
            {
                nextToken = current();

                if ( !nextToken )
                    return 0;

                blockIndent = nextToken->indent;
            }

            while ( ( isBraced && !accept( Token_rightBrace ) ) || ( !isBraced && nextToken && nextToken->indent >= blockIndent ) )
            {
                if ( accept( Token_else ) )
                {
                    if ( !accept( Token_colon ) )
                        syntaxError("#switch: expected ':' after 'else' ");

                    if ( switch_->hasDefaultHandler() )
                        syntaxError("#switch: duplicate default handler ");

                    auto defaultHandler = parseBlock();

                    if ( !defaultHandler )
                        syntaxError("#switch: expected block after ':' ");

                    switch_->setDefaultHandler(move(defaultHandler));
                }
                else
                {
                    auto list = make_pooled<AstNodeList>(currentToken->span);

                    do {
                        auto next = parseConstantExpression();

                        if ( !next )
                        {
                            if (list->getItems().empty())
                                syntaxError("#switch: expected 'else' or constant expression ");
                            else
                                syntaxError("#switch: expected constant expression after ',' ");
                        }

                        list->emplace_back( move(next) );
                    }
                    while ( accept( Token_comma ) );

                    if ( !accept( Token_colon ) )
                    {
                        syntaxError("#switch: expected ':' after constant expression(s) ");
                    }

                    auto block = parseBlock();

                    if ( block )
                        switch_->addCase(move(list), move(block));
                    else
                    {
                        syntaxError("#switch: expected block after ':' ");
                    }
                }

                accept( Token_comma );

                if ( !isBraced )
                    nextToken = current();
            }

            return switch_;
        }
        else if ( Token* tok = accept( Token_throw ) )
        {
            auto span = tok->span;

            auto expr = parseExpression();

            if ( !expr )
				syntaxError("#throw: expected expression ");

            accept( Token_semicolon );

            return make_pooled<AstNodeThrow>(move(expr), span);
        }
        else if ( Token* tok = accept( Token_try ) )
        {
            auto span = tok->span;

            auto tryBlock = parseBlock();

            if ( !tryBlock )
                syntaxError("#try: expected block ");

            if ( !accept( Token_catch ) )
                syntaxError("expected 'catch' after 'try' block");

            auto caughtVariable = parseIdentifier();

            if ( !caughtVariable )
                syntaxError("#try: expected variable name following 'catch' ");

            auto catchBlock = parseBlock();

            if ( !catchBlock )
                syntaxError("#try: expected block following variable name ");

            return make_pooled<AstNodeTryCatch>(move(tryBlock), move(catchBlock), string(caughtVariable->name), span);
        }
        else if ( Token* tok = accept( Token_while ) )
        {
            auto span = tok->span;
            auto expr = parseExpression();

            if (!expr)
                syntaxError("expected expression after 'while'");

            auto block = parseBlock();

            if (!block)
                syntaxError("expected block after 'while <expression>'");

            return make_pooled<AstNodeWhile>(move(expr), move(block), span);
        }

        auto expr = parseExpression();

        if (expr) {
            pool_ptr<AstNodeStatement> statement;

            if ( Token* tok = accept( Token_assignation ) )
            {
                auto span = tok->span;
                auto right = parseAddSubExpression();

                if (!right)
                    syntaxError("Expected expression after '='");

                statement = make_pooled<AstNodeAssignment>(move(expr), move(right), span);
            }
            else
                statement = make_pooled<AstNodeStatementExpression>(move(expr), expr->span);

            if (!accept(Token_semicolon) == terminated)
                syntaxError("#statement: missing or unexpected ';'");

            return statement;
        }

        return {};
    }

    void Parser::syntaxError(const char* message) {
        throw CompileException("SyntaxError", (string(message) + line()).c_str());

        // TODO: emit error & skip to next line or something
    }
}
