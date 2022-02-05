#include "element.h"

#include "element.h"
#include "element.h"
#include "element.h"
#include "element.h"

using namespace std::string_literals;

namespace element
{
    Parser::Parser(Logger& logger) : m_logger(logger), m_lexer(logger)
    {
    }

    std::unique_ptr<ast::FunctionNode> Parser::Parse(std::istream& input)
    {
        m_lexer.setInputStream(input);
        m_lexer.getNextToken();

        std::vector<std::shared_ptr<ast::Node>> expressions;

        std::shared_ptr<ast::Node> node = ParseExpression();

        while(node)
        {
            expressions.push_back(node);
            node = ParseExpression();
        }

        if(m_logger.hasMessages())
        {
            for(auto expression : expressions)
                expression.reset();

            return nullptr;
        }

        std::shared_ptr<ast::Node> body = nullptr;

        if(expressions.size() == 1)
            body = expressions.front();
        else// zero or more than one expressions form a block
            body = std::make_shared<ast::BlockNode>(expressions, Location());

        // return the global "main" function
        return std::make_unique<ast::FunctionNode>(std::vector<std::string>(), body, Location());
    }

    std::shared_ptr<ast::Node> Parser::ParseExpression()
    {
        Token prevToken = T_InvalidToken;
        Token currentToken = m_lexer.getCurrentToken();

        // Check for left over expression termination symbols
        while(currentToken == T_NewLine || currentToken == T_Semicolumn)
            currentToken = m_lexer.getNextTokenNoLF();// eat \n or ;

        if(currentToken == T_EOF)
            return nullptr;

        // https://www.engr.mun.ca/~theo/Misc/exp_parsing.htm
        // https://en.wikipedia.org/wiki/Shunting-yard_algorithm
        std::vector<Operator> operators;
        std::vector<std::shared_ptr<ast::Node>> operands;

        operators.push_back({ T_InvalidToken, -1, ET_UnaryOperator, nullptr });// sentinel
        Location coords;
        ExpressionType expressionType = ET_Unknown;

        bool primaryExpected = false;

        do// the actual parsing
        {
            primaryExpected = false;
            coords = m_lexer.location();
            expressionType = CurrentExpressionType(prevToken, currentToken);

            switch(expressionType)
            {
                case ET_BinaryOperator:
                case ET_UnaryOperator:
                {
                    const OperatorInfo& op = GetOperatorInfo(currentToken, expressionType == ET_BinaryOperator);

                    while((op.precedence < operators.back().precedence)
                          || (op.precedence == operators.back().precedence && !op.isRightAssociative))
                    {
                        FoldOperatorStacks(operators, operands);
                    }

                    operators.push_back({ currentToken, op.precedence, expressionType, nullptr, coords });

                    primaryExpected = true;
                    m_lexer.getNextTokenNoLF();
                    break;
                }

                case ET_IndexOperator:
                {
                    const OperatorInfo& op = GetOperatorInfo(currentToken, false);

                    while((op.precedence < operators.back().precedence)
                          || (op.precedence == operators.back().precedence && !op.isRightAssociative))
                    {
                        FoldOperatorStacks(operators, operands);
                    }

                    std::shared_ptr<ast::Node> auxNode = ParseIndexOperator();

                    if(!auxNode)// propagate error
                    {
                        for(auto trash : operands)
                            trash.reset();
                        return nullptr;
                    }

                    operators.push_back({ currentToken, op.precedence, expressionType, auxNode, coords });
                    break;
                }

                case ET_FunctionCall:
                {
                    const OperatorInfo& op = GetOperatorInfo(currentToken, false);

                    while((op.precedence < operators.back().precedence)
                          || (op.precedence == operators.back().precedence && !op.isRightAssociative))
                    {
                        FoldOperatorStacks(operators, operands);
                    }

                    std::shared_ptr<ast::Node> auxNode = ParseArguments();

                    if(!auxNode)// propagate error
                    {
                        for(auto trash : operands)
                            trash.reset();
                        return nullptr;
                    }

                    operators.push_back({ currentToken, op.precedence, expressionType, auxNode, coords });
                    break;
                }

                case ET_FunctionAssignment:
                {
                    const OperatorInfo& op = GetOperatorInfo(currentToken, true);

                    while((op.precedence < operators.back().precedence)
                          || (op.precedence == operators.back().precedence && !op.isRightAssociative))
                    {
                        FoldOperatorStacks(operators, operands);
                    }

                    std::shared_ptr<ast::Node> auxNode = ParseFunction();

                    if(!auxNode)// propagate error
                    {
                        for(auto trash : operands)
                            trash.reset();
                        return nullptr;
                    };

                    operators.push_back({ currentToken, op.precedence, expressionType, auxNode, coords });
                    break;
                }

                case ET_PrimaryExpression:
                {
                    std::shared_ptr<ast::Node> node = ParsePrimary();

                    if(!node)// propagate error
                    {
                        for(auto trash : operands)
                            trash.reset();
                        return nullptr;
                    }

                    operands.push_back(node);
                    break;
                }

                case ET_Unknown:
                default:
                {
                    m_logger.pushError(coords, "Syntax error: operator expected");
                    for(auto trash : operands)
                        trash.reset();
                    return nullptr;
                }
            }

            prevToken = currentToken;
            currentToken = m_lexer.getCurrentToken();
        } while(!IsExpressionTerminator(currentToken) || primaryExpected);

        // no more of this expression, fold everything to a single node
        while(operands.size() > 1 ||// last thing left is the result
              operators.size() > 1)// parse all, leave only the sentinel
        {
            FoldOperatorStacks(operators, operands);
        }

        return operands.back();
    }

