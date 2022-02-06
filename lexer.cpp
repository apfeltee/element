#include "element.h"

#include <istream>
#include <string>
#include "element.h"

namespace element
{
    bool IsSpace(char c)
    {
        return c == ' ' || c == '\t';
    }

    bool IsNewLine(char c)
    {
        return c == '\n' || c == '\r';
    }

    bool IsDigit(char c)
    {
        return c >= '0' && c <= '9';
    }

    bool IsAlpha(char c)
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
    }

    Lexer::Lexer(Logger& logger) : m_instream(nullptr), m_logger(logger)
    {
        reset();
    }

    Lexer::Lexer(std::istream& input, Logger& logger) : m_instream(&input), m_logger(logger)
    {
        reset();
    }

    void Lexer::setInputStream(std::istream& input)
    {
        m_instream = &input;

        reset();
    }

    Token Lexer::getNextToken()
    {
        m_lastbuf.clear();
        m_lastbuf.push_back(m_currch);

    begining:
        while(IsSpace(m_currch))
            m_currch = getNextChar();

        if(IsNewLine(m_currch))
        {
            m_currch = getNextChar();
            m_currtoken = T_NewLine;
            m_location.line += 1;
            m_currcolumn = 1;
            return T_NewLine;
        }

        m_location.column = m_currcolumn;

        if(handleCommentOrDiv())// anything that starts with '/'
        {
            if(m_muststartover)
            {
                m_muststartover = false;
                goto begining;
            }

            return m_currtoken;
        }

        if(doWord() ||// identifiers or keywords
           doNumber() ||// integer or float
           doString() ||// "something..."
           doColumn() ||// : ::
           handleSubOrArray() ||// - -= ->
           doDollarSign() ||// $ $0 $1 $$
           doLessOrPush() ||// < <= <<
           doGreaterOrPop() ||// > >= >>
           handleSingleChar('(', T_LeftParent) || handleSingleChar(')', T_RightParent) || handleSingleChar('{', T_LeftBrace)
           || handleSingleChar('}', T_RightBrace) || handleSingleChar('[', T_LeftBracket) || handleSingleChar(']', T_RightBracket)
           || handleSingleChar(';', T_Semicolumn) || handleSingleChar(',', T_Comma) || handleSingleChar('.', T_Dot)
           || handleSingleChar('#', T_SizeOf) || handleSingleChar(EOF, T_EOF) || doCharAndEqual('+', T_Add, T_AssignAdd)
           || doCharAndEqual('*', T_Multiply, T_AssignMultiply) || doCharAndEqual('^', T_Power, T_AssignPower)
           || doCharAndEqual('%', T_Modulo, T_AssignModulo) || doCharAndEqual('~', T_Concatenate, T_AssignConcatenate)
           || doCharAndEqual('=', T_Assignment, T_Equal) || doCharAndEqual('!', T_InvalidToken, T_NotEqual))
        {
            return m_currtoken;
        }

        m_logger.pushError(m_location, std::string("Unrecognized token ") + m_currch);
        return m_currtoken = T_InvalidToken;
        ;
    }

    Token Lexer::getNextTokenNoLF()
    {
        Token t = getNextToken();

        while(t == T_NewLine)
            t = getNextToken();

        return t;
    }

    Token Lexer::getCurrentToken() const
    {
        return m_currtoken;
    }

    // HACK ////////////////////////////////////////////////////////////////////
    // This is used when parsing 'if' expressions. We need to check if there will
    // be an 'else' clause for the 'if' which will eat the new line, which will
    // in turn cause trouble for the 'Parser::parseExpr' function.
    // To prevent this we will "rewind" the lexer input back to its original state
    // before eating the new line.
    void Lexer::rewindBecauseMissingElse()
    {
        for(int i = m_lastbuf.size() - 1; i >= 0; --i)
            m_instream->putback(m_lastbuf[i]);

        m_currch = '\n';

        m_currtoken = T_NewLine;

        m_location.line -= 1;
        m_currcolumn = 1;
    }

    const Location& Lexer::location() const
    {
        return m_location;
    }

    const std::string& Lexer::getLastIdent() const
    {
        return m_lastident;
    }

    const std::string& Lexer::GetLastString() const
    {
        return m_laststring;
    }

    int Lexer::getLastInteger() const
    {
        return m_lastinteger;
    }

    int Lexer::getLastArgIndex() const
    {
        return m_lastargidx;
    }

    float Lexer::getLastFloat() const
    {
        return m_lastfloat;
    }

    bool Lexer::getLastBool() const
    {
        return m_lastbool;
    }

    void Lexer::reset()
    {
        m_currch = ' ';

        m_currtoken = T_InvalidToken;
        m_location = Location();
        m_currcolumn = 1;
        m_lastident = "";
        m_laststring = "";
        m_lastinteger = 0;
        m_lastargidx = 0;
        m_lastfloat = 0.0f;
        m_lastbool = false;

        m_muststartover = false;
    }

    char Lexer::getNextChar()
    {
        ++m_currcolumn;

        char ch = m_instream->get();

        m_lastbuf.push_back(ch);

        return ch;
    }

    bool Lexer::handleCommentOrDiv()
    {
        if(m_currch != '/')// this is not a comment or division
            return false;

        m_currch = getNextChar();// eat /

        // single line comment ///////////////////////////////////////
        if(m_currch == '/')
        {
            while(!IsNewLine(m_currch))
                m_currch = getNextChar();
            m_currch = getNextChar();// eat the new line

            m_currtoken = T_NewLine;
            m_location.line += 1;
            m_location.column = m_currcolumn = 1;

            return true;
        }

        // multi line comment ////////////////////////////////////////
        if(m_currch == '*')
        {
            m_currch = getNextChar();// eat *

            int nestedLevel = 1;// nested multiline comments
            while(nestedLevel > 0)
            {
                if(m_currch == EOF)
                {
                    m_logger.pushError(m_location, "Unterminated multiline comment");
                    return false;
                }

                while(m_currch != '*' && m_currch != '/')
                {
                    if(IsNewLine(m_currch))
                    {
                        m_location.line += 1;
                        m_location.column = m_currcolumn = 0;
                    }

                    if(m_currch == EOF)
                    {
                        m_logger.pushError(m_location, "Unterminated multiline comment");
                        return false;
                    }

                    m_currch = getNextChar();
                }

                if(m_currch == '*')
                {
                    m_currch = getNextChar();// eat *

                    if(m_currch == '/')
                    {
                        m_currch = getNextChar();// eat /
                        nestedLevel -= 1;
                    }
                }
                else// m_currch == '/'
                {
                    m_currch = getNextChar();// eat /

                    if(m_currch == '*')
                    {
                        m_currch = getNextChar();// eat *
                        nestedLevel += 1;
                    }
                }
            }

            // get back to searching for next token...
            m_muststartover = true;
            return true;
        }

        // dvision with assignment ///////////////////////////////////
        if(m_currch == '=')
        {
            m_currtoken = T_AssignDivide;
            m_currch = getNextChar();// eat the '='
            return true;
        }

        // just division /////////////////////////////////////////////
        m_currtoken = T_Divide;
        return true;
    }

    bool Lexer::doWord()
    {
        if(!IsAlpha(m_currch))
            return false;

        std::string word;
        word.push_back(m_currch);

        m_currch = getNextChar();

        while(IsAlpha(m_currch) || IsDigit(m_currch))
        {
            word.push_back(m_currch);
            m_currch = getNextChar();
        }

        if(word == "true")
        {
            m_lastbool = true;
            m_currtoken = T_Bool;
        }

        else if(word == "false")
        {
            m_lastbool = false;
            m_currtoken = T_Bool;
        }

        else if(word == "if")
            m_currtoken = T_If;

        else if(word == "else")
            m_currtoken = T_Else;

        else if(word == "elif")
            m_currtoken = T_Elif;

        else if(word == "for")
            m_currtoken = T_For;

        else if(word == "in")
            m_currtoken = T_In;

        else if(word == "while")
            m_currtoken = T_While;

        else if(word == "this")
            m_currtoken = T_This;

        else if(word == "nil")
            m_currtoken = T_Nil;

        else if(word == "return")
            m_currtoken = T_Return;

        else if(word == "break")
            m_currtoken = T_Break;

        else if(word == "continue")
            m_currtoken = T_Continue;

        else if(word == "yield")
            m_currtoken = T_Yield;

        else if(word == "and")
            m_currtoken = T_And;

        else if(word == "or")
            m_currtoken = T_Or;

        else if(word == "xor")
            m_currtoken = T_Xor;

        else if(word == "not")
            m_currtoken = T_Not;

        else if(word == "_")
            m_currtoken = T_Underscore;

        else// must be an identifier
        {
            m_lastident = word;
            m_currtoken = T_Identifier;
        }

        return true;
    }

    bool Lexer::doNumber()
    {
        if(!IsDigit(m_currch))
            return false;

        std::string number;
        number.push_back(m_currch);

        m_currch = getNextChar();

        while(IsDigit(m_currch) || m_currch == '_')
        {
            if(m_currch != '_')
                number.push_back(m_currch);

            m_currch = getNextChar();
        }

        if(m_currch != '.')// integer
        {
            m_lastinteger = std::stoi(number);
            m_currtoken = T_Integer;
            return true;
        }
        else// float
        {
            number.push_back('.');

            m_currch = getNextChar();

            if(m_currch == '.')// the case of "123.." ranges maybe?!?!
            {
            }

            while(IsDigit(m_currch) || m_currch == '_')
            {
                if(m_currch != '_')
                    number.push_back(m_currch);

                m_currch = getNextChar();
            }

            m_lastfloat = std::stof(number);
            m_currtoken = T_Float;
            return true;
        }

        return false;
    }

    bool Lexer::handleSingleChar(const char ch, const Token t)
    {
        if(m_currch == ch)
        {
            m_currtoken = t;
            m_currch = getNextChar();
            return true;
        }

        return false;
    }

    bool Lexer::doColumn()
    {
        if(m_currch != ':')
            return false;

        m_currch = getNextChar();// eat :

        if(m_currch == ':')
        {
            m_currtoken = T_DoubleColumn;
            m_currch = getNextChar();// eat :
            return true;
        }

        m_currtoken = T_Column;
        return true;
    }

    bool Lexer::doString()
    {
        if(m_currch != '"')
            return false;

        m_laststring.clear();

        m_currch = getNextChar();

        while(m_currch != '"')
        {
            if(m_currch == '\\')
            {
            }// TODO: handle escaping...

            m_laststring.push_back(m_currch);
            m_currch = getNextChar();
        }

        m_currch = getNextChar();// eat the last '"'

        m_currtoken = T_String;
        return true;
    }

    bool Lexer::handleSubOrArray()
    {
        if(m_currch != '-')
            return false;

        m_currch = getNextChar();// eat -

        if(m_currch == '=')
        {
            m_currch = getNextChar();// eat =
            m_currtoken = T_AssignSubtract;
            return true;
        }

        if(m_currch == '>')
        {
            m_currch = getNextChar();// eat >
            m_currtoken = T_Arrow;
            return true;
        }

        m_currtoken = T_Subtract;
        return true;
    }

    bool Lexer::doLessOrPush()
    {
        if(m_currch != '<')
            return false;

        m_currch = getNextChar();// eat <

        if(m_currch == '=')
        {
            m_currch = getNextChar();// eat =
            m_currtoken = T_LessEqual;
            return true;
        }

        if(m_currch == '<')
        {
            m_currch = getNextChar();// eat <
            m_currtoken = T_ArrayPushBack;
            return true;
        }

        m_currtoken = T_Less;
        return true;
    }

    bool Lexer::doGreaterOrPop()
    {
        if(m_currch != '>')
            return false;

        m_currch = getNextChar();// eat >

        if(m_currch == '=')
        {
            m_currch = getNextChar();// eat =
            m_currtoken = T_GreaterEqual;
            return true;
        }

        if(m_currch == '>')
        {
            m_currch = getNextChar();// eat >
            m_currtoken = T_ArrayPopBack;
            return true;
        }

        m_currtoken = T_Greater;
        return true;
    }

    bool Lexer::doCharAndEqual(char ch, Token withoutAssign, Token withAssign)
    {
        if(m_currch != ch)
            return false;

        m_currch = getNextChar();// eat ch

        if(m_currch == '=')
        {
            m_currch = getNextChar();// eat =
            m_currtoken = withAssign;
            return true;
        }

        m_currtoken = withoutAssign;
        return true;
    }

    bool Lexer::doDollarSign()
    {
        if(m_currch != '$')
            return false;

        m_currch = getNextChar();// eat $

        if(m_currch == '$')
        {
            m_currch = getNextChar();// eat $
            m_currtoken = T_ArgumentList;
            return true;
        }

        if(IsDigit(m_currch))
        {
            m_lastargidx = unsigned(m_currch - '0');

            m_currch = getNextChar();// eat digit
            m_currtoken = T_Argument;
            return true;
        }

        m_lastargidx = 0;
        m_currtoken = T_Argument;
        return true;
    }

}// namespace element
