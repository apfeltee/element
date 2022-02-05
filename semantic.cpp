#include "element.h"

#include <algorithm>
#include "element.h"
#include "element.h"

namespace element
{
    SemanticAnalyzer::FunctionScope::FunctionScope(const std::shared_ptr<ast::FunctionNode>& n) : node(n)
    {
        blocks.emplace_back();

        parameters = n->namedParameters;

        n->localVariablesCount = int(parameters.size());
    }

    SemanticAnalyzer::SemanticAnalyzer(Logger& logger) : m_logger(logger), m_currfuncnode(nullptr)
    {
    }

    void SemanticAnalyzer::Analyze(const std::shared_ptr<ast::FunctionNode>& node)
    {
        analyzeNode(node);

        ResolveNamesInNodes({ node });

        m_context.clear();
        mFunctionScopes.clear();
        m_globalvars.clear();
    }

    void SemanticAnalyzer::AddNativeFunction(const std::string& name, int index)
    {
        m_natfuncs[name] = index;
    }

    void SemanticAnalyzer::resetState()
    {
        m_context.clear();
        mFunctionScopes.clear();
        m_globalvars.clear();
        m_natfuncs.clear();

        m_currfuncnode = nullptr;
    }

    bool SemanticAnalyzer::analyzeNode(const std::shared_ptr<ast::Node>& node)
    {
        if(!node)
            return false;

        switch(node->type)
        {
            case ast::Node::N_Nil:
            case ast::Node::N_Bool:
            case ast::Node::N_Integer:
            case ast::Node::N_Float:
            case ast::Node::N_String:
                return true;

            case ast::Node::N_Variable:
                m_currfuncnode->referencedVariables.push_back(std::dynamic_pointer_cast<ast::VariableNode>(node));
                return true;

            case ast::Node::N_Arguments:
            {
                auto n = std::dynamic_pointer_cast<ast::ArgumentsNode>(node);

                m_context.push_back(CXT_InArguments);

                for(auto& argument : n->arguments)
                    if(!analyzeNode(argument))
                        return false;

                m_context.pop_back();

                return true;
            }

            case ast::Node::N_UnaryOperator:
            {
                auto n = std::dynamic_pointer_cast<ast::UnaryOperatorNode>(node);

                if(isBreakContinueReturn(n->operand))
                {
                    m_logger.pushError(node->coords, "break, continue, return cannot be arguments to unary operators");
                    return false;
                }

                return analyzeNode(n->operand);
            }

            case ast::Node::N_BinaryOperator:
                return analyzeBinaryOperator(std::dynamic_pointer_cast<ast::BinaryOperatorNode>(node));

            case ast::Node::N_If:
            {
                auto n = std::dynamic_pointer_cast<ast::IfNode>(node);

                if(n->elsePath)
                    return analyzeNode(n->condition) && analyzeNode(n->thenPath) && analyzeNode(n->elsePath);
                else
                    return analyzeNode(n->condition) && analyzeNode(n->thenPath);
            }

            case ast::Node::N_While:
            {
                auto n = std::dynamic_pointer_cast<ast::WhileNode>(node);

                m_context.push_back(CXT_InLoop);

                bool ok = analyzeNode(n->condition) && analyzeNode(n->body);

                m_context.pop_back();

                return ok;
            }

            case ast::Node::N_For:
            {
                auto n = std::dynamic_pointer_cast<ast::ForNode>(node);

                if(!checkAssignable(n->iteratingVariable))
                    return false;

                m_context.push_back(CXT_InLoop);

                bool ok = analyzeNode(n->iteratingVariable) && analyzeNode(n->iteratedExpression) && analyzeNode(n->body);

                m_context.pop_back();

                return ok;
            }

            case ast::Node::N_Block:
            {
                auto n = std::dynamic_pointer_cast<ast::BlockNode>(node);

                for(auto& it : n->nodes)
                    if(!analyzeNode(it))
                        return false;

                return true;
            }

            case ast::Node::N_Array:
            {
                auto n = std::dynamic_pointer_cast<ast::ArrayNode>(node);

                m_context.push_back(CXT_InArray);

                for(auto& it : n->elements)
                    if(!analyzeNode(it))
                        return false;

                m_context.pop_back();

                return true;
            }

            case ast::Node::N_Object:
            {
                auto n = std::dynamic_pointer_cast<ast::ObjectNode>(node);

                m_context.push_back(CXT_InObject);

                for(auto& it : n->members)
                {
                    if(it.first->type != ast::Node::N_Variable)
                    {
                        m_logger.pushError(it.first->coords, "only valid identifiers can be object keys");
                        return false;
                    }

                    if(std::dynamic_pointer_cast<ast::VariableNode>(it.first)->variableType != ast::VariableNode::V_Named)
                    {
                        m_logger.pushError(it.first->coords, "only valid identifiers can be object keys");
                        return false;
                    }

                    if(!analyzeNode(it.second))
                        return false;
                }

                m_context.pop_back();

                return true;
            }

            case ast::Node::N_Function:
            {
                auto n = std::dynamic_pointer_cast<ast::FunctionNode>(node);

                if(n->body->type == ast::Node::N_Block)
                    std::dynamic_pointer_cast<ast::BlockNode>(n->body)->explicitFunctionBlock = true;

                m_context.push_back(m_context.empty() ? ContextType::CXT_InGlobal : ContextType::CXT_InFunction);

                auto oldFunctionNode = m_currfuncnode;
                m_currfuncnode = n;

                bool ok = analyzeNode(n->body);

                m_currfuncnode = oldFunctionNode;

                m_context.pop_back();

                return ok;
            }

            case ast::Node::N_FunctionCall:
            {
                auto n = std::dynamic_pointer_cast<ast::FunctionCallNode>(node);

                int nodeType = n->function->type;

                if(nodeType == ast::Node::N_Nil || nodeType == ast::Node::N_Integer || nodeType == ast::Node::N_Float
                   || nodeType == ast::Node::N_Bool || nodeType == ast::Node::N_String || nodeType == ast::Node::N_Array
                   || nodeType == ast::Node::N_Object || nodeType == ast::Node::N_Arguments || nodeType == ast::Node::N_Return
                   || nodeType == ast::Node::N_Break || nodeType == ast::Node::N_Continue || nodeType == ast::Node::N_Yield)
                {
                    m_logger.pushError(n->coords, "invalid target for function call");
                    return false;
                }

                return analyzeNode(n->function) && analyzeNode(n->arguments);
            }

            case ast::Node::N_Return:
            {
                auto n = std::dynamic_pointer_cast<ast::ReturnNode>(node);

                if(isInConstruction())
                {
                    m_logger.pushError(n->coords, "return cannot be used inside array, object or argument constructions");
                    return false;
                }

                if(n->value)
                    return analyzeNode(n->value);

                return true;
            }

            case ast::Node::N_Break:
            {
                auto n = std::dynamic_pointer_cast<ast::BreakNode>(node);

                if(isInConstruction())
                {
                    m_logger.pushError(n->coords, "break cannot be used inside array, object or argument constructions");
                    return false;
                }

                if(!isInLoop())
                {
                    m_logger.pushError(n->coords, "break can only be used inside a loop");
                    return false;
                }

                if(n->value)
                    return analyzeNode(n->value);

                return true;
            }

            case ast::Node::N_Continue:
            {
                auto n = std::dynamic_pointer_cast<ast::ContinueNode>(node);

                if(isInConstruction())
                {
                    m_logger.pushError(n->coords, "continue cannot be used inside array, object or argument constructions");
                    return false;
                }

                if(!isInLoop())
                {
                    m_logger.pushError(n->coords, "continue can only be used inside a loop");
                    return false;
                }

                if(n->value)
                    return analyzeNode(n->value);

                return true;
            }

            case ast::Node::N_Yield:
            {
                auto n = std::dynamic_pointer_cast<ast::YieldNode>(node);

                if(isInConstruction())
                {
                    m_logger.pushError(n->coords, "yield cannot be used inside array, object or argument constructions");
                    return false;
                }

                if(!isInFunction())
                {
                    m_logger.pushError(n->coords, "yield can only be used inside a function");
                    return false;
                }

                if(n->value)
                    return analyzeNode(n->value);

                return true;
            }

            default:
            {
            }
        }

        return true;
    }