    std::shared_ptr<ast::Node> Parser::ParsePrimary()
    {
        switch(m_lexer.getCurrentToken())
        {
            case T_Nil:
            case T_Integer:
            case T_Float:
            case T_String:
            case T_Bool:
                return ParsePrimitive();

            case T_This:
            case T_Argument:
            case T_ArgumentList:
            case T_Identifier:
            case T_Underscore:
                return ParseVarialbe();

            case T_LeftParent:
                return ParseParenthesis();

            case T_LeftBrace:
                return ParseBlock();

            case T_LeftBracket:
                return ParseArrayOrObject();

            case T_Column:
            case T_DoubleColumn:
                return ParseFunction();

            case T_If:
                return ParseIf();

            case T_While:
                return ParseWhile();

            case T_For:
                return ParseFor();

            case T_Return:
            case T_Break:
            case T_Continue:
            case T_Yield:
                return ParseControlExpression();

            default:
                m_logger.pushError(m_lexer.location(),
                                  "Syntax error: unexpected token "s + TokenAsString(m_lexer.getCurrentToken()));
                return nullptr;
        }
    }

    std::shared_ptr<ast::Node> Parser::ParsePrimitive()
    {
        Location coords = m_lexer.location();

        switch(m_lexer.getCurrentToken())
        {
            case T_Nil:
            {
                m_lexer.getNextToken();// eat nil
                return std::make_shared<ast::Node>(ast::Node::N_Nil, coords);
            }
            case T_Integer:
            {
                int i = m_lexer.getLastInteger();
                m_lexer.getNextToken();// eat integer
                return std::make_shared<ast::IntegerNode>(i, coords);
            }
            case T_Float:
            {
                float f = m_lexer.getLastFloat();
                m_lexer.getNextToken();// eat float
                return std::make_shared<ast::FloatNode>(f, coords);
            }
            case T_String:
            {
                std::string s = m_lexer.GetLastString();
                m_lexer.getNextToken();// eat string
                return std::make_shared<ast::StringNode>(s, coords);
            }
            case T_Bool:
            {
                bool b = m_lexer.getLastBool();
                m_lexer.getNextToken();// eat bool
                return std::make_shared<ast::BoolNode>(b, coords);
            }
            default:
                m_logger.pushError(coords, "Syntax error: unexpected token "s + TokenAsString(m_lexer.getCurrentToken()));
                return nullptr;
        }
    }

