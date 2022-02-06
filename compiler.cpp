#include "element.h"

#include "element.h"
#include "element.h"
#include "element.h"
#include "element.h"

using namespace std::string_literals;

namespace element
{
    Compiler::Compiler(Logger& logger)
    : m_logger(logger), m_currfunction(nullptr), m_constoffset(0), m_symsoffset(0)
    {
        resetState();
    }

    std::unique_ptr<char[]> Compiler::compile(const std::shared_ptr<ast::FunctionNode>& node)
    {
        buildFuncStmt(node, true);

        return buildBinaryData();
    }

    void Compiler::resetState()
    {
        m_loopcontexts.clear();
        m_funcontexts.clear();

        m_currfunction = nullptr;

        m_constants.clear();
        m_constants.emplace_back();// nil
        m_constants.emplace_back(true);// true
        m_constants.emplace_back(false);// false

        m_constoffset = 0;

        m_symindices.clear();
        m_symindices[Symbol::ProtoHash] = 0;

        m_symbols.clear();
        m_symbols.emplace_back("proto", Symbol::ProtoHash);

        m_symsoffset = 0;
    }

    void Compiler::emitInstructions(const std::shared_ptr<ast::Node>& node, bool keepValue)
    {
        if(node->type != ast::Node::N_Block && node->type != ast::Node::N_Array && node->type != ast::Node::N_Object)
        {
            // record from which line did the next instruction come from
            int line = node->coords.line;

            std::vector<SourceCodeLine>& lines = m_currfunction->instructionLines;

            if(lines.empty() || lines.back().line != line)
                lines.push_back({ line, int(m_currfunction->instructions.size()) });
        }

        // emit the instruction
        switch(node->type)
        {
            case ast::Node::N_Nil:
            case ast::Node::N_Integer:
            case ast::Node::N_Float:
            case ast::Node::N_Bool:
            case ast::Node::N_String:
                buildConstLoad(node, keepValue);
                return;

            case ast::Node::N_Array:
                buildArrayLiteral(node, keepValue);
                return;

            case ast::Node::N_Object:
                buildObjectLiteral(node, keepValue);
                return;

            case ast::Node::N_BinaryOperator:
                buildBinaryOp(node, keepValue);
                return;

            case ast::Node::N_UnaryOperator:
                buildUnaryOp(node, keepValue);
                return;

            case ast::Node::N_Variable:
                buildVarLoad(node, keepValue);
                return;

            case ast::Node::N_If:
                buildIfStmt(node, keepValue);
                return;

            case ast::Node::N_While:
                buildWhileStmt(node, keepValue);
                return;

            case ast::Node::N_For:
                buildForStmt(node, keepValue);
                return;

            case ast::Node::N_Block:
                buildBlockStmt(node, keepValue);
                return;

            case ast::Node::N_Function:
                buildFuncStmt(node, keepValue);
                return;

            case ast::Node::N_FunctionCall:
                buildFuncCall(node, keepValue);
                return;

            case ast::Node::N_Yield:
                buildYield(node, keepValue);
                return;

            case ast::Node::N_Break:
            case ast::Node::N_Continue:
            case ast::Node::N_Return:
                buildJumpStmt(node);
                return;

            default:
                m_logger.pushError(node->coords, "Unknown AST node type "s + std::to_string(int(node->type)));
                break;
        }
    }

    void Compiler::buildConstLoad(const std::shared_ptr<ast::Node>& node, bool keepValue)
    {
        if(!keepValue)
            return;

        int index = 0;

        switch(node->type)
        {
            case ast::Node::N_Nil:
            {
                index = 0;
                break;
            }

            case ast::Node::N_Bool:
            {
                bool b = (std::dynamic_pointer_cast<ast::BoolNode>(node))->value;
                index = b ? 1 : 2;
                break;
            }

            case ast::Node::N_Integer:
            {
                int n = std::dynamic_pointer_cast<ast::IntegerNode>(node)->value;
                for(unsigned i = 3; i < m_constants.size(); ++i)
                {
                    if(m_constants[i].equals(n))
                    {
                        index = i;
                        break;
                    }
                }
                if(index == 0)
                {
                    index = m_constants.size();
                    m_constants.emplace_back(n);
                }
                break;
            }

            case ast::Node::N_Float:
            {
                float f = std::dynamic_pointer_cast<ast::FloatNode>(node)->value;
                for(unsigned i = 3; i < m_constants.size(); ++i)
                {
                    if(m_constants[i].equals(f))
                    {
                        index = i;
                        break;
                    }
                }
                if(index == 0)
                {
                    index = m_constants.size();
                    m_constants.emplace_back(f);
                }
                break;
            }

            case ast::Node::N_String:
            {
                const std::string& s = std::dynamic_pointer_cast<ast::StringNode>(node)->value;
                for(unsigned i = 3; i < m_constants.size(); ++i)
                {
                    if(m_constants[i].equals(s))
                    {
                        index = i;
                        break;
                    }
                }
                if(index == 0)
                {
                    index = m_constants.size();
                    m_constants.emplace_back(s);
                }
                break;
            }

            default:
                m_logger.pushError(node->coords, "Unknown AST node type "s + std::to_string(int(node->type)));
                break;
        }

        m_currfunction->instructions.emplace_back(OpCode::OC_LoadConstant, index);
    }