    bool SemanticAnalyzer::analyzeBinaryOperator(const std::shared_ptr<ast::BinaryOperatorNode>& n)
    {
        if(n->op != Token::T_And && n->op != Token::T_Or && (isBreakContinueReturn(n->lhs) || isBreakContinueReturn(n->rhs)))
        {
            m_logger.pushError(n->coords, "break, continue, return can only be used with 'and' and 'or' operators");
            return false;
        }

        int lhsType = n->lhs->type;

        if(n->op == Token::T_Assignment || n->op == Token::T_AssignAdd || n->op == Token::T_AssignSubtract
           || n->op == Token::T_AssignMultiply || n->op == Token::T_AssignDivide || n->op == Token::T_AssignConcatenate
           || n->op == Token::T_AssignPower || n->op == Token::T_AssignModulo)
        {
            if(lhsType == ast::Node::N_Array && n->op != Token::T_Assignment)
            {
                m_logger.pushError(n->lhs->coords, "cannot use compound assignment with an array on the left hand side");
                return false;
            }

            if(!checkAssignable(n->lhs))
                return false;
        }

        if(n->op == Token::T_Assignment && n->rhs->type == ast::Node::N_Variable
           && (std::dynamic_pointer_cast<ast::VariableNode>(n->rhs))->variableType == ast::VariableNode::V_ArgumentList)
        {
            m_logger.pushError(n->rhs->coords, "argument arrays cannot be assigned to variables, they must be copied");
            return false;
        }

        if(n->op == Token::T_ArrayPopBack)
        {
            if(!checkAssignable(n->rhs))
                return false;
        }

        if(n->op == Token::T_LeftBracket)
        {
            if(lhsType == ast::Node::N_Nil || lhsType == ast::Node::N_Integer || lhsType == ast::Node::N_Float
               || lhsType == ast::Node::N_Bool || lhsType == ast::Node::N_Function || lhsType == ast::Node::N_Arguments
               || lhsType == ast::Node::N_Return || lhsType == ast::Node::N_Break || lhsType == ast::Node::N_Continue
               || lhsType == ast::Node::N_Yield)
            {
                m_logger.pushError(n->lhs->coords, "invalid target for index operation");
                return false;
            }
        }

        if(n->op == Token::T_Dot)
        {
            if(lhsType == ast::Node::N_Nil || lhsType == ast::Node::N_Integer || lhsType == ast::Node::N_Float
               || lhsType == ast::Node::N_Bool || lhsType == ast::Node::N_String || lhsType == ast::Node::N_Array
               || lhsType == ast::Node::N_Function || lhsType == ast::Node::N_Arguments || lhsType == ast::Node::N_Return
               || lhsType == ast::Node::N_Break || lhsType == ast::Node::N_Continue || lhsType == ast::Node::N_Yield)
            {
                m_logger.pushError(n->lhs->coords, "invalid target for member access");
                return false;
            }
        }

        return analyzeNode(n->lhs) && analyzeNode(n->rhs);
    }