    std::shared_ptr<ast::Node> Parser::ParseVarialbe()
    {
        Location coords = m_lexer.location();

        switch(m_lexer.getCurrentToken())
        {
            case T_This:
            {
                m_lexer.getNextToken();// eat this
                return std::make_shared<ast::VariableNode>(ast::VariableNode::V_This, coords);
            }
            case T_Argument:
            {
                int n = m_lexer.GetLastArgumentIndex();
                m_lexer.getNextToken();// eat $
                return std::make_shared<ast::VariableNode>(n, coords);
            }
            case T_ArgumentList:
            {
                m_lexer.getNextToken();// eat $$
                return std::make_shared<ast::VariableNode>(ast::VariableNode::V_ArgumentList, coords);
            }
            case T_Underscore:
            {
                m_lexer.getNextToken();// eat _
                return std::make_shared<ast::VariableNode>(ast::VariableNode::V_Underscore, coords);
            }
            case T_Identifier:
            {
                std::string s = m_lexer.getLastIdent();
                m_lexer.getNextToken();// eat identifier
                return std::make_shared<ast::VariableNode>(s, coords);
            }
            default:
                m_logger.pushError(coords, "Syntax error: unexpected token "s + TokenAsString(m_lexer.getCurrentToken()));
                return nullptr;
        }
    }

    std::shared_ptr<ast::Node> Parser::ParseParenthesis()
    {
        m_lexer.getNextTokenNoLF();// eat (

        if(IsExpressionTerminator(m_lexer.getCurrentToken()))
        {
            m_logger.pushError(m_lexer.location(), "Syntax error: expression expected");
            return nullptr;
        }

        std::shared_ptr<ast::Node> node = ParseExpression();

        if(!node)
            return nullptr;

        if(m_lexer.getCurrentToken() != T_RightParent)
        {
            m_logger.pushError(m_lexer.location(), "Syntax error: expected )");
            node.reset();
            return nullptr;
        }

        m_lexer.getNextToken();// eat )

        return node;
    }

    std::shared_ptr<ast::Node> Parser::ParseIndexOperator()
    {
        m_lexer.getNextTokenNoLF();// eat [

        if(IsExpressionTerminator(m_lexer.getCurrentToken()))
        {
            m_logger.pushError(m_lexer.location(), "Syntax error: expression expected");
            return nullptr;
        }

        std::shared_ptr<ast::Node> node = ParseExpression();

        if(!node)
            return nullptr;

        if(m_lexer.getCurrentToken() != T_RightBracket)
        {
            m_logger.pushError(m_lexer.location(), "Syntax error: expected ]");
            node.reset();
            return nullptr;
        }

        m_lexer.getNextToken();// eat ]

        return node;
    }

    std::shared_ptr<ast::Node> Parser::ParseBlock()
    {
        Location coords = m_lexer.location();

        m_lexer.getNextTokenNoLF();// eat {

        std::vector<std::shared_ptr<ast::Node>> nodes;

        while(m_lexer.getCurrentToken() != T_RightBrace)
        {
            std::shared_ptr<ast::Node> node = ParseExpression();

            if(!node)
            {
                if(!m_logger.hasMessages())
                    m_logger.pushError(m_lexer.location(), "Syntax error: expression expected");

                for(auto trash : nodes)
                    trash.reset();
                return nullptr;
            }

            nodes.push_back(node);

            // Check for left over expression termination symbols
            Token token = m_lexer.getCurrentToken();
            while(token == T_NewLine || token == T_Semicolumn)
                token = m_lexer.getNextTokenNoLF();// eat \n or ;
        }

        m_lexer.getNextToken();// eat }

        return std::make_shared<ast::BlockNode>(nodes, coords);
    }

    std::shared_ptr<ast::Node> Parser::ParseFunction()
    {
        Location coords = m_lexer.location();
        std::vector<std::string> namedParameters;

        if(m_lexer.getCurrentToken() == T_DoubleColumn)
        {
            m_lexer.getNextTokenNoLF();// eat ::
        }
        else if(m_lexer.getCurrentToken() == T_Column)
        {
            m_lexer.getNextTokenNoLF();// eat :

            if(m_lexer.getCurrentToken() != T_LeftParent)
            {
                m_logger.pushError(coords, "Syntax error: expected (");
                return nullptr;
            }

            m_lexer.getNextTokenNoLF();// eat (

            while(m_lexer.getCurrentToken() != T_RightParent)
            {
                if(m_lexer.getCurrentToken() != T_Identifier)
                {
                    m_logger.pushError(m_lexer.location(), "Syntax error: identifier expected");
                    return nullptr;
                }

                namedParameters.push_back(m_lexer.getLastIdent());

                Token token = m_lexer.getNextTokenNoLF();// eat identifier

                if(token == T_Comma)
                {
                    m_lexer.getNextTokenNoLF();// eat ,
                }
                else if(token != T_RightParent)
                {
                    m_logger.pushError(m_lexer.location(), "Syntax error: expected , or )");
                    return nullptr;
                }
            }

            m_lexer.getNextTokenNoLF();// eat )
        }

        if(IsExpressionTerminator(m_lexer.getCurrentToken()))
        {
            m_logger.pushError(m_lexer.location(), "Syntax error: expression expected");
            return nullptr;
        }

        std::shared_ptr<ast::Node> body = ParseExpression();

        if(!body)
            return nullptr;

        return std::make_shared<ast::FunctionNode>(namedParameters, body, coords);
    }

