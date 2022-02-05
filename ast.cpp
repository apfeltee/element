#include "element.h"

#include <sstream>

namespace element
{
    namespace ast
    {
        Node::Node(const Location& coords) : type(N_Nil), coords(coords)
        {
        }

        Node::Node(NodeType type, const Location& coords) : type(type), coords(coords)
        {
        }

        Node::~Node()
        {
        }

        VariableNode::VariableNode(int variableType, const Location& coords)
        : Node(N_Variable, coords), variableType(variableType)

          ,
          semanticType(SMT_Global), firstOccurrence(false), index(-1)
        {
        }

        VariableNode::VariableNode(const std::string& name, const Location& coords)
        : Node(N_Variable, coords), variableType(V_Named), name(name)

          ,
          semanticType(SMT_Global), firstOccurrence(false), index(-1)
        {
        }

        IntegerNode::IntegerNode(int value, const Location& coords) : Node(N_Integer, coords), value(value)
        {
        }

        FloatNode::FloatNode(float value, const Location& coords) : Node(N_Float, coords), value(value)
        {
        }

        BoolNode::BoolNode(bool value, const Location& coords) : Node(N_Bool, coords), value(value)
        {
        }

        StringNode::StringNode(const std::string& value, const Location& coords)
        : Node(N_String, coords), value(value)
        {
        }

        ArrayNode::ArrayNode(const std::vector<std::shared_ptr<Node>>& elements, const Location& coords)
        : Node(N_Array, coords), elements(elements)
        {
        }

        ArrayNode::~ArrayNode()
        {
            for(auto& it : elements)
                it.reset();
        }

        ObjectNode::ObjectNode(const Location& coords) : Node(N_Object, coords)
        {
        }

        ObjectNode::ObjectNode(const KeyValuePairs& members, const Location& coords)
        : Node(N_Object, coords), members(members)
        {
        }

        ObjectNode::~ObjectNode()
        {
            for(auto& pair : members)
            {
                pair.first.reset();
                pair.second.reset();
            }
        }

        FunctionNode::FunctionNode(const NamedParameters& namedParameters, const std::shared_ptr<Node>& body, const Location& coords)
        : Node(N_Function, coords), namedParameters(namedParameters), body(body)

          ,
          localVariablesCount(0)
        {
        }

        FunctionNode::~FunctionNode()
        {
            body.reset();
        }

        FunctionCallNode::FunctionCallNode(const std::shared_ptr<Node>& function, const std::shared_ptr<Node>& arguments, const Location& coords)
        : Node(N_FunctionCall, coords), function(function), arguments(arguments)
        {
        }

        FunctionCallNode::~FunctionCallNode()
        {
            function.reset();
            arguments.reset();
        }

        ArgumentsNode::ArgumentsNode(std::vector<std::shared_ptr<Node>>& arguments, const Location& coords)
        : Node(N_Arguments, coords), arguments(arguments)
        {
        }

        ArgumentsNode::~ArgumentsNode()
        {
            for(auto& it : arguments)
                it.reset();
        }

        UnaryOperatorNode::UnaryOperatorNode(Token op, const std::shared_ptr<Node>& operand, const Location& coords)
        : Node(N_UnaryOperator, coords), op(op), operand(operand)
        {
        }

        UnaryOperatorNode::~UnaryOperatorNode()
        {
            operand.reset();
        }

        BinaryOperatorNode::BinaryOperatorNode(Token op, const std::shared_ptr<Node>& lhs, const std::shared_ptr<Node>& rhs, const Location& coords)
        : Node(N_BinaryOperator, coords), op(op), lhs(lhs), rhs(rhs)
        {
        }

        BinaryOperatorNode::~BinaryOperatorNode()
        {
            lhs.reset();
            rhs.reset();
        }

        BlockNode::BlockNode(std::vector<std::shared_ptr<Node>>& nodes, const Location& coords)
        : Node(N_Block, coords), nodes(nodes)

          ,
          explicitFunctionBlock(false)
        {
        }

        BlockNode::~BlockNode()
        {
            for(auto& it : nodes)
                it.reset();
        }

        IfNode::IfNode(const std::shared_ptr<Node>& condition, const std::shared_ptr<Node>& thenPath, const std::shared_ptr<Node>& elsePath, const Location& coords)
        : Node(N_If, coords), condition(condition), thenPath(thenPath), elsePath(elsePath)
        {
        }

        IfNode::~IfNode()
        {
            condition.reset();
            thenPath.reset();
            if(elsePath)
                elsePath.reset();
        }

        WhileNode::WhileNode(const std::shared_ptr<Node>& condition, const std::shared_ptr<Node>& body, const Location& coords)
        : Node(N_While, coords), condition(condition), body(body)
        {
        }