    bool SemanticAnalyzer::checkAssignable(const std::shared_ptr<ast::Node>& node) const
    {
        if(node->type == ast::Node::N_Variable)
        {
            auto variable = std::dynamic_pointer_cast<ast::VariableNode>(node);

            if(variable->variableType == ast::VariableNode::V_This)
            {
                m_logger.pushError(variable->coords, "the 'this' variable is not assignable");
                return false;
            }
            if(variable->variableType == ast::VariableNode::V_ArgumentList)
            {
                m_logger.pushError(variable->coords, "the $$ array is not assignable");
                return false;
            }
            if(variable->variableType >= 0)// anonymous parameter
            {
                m_logger.pushError(variable->coords, std::string("the $") + std::to_string(int(variable->variableType))
                                                    + " variable is not assignable");
                return false;
            }

            return true;
        }
        else if(node->type == ast::Node::N_BinaryOperator)
        {
            auto bin = std::dynamic_pointer_cast<ast::BinaryOperatorNode>(node);

            if(bin->op != Token::T_LeftBracket &&// array access
               bin->op != Token::T_Dot)// member access
            {
                m_logger.pushError(bin->coords, "assignment must have a variable on the left hand side");
                return false;
            }

            if(bin->op == Token::T_LeftBracket && bin->lhs->type == ast::Node::N_Variable
               && (std::dynamic_pointer_cast<ast::VariableNode>(bin->lhs))->variableType == ast::VariableNode::V_ArgumentList)
            {
                m_logger.pushError(bin->coords, "elements of the $$ array are not assignable");
                return false;
            }

            if(bin->op == Token::T_Dot)
                return checkAssignable(bin->rhs);

            return true;
        }
        else if(node->type == ast::Node::N_Array)
        {
            auto array = std::dynamic_pointer_cast<ast::ArrayNode>(node);

            for(auto e : array->elements)
                if(!checkAssignable(e))
                    return false;

            return true;
        }

        // nothing else is assignable
        m_logger.pushError(node->coords, "invalid assignment");
        return false;
    }

