#pragma once

#include <Helium/Compiler/Compiler.hpp>
#include <Helium/Runtime/Value.hpp>

namespace Helium
{
    // TODO: rename Token_number, Token_integer
    enum TokenType
    {
        Token_none,

        //* Primitives & constants
        Token_integer,
		Token_nil,
        Token_literalBool,          // added for Brave (16.08.17)
        Token_number,
        Token_string,
        Token_identifier,

        //* (), {}, [], ...
        Token_leftBrace,
        Token_leftParen,
        Token_leftSquareBrace,
        Token_rightBrace,
        Token_rightParen,
        Token_rightSquareBrace,

        //* +, -, *, /, ...
        Token_dividedBy,
        Token_plus,
        Token_plusEquals,
        Token_minus,
        Token_minusEquals,
        Token_modulo,
        Token_times,

        //* Compares
        Token_equals,
        Token_greater,
        Token_greaterEquals,
        Token_less,
        Token_lessEquals,
        Token_notEquals,

        //* Logicals
        Token_and,
        Token_not,
        Token_or,
        //Token_xor,            // removed for Brave (16.08.17)

        //* Keywords
        Token_as,           // Added for A2010 (08.12.10)
        Token_assert,           // added for Brave (16.08.17)
        Token_break,
        Token_catch,        //* Added for A2010 (09.01.10)
        Token_class,        //* Added for A2010 (19.06.10)
        Token_continue,
        Token_else,
        Token_for,
        Token_function,
        Token_if,
        Token_in,           // Added for A2010 (07.12.10)
        Token_infer,        // added for Brave (21.09.17)
        Token_iterate,      // Added for A2010 (29.05.10)
        Token_local,        // Added for A2010 (08.12.10)
        Token_member,       // Added for A2010 (05.12.10)
		Token_private,
        Token_return,
        Token_switch,           // Added for A2010 (09.01.10)
        Token_throw,            // Added for A2010 (08.07.10)
        Token_try,              // Added for A2010 (08.07.10)
        Token_while,
        Token_with,

        //* Various
        Token_assignation,
        Token_colon,
        Token_comma,
        Token_construction,     // Added for a7 (24.09.09)
        //Token_global,           // Added for A2010 (29.05.10); removed for Brave (15.08.17)
        Token_myMember,         // Added for A2010 (03.03.10); updated 05.12.10
        Token_period,
        Token_semicolon,

        Token_questionMark,

        Token_MAX
    };

    struct Token
    {
        TokenType type;
        SourceSpan span;
        unsigned indent;
        bool usedFixedIndent;       // TODO: what is this for?

        std::string text;
        Int_t integerValue;
        Real_t realValue;
        bool boolValue;

        // Keep this for debugging
        void print()
        {
            printf( "%3i:%02i - [%02i]: '%s'\n", span.start.line, span.start.column, type, text.c_str() );
        }

    protected:
        Token( TokenType type, SourceSpan span, unsigned indent, int fixedIndent,
               std::string&& text, Int_t integerValue, Real_t realValue, bool boolValue )
                : type( type ), span( span ), indent( indent ), text( std::move(text) ), integerValue(integerValue),
                  realValue(realValue), boolValue(boolValue)
        {
            if ( fixedIndent < 0 )
                usedFixedIndent = false;
            else
            {
                indent = fixedIndent;
                usedFixedIndent = true;
            }
        }
    };

    struct TokenLiteralInteger : public Token {
        TokenLiteralInteger(TokenType type, SourceSpan span, unsigned int indent, Int_t integerValue)
                : Token(type, span, indent, -1, "", integerValue, 0, false) {
        }
    };

    struct TokenLiteralReal : public Token {
        TokenLiteralReal(TokenType type, SourceSpan span, unsigned int indent, Real_t realValue)
        : Token(type, span, indent, -1, "", 0, realValue, false) {
        }
    };

    struct TokenLiteralString : public Token {
        TokenLiteralString(TokenType type, SourceSpan span, unsigned int indent, int fixedIndent, std::string&& text)
                : Token(type, span, indent, fixedIndent, std::move(text), 0, 0.0, false) {
        }
    };

    struct TokenPrimitive : public Token {
        TokenPrimitive(TokenType type, SourceSpan span, unsigned int indent)
                : Token(type, span, indent, -1, "", 0, 0.0, false) {
        }
    };

    class Lexer
    {
        SourcePoint point, nextPoint;
        const char* source_start;
        const char* source, * end;
        unsigned indent;
        int fixedIndent;

        private:
            char read();
            bool read( char what );
            bool read( const char* what, size_t length );

            void parseError(const char* message);

        public:
            // TODO: pass fileName as std::filesystem::path (or, if it needs to be pooled somehow, use a custom type)
            Lexer( Compiler* compiler, const char* fileName, const std::string_view script );

            std::string getTextOfSpan(SourceSpan span);
            Token* readToken();
            void setFixedIndent( unsigned fixedIndent ) { this->fixedIndent = fixedIndent; }
    };
}