        WhileNode::~WhileNode()
        {
            condition.reset();
            body.reset();
        }

        ForNode::ForNode(const std::shared_ptr<Node>& iteratingVariable, const std::shared_ptr<Node>& iteratedExpression, const std::shared_ptr<Node>& body, const Location& coords)
        : Node(N_For, coords), iteratingVariable(iteratingVariable), iteratedExpression(iteratedExpression), body(body)
        {
        }

        ForNode::~ForNode()
        {
            iteratingVariable.reset();
            iteratedExpression.reset();
            body.reset();
        }

        ReturnNode::ReturnNode(const std::shared_ptr<Node>& value, const Location& coords) : Node(N_Return, coords), value(value)
        {
        }

        ReturnNode::~ReturnNode()
        {
            if(value)
                value.reset();
        }

        BreakNode::BreakNode(const std::shared_ptr<Node>& value, const Location& coords) : Node(N_Break, coords), value(value)
        {
        }

        BreakNode::~BreakNode()
        {
            if(value)
                value.reset();
        }

        ContinueNode::ContinueNode(const std::shared_ptr<Node>& value, const Location& coords) : Node(N_Continue, coords), value(value)
        {
        }

        ContinueNode::~ContinueNode()
        {
            if(value)
                value.reset();
        }

        YieldNode::YieldNode(const std::shared_ptr<Node>& value, const Location& coords) : Node(N_Yield, coords), value(value)
        {
        }

        YieldNode::~YieldNode()
        {
            if(value)
                value.reset();
        }