    bool SemanticAnalyzer::isBreakContinueReturn(const std::shared_ptr<ast::Node>& node) const
    {
        return node->type == ast::Node::N_Break || node->type == ast::Node::N_Continue || node->type == ast::Node::N_Return;
    }

    bool SemanticAnalyzer::isBreakContinue(const std::shared_ptr<ast::Node>& node) const
    {
        return node->type == ast::Node::N_Break || node->type == ast::Node::N_Continue;
    }

    bool SemanticAnalyzer::isReturn(const std::shared_ptr<ast::Node>& node) const
    {
        return node->type == ast::Node::N_Return;
    }

    bool SemanticAnalyzer::isInLoop() const
    {
        for(auto rit = m_context.rbegin(); rit != m_context.rend(); ++rit)
        {
            if(*rit == CXT_InFunction)
                return false;
            if(*rit == CXT_InLoop)
                return true;
        }

        return false;
    }

    bool SemanticAnalyzer::isInFunction() const
    {
        for(auto rit = m_context.rbegin(); rit != m_context.rend(); ++rit)
            if(*rit == CXT_InFunction)
                return true;

        return false;
    }

    bool SemanticAnalyzer::isInConstruction() const
    {
        return m_context.back() == CXT_InArray || m_context.back() == CXT_InObject || m_context.back() == CXT_InArguments;
    }