    void Compiler::buildVarLoad(const std::shared_ptr<ast::Node>& node, bool keepValue)
    {
        if(!keepValue)
            return;

        auto n = std::dynamic_pointer_cast<ast::VariableNode>(node);

        if(n->variableType == ast::VariableNode::V_Named)
        {
            updateSymbol(n->name);

            if(n->semanticType == ast::VariableNode::SMT_LocalBoxed && n->firstOccurrence)
                m_currfunction->instructions.emplace_back(OpCode::OC_MakeBox, n->index);

            OpCode opCode;

            switch(n->semanticType)
            {
                case ast::VariableNode::SMT_Local:
                    opCode = OpCode::OC_LoadLocal;
                    break;
                case ast::VariableNode::SMT_Global:
                    opCode = OpCode::OC_LoadGlobal;
                    break;
                case ast::VariableNode::SMT_Native:
                    opCode = OpCode::OC_LoadNative;
                    break;
                case ast::VariableNode::SMT_LocalBoxed:
                    opCode = OpCode::OC_LoadFromBox;
                    break;
                case ast::VariableNode::SMT_FreeVariable:
                    opCode = OpCode::OC_LoadFromClosure;
                    break;
            }

            m_currfunction->instructions.emplace_back(opCode, n->index);
        }
        else if(n->variableType == ast::VariableNode::V_This)
        {
            m_currfunction->instructions.emplace_back(OpCode::OC_LoadThis);
        }
        else if(n->variableType == ast::VariableNode::V_ArgumentList)
        {
            m_currfunction->instructions.emplace_back(OpCode::OC_LoadArgsArray);
        }
        else if(n->variableType == ast::VariableNode::V_Underscore)
        {
            m_logger.pushError(n->coords, "Cannot load from the underscore variable\n");
            return;
        }
        else// load anonymous argument $ $1 $2 ...
        {
            int argumentIndex = n->variableType;
            m_currfunction->instructions.emplace_back(OpCode::OC_LoadArgument, argumentIndex);
        }
    }

    void Compiler::buildVarStore(const std::shared_ptr<ast::Node>& node, bool keepValue)
    {
        ast::Node::NodeType lhsType = node->type;

        if(lhsType == ast::Node::N_BinaryOperator)////////////////////////////////////////
        {
            auto n = std::dynamic_pointer_cast<ast::BinaryOperatorNode>(node);

            if(n->op == T_LeftBracket)
            {
                emitInstructions(n->lhs, true);
                emitInstructions(n->rhs, true);

                OpCode opCode = keepValue ? OpCode::OC_StoreElement : OpCode::OC_PopStoreElement;

                m_currfunction->instructions.emplace_back(opCode);
            }
            else if(n->op == T_Dot)
            {
                emitInstructions(n->lhs, true);
                buildHashLoadOp(n->rhs);

                OpCode opCode = keepValue ? OpCode::OC_StoreMember : OpCode::OC_PopStoreMember;

                m_currfunction->instructions.emplace_back(opCode);
            }
            else// error
            {
                m_logger.pushError(node->coords, "Invalid assignment\n");
            }
        }
        else if(lhsType == ast::Node::N_Array)////////////////////////////////////////////
        {
            auto elements = std::dynamic_pointer_cast<ast::ArrayNode>(node)->elements;

            if(keepValue)
                m_currfunction->instructions.emplace_back(OC_Duplicate);

            m_currfunction->instructions.emplace_back(OC_Unpack, int(elements.size()));

            for(auto receivingVariable : elements)
                buildVarStore(receivingVariable, false);
        }
        else if(lhsType == ast::Node::N_Variable)/////////////////////////////////////////
        {
            auto n = std::dynamic_pointer_cast<ast::VariableNode>(node);

            if(n->variableType == ast::VariableNode::V_Named)
            {
                updateSymbol(n->name);

                if(n->semanticType == ast::VariableNode::SMT_LocalBoxed && n->firstOccurrence)
                    m_currfunction->instructions.emplace_back(OpCode::OC_MakeBox, n->index);

                OpCode opCode;

                if(keepValue)
                {
                    switch(n->semanticType)
                    {
                        case ast::VariableNode::SMT_Local:
                            opCode = OpCode::OC_StoreLocal;
                            break;
                        case ast::VariableNode::SMT_Global:
                            opCode = OpCode::OC_StoreGlobal;
                            break;
                        case ast::VariableNode::SMT_LocalBoxed:
                            opCode = OpCode::OC_StoreToBox;
                            break;
                        case ast::VariableNode::SMT_FreeVariable:
                            opCode = OpCode::OC_StoreToClosure;
                            break;
                        default:
                            m_logger.pushError(n->coords, "Cannot store into native variables "s + std::to_string(int(n->index)));
                            break;
                    }
                }
                else
                {
                    switch(n->semanticType)
                    {
                        case ast::VariableNode::SMT_Local:
                            opCode = OpCode::OC_PopStoreLocal;
                            break;
                        case ast::VariableNode::SMT_Global:
                            opCode = OpCode::OC_PopStoreGlobal;
                            break;
                        case ast::VariableNode::SMT_LocalBoxed:
                            opCode = OpCode::OC_PopStoreToBox;
                            break;
                        case ast::VariableNode::SMT_FreeVariable:
                            opCode = OpCode::OC_PopStoreToClosure;
                            break;
                        default:
                            m_logger.pushError(n->coords, "Cannot store into native variables "s + std::to_string(int(n->index)));
                            break;
                    }
                }

                m_currfunction->instructions.emplace_back(opCode, n->index);
            }
            else if(n->variableType == ast::VariableNode::V_Underscore)
            {
                if(!keepValue)
                    m_currfunction->instructions.emplace_back(OpCode::OC_Pop);
            }
            else
            {
                m_logger.pushError(n->coords, "The variables: this $ $0 $1 $2 $n $$ and $$[n] shall not be assignable\n");
            }
        }
        else// error ///////////////////////////////////////////////////////////////////////
        {
            m_logger.pushError(node->coords, "Invalid assignment\n");
        }
    }