    std::shared_ptr<ast::Node> Parser::ParseArguments()
    {
        Location coords = m_lexer.location();
        m_lexer.getNextTokenNoLF();// eat (

        std::vector<std::shared_ptr<ast::Node>> arguments;

        while(m_lexer.getCurrentToken() != T_RightParent)
        {
            if(IsExpressionTerminator(m_lexer.getCurrentToken()))
            {
                m_logger.pushError(m_lexer.location(), "Syntax error: expression expected");
                for(std::shared_ptr<ast::Node> trash : arguments)
                    trash.reset();
                return nullptr;
            }

            std::shared_ptr<ast::Node> argument = ParseExpression();

            if(!argument)
            {
                for(auto trash : arguments)
                    trash.reset();
                return nullptr;
            }

            arguments.push_back(argument);

            if(m_lexer.getCurrentToken() == T_NewLine)
                m_lexer.getNextTokenNoLF();// eat \n

            if(m_lexer.getCurrentToken() == T_Comma)
            {
                m_lexer.getNextTokenNoLF();// eat ,
            }
            else if(m_lexer.getCurrentToken() != T_RightParent)
            {
                m_logger.pushError(m_lexer.location(), "Syntax error: expected )");
                for(auto trash : arguments)
                    trash.reset();
                return nullptr;
            }
        }

        m_lexer.getNextToken();// eat )

        return std::make_shared<ast::ArgumentsNode>(arguments, coords);
    }

    std::shared_ptr<ast::Node> Parser::ParseArrayOrObject()
    {
        Location coords = m_lexer.location();
        m_lexer.getNextTokenNoLF();// eat [

        // check for empty object literal [=] //////////////////////////////////////
        if(m_lexer.getCurrentToken() == T_Assignment)
        {
            m_lexer.getNextTokenNoLF();// eat =

            if(m_lexer.getCurrentToken() == T_RightBracket)
            {
                m_lexer.getNextToken();// eat ]
                return std::make_shared<ast::ObjectNode>(coords);
            }
            else
            {
                m_logger.pushError(m_lexer.location(), "Syntax error: expression expected");
                return nullptr;
            }
        }

        // otherwise it could be either an array or an object with members /////////
        auto IsAssignmentOperator = [](const std::shared_ptr<ast::Node>& n)
        { return n->type == ast::Node::N_BinaryOperator && (std::dynamic_pointer_cast<ast::BinaryOperatorNode>(n))->op == T_Assignment; };

        bool firstExpression = true;
        bool isObject = false;

        std::vector<std::shared_ptr<ast::Node>> keys;
        std::vector<std::shared_ptr<ast::Node>> elements;

        while(m_lexer.getCurrentToken() != T_RightBracket)
        {
            if(IsExpressionTerminator(m_lexer.getCurrentToken()))
            {
                m_logger.pushError(m_lexer.location(), "Syntax error: expression expected");
                for(auto trash : keys)
                    trash.reset();
                for(auto trash : elements)
                    trash.reset();
                return nullptr;
            }

            std::shared_ptr<ast::Node> element = ParseExpression();

            if(!element)
            {
                for(auto trash : keys)
                    trash.reset();
                for(auto trash : elements)
                    trash.reset();
                return nullptr;
            }

            if(firstExpression)
            {
                isObject = IsAssignmentOperator(element);
                firstExpression = false;
            }
            else if(isObject != IsAssignmentOperator(element))
            {
                m_logger.pushError(m_lexer.location(), "Syntax error: mixing together syntax for arrays and objects");
                for(auto trash : keys)
                    trash.reset();
                for(auto trash : elements)
                    trash.reset();
                return nullptr;
            }

            if(isObject)
            {
                auto node = std::dynamic_pointer_cast<ast::BinaryOperatorNode>(element);
                keys.push_back(node->lhs);
                elements.push_back(node->rhs);
            }
            else
            {
                elements.push_back(element);
            }

            Token trailingToken = m_lexer.getCurrentToken();
            while(trailingToken == T_NewLine)
                trailingToken = m_lexer.getNextTokenNoLF();// eat \n

            if(trailingToken == T_Comma)
            {
                m_lexer.getNextTokenNoLF();// eat ,
            }
            else if(trailingToken != T_RightBracket)
            {
                m_logger.pushError(m_lexer.location(), "Syntax error: expression expected, elements should be separated by commas");
                for(auto trash : keys)
                    trash.reset();
                for(auto trash : elements)
                    trash.reset();
                return nullptr;
            }
        }

        m_lexer.getNextToken();// eat ]

        if(isObject)
        {
            size_t size = elements.size();

            ast::ObjectNode::KeyValuePairs pairs;
            pairs.reserve(size);

            for(size_t i = 0; i < size; ++i)
                pairs.push_back(std::make_pair(keys[i], elements[i]));

            return  std::make_shared<ast::ObjectNode>(pairs, coords);
        }
        else
        {
            return std::make_shared<ast::ArrayNode>(elements, coords);
        }
    }