    void SemanticAnalyzer::ResolveNamesInNodes(std::vector<std::shared_ptr<ast::Node>> nodesToProcess)
    {
        std::vector<std::shared_ptr<ast::Node>> nodesToDefer;
        std::shared_ptr<ast::Node> node = nullptr;

        while(!nodesToProcess.empty())
        {
            node = nodesToProcess.back();

            nodesToProcess.pop_back();

            switch(node->type)
            {
                case ast::Node::N_Nil:
                case ast::Node::N_Bool:
                case ast::Node::N_Integer:
                case ast::Node::N_Float:
                case ast::Node::N_String:
                default:
                    break;

                case ast::Node::N_Variable:
                {
                    auto vn = std::dynamic_pointer_cast<ast::VariableNode>(node);
                    if(vn->variableType == ast::VariableNode::V_Named)
                        resolveName(vn);
                    break;
                }

                case ast::Node::N_Block:
                case ast::Node::N_Function:
                    nodesToDefer.push_back(node);
                    break;

                case ast::Node::N_Arguments:
                {
                    auto n = std::dynamic_pointer_cast<ast::ArgumentsNode>(node);
                    for(auto it = n->arguments.rbegin(); it != n->arguments.rend(); ++it)
                        nodesToProcess.push_back(*it);
                    break;
                }

                case ast::Node::N_UnaryOperator:
                {
                    auto n = std::dynamic_pointer_cast<ast::UnaryOperatorNode>(node);
                    nodesToProcess.push_back(n->operand);
                    break;
                }

                case ast::Node::N_BinaryOperator:
                {
                    auto n = std::dynamic_pointer_cast<ast::BinaryOperatorNode>(node);
                    nodesToProcess.push_back(n->rhs);
                    nodesToProcess.push_back(n->lhs);
                    break;
                }

                case ast::Node::N_If:
                {
                    auto n = std::dynamic_pointer_cast<ast::IfNode>(node);
                    if(n->elsePath)
                        nodesToProcess.push_back(n->elsePath);
                    nodesToProcess.push_back(n->thenPath);
                    nodesToProcess.push_back(n->condition);
                    break;
                }

                case ast::Node::N_While:
                {
                    auto n = std::dynamic_pointer_cast<ast::WhileNode>(node);
                    nodesToProcess.push_back(n->body);
                    nodesToProcess.push_back(n->condition);
                    break;
                }

                case ast::Node::N_For:
                {
                    auto n = std::dynamic_pointer_cast<ast::ForNode>(node);
                    nodesToProcess.push_back(n->body);
                    nodesToProcess.push_back(n->iteratedExpression);
                    nodesToProcess.push_back(n->iteratingVariable);
                    break;
                }

                case ast::Node::N_Array:
                {
                    auto n = std::dynamic_pointer_cast<ast::ArrayNode>(node);
                    for(auto it = n->elements.rbegin(); it != n->elements.rend(); ++it)
                        nodesToProcess.push_back(*it);
                    break;
                }

                case ast::Node::N_Object:
                {
                    auto n = std::dynamic_pointer_cast<ast::ObjectNode>(node);
                    for(auto it = n->members.rbegin(); it != n->members.rend(); ++it)
                    {
                        nodesToProcess.push_back(it->second);
                        nodesToProcess.push_back(it->first);
                    }
                    break;
                }

                case ast::Node::N_FunctionCall:
                {
                    auto n = std::dynamic_pointer_cast<ast::FunctionCallNode>(node);
                    nodesToProcess.push_back(n->arguments);
                    nodesToProcess.push_back(n->function);
                    break;
                }

                case ast::Node::N_Return:
                {
                    auto n = std::dynamic_pointer_cast<ast::ReturnNode>(node);
                    if(n->value)
                        nodesToProcess.push_back(n->value);
                    break;
                }

                case ast::Node::N_Break:
                {
                    auto n = std::dynamic_pointer_cast<ast::BreakNode>(node);
                    if(n->value)
                        nodesToProcess.push_back(n->value);
                    break;
                }

                case ast::Node::N_Continue:
                {
                    auto n = std::dynamic_pointer_cast<ast::ContinueNode>(node);
                    if(n->value)
                        nodesToProcess.push_back(n->value);
                    break;
                }

                case ast::Node::N_Yield:
                {
                    auto n = std::dynamic_pointer_cast<ast::YieldNode>(node);
                    if(n->value)
                        nodesToProcess.push_back(n->value);
                    break;
                }
            }
        }

        for(auto deferredNode : nodesToDefer)
        {
            switch(deferredNode->type)
            {
                default:
                {
                }

                case ast::Node::N_Block:
                {
                    auto n = std::dynamic_pointer_cast<ast::BlockNode>(deferredNode);

                    std::vector<std::shared_ptr<ast::Node>> toProcess(n->nodes.rbegin(), n->nodes.rend());

                    if(n->explicitFunctionBlock)
                    {
                        ResolveNamesInNodes(toProcess);
                    }
                    else// regular block
                    {
                        mFunctionScopes.back().blocks.emplace_back();

                        ResolveNamesInNodes(toProcess);

                        mFunctionScopes.back().blocks.pop_back();
                    }

                    break;
                }

                case ast::Node::N_Function:
                {
                    auto n = std::dynamic_pointer_cast<ast::FunctionNode>(deferredNode);

                    bool isGlobal = mFunctionScopes.empty();

                    mFunctionScopes.emplace_back(n);

                    if(isGlobal)
                        mFunctionScopes.back().blocks.pop_back();

                    ResolveNamesInNodes({ n->body });

                    mFunctionScopes.pop_back();

                    break;
                }
            }
        }
    }