    void Compiler::BuildAssignOp(const std::shared_ptr<ast::Node>& node, bool keepValue)
    {
        auto n = std::dynamic_pointer_cast<ast::BinaryOperatorNode>(node);

        if(n->op == T_Assignment)
        {
            emitInstructions(n->rhs, true);
        }
        else// compound assignment: += -= *= /= ^= %= ~=
        {
            emitInstructions(n->lhs, true);
            emitInstructions(n->rhs, true);

            OpCode binaryOperation;

            switch(n->op)
            {
                case T_AssignAdd:
                    binaryOperation = OpCode::OC_Add;
                    break;
                case T_AssignSubtract:
                    binaryOperation = OpCode::OC_Subtract;
                    break;
                case T_AssignMultiply:
                    binaryOperation = OpCode::OC_Multiply;
                    break;
                case T_AssignDivide:
                    binaryOperation = OpCode::OC_Divide;
                    break;
                case T_AssignPower:
                    binaryOperation = OpCode::OC_Power;
                    break;
                case T_AssignModulo:
                    binaryOperation = OpCode::OC_Modulo;
                    break;
                case T_AssignConcatenate:
                    binaryOperation = OpCode::OC_Concatenate;
                    break;
                default:
                    m_logger.pushError(n->coords, "Invalid assign type "s + std::to_string(int(n->op)));
                    break;
            }

            m_currfunction->instructions.emplace_back(binaryOperation);
        }

        buildVarStore(n->lhs, keepValue);
    }

    void Compiler::buildBoolOp(const std::shared_ptr<ast::Node>& node, bool keepValue)
    {
        auto n = std::dynamic_pointer_cast<ast::BinaryOperatorNode>(node);

        emitInstructions(n->lhs, true);

        unsigned jumpIndex = m_currfunction->instructions.size();

        if(n->op == T_And)
            m_currfunction->instructions.emplace_back(OpCode::OC_JumpIfFalseOrPop);
        else// n->op == T_Or
            m_currfunction->instructions.emplace_back(OpCode::OC_JumpIfTrueOrPop);

        emitInstructions(n->rhs, true);

        // TODO: right now if we short-circuit an expression like 'a and b and c' at 'a'
        // then the jump will take us to another jump at 'b' which will execute right away
        // because there will be a 'false' on the stack from the 'a' evaluation and then
        // one more for 'c'. If 'and' and 'or' have right-to-left associativity then the
        // 'n->rhs' will contain all the stuff we need to jump over and we will have only
        // one jump. This will not work if someone explicitly defines '(a and b) and c'
        // but usually people don't do that. In Python both cases generate only one jump.

        unsigned jumpTarget = m_currfunction->instructions.size();
        m_currfunction->instructions[jumpIndex].A = jumpTarget;

        if(!keepValue)
            m_currfunction->instructions.emplace_back(OpCode::OC_Pop);
    }