        std::string nodeToString(const std::shared_ptr<ast::Node>& root, int indent)
        {
            if(!root)
                return "";

            static const int TabSize = 2;

            std::string space(indent, ' ');

            switch(root->type)
            {
                case ast::Node::N_Nil:
                    return space + "Nil\n";

                case ast::Node::N_Bool:
                {
                    auto n = std::dynamic_pointer_cast<ast::BoolNode>(root);
                    return space + "Bool " + (n->value ? "true\n" : "false\n");
                }
                case ast::Node::N_Integer:
                {
                    auto n = std::dynamic_pointer_cast<ast::IntegerNode>(root);
                    return space + "Int " + std::to_string(n->value) + "\n";
                }
                case ast::Node::N_Float:
                {
                    auto n = std::dynamic_pointer_cast<ast::FloatNode>(root);
                    return space + "Float " + std::to_string(n->value) + "\n";
                }
                case ast::Node::N_String:
                {
                    auto n = std::dynamic_pointer_cast<ast::StringNode>(root);
                    return space + "String \"" + n->value + "\"\n";
                }
                case ast::Node::N_Variable:
                {
                    auto n = std::dynamic_pointer_cast<ast::VariableNode>(root);

                    if(n->variableType == ast::VariableNode::V_Named)
                        return space + "Variable " + n->name + "\n";

                    else if(n->variableType == ast::VariableNode::V_This)
                        return space + "Variable this\n";

                    else if(n->variableType == ast::VariableNode::V_ArgumentList)
                        return space + "Variable argument list\n";

                    else if(n->variableType == ast::VariableNode::V_Underscore)
                        return space + "Variable _\n";

                    else
                        return space + "Variable argument at index " + std::to_string(int(n->variableType)) + "\n";
                }
                case ast::Node::N_Arguments:
                {
                    auto n = std::dynamic_pointer_cast<ast::ArgumentsNode>(root);

                    if(n->arguments.empty())
                    {
                        return space + "Empty argument list\n";
                    }
                    else
                    {
                        std::stringstream ss;
                        for(const auto& argument : n->arguments)
                            ss << space << "Argument\n" << nodeToString(argument, indent + TabSize);
                        return ss.str();
                    }
                }
                case ast::Node::N_UnaryOperator:
                {
                    auto n = std::dynamic_pointer_cast<ast::UnaryOperatorNode>(root);
                    return space + "Unary + " + TokenAsString(n->op) + "\n" + nodeToString(n->operand, indent + TabSize);
                }
                case ast::Node::N_BinaryOperator:
                {
                    auto n = std::dynamic_pointer_cast<ast::BinaryOperatorNode>(root);
                    std::stringstream ss;

                    if(n->op != T_LeftBracket)
                        ss << space << TokenAsString(n->op) << "\n";
                    else
                        ss << space << "[]\n";

                    ss << nodeToString(n->lhs, indent + TabSize);
                    ss << nodeToString(n->rhs, indent + TabSize);
                    return ss.str();
                }
                case ast::Node::N_If:
                {
                    auto n = std::dynamic_pointer_cast<ast::IfNode>(root);
                    std::stringstream ss;

                    ss << space << "If\n" << nodeToString(n->condition, indent + TabSize);
                    ss << space << "Then\n" << nodeToString(n->thenPath, indent + TabSize);
                    if(n->elsePath)
                        ss << space << "Else\n" << nodeToString(n->elsePath, indent + TabSize);

                    return ss.str();
                }
                case ast::Node::N_While:
                {
                    auto n = std::dynamic_pointer_cast<ast::WhileNode>(root);
                    std::stringstream ss;

                    ss << space << "While\n" << nodeToString(n->condition, indent + TabSize);
                    ss << space << "Do\n" << nodeToString(n->body, indent + TabSize);

                    return ss.str();
                }
                case ast::Node::N_For:
                {
                    auto n = std::dynamic_pointer_cast<ast::ForNode>(root);
                    std::stringstream ss;

                    ss << space << "For\n" << nodeToString(n->iteratingVariable, indent + TabSize);
                    ss << space << "In\n" << nodeToString(n->iteratedExpression, indent + TabSize);
                    ss << space << "Do\n" << nodeToString(n->body, indent + TabSize);

                    return ss.str();
                }
                case ast::Node::N_Block:
                {
                    auto n = std::dynamic_pointer_cast<ast::BlockNode>(root);
                    std::stringstream ss;

                    ss << space << "{\n";
                    for(const auto& node : n->nodes)
                        ss << nodeToString(node, indent + TabSize);
                    ss << space << "}\n";

                    return ss.str();
                }
                case ast::Node::N_Array:
                {
                    auto n = std::dynamic_pointer_cast<ast::ArrayNode>(root);
                    std::stringstream ss;

                    ss << space << "[\n";
                    for(const auto& node : n->elements)
                        ss << nodeToString(node, indent + TabSize);
                    ss << space << "]\n";

                    return ss.str();
                }
                case ast::Node::N_Object:
                {
                    auto n = std::dynamic_pointer_cast<ast::ObjectNode>(root);
                    if(n->members.empty())
                    {
                        return space + "[=]\n";
                    }
                    else
                    {
                        std::stringstream ss;

                        ss << space << "[\n";
                        for(const auto& member : n->members)
                        {
                            ss << space << "Key\n" << nodeToString(member.first, indent + TabSize);
                            ss << space << "Value\n" << nodeToString(member.second, indent + TabSize);
                        }
                        ss << space << "]\n";

                        return ss.str();
                    }
                }
                case ast::Node::N_Function:
                {
                    auto n = std::dynamic_pointer_cast<ast::FunctionNode>(root);
                    std::stringstream ss;

                    size_t sz = n->namedParameters.size();

                    ss << space << "Function (";
                    for(size_t i = 0; i < sz; ++i)
                        ss << n->namedParameters[i] << (i == sz - 1 ? "" : ", ");
                    ss << ")\n";
                    ss << nodeToString(n->body, indent + TabSize);

                    return ss.str();
                }
                case ast::Node::N_FunctionCall:
                {
                    auto n = std::dynamic_pointer_cast<ast::FunctionCallNode>(root);
                    std::stringstream ss;

                    ss << space << "FunctionCall\n";
                    ss << nodeToString(n->function, indent + TabSize);
                    ss << nodeToString(n->arguments, indent + TabSize);

                    return ss.str();
                }
                case ast::Node::N_Return:
                {
                    auto n = std::dynamic_pointer_cast<ast::ReturnNode>(root);
                    std::stringstream ss;

                    ss << space << "Return\n";
                    if(n->value)
                        ss << nodeToString(n->value, indent + TabSize);

                    return ss.str();
                }
                case ast::Node::N_Break:
                {
                    auto n = std::dynamic_pointer_cast<ast::BreakNode>(root);
                    std::stringstream ss;

                    ss << space << "Break\n";
                    if(n->value)
                        ss << nodeToString(n->value, indent + TabSize);

                    return ss.str();
                }
                case ast::Node::N_Continue:
                {
                    auto n = std::dynamic_pointer_cast<ast::ContinueNode>(root);
                    std::stringstream ss;

                    ss << space << "Continue\n";
                    if(n->value)
                        ss << nodeToString(n->value, indent + TabSize);

                    return ss.str();
                }
                case ast::Node::N_Yield:
                {
                    auto n = std::dynamic_pointer_cast<ast::YieldNode>(root);
                    std::stringstream ss;

                    ss << space << "Yield\n";
                    if(n->value)
                        ss << nodeToString(n->value, indent + TabSize);

                    return ss.str();
                }
                default:
                    return "Unknown ast::Node type!";
            }
        }

    }// namespace ast

}// namespace element