    void SemanticAnalyzer::resolveName(const std::shared_ptr<ast::VariableNode>& vn)
    {
        const std::string& name = vn->name;

        // if this is the global function scope
        if(mFunctionScopes.size() == 1 && mFunctionScopes.front().blocks.empty())
        {
            // try the global scope
            auto globalIt = std::find(m_globalvars.begin(), m_globalvars.end(), name);

            if(globalIt != m_globalvars.end())
            {
                vn->semanticType = ast::VariableNode::SMT_Global;
                vn->index = std::distance(m_globalvars.begin(), globalIt);
                vn->firstOccurrence = false;
                return;
            }

            // try the native constants
            auto nativeIt = m_natfuncs.find(name);

            if(nativeIt != m_natfuncs.end())
            {
                vn->semanticType = ast::VariableNode::SMT_Native;
                vn->index = nativeIt->second;
                vn->firstOccurrence = false;
                return;
            }

            // otherwise create a new global
            vn->semanticType = ast::VariableNode::SMT_Global;
            vn->index = int(m_globalvars.size());
            vn->firstOccurrence = true;

            m_globalvars.push_back(name);
            return;
        }

        // otherwise we are inside a function
        FunctionScope& localFunctionScope = mFunctionScopes.back();

        // try the parameters of this function
        int parametersSize = int(localFunctionScope.parameters.size());
        for(int i = 0; i < parametersSize; ++i)
        {
            if(name == localFunctionScope.parameters[i])
            {
                vn->semanticType = ast::VariableNode::SMT_Local;
                vn->index = i;
                vn->firstOccurrence = false;
                return;
            }
        }

        // try the free variables captured by this function
        int freeVariablesSize = int(localFunctionScope.freeVariables.size());
        for(int i = 0; i < freeVariablesSize; ++i)
        {
            if(name == localFunctionScope.freeVariables[i])
            {
                vn->semanticType = ast::VariableNode::SMT_FreeVariable;
                vn->index = i;
                vn->firstOccurrence = false;
                return;
            }
        }

        // try the blocks in the current function scope in reverse
        for(auto blockIt = localFunctionScope.blocks.rbegin(); blockIt != localFunctionScope.blocks.rend(); ++blockIt)
        {
            auto localNameIt = blockIt->variables.find(name);

            if(localNameIt != blockIt->variables.end())
            {
                vn->semanticType = localNameIt->second->semanticType;
                vn->index = localNameIt->second->index;
                vn->firstOccurrence = false;
                return;
            }
        }

        // try the enclosing function scopes if this is part of a closure
        if(TryToFindNameInTheEnclosingFunctions(vn))
            return;

        // try the global scope (check this after the parameters, because they can hide globals)
        auto globalIt = std::find(m_globalvars.begin(), m_globalvars.end(), name);

        if(globalIt != m_globalvars.end())
        {
            vn->semanticType = ast::VariableNode::SMT_Global;
            vn->index = std::distance(m_globalvars.begin(), globalIt);
            vn->firstOccurrence = false;
            return;
        }

        // try the native constants (check this after the parameters, because they can hide natives)
        auto nativeIt = m_natfuncs.find(name);

        if(nativeIt != m_natfuncs.end())
        {
            vn->semanticType = ast::VariableNode::SMT_Native;
            vn->index = nativeIt->second;
            vn->firstOccurrence = false;
            return;
        }

        // we didn't find it anywhere, create it locally
        vn->semanticType = ast::VariableNode::SMT_Local;
        vn->index = localFunctionScope.node->localVariablesCount++;
        vn->firstOccurrence = true;

        localFunctionScope.blocks.back().variables[name] = vn;
    }