    void Compiler::buildArrowOp(const std::shared_ptr<ast::Node>& node, bool keepValue)
    {
        auto n = std::dynamic_pointer_cast<ast::BinaryOperatorNode>(node);

        if(n->rhs->type != ast::Node::N_FunctionCall)
        {
            m_logger.pushError(n->rhs->coords, "The -> operator must have a function call on the right-hand side");
            return;
        }

        // emit the first argument
        emitInstructions(n->lhs, true);

        // emit the rest of the arguments and the function call
        buildFuncCall(n->rhs, keepValue);

        // find the function call instruction that we just pushed
        // if we didn't keep the value, there will be a 'pop' after it
        int callIndex = int(m_currfunction->instructions.size()) - (keepValue ? 1 : 2);

        // A is the number of arguments used
        // adjust it so that it knows about the new argument we added
        m_currfunction->instructions[callIndex].A += 1;
    }

    void Compiler::BuildArrayPushPop(const std::shared_ptr<ast::Node>& node, bool keepValue)
    {
        auto n = std::dynamic_pointer_cast<ast::BinaryOperatorNode>(node);

        emitInstructions(n->lhs, true);// the array

        if(n->op == T_ArrayPushBack)
        {
            emitInstructions(n->rhs, true);// the thing we are pushing

            m_currfunction->instructions.emplace_back(OpCode::OC_ArrayPushBack);

            if(!keepValue)
                m_currfunction->instructions.emplace_back(OpCode::OC_Pop);
        }
        else// n->op == T_ArrayPopBack
        {
            m_currfunction->instructions.emplace_back(OpCode::OC_ArrayPopBack);

            buildVarStore(n->rhs, keepValue);// the thing we are popping into
        }
    }

    void Compiler::buildBinaryOp(const std::shared_ptr<ast::Node>& node, bool keepValue)
    {
        auto n = std::dynamic_pointer_cast<ast::BinaryOperatorNode>(node);

        switch(n->op)
        {
            case T_Assignment:
            case T_AssignAdd:
            case T_AssignSubtract:
            case T_AssignMultiply:
            case T_AssignDivide:
            case T_AssignPower:
            case T_AssignModulo:
            case T_AssignConcatenate:
                BuildAssignOp(node, keepValue);
                return;
            case T_Or:
            case T_And:
                buildBoolOp(node, keepValue);
                return;
            case T_Arrow:
                buildArrowOp(node, keepValue);
                return;
            case T_ArrayPushBack:
            case T_ArrayPopBack:
                BuildArrayPushPop(node, keepValue);
                return;
            default:
                break;
        }

        emitInstructions(n->lhs, true);

        if(n->op != T_Dot)
            emitInstructions(n->rhs, true);
        else
            buildHashLoadOp(n->rhs);

        OpCode binaryOperation;

        switch(n->op)
        {
            case T_Add:
                binaryOperation = OpCode::OC_Add;
                break;
            case T_Subtract:
                binaryOperation = OpCode::OC_Subtract;
                break;
            case T_Multiply:
                binaryOperation = OpCode::OC_Multiply;
                break;
            case T_Divide:
                binaryOperation = OpCode::OC_Divide;
                break;
            case T_Power:
                binaryOperation = OpCode::OC_Power;
                break;
            case T_Modulo:
                binaryOperation = OpCode::OC_Modulo;
                break;
            case T_Concatenate:
                binaryOperation = OpCode::OC_Concatenate;
                break;
            case T_Xor:
                binaryOperation = OpCode::OC_Xor;
                break;
            case T_Equal:
                binaryOperation = OpCode::OC_Equal;
                break;
            case T_NotEqual:
                binaryOperation = OpCode::OC_NotEqual;
                break;
            case T_Less:
                binaryOperation = OpCode::OC_Less;
                break;
            case T_Greater:
                binaryOperation = OpCode::OC_Greater;
                break;
            case T_LessEqual:
                binaryOperation = OpCode::OC_LessEqual;
                break;
            case T_GreaterEqual:
                binaryOperation = OpCode::OC_GreaterEqual;
                break;
            case T_LeftBracket:
                binaryOperation = OpCode::OC_LoadElement;
                break;
            case T_Dot:
                binaryOperation = OpCode::OC_LoadMember;
                break;
            default:
                m_logger.pushError(n->coords, "Unknown binary operation "s + std::to_string(int(n->op)));
                break;
        }

        m_currfunction->instructions.emplace_back(binaryOperation);

        if(!keepValue)
            m_currfunction->instructions.emplace_back(OpCode::OC_Pop);
    }