    std::shared_ptr<ast::Node> Parser::ParseIf()
    {
        Location coords = m_lexer.location();
        m_lexer.getNextTokenNoLF();// eat if

        if(m_lexer.getCurrentToken() != T_LeftParent)
        {
            m_logger.pushError(m_lexer.location(), "Syntax error: expected (");
            return nullptr;// Syntax error
        }

        m_lexer.getNextTokenNoLF();// eat (

        if(IsExpressionTerminator(m_lexer.getCurrentToken()))
        {
            m_logger.pushError(m_lexer.location(), "Syntax error: expression expected");
            return nullptr;
        }

        std::shared_ptr<ast::Node> condition = ParseExpression();

        if(!condition)
            return nullptr;

        if(m_lexer.getCurrentToken() != T_RightParent)
        {
            m_logger.pushError(m_lexer.location(), "Syntax error: expected )");
            condition.reset();
            return nullptr;// Syntax error
        }

        m_lexer.getNextTokenNoLF();// eat )

        if(IsExpressionTerminator(m_lexer.getCurrentToken()))
        {
            m_logger.pushError(m_lexer.location(), "Syntax error: expression expected");
            condition.reset();
            return nullptr;
        }

        std::shared_ptr<ast::Node> thenPath = ParseExpression();

        if(!thenPath)// propagate error
        {
            condition.reset();
            return nullptr;
        }

        std::shared_ptr<ast::Node> elsePath = nullptr;

        // HACK ////////////////////////////////////////////////////////////////////
        // we need to check if there will be an 'else' clause, but it may be after
        // the new line, so we have to eat it. This is troublesome because later the
        // 'ParseExpression' function will not know to terminate the expression.
        // To prevent this we will "rewind" the lexer input back to its original state.
        bool shouldRewind = true;
        if(m_lexer.getCurrentToken() == T_NewLine)
            m_lexer.getNextTokenNoLF();// eat \n

        if(m_lexer.getCurrentToken() == T_Elif)
        {
            elsePath = ParseIf();

            if(!elsePath)
            {
                condition.reset();
                thenPath.reset();
                return nullptr;
            }

            shouldRewind = false;
        }
        else if(m_lexer.getCurrentToken() == T_Else)
        {
            m_lexer.getNextTokenNoLF();// eat else

            if(IsExpressionTerminator(m_lexer.getCurrentToken()))
            {
                m_logger.pushError(m_lexer.location(), "Syntax error: expression expected");
                condition.reset();
                thenPath.reset();
                return nullptr;
            }

            elsePath = ParseExpression();

            if(!elsePath)
            {
                condition.reset();
                thenPath.reset();
                return nullptr;
            }

            shouldRewind = false;
        }

        if(shouldRewind)
            m_lexer.RewindDueToMissingElse();

        return std::make_shared<ast::IfNode>(condition, thenPath, elsePath, coords);
    }

