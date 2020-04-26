#include <Helium/Compiler/Lexer.hpp>
#include <sstream>

#include <cstring>

#define charToken( char_, type_ ) else if ( read( char_ ) )\
    return new TokenPrimitive( type_, SourceSpan(start, point), indent );

#define bicharToken( chars_, type_ ) else if ( read( chars_, 2 ) )\
    return new TokenPrimitive( type_, SourceSpan(start, point), indent );

namespace Helium
{
    // TODO: refactor in a way that Token can be const
    static void translateToken( Token* );

    static bool isNumeric( int c )
    {
        return c >= '0' && c <= '9';
    }

    static bool isHexaNumeric( int c )
    {
        return isNumeric( c ) || ( c >= 'a' && c <= 'f' ) || ( c >= 'A' && c <= 'F' );
    }

    static bool isIdentifier( int c )
    {
        return isNumeric( c ) || ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) || c == '_';
    }

    Lexer::Lexer( Compiler* compiler, const char* fileName, std::string_view script )
            : point{fileName, 1, 0, 0},
              nextPoint{fileName, 1, 1, 0},
              source_start(script.data()),
              source(script.data()),
              indent( 0 ),
              fixedIndent( -1 )
    {
        end = source_start + script.size();
    }

    std::string Lexer::getTextOfSpan(SourceSpan span) {
        // TODO: check if this isn't off-by-1
        helium_assert(source_start + span.start.byteOffset < end);
        helium_assert(source_start + span.end.byteOffset + 1 < end);

        return std::string(source_start + span.start.byteOffset, span.end.byteOffset + 1 - span.start.byteOffset);
    }

    void Lexer::parseError(const char* message) {
        std::stringstream ss;
        ss << message << " (" << point.unit << ":" << point.line << ":" << point.column << ")";

        throw CompileException("ParseError", ss.str().c_str());

        // TODO: log error & skip to next line
    }

    char Lexer::read() {
        point = nextPoint;

        if (*source == '\n') {
            nextPoint.line++;
            nextPoint.column = 1;
        }
        else {
            // TODO: treat tab
            nextPoint.column++;
        }

        nextPoint.byteOffset++;
        return *(source++);
    }

    bool Lexer::read( char what )
    {
        if ( source < end && *source == what )
        {
            read();
            return true;
        }
        else
            return false;
    }

    bool Lexer::read( const char* what, size_t length )
    {
        if ( source + length <= end && memcmp( what, source, length ) == 0 )
        {
            for ( size_t i = 0; i < length; i++ )
                read();

            return true;
        }
        else
            return false;
    }

    Token* Lexer::readToken()
    {
        while ( source < end )
        {
            if ( read( '\n' ) )
            {
                indent = 0;
                fixedIndent = -1;
            }
            else if ( read( "--", 2 ) )
            {
                // Skip comments

                if ( read( '{' ) )
                {
                    while ( source < end && !read( "}--", 3 ) ) {
                        read();
                    }
                }
                else
                {
                    while ( source < end && read() != '\n' ) {
                    }
                }

                indent = 0;
            }
            // Skip whitespace
            else if ( read( ' ' ) )
                indent++;
            else if ( read( '\t' ) )
                //indent += 4;
                parseError("Tabs are not allowed, please use spaces");
            else if ( !read( '\r' ) )
                break;
        }

        if ( source >= end )
            return 0;

        auto start = nextPoint;

        // Numeric values
        if ( isNumeric( *source ) )
        {
            // TODO: a more efficient type?
            std::stringstream number;
            number << read();

            bool isFloating = false;

			while ( ( isNumeric( *source ) || *source == '.' ) && source < end )
            {
                char c = read();
                number << c;

                if ( c == '.' )
                    isFloating = true;
            }

			if ( isFloating )
                return new TokenLiteralReal(Token_number, SourceSpan(start, point), indent, strtod(number.str().c_str(), 0));
			else
                return new TokenLiteralInteger(Token_integer, SourceSpan(start, point), indent, strtoul( number.str().c_str(), 0, 10 ));
        }

		// Hexadecimal numeric values
		if ( *source == '#' )
        {
            read();
            std::string number;

			while ( isHexaNumeric( *source ) && source < end )
                number += read();

            return new TokenLiteralInteger(Token_integer, SourceSpan(start, point), indent, strtoul( number.c_str(), 0, 16 ));
		}

        // String constants
        if ( read( '\'' ) )
        {
            std::string buffer = "";
            bool wasApostrophe = false, wasDollar = false;  //* Was the last character an apostrophe or a dollar sign?

            //* Read the whole string  -- we will stop when reaching an apostrophe NOT preceeded by another one.
            //* (or when reaching end of the input - error)

            while ( source < end )
            {
                bool wasApostropheNew = false, wasDollarNew = false;

                if ( *source == '\'' )    //* if a ' follows...
                {
                    //* If it's neither preceeded nor followed by ', it is the ending.
                    //* But, if we are at the input end, preceeding apostrophe, if any, is ignored
                    //* Example: 'abc'' (when at file end) is read as "abc'"

                    if ( source + 1 >= end || ( source[1] != '\'' && !wasApostrophe ) )
                    {
                        read();
                        break;
                    }
                    else if ( !wasApostrophe )   //* do not set the wasApostrophe flag twice in a row
                        wasApostropheNew = true;
                }

                //* A dollarSign NOT preceeded by another one
                if ( *source == '$' && !wasDollar )
                    wasDollarNew = true;
                //* $-stuff start
                else if ( wasDollar && *source == 't' )
                    buffer += '\t';
                else if ( wasDollar && *source == 'n' )
                    buffer += '\n';
                // $-stuff end
                else if ( !wasApostrophe ) //* any other character
                    buffer += *source;

                wasApostrophe = wasApostropheNew;
                wasDollar = wasDollarNew;
                read();
            }

            if ( source >= end ) {
                parseError("Reached end of input while parsing a string constant");
                return nullptr;
            }

            return new TokenLiteralString(Token_string, SourceSpan(start, point), indent, fixedIndent, std::move(buffer));
        }

        //* Identifier
        bool tmp = false;

        if ( ( tmp = read( "~.", 2 ) ) || isIdentifier( *source ) )
        {
            std::string buffer = ( tmp == false ) ? "" : "~";

            while ( source < end && ( isIdentifier( *source ) || *source == '~' || *source == '|' ) )
            {
                buffer += read();

                if ( read( "::", 2 ) )
                    buffer += "::";
            }

            Token* t = new TokenLiteralString(Token_identifier, SourceSpan(start, point), indent, fixedIndent, std::move(buffer));
            translateToken( t );
            return t;
        }

        bicharToken( "&&", Token_and )
        bicharToken( "${", Token_construction )
        bicharToken( "==", Token_equals )
        bicharToken( ">=", Token_greaterEquals )
        bicharToken( "<=", Token_lessEquals )
        bicharToken( "-=", Token_minusEquals )
        bicharToken( "+=", Token_plusEquals )
        bicharToken( "||", Token_or )
        bicharToken( "!=", Token_notEquals )
        //bicharToken( "^^", Token_xor )
        charToken( '=', Token_assignation )
        charToken( ':', Token_colon )
        charToken( ',', Token_comma )
        charToken( '/', Token_dividedBy )
        //charToken( '@', Token_global )
        charToken( '>', Token_greater )
        charToken( '{', Token_leftBrace )
        charToken( '(', Token_leftParen )
        charToken( '[', Token_leftSquareBrace )
        charToken( '<', Token_less )
        charToken( '$', Token_myMember )
        charToken( '-', Token_minus )
        charToken( '%', Token_modulo )
        charToken( '!', Token_not )
        charToken( '.', Token_period )
        charToken( '+', Token_plus )
        charToken( '?', Token_questionMark )
        charToken( '}', Token_rightBrace )
        charToken( ')', Token_rightParen )
        charToken( ']', Token_rightSquareBrace )
        charToken( ';', Token_semicolon )
        charToken( '*', Token_times )
        else {
            parseError("Invalid character");
            return nullptr;
        }
    }

    /* ---- Automatic translation of keywords & numeric constants ---- */

    struct Keyword
    {
        const char* word;
        TokenType replacement;
    };

    struct BooleanConstant
    {
        const char* name;
        bool value;
    };

    struct FloatConstant
    {
        const char* name;
        double value;
    };

    struct StringConstant
    {
        const char* name, * value;
    };

    const static Keyword keywords[] =
    {
        { "as",         Token_as },
        { "assert",     Token_assert },
        { "catch",      Token_catch },
        { "class",      Token_class },
        { "else",       Token_else },
        { "for",        Token_for },
        { "function",   Token_function },
        { "if",         Token_if },
        { "in",         Token_in },
        { "infer",      Token_infer },
        { "iterate",    Token_iterate },
        { "local",      Token_local },
        { "member",     Token_member },
        { "nil",        Token_nil },
        { "private",    Token_private },
        { "return",     Token_return },
        { "switch",     Token_switch },
        { "throw",      Token_throw },
        { "try",        Token_try },
        { "while",      Token_while },
        { "with",       Token_with },
        { 0,            Token_none }
    };

    static const BooleanConstant boolConstants[] =
    {
        { "true",           true },
        { "false",          false },
        { nullptr,          0 }
    };

    const static FloatConstant floatConstants[] =
    {
        { "pi",         3.1415926535897932384626433832795028841968 },
        { 0,            0 }
    };

    const static StringConstant stringConstants[] =
    {
        { "newline",    "\n" },
        //{ "tab",        "\t" },
        { 0,            0 }
    };

    static void translateToken( Token* token )
    {
        if ( token->type != Token_identifier )
            return;

        for ( const Keyword* kw = keywords; kw->word; kw++ )
            if ( token->text == kw->word )
            {
                token->type = kw->replacement;
                return;
            }

        for ( const BooleanConstant* cnst = boolConstants; cnst->name; cnst++ )
            if ( token->text == cnst->name )
            {
                token->type = Token_literalBool;
                token->boolValue = cnst->value;
                return;
            }

        for ( const FloatConstant* cnst = floatConstants; cnst->name; cnst++ )
            if ( token->text == cnst->name )
            {
                token->type = Token_number;
                token->realValue = cnst->value;
                return;
            }

        for ( const StringConstant* cnst = stringConstants; cnst->name; cnst++ )
            if ( token->text == cnst->name )
            {
                token->type = Token_string;
                token->text = cnst->value;
                return;
            }
    }
}