    void Compiler::buildUnaryOp(const std::shared_ptr<ast::Node>& node, bool keepValue)
    {
        auto n = std::dynamic_pointer_cast<ast::UnaryOperatorNode>(node);

        emitInstructions(n->operand, true);

        OpCode unaryOperation;

        switch(n->op)
        {
            case T_Add:
                unaryOperation = OpCode::OC_UnaryPlus;
                break;
            case T_Subtract:
                unaryOperation = OpCode::OC_UnaryMinus;
                break;
            case T_Not:
                unaryOperation = OpCode::OC_UnaryNot;
                break;
            case T_Concatenate:
                unaryOperation = OpCode::OC_UnaryConcatenate;
                break;
            case T_SizeOf:
                unaryOperation = OpCode::OC_UnarySizeOf;
                break;
            default:
                m_logger.pushError(n->coords, "Unknown unary operation "s + std::to_string(int(n->op)));
                break;
        }

        m_currfunction->instructions.emplace_back(unaryOperation);

        if(!keepValue)
            m_currfunction->instructions.emplace_back(OpCode::OC_Pop);
    }

    void Compiler::buildIfStmt(const std::shared_ptr<ast::Node>& node, bool keepValue)
    {
        auto n = std::dynamic_pointer_cast<ast::IfNode>(node);

        emitInstructions(n->condition, true);

        if(n->elsePath)
        {
            // if the condition doesn't hold we should jump to the 'else' path
            unsigned jumpToElseIndex = m_currfunction->instructions.size();
            m_currfunction->instructions.emplace_back(OpCode::OC_PopJumpIfFalse);

            // emit the 'then' path
            emitInstructions(n->thenPath, keepValue);

            // when done jump after the 'else' path
            unsigned jumpToEndIndex = m_currfunction->instructions.size();
            m_currfunction->instructions.emplace_back(OpCode::OC_Jump);

            unsigned elseLocation = m_currfunction->instructions.size();

            // emit the 'else' path
            emitInstructions(n->elsePath, keepValue);

            unsigned endLocation = m_currfunction->instructions.size();

            // fill in the placeholder jumps with the proper locations
            Instruction& jumpToElse = m_currfunction->instructions[jumpToElseIndex];
            jumpToElse.A = elseLocation;

            Instruction& jumpToEnd = m_currfunction->instructions[jumpToEndIndex];
            jumpToEnd.A = endLocation;
        }
        else if(keepValue)// no 'else' clause, but result is expected
        {
            // if the condition doesn't hold we should jump to the 'else' path
            unsigned jumpToElseIndex = m_currfunction->instructions.size();
            m_currfunction->instructions.emplace_back(OpCode::OC_PopJumpIfFalse);

            // emit the 'then' path
            emitInstructions(n->thenPath, true);

            // when done jump after the 'else' path
            unsigned jumpToEndIndex = m_currfunction->instructions.size();
            m_currfunction->instructions.emplace_back(OpCode::OC_Jump);

            unsigned elseLocation = m_currfunction->instructions.size();

            // the 'else' path will simply push a nil to the stack
            m_currfunction->instructions.emplace_back(OpCode::OC_LoadConstant, 0);

            unsigned endLocation = m_currfunction->instructions.size();

            // fill in the placeholder jumps with the proper locations
            Instruction& jumpToElse = m_currfunction->instructions[jumpToElseIndex];
            jumpToElse.A = elseLocation;

            Instruction& jumpToEnd = m_currfunction->instructions[jumpToEndIndex];
            jumpToEnd.A = endLocation;
        }
        else// no 'else' path and no result expected
        {
            // if the condition doesn't hold, we should jump after the 'then' path
            unsigned jumpToEndIndex = m_currfunction->instructions.size();
            m_currfunction->instructions.emplace_back(OpCode::OC_PopJumpIfFalse);

            // emit the 'then' path
            emitInstructions(n->thenPath, false);

            unsigned endLocation = m_currfunction->instructions.size();

            // fill in the placeholder jump with the proper location
            Instruction& jumpToEnd = m_currfunction->instructions[jumpToEndIndex];
            jumpToEnd.A = endLocation;
        }
    }

    void Compiler::buildWhileStmt(const std::shared_ptr<ast::Node>& node, bool keepValue)
    {
        auto n = std::dynamic_pointer_cast<ast::WhileNode>(node);

        m_loopcontexts.emplace_back();
        m_loopcontexts.back().keepValue = keepValue;
        m_loopcontexts.back().forLoop = false;

        if(keepValue)// if the loop doesn't run not even once, we still expect a value
            m_currfunction->instructions.emplace_back(OpCode::OC_LoadConstant, 0);

        unsigned conditionLocation = m_currfunction->instructions.size();

        // emit the condition
        emitInstructions(n->condition, true);

        // if the condition fails jump to 'end'
        m_loopcontexts.back().jumpToEndIndices.push_back(m_currfunction->instructions.size());
        m_currfunction->instructions.emplace_back(OpCode::OC_PopJumpIfFalse);

        if(keepValue)// discard old value
            m_currfunction->instructions.emplace_back(OpCode::OC_Pop);

        // emit the body
        emitInstructions(n->body, keepValue);

        // jump back to the condition to try it again
        m_loopcontexts.back().jumpToConditionIndices.push_back(m_currfunction->instructions.size());
        m_currfunction->instructions.emplace_back(OpCode::OC_Jump);

        unsigned endLocation = m_currfunction->instructions.size();

        // fill placeholder jumps with proper locations
        LoopContext& context = m_loopcontexts.back();
        for(int i : context.jumpToConditionIndices)
        {
            Instruction& instruction = m_currfunction->instructions[i];
            instruction.A = conditionLocation;
        }
        for(int i : context.jumpToEndIndices)
        {
            Instruction& instruction = m_currfunction->instructions[i];
            instruction.A = endLocation;
        }

        m_loopcontexts.pop_back();
    }