    std::shared_ptr<ast::Node> Parser::ParseWhile()
    {
        Location coords = m_lexer.location();
        m_lexer.getNextTokenNoLF();// eat while

        if(m_lexer.getCurrentToken() != T_LeftParent)
        {
            m_logger.pushError(m_lexer.location(), "Syntax error: expected (");
            return nullptr;
        }

        m_lexer.getNextTokenNoLF();// eat (

        if(IsExpressionTerminator(m_lexer.getCurrentToken()))
        {
            m_logger.pushError(m_lexer.location(), "Syntax error: expression expected");
            return nullptr;
        }

        std::shared_ptr<ast::Node> condition = ParseExpression();

        if(!condition)
            return nullptr;

        if(m_lexer.getCurrentToken() != T_RightParent)
        {
            m_logger.pushError(m_lexer.location(), "Syntax error: expected )");
            condition.reset();
            return nullptr;
        }

        m_lexer.getNextTokenNoLF();// eat )

        if(IsExpressionTerminator(m_lexer.getCurrentToken()))
        {
            m_logger.pushError(m_lexer.location(), "Syntax error: expression expected");
            condition.reset();
            return nullptr;
        }

        std::shared_ptr<ast::Node> body = ParseExpression();

        if(!body)// propagate error
        {
            condition.reset();
            return nullptr;
        }

        return std::make_shared<ast::WhileNode>(condition, body, coords);
    }

    std::shared_ptr<ast::Node> Parser::ParseFor()
    {
        Location coords = m_lexer.location();
        m_lexer.getNextTokenNoLF();// eat for

        if(m_lexer.getCurrentToken() != T_LeftParent)
        {
            m_logger.pushError(m_lexer.location(), "Syntax error: expected (");
            return nullptr;
        }

        m_lexer.getNextTokenNoLF();// eat (

        if(IsExpressionTerminator(m_lexer.getCurrentToken()))
        {
            m_logger.pushError(m_lexer.location(), "Syntax error: expression expected");
            return nullptr;
        }

        std::shared_ptr<ast::Node> iteratorVariable = ParseExpression();

        if(!iteratorVariable)
            return nullptr;

        if(m_lexer.getCurrentToken() != T_In)
        {
            m_logger.pushError(m_lexer.location(), "Syntax error: expected 'in'");
            iteratorVariable.reset();
            return nullptr;
        }

        m_lexer.getNextTokenNoLF();// eat in

        if(IsExpressionTerminator(m_lexer.getCurrentToken()))
        {
            m_logger.pushError(m_lexer.location(), "Syntax error: expression expected");
            iteratorVariable.reset();
            return nullptr;
        }

        std::shared_ptr<ast::Node> iteratedExpression = ParseExpression();

        if(!iteratedExpression)
        {
            iteratorVariable.reset();
            return nullptr;
        }

        if(m_lexer.getCurrentToken() != T_RightParent)
        {
            m_logger.pushError(m_lexer.location(), "Syntax error: expected )");
            iteratorVariable.reset();
            iteratedExpression.reset();
            return nullptr;
        }

        m_lexer.getNextTokenNoLF();// eat )

        if(IsExpressionTerminator(m_lexer.getCurrentToken()))
        {
            m_logger.pushError(m_lexer.location(), "Syntax error: expression expected");
            iteratorVariable.reset();
            iteratedExpression.reset();
            return nullptr;
        }

        std::shared_ptr<ast::Node> body = ParseExpression();

        if(!body)// propagate error
        {
            iteratorVariable.reset();
            iteratedExpression.reset();
            return nullptr;
        }

        return std::make_shared<ast::ForNode>(iteratorVariable, iteratedExpression, body, coords);
    }

    std::shared_ptr<ast::Node> Parser::ParseControlExpression()
    {
        Location coords = m_lexer.location();
        Token controlType = m_lexer.getCurrentToken();

        Token token = m_lexer.getNextToken();// eat the control expression

        std::shared_ptr<ast::Node> value = nullptr;

        if(!IsExpressionTerminator(token))
        {
            value = ParseExpression();

            if(!value)// propagate error
                return nullptr;
        }

        switch(controlType)
        {
            case T_Return:
                return std::make_shared<ast::ReturnNode>(value, coords);
            case T_Break:
                return std::make_shared<ast::BreakNode>(value, coords);
            case T_Continue:
                return std::make_shared<ast::ContinueNode>(value, coords);
            case T_Yield:
                return std::make_shared<ast::YieldNode>(value, coords);
            default:
                value.reset();
                return nullptr;
        }
    }