    bool SemanticAnalyzer::TryToFindNameInTheEnclosingFunctions(const std::shared_ptr<ast::VariableNode>& vn)
    {
        bool found = false;
        int foundAtIndex = 0;
        const std::string& name = vn->name;

        const auto makeBoxed = [](FunctionScope& fs, int index)
        {
            for(auto vn : fs.node->referencedVariables)
            {
                if(vn->semanticType == ast::VariableNode::SMT_Local && vn->index == index)
                    vn->semanticType = ast::VariableNode::SMT_LocalBoxed;
            }

            if(index < int(fs.node->namedParameters.size()))
            {
                fs.node->parametersToBox.emplace_back(index);
            }
        };

        // try each of the enclosing function scopes in reverse
        for(auto functionIt = ++mFunctionScopes.rbegin(); functionIt != mFunctionScopes.rend(); ++functionIt)
        {
            int freeVariablesSize = int(functionIt->freeVariables.size());
            for(int i = 0; i < freeVariablesSize; ++i)
            {
                if(name == functionIt->freeVariables[i])
                {
                    found = true;
                    foundAtIndex = -i - 1;// negative index for free variables
                    break;
                }
            }

            if(!found)
            {
                int parametersSize = int(functionIt->parameters.size());
                for(int i = 0; i < parametersSize; ++i)
                {
                    if(name == functionIt->parameters[i])
                    {
                        found = true;
                        foundAtIndex = i;

                        // If this is the first time this parameter has ever been captured,
                        // all references to it in this function scope must become 'SMT_LocalBoxed'.
                        const auto& ptb = functionIt->node->parametersToBox;
                        if(std::find(ptb.begin(), ptb.end(), i) == ptb.end())
                        {
                            makeBoxed(*functionIt, foundAtIndex);
                        }
                        break;
                    }
                }
            }

            if(!found)
            {
                std::vector<BlockScope>& blocks = functionIt->blocks;

                for(auto blockIt = blocks.rbegin(); blockIt != blocks.rend(); ++blockIt)
                {
                    auto localNameIt = blockIt->variables.find(name);

                    if(localNameIt != blockIt->variables.end())
                    {
                        found = true;
                        foundAtIndex = localNameIt->second->index;

                        // If this is the first time this variable has ever been captured,
                        // all references to it in this function scope must become 'SMT_LocalBoxed'.
                        if(localNameIt->second->semanticType == ast::VariableNode::SMT_Local)
                        {
                            makeBoxed(*functionIt, foundAtIndex);
                        }
                        break;
                    }
                }
            }

            if(found)
            {
                int foundFunctionScopeIndex = std::distance(mFunctionScopes.begin(), functionIt.base()) - 1;
                int localFunctionScopeIndex = int(mFunctionScopes.size() - 1);

                while(foundFunctionScopeIndex + 1 < localFunctionScopeIndex)
                {
                    FunctionScope& functionScope = mFunctionScopes[foundFunctionScopeIndex + 1];

                    int newFreeVarIndex = int(functionScope.freeVariables.size());
                    newFreeVarIndex = -newFreeVarIndex - 1;// negative index for free variables

                    functionScope.freeVariables.push_back(name);
                    functionScope.node->closureMapping.push_back(foundAtIndex);

                    foundAtIndex = newFreeVarIndex;

                    ++foundFunctionScopeIndex;
                }

                FunctionScope& localFunctionScope = mFunctionScopes.back();

                localFunctionScope.freeVariables.push_back(name);
                localFunctionScope.node->closureMapping.push_back(foundAtIndex);

                vn->semanticType = ast::VariableNode::SMT_FreeVariable;
                vn->index = int(localFunctionScope.freeVariables.size()) - 1;
                vn->firstOccurrence = false;// first occurrence was where it was originally defined

                return true;
            }
        }

        return false;
    }

}// namespace element