    void Compiler::buildForStmt(const std::shared_ptr<ast::Node>& node, bool keepValue)
    {
        auto n = std::dynamic_pointer_cast<ast::ForNode>(node);

        // emit the value we will be iterating over
        emitInstructions(n->iteratedExpression, true);

        m_loopcontexts.emplace_back();
        m_loopcontexts.back().keepValue = keepValue;
        m_loopcontexts.back().forLoop = true;

        // should we need to call 'return' from inside the loop, we will need to clean up
        m_funcontexts.back().forLoopsGarbage += keepValue ? 2 : 1;

        // confirm we have an iterator object or make a default one for arrays and strings
        m_currfunction->instructions.emplace_back(OpCode::OC_MakeIterator);

        if(keepValue)// the default result is nil, it will be kept beneath the iterator
        {
            m_currfunction->instructions.emplace_back(OpCode::OC_LoadConstant, 0);
            m_currfunction->instructions.emplace_back(OpCode::OC_Rotate2);
        }

        unsigned conditionLocation = m_currfunction->instructions.size();

        // 'has_next' will provide the condition
        m_currfunction->instructions.emplace_back(OpCode::OC_IteratorHasNext);

        // if the condition fails jump to 'end'
        m_loopcontexts.back().jumpToEndIndices.push_back(m_currfunction->instructions.size());
        m_currfunction->instructions.emplace_back(OpCode::OC_PopJumpIfFalse);

        // 'get_next' will provide the new iterating variable
        m_currfunction->instructions.emplace_back(OpCode::OC_IteratorGetNext);

        // assign it to the iterating variable
        buildVarStore(n->iteratingVariable, false);

        // emit the body
        emitInstructions(n->body, keepValue);

        if(keepValue)// save the result value beneath the iterator object which will be at TOS1
            m_currfunction->instructions.emplace_back(OpCode::OC_MoveToTOS2);

        // jump back to the condition to try it again
        m_loopcontexts.back().jumpToConditionIndices.push_back(m_currfunction->instructions.size());
        m_currfunction->instructions.emplace_back(OpCode::OC_Jump);

        unsigned endLocation = m_currfunction->instructions.size();

        // pop the iterator object
        m_currfunction->instructions.emplace_back(OpCode::OC_Pop);

        // fill placeholder jumps with proper locations
        LoopContext& context = m_loopcontexts.back();
        for(int i : context.jumpToConditionIndices)
        {
            Instruction& instruction = m_currfunction->instructions[i];
            instruction.A = conditionLocation;
        }
        for(int i : context.jumpToEndIndices)
        {
            Instruction& instruction = m_currfunction->instructions[i];
            instruction.A = endLocation;
        }

        m_loopcontexts.pop_back();

        m_funcontexts.back().forLoopsGarbage -= keepValue ? 2 : 1;
    }

    void Compiler::buildBlockStmt(const std::shared_ptr<ast::Node>& node, bool keepValue)
    {
        auto n = std::dynamic_pointer_cast<ast::BlockNode>(node);

        // emit instructions for all nodes
        int lastNodeIndex = int(n->nodes.size()) - 1;

        if(lastNodeIndex >= 0)
        {
            for(int i = 0; i < lastNodeIndex; ++i)
                emitInstructions(n->nodes[i], false);

            emitInstructions(n->nodes[lastNodeIndex], keepValue);
        }
        else if(keepValue)// empty block, but we expect a value, so we push a nil
        {
            m_currfunction->instructions.emplace_back(OpCode::OC_LoadConstant, 0);
        }
    }