    Parser::ExpressionType Parser::CurrentExpressionType(Token prevToken, Token token) const
    {
        static const Token operators[] = {
            T_Add,
            T_Subtract,
            T_Divide,
            T_Multiply,
            T_Power,
            T_Modulo,
            T_Concatenate,
            T_Assignment,
            T_AssignAdd,
            T_AssignSubtract,
            T_AssignDivide,
            T_AssignMultiply,
            T_AssignPower,
            T_AssignModulo,
            T_AssignConcatenate,
            T_Dot,
            T_Arrow,
            T_ArrayPushBack,
            T_ArrayPopBack,
            T_Equal,
            T_NotEqual,
            T_Less,
            T_Greater,
            T_LessEqual,
            T_GreaterEqual,
            T_And,
            T_Or,
            T_Xor,
            T_Not,
            T_SizeOf,
        };

        auto isOperator = [&](const Token t) -> bool
        {
            for(const Token o : operators)
                if(t == o)
                    return true;
            return false;
        };

        static const Token unaryOperators[] = {
            T_Not, T_SizeOf, T_Subtract, T_Add, T_Concatenate,
        };

        auto isUnaryOperator = [&](const Token t) -> bool
        {
            for(const Token o : unaryOperators)
                if(t == o)
                    return true;
            return false;
        };

        // weird logic follows...

        if(prevToken == T_InvalidToken)// no previous token
        {
            if(isUnaryOperator(token))
                return ET_UnaryOperator;
            else
                return ET_PrimaryExpression;
        }
        else
        {
            if(isOperator(prevToken))
            {
                if(isUnaryOperator(token))
                    return ET_UnaryOperator;
                else
                    return ET_PrimaryExpression;
            }
            else
            {
                if(token == T_LeftBracket)
                    return ET_IndexOperator;

                if(token == T_LeftParent)
                    return ET_FunctionCall;

                if(token == T_Column || token == T_DoubleColumn)
                    return ET_FunctionAssignment;

                if(isOperator(token) && token != T_Not && token != T_SizeOf)
                    return ET_BinaryOperator;
            }
        }

        return ET_Unknown;
    }

    void Parser::FoldOperatorStacks(std::vector<Operator>& operators, std::vector<std::shared_ptr<ast::Node>>& operands) const
    {
        std::shared_ptr<ast::Node> newNode = nullptr;

        Operator topOperator = operators.back();
        operators.pop_back();

        switch(topOperator.type)
        {
            case ET_BinaryOperator:
            {
                std::shared_ptr<ast::Node> rhs = operands.back();
                operands.pop_back();
                std::shared_ptr<ast::Node> lhs = operands.back();
                operands.pop_back();

                newNode = std::make_shared<ast::BinaryOperatorNode>(topOperator.token, lhs, rhs, topOperator.coords);
                break;
            }

            case ET_UnaryOperator:
            {
                std::shared_ptr<ast::Node> operand = operands.back();
                operands.pop_back();

                newNode = std::make_shared<ast::UnaryOperatorNode>(topOperator.token, operand, topOperator.coords);
                break;
            }

            case ET_IndexOperator:
            {
                std::shared_ptr<ast::Node> operand = operands.back();
                operands.pop_back();

                newNode = std::make_shared<ast::BinaryOperatorNode>(topOperator.token, operand, topOperator.auxNode, topOperator.coords);
                break;
            }

            case ET_FunctionCall:
            {
                std::shared_ptr<ast::Node> function = operands.back();
                operands.pop_back();

                newNode = std::make_shared<ast::FunctionCallNode>(function, topOperator.auxNode, topOperator.coords);
                break;
            }

            case ET_FunctionAssignment:
            {
                std::shared_ptr<ast::Node> lhs = operands.back();
                operands.pop_back();

                newNode = std::make_shared<ast::BinaryOperatorNode>(T_Assignment, lhs, topOperator.auxNode, topOperator.coords);
                break;
            }
            default:
                break;
        }

        operands.push_back(newNode);
    }

    bool Parser::IsExpressionTerminator(Token token) const
    {
        return token == T_NewLine || token == T_Semicolumn || token == T_Comma || token == T_RightParent || token == T_RightBracket
               || token == T_RightBrace || token == T_Else || token == T_Elif || token == T_In || token == T_EOF;
    }

}// namespace element