    void Compiler::buildFuncStmt(const std::shared_ptr<ast::Node>& node, bool keepValue)
    {
        auto n = std::dynamic_pointer_cast<ast::FunctionNode>(node);

        // new function goes in a new constant
        int thisFunctionIndex = int(m_constants.size());

        m_funcontexts.emplace_back();
        m_funcontexts.back().index = thisFunctionIndex;

        m_constants.emplace_back(new CodeObject());
        m_currfunction = m_constants.back().codeObject;

        m_currfunction->namedParametersCount = int(n->namedParameters.size());
        m_currfunction->localVariablesCount = n->localVariablesCount;
        m_currfunction->closureMapping = n->closureMapping;

        // if parameters need to be boxed, this is their first occurrence, so we box them right away
        for(int parameterIndex : n->parametersToBox)
            m_currfunction->instructions.emplace_back(OpCode::OC_MakeBox, parameterIndex);

        // emit instructions
        if(n->body && n->body->type == ast::Node::N_Block)
            buildBlockStmt(n->body, true);
        else
            emitInstructions(n->body, true);

        unsigned endLocation = m_currfunction->instructions.size();

        m_currfunction->instructions.emplace_back(OpCode::OC_EndFunction);

        // some 'jump' instructions may have needed to know where to jump to
        for(int i : m_funcontexts.back().jumpToEndIndices)
        {
            Instruction& jumpToEndInstruction = m_currfunction->instructions[i];
            jumpToEndInstruction.A = endLocation;
        }

        m_funcontexts.pop_back();

        // back to old constant
        if(!m_funcontexts.empty())
        {
            m_currfunction = m_constants[m_funcontexts.back().index].codeObject;

            if(keepValue)
            {
                m_currfunction->instructions.emplace_back(OpCode::OC_LoadConstant, thisFunctionIndex);

                if(!n->closureMapping.empty())
                    m_currfunction->instructions.emplace_back(OpCode::OC_MakeClosure);
            }
        }
    }

    void Compiler::buildFuncCall(const std::shared_ptr<ast::Node>& node, bool keepValue)
    {
        auto n = std::dynamic_pointer_cast<ast::FunctionCallNode>(node);

        auto argsNode = std::dynamic_pointer_cast<ast::ArgumentsNode>(n->arguments);

        for(auto argument : argsNode->arguments)
            emitInstructions(argument, true);

        emitInstructions(n->function, true);

        m_currfunction->instructions.emplace_back(OpCode::OC_FunctionCall, argsNode->arguments.size());

        if(!keepValue)
            m_currfunction->instructions.emplace_back(OpCode::OC_Pop);
    }

    void Compiler::buildArrayLiteral(const std::shared_ptr<ast::Node>& node, bool keepValue)
    {
        auto n = std::dynamic_pointer_cast<ast::ArrayNode>(node);

        for(auto element : n->elements)
            emitInstructions(element, true);

        m_currfunction->instructions.emplace_back(OpCode::OC_MakeArray, n->elements.size());

        if(!keepValue)
            m_currfunction->instructions.emplace_back(OpCode::OC_Pop);
    }

    void Compiler::buildObjectLiteral(const std::shared_ptr<ast::Node>& node, bool keepValue)
    {
        auto n = std::dynamic_pointer_cast<ast::ObjectNode>(node);

        unsigned membersCount = n->members.size();

        if(membersCount == 0)
        {
            m_currfunction->instructions.emplace_back(OpCode::OC_MakeEmptyObject);
        }
        else// at least one member
        {
            bool protoDefined = false;

            for(const ast::ObjectNode::KeyValuePair& member : n->members)
            {
                // hash key
                protoDefined = buildHashLoadOp(member.first) || protoDefined;
                // value
                emitInstructions(member.second, true);
            }

            if(!protoDefined)
            {
                Instruction loadProtoHash(OpCode::OC_LoadHash);
                loadProtoHash.H = Symbol::ProtoHash;

                m_currfunction->instructions.push_back(loadProtoHash);
                m_currfunction->instructions.emplace_back(OpCode::OC_LoadConstant, 0);// nil

                ++membersCount;
            }

            m_currfunction->instructions.emplace_back(OpCode::OC_MakeObject, membersCount);
        }

        if(!keepValue)
            m_currfunction->instructions.emplace_back(OpCode::OC_Pop);
    }

    void Compiler::buildYield(const std::shared_ptr<ast::Node>& node, bool keepValue)
    {
        auto n = std::dynamic_pointer_cast<ast::YieldNode>(node);

        if(n->value)
            emitInstructions(n->value, true);
        else
            m_currfunction->instructions.emplace_back(OpCode::OC_LoadConstant, 0);// nil

        m_currfunction->instructions.emplace_back(OpCode::OC_Yield);

        if(!keepValue)
            m_currfunction->instructions.emplace_back(OpCode::OC_Pop);
    }

    bool Compiler::buildHashLoadOp(const std::shared_ptr<ast::Node>& node)
    {
        if(node->type == ast::Node::N_Variable)
        {
            auto n = std::dynamic_pointer_cast<ast::VariableNode>(node);

            if(n->variableType == ast::VariableNode::V_Named)
            {
                Instruction loadHash(OpCode::OC_LoadHash);
                loadHash.H = updateSymbol(n->name);

                m_currfunction->instructions.push_back(loadHash);

                if(loadHash.H == Symbol::ProtoHash)
                    return true;
            }
            else
            {
                m_logger.pushError(node->coords, "Can only create hash load operation on named variables\n");
            }
        }
        else
        {
            m_logger.pushError(node->coords, "Can only create hash load operation on variables\n");
        }

        return false;
    }

    void Compiler::buildJumpStmt(const std::shared_ptr<ast::Node>& node)
    {
        switch(node->type)
        {
            case ast::Node::N_Break:
            {
                auto n = std::dynamic_pointer_cast<ast::BreakNode>(node);

                LoopContext& context = m_loopcontexts.back();

                if(context.keepValue)
                {
                    if(n->value)
                        emitInstructions(n->value, true);
                    else
                        m_currfunction->instructions.emplace_back(OpCode::OC_LoadConstant, 0);

                    if(context.forLoop)
                        m_currfunction->instructions.emplace_back(OpCode::OC_MoveToTOS2);
                }

                context.jumpToEndIndices.push_back(m_currfunction->instructions.size());
                m_currfunction->instructions.emplace_back(OpCode::OC_Jump);
                return;
            }
            case ast::Node::N_Continue:
            {
                auto n = std::dynamic_pointer_cast<ast::ContinueNode>(node);

                LoopContext& context = m_loopcontexts.back();

                if(context.keepValue)
                {
                    if(n->value)
                        emitInstructions(n->value, true);
                    else
                        m_currfunction->instructions.emplace_back(OpCode::OC_LoadConstant, 0);

                    if(context.forLoop)
                        m_currfunction->instructions.emplace_back(OpCode::OC_MoveToTOS2);
                }

                context.jumpToConditionIndices.push_back(m_currfunction->instructions.size());
                m_currfunction->instructions.emplace_back(OpCode::OC_Jump);
                return;
            }
            case ast::Node::N_Return:
            {
                auto n = std::dynamic_pointer_cast<ast::ReturnNode>(node);

                FunctionContext& context = m_funcontexts.back();

                if(context.forLoopsGarbage > 0)
                    m_currfunction->instructions.emplace_back(OpCode::OC_PopN, context.forLoopsGarbage);

                if(n->value)
                    emitInstructions(n->value, true);
                else
                    m_currfunction->instructions.emplace_back(OpCode::OC_LoadConstant, 0);

                context.jumpToEndIndices.push_back(m_currfunction->instructions.size());
                m_currfunction->instructions.emplace_back(OpCode::OC_Jump);
                return;
            }
            default:
                m_logger.pushError(node->coords, "Unknown jump statement type "s + std::to_string(int(node->type)));
                break;
        }
    }

    unsigned Compiler::updateSymbol(const std::string& name)
    {
        unsigned hash = Symbol::Hash(name);
        unsigned step = Symbol::HashStep(hash);

        if(name == "proto")
            hash = Symbol::ProtoHash;

        auto it = m_symindices.find(hash);

        while(it != m_symindices.end() &&// found the hash but
              m_symbols[it->second].name != name)// didn't find the name
        {
            hash += step;
            it = m_symindices.find(hash);
        }

        if(it == m_symindices.end())// not found, add new symbol
        {
            m_symindices[hash] = m_symbols.size();
            m_symbols.emplace_back(name, hash);
        }

        return hash;
    }

    std::unique_ptr<char[]> Compiler::buildBinaryData()
    {
        unsigned symbolsCount = m_symbols.size();
        unsigned symbolsSize = 0;

        for(unsigned i = m_symsoffset; i < symbolsCount; ++i)
            symbolsSize += m_symbols[i].getSize();

        unsigned constantsCount = m_constants.size();
        unsigned constantsSize = 0;

        for(unsigned i = m_constoffset; i < constantsCount; ++i)
            constantsSize += m_constants[i].getSize();

        unsigned totalSize = 3 * sizeof(unsigned) + symbolsSize + 3 * sizeof(unsigned) + constantsSize;
        // allocate data
        std::unique_ptr<char[]> binaryData = std::make_unique<char[]>(totalSize);

        // write symbols size
        unsigned* p = (unsigned*)binaryData.get();
        *p = symbolsSize;

        // write symbols count
        ++p;
        *p = symbolsCount - m_symsoffset;

        // write all symbols offset
        ++p;
        *p = m_symsoffset;

        // write all symbols
        char* c = binaryData.get() + 3 * sizeof(unsigned);

        for(unsigned i = m_symsoffset; i < symbolsCount; ++i)
            c = m_symbols[i].writeSymbol(c);

        // write constants size
        p = (unsigned*)c;
        *p = constantsSize;

        // write constants count
        ++p;
        *p = constantsCount - m_constoffset;

        // write all constant indices offset
        ++p;
        *p = m_constoffset;

        // write all constants
        c = c + 3 * sizeof(unsigned);

        for(unsigned i = m_constoffset; i < constantsCount; ++i)
            c = m_constants[i].writeConst(c);

        // prepare for the next build iteration
        m_symsoffset = symbolsCount;
        m_constoffset = constantsCount;

        return binaryData;
    }

}// namespace element
