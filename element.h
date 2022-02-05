
#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <memory>
#include <deque>
#include <unordered_map>
#include <cstring>
#include <iomanip>
#include <algorithm>
#include <map>
#include <limits>
#include <istream>
#include <iostream>
#include <fstream>
#include <cmath>
#include <locale>
#include <iterator>
#include <stdarg.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>

namespace element
{
    class Logger;
    struct CodeObject;
    struct String;
    struct Array;
    struct Object;
    struct Function;
    struct Box;
    struct Iterator;
    struct GarbageCollected;
    struct Error;
    class VirtualMachine;
    struct ExecutionContext;

    namespace ast
    {
        struct Node;
        struct FunctionNode;
        struct VariableNode;
        struct BinaryOperatorNode;
    }// namespace ast

    enum Token : int
    {
        T_EOF = 0,
        T_NewLine,

        T_Identifier,// name

        T_Integer,// 123
        T_Float,// 123.456
        T_String,// "abc"
        T_Bool,// true false

        T_If,// if
        T_Else,// else
        T_Elif,// elif
        T_For,// for
        T_In,// in
        T_While,// while

        T_This,// this
        T_Nil,// nil

        T_Return,// return
        T_Break,// break
        T_Continue,// continue
        T_Yield,// yield

        T_And,// and
        T_Or,// or
        T_Xor,// xor
        T_Not,// not

        T_Underscore,// _

        T_LeftParent,// (
        T_RightParent,// )

        T_LeftBrace,// {
        T_RightBrace,// }

        T_LeftBracket,// [
        T_RightBracket,// ]

        T_Column,// :
        T_DoubleColumn,// ::

        T_Semicolumn,// ;
        T_Comma,// ,
        T_Dot,// .

        T_Add,// +
        T_Subtract,// -
        T_Divide,// /
        T_Multiply,// *
        T_Power,// ^
        T_Modulo,// %
        T_Concatenate,// ~

        T_AssignAdd,// +=
        T_AssignSubtract,// -=
        T_AssignDivide,// /=
        T_AssignMultiply,// *=
        T_AssignPower,// ^=
        T_AssignModulo,// %=
        T_AssignConcatenate,// ~=

        T_Assignment,// =

        T_Equal,// ==
        T_NotEqual,// !=
        T_Less,// <
        T_Greater,// >
        T_LessEqual,// <=
        T_GreaterEqual,// >=

        T_Argument,// $ $1 $2 ...
        T_ArgumentList,// $$
        T_Arrow,// ->
        T_ArrayPushBack,// <<
        T_ArrayPopBack,// >>
        T_SizeOf,// #

        T_InvalidToken
    };

    enum OpCode : char
    {
        OC_Pop,// pop TOS
        OC_PopN,// pop A values from the stack
        OC_Rotate2,// swap TOS and TOS1
        OC_MoveToTOS2,// copy TOS over TOS2 and pop TOS
        OC_Duplicate,// make a copy of TOS and push it to the stack
        OC_Unpack,// A is the number of values to be produced from the TOS value

        // push a value to TOS
        OC_LoadConstant,// A is the index in the constants vector
        OC_LoadLocal,// A is the index in the function scope
        OC_LoadGlobal,// A is the index in the global scope
        OC_LoadNative,// A is the index in the native functions
        OC_LoadArgument,// A is the index in the arguments array
        OC_LoadArgsArray,// load the current frame's arguments array
        OC_LoadThis,// load the current frame's this object

        // store a value from TOS in A
        OC_StoreLocal,// A is the index in the function scope
        OC_StoreGlobal,// A is the index in the global scope

        // pop a value from TOS and store it in A
        OC_PopStoreLocal,// A is the index in the function scope
        OC_PopStoreGlobal,// A is the index in the global scope

        // arrays
        OC_MakeArray,// A is the number of elements to be taken from the stack
        OC_LoadElement,// TOS is the index in the TOS1 array or object
        OC_StoreElement,// TOS index, TOS1 array or object, TOS2 new value
        OC_PopStoreElement,// TOS index, TOS1 array or object, TOS2 new value
        OC_ArrayPushBack,
        OC_ArrayPopBack,

        // objects
        OC_MakeObject,// A is number of key-value pairs to be taken from the stack
        OC_MakeEmptyObject,// make an object with just the proto member value
        OC_LoadHash,// H is the hash to load on the stack
        OC_LoadMember,// TOS is the member hash in the TOS1 object
        OC_StoreMember,// TOS member hash, TOS1 object, TOS2 new value
        OC_PopStoreMember,// TOS member hash, TOS1 object, TOS2 new value

        // iterators
        OC_MakeIterator,// make an iterator object from TOS and replace it at TOS
        OC_IteratorHasNext,// call 'has_next' from the TOS object
        OC_IteratorGetNext,// call 'get_next' from the TOS object

        // closures
        OC_MakeBox,// A is the index of the box that needs to be created
        OC_LoadFromBox,// load the value stored in the box at index A
        OC_StoreToBox,// A is the index of the box that holds the value
        OC_PopStoreToBox,// A is the index of the box that holds the value

        OC_MakeClosure,// Create a closure from the function object at TOS and replace it
        OC_LoadFromClosure,// load the value of the free variable inside the closure at index A
        OC_StoreToClosure,// A is the index of the free variable inside the closure
        OC_PopStoreToClosure,// A is the index of the free variable inside the closure

        // move the instruction pointer
        OC_Jump,// jump to A
        OC_JumpIfFalse,// jump to A, if TOS is false
        OC_PopJumpIfFalse,// jump to A, if TOS is false, pop TOS either way
        OC_JumpIfFalseOrPop,// jump to A, if TOS is false, otherwise pop TOS (and-op)
        OC_JumpIfTrueOrPop,// jump to A, if TOS is true, otherwise pop TOS (or-op)

        OC_FunctionCall,// function to call and arguments are on stack, A is arguments count
        OC_Yield,// yield the value from TOS to the parent execution context
        OC_EndFunction,// end function sentinel

        // binary operations take two arguments from the stack, result in TOS
        OC_Add,
        OC_Subtract,
        OC_Multiply,
        OC_Divide,
        OC_Power,
        OC_Modulo,
        OC_Concatenate,
        OC_Xor,

        OC_Equal,
        OC_NotEqual,
        OC_Less,
        OC_Greater,
        OC_LessEqual,
        OC_GreaterEqual,

        // unary operations take one argument from the stack, result in TOS
        OC_UnaryPlus,
        OC_UnaryMinus,
        OC_UnaryNot,
        OC_UnaryConcatenate,
        OC_UnarySizeOf,
    };


    const char* TokenAsString(Token token);

    struct Location
    {
        int line = 1;
        int column = 1;
    };

    class Logger
    {
        private:
            std::vector<std::string> m_messages;

        public:
            void pushError(const std::string& errorMessage);
            void pushError(int line, const std::string& errorMessage);
            void pushError(const Location& coords, const std::string& errorMessage);

            bool hasMessages() const;
            std::string getCombined() const;
            void clearMessages();
    };

    namespace ast
    {
        struct Node
        {
            enum NodeType : int
            {
                N_Nil,
                N_Variable,
                N_Integer,
                N_Float,
                N_Bool,
                N_String,
                N_Array,
                N_Object,
                N_Function,
                N_FunctionCall,
                N_Arguments,
                N_UnaryOperator,
                N_BinaryOperator,
                N_Block,
                N_If,
                N_While,
                N_For,
                N_Return,
                N_Break,
                N_Continue,
                N_Yield,
            };

            NodeType type;
            Location coords;

            Node(const Location& coords);
            Node(NodeType type, const Location& coords);
            virtual ~Node();
        };

        struct VariableNode : public Node
        {
            enum VariableType : int
            {
                V_Named = -1,
                V_This = -2,
                V_ArgumentList = -3,
                V_Underscore = -4,
            };

            // Semantic information ////////////////////////////////////////////////////
            enum SemanticType : char
            {
                SMT_Global,// global variable defined outside of any function or block
                SMT_Local,// local to the current function or block
                SMT_Native,// global variable that is defined in C/C++
                SMT_FreeVariable,// unbound variable that will be determined by the closure
                SMT_LocalBoxed,// a free variable will later be bound to this local variable
            };


            // if type < 0 then it is a special variable
            // if type >= 0 then it is argument number N
            int variableType;
            std::string name;
            SemanticType semanticType;
            bool firstOccurrence;
            int index;

            VariableNode(int variableType, const Location& coords);
            VariableNode(const std::string& name, const Location& coords);

        };

        struct IntegerNode : public Node
        {
            int value;

            IntegerNode(int value, const Location& coords);
        };

        struct FloatNode : public Node
        {
            float value;

            FloatNode(float value, const Location& coords);
        };

        struct BoolNode : public Node
        {
            bool value;

            BoolNode(bool value, const Location& coords);
        };

        struct StringNode : public Node
        {
            std::string value;

            StringNode(const std::string& value, const Location& coords);
        };

        struct ArrayNode : public Node
        {
            std::vector<std::shared_ptr<Node>> elements;

            ArrayNode(const std::vector<std::shared_ptr<Node>>& elements, const Location& coords);
            ~ArrayNode();
        };

        struct ObjectNode : public Node
        {
            using KeyValuePair = std::pair<std::shared_ptr<Node>, std::shared_ptr<Node>>;
            using KeyValuePairs = std::vector<KeyValuePair>;

            KeyValuePairs members;

            ObjectNode(const Location& coords);
            ObjectNode(const KeyValuePairs& members, const Location& coords);
            ~ObjectNode();
        };

        struct FunctionNode : public Node
        {
            typedef std::vector<std::string> NamedParameters;

            NamedParameters namedParameters;
            std::shared_ptr<Node> body;
            // Semantic information ////////////////////////////////////////////////////
            int localVariablesCount;
            std::vector<std::shared_ptr<VariableNode>> referencedVariables;
            // These are the indices of the variables from the enclosing function scope
            // during the creation of the closure object. We create a new free variable
            // for each one of them. If the index is negative this means that it is taken
            // from the enclosing function scope's closure. Convert it using this formula
            //    freeVariableIndex = abs(i) - 1
            std::vector<int> closureMapping;

            // These are indices of the local variables that represent the function's
            // parameters. Since they are declared by the FunctionNode there may not be
            // any references to them by VariableNodes. If that is so, then nothing will
            // claim the firstOccurrence flag. If there is no firstOccurrence and the
            // variable is part of a closure, it will still need to be boxed.
            std::vector<int> parametersToBox;

            FunctionNode(const NamedParameters& namedParameters, const std::shared_ptr<Node>& body, const Location& coords);
            ~FunctionNode();
        };

        struct FunctionCallNode : public Node
        {
            std::shared_ptr<Node> function;
            std::shared_ptr<Node> arguments;

            FunctionCallNode(const std::shared_ptr<Node>& function, const std::shared_ptr<Node>& arguments, const Location& coords);
            ~FunctionCallNode();
        };

        struct ArgumentsNode : public Node
        {
            std::vector<std::shared_ptr<Node>> arguments;

            ArgumentsNode(std::vector<std::shared_ptr<Node>>& arguments, const Location& coords);
            ~ArgumentsNode();
        };

        struct UnaryOperatorNode : public Node
        {
            Token op;
            std::shared_ptr<Node> operand;

            UnaryOperatorNode(Token op, const std::shared_ptr<Node>& operand, const Location& coords);
            ~UnaryOperatorNode();
        };

        struct BinaryOperatorNode : public Node
        {
            Token op;
            std::shared_ptr<Node> lhs;
            std::shared_ptr<Node> rhs;

            BinaryOperatorNode(Token op, const std::shared_ptr<Node>& lhs, const std::shared_ptr<Node>& rhs, const Location& coords);
            ~BinaryOperatorNode();
        };

        struct BlockNode : Node
        {
            std::vector<std::shared_ptr<Node>> nodes;
            // Semantic information
            bool explicitFunctionBlock;

            BlockNode(std::vector<std::shared_ptr<Node>>& nodes, const Location& coords);
            ~BlockNode();


        };

        struct IfNode : public Node
        {
            std::shared_ptr<Node> condition;
            std::shared_ptr<Node> thenPath;
            std::shared_ptr<Node> elsePath;

            IfNode(const std::shared_ptr<Node>& condition, const std::shared_ptr<Node>& thenPath, const std::shared_ptr<Node>& elsePath, const Location& coords);
            ~IfNode();
        };

        struct WhileNode : public Node
        {
            std::shared_ptr<Node> condition;
            std::shared_ptr<Node> body;

            WhileNode(const std::shared_ptr<Node>& condition, const std::shared_ptr<Node>& body, const Location& coords);
            ~WhileNode();
        };

        struct ForNode : public Node
        {
            std::shared_ptr<Node> iteratingVariable;
            std::shared_ptr<Node> iteratedExpression;
            std::shared_ptr<Node> body;

            ForNode(const std::shared_ptr<Node>& iteratingVariable, const std::shared_ptr<Node>& iteratedExpression, const std::shared_ptr<Node>& body, const Location& coords);
            ~ForNode();
        };

        struct ReturnNode : public Node
        {
            std::shared_ptr<Node> value;

            ReturnNode(const std::shared_ptr<Node>& value, const Location& coords);
            ~ReturnNode();
        };

        struct BreakNode : public Node
        {
            std::shared_ptr<Node> value;

            BreakNode(const std::shared_ptr<Node>& value, const Location& coords);
            ~BreakNode();
        };

        struct ContinueNode : public Node
        {
            std::shared_ptr<Node> value;

            ContinueNode(const std::shared_ptr<Node>& value, const Location& coords);
            ~ContinueNode();
        };

        struct YieldNode : public Node
        {
            std::shared_ptr<Node> value;

            YieldNode(const std::shared_ptr<Node>& value, const Location& coords);
            ~YieldNode();
        };

        std::string nodeToString(const std::shared_ptr<ast::Node>& root, int indent = 0);

    }// namespace ast

    class Constant
    {
        public:
            enum Type : int
            {
                CT_Nil,
                CT_Bool,
                CT_Integer,
                CT_Float,
                CT_String,
                CT_CodeObject,
            };

        public:
            Type type;

            union
            {
                bool boolean;
                int integer;
                float floatingPoint;
                std::string* string;
                CodeObject* codeObject;
                unsigned size;
            };

        public:
            Constant();
            Constant(bool boolean);
            Constant(int integer);
            Constant(float floatingPoint);
            Constant(const std::string& string);
            Constant(CodeObject* codeObject);
            ~Constant();

            bool equals(int i) const;
            bool equals(float f) const;
            bool equals(const std::string& str) const;

            void clear();

            unsigned getSize() const;

            char* writeConst(char* memoryDestination) const;
            char* readConst(char* memorySource);

            std::string toString() const;
    };

    struct Symbol
    {
        static unsigned Hash(const std::string& str);
        static unsigned HashStep(unsigned key);

        static const unsigned ProtoHash;
        static const unsigned HasNextHash;
        static const unsigned GetNextHash;

        std::string name;
        unsigned hash;

        Symbol();
        Symbol(const std::string& name, unsigned hash);

        unsigned getSize() const;

        char* writeSymbol(char* memoryDestination) const;
        char* readSymbol(char* memorySource);

        std::string toString() const;
    };

    class Compiler
    {
        public:
            struct FunctionContext
            {
                std::vector<unsigned> jumpToEndIndices;
                int index = -1;
                int totalIndices = 0;
                int forLoopsGarbage = 0;
            };

            struct LoopContext
            {
                std::vector<unsigned> jumpToConditionIndices;
                std::vector<unsigned> jumpToEndIndices;
                bool keepValue;
                bool forLoop;
            };

       private:
            Logger& m_logger;

            std::vector<LoopContext> m_loopcontexts;
            std::vector<FunctionContext> m_funcontexts;

            CodeObject* m_currfunction;

            std::deque<Constant> m_constants;
            unsigned m_constoffset;

            std::unordered_map<unsigned, unsigned> m_symindices;
            std::vector<Symbol> m_symbols;
            unsigned mSymbolsOffset;

        public:
            Compiler(Logger& logger);

            auto compile(const std::shared_ptr<ast::FunctionNode>& node) -> std::unique_ptr<char[]>;

            void resetState();

        protected:
            void emitInstructions(const std::shared_ptr<ast::Node>& node, bool keepValue);

            void buildConstLoad(const std::shared_ptr<ast::Node>& node, bool keepValue);
            void buildVarLoad(const std::shared_ptr<ast::Node>& node, bool keepValue);
            void buildVarStore(const std::shared_ptr<ast::Node>& node, bool keepValue);
            void BuildAssignOp(const std::shared_ptr<ast::Node>& node, bool keepValue);
            void buildBoolOp(const std::shared_ptr<ast::Node>& node, bool keepValue);
            void buildArrowOp(const std::shared_ptr<ast::Node>& node, bool keepValue);
            void BuildArrayPushPop(const std::shared_ptr<ast::Node>& node, bool keepValue);
            void buildBinaryOp(const std::shared_ptr<ast::Node>& node, bool keepValue);
            void buildUnaryOp(const std::shared_ptr<ast::Node>& node, bool keepValue);
            void buildIfStmt(const std::shared_ptr<ast::Node>& node, bool keepValue);
            void buildWhileStmt(const std::shared_ptr<ast::Node>& node, bool keepValue);
            void buildForStmt(const std::shared_ptr<ast::Node>& node, bool keepValue);
            void buildBlockStmt(const std::shared_ptr<ast::Node>& node, bool keepValue);
            void buildFuncStmt(const std::shared_ptr<ast::Node>& node, bool keepValue);
            void buildFuncCall(const std::shared_ptr<ast::Node>& node, bool keepValue);
            void buildArrayLiteral(const std::shared_ptr<ast::Node>& node, bool keepValue);
            void buildObjectLiteral(const std::shared_ptr<ast::Node>& node, bool keepValue);
            void buildYield(const std::shared_ptr<ast::Node>& node, bool keepValue);
            bool buildHashLoadOp(const std::shared_ptr<ast::Node>& node);
            void buildJumpStmt(const std::shared_ptr<ast::Node>& node);

            unsigned updateSymbol(const std::string& name);

            std::unique_ptr<char[]> buildBinaryData();
    };

    // TOS  == Top Of Stack
    // TOS1 == The value beneath TOS
    // TOS2 == The value beneath TOS1

    struct Instruction
    {
        OpCode opCode;

        union
        {
            int A;
            unsigned H;
        };

        Instruction(OpCode opCode, int A = 0);

        std::string asString() const;
    };



    struct Value
    {
        enum Type : char
        {
            VT_Nil = 0,
            VT_Int = 1,
            VT_Float = 2,
            VT_Bool = 3,
            VT_Hash = 4,
            VT_NativeFunction = 5,

            VT_String = 6,
            VT_Function = 7,
            VT_Array = 8,
            VT_Object = 9,
            VT_Box = 10,
            VT_Iterator = 11,
            VT_Error = 12,
        };

        Type type;

        typedef Value (*NativeFunction)(VirtualMachine&, const Value&, const std::vector<Value>&);

        union
        {
            int integer;
            float floatingPoint;
            bool boolean;
            unsigned hash;
            String* string;
            Array* array;
            Object* object;
            Function* function;
            Box* box;
            Iterator* iterator;
            NativeFunction nativeFunction;
            Error* error;

            GarbageCollected* garbageCollected;
        };

        Value();
        Value(int integer);
        Value(float floatingPoint);
        Value(bool boolean);
        Value(unsigned hash);
        Value(String* string);
        Value(Array* array);
        Value(Object* object);
        Value(Function* function);
        Value(Box* box);
        Value(Iterator* iterator);
        Value(NativeFunction nativeFunction);
        Value(Error* error);

        Value(const Value& o);

        bool isManaged() const;
        bool isNil() const;
        bool isFunction() const;
        bool isArray() const;
        bool isObject() const;
        bool isString() const;
        bool isBoolean() const;
        bool isNumber() const;
        bool isFloat() const;
        bool isInt() const;
        bool IsHash() const;
        bool isBox() const;
        bool isIterator() const;
        bool isError() const;

        int toInt() const;
        float asFloat() const;
        bool asBool() const;
        unsigned asHash() const;
        std::string asString() const;
    };
    struct GarbageCollected
    {
        enum State : char// Tri-color marking (incremental garbage collection)
        {
            GC_White0 = 0,// not used 0
            GC_White1 = 1,// not used 1
            GC_Gray = 2,// to be checked
            GC_Black = 3,// used

            GC_Static = 4,// not part of garbage collection (constants)
        };

        GarbageCollected* next;
        Value::Type type;
        State state;

        GarbageCollected(Value::Type type);
    };

    struct String : public GarbageCollected
    {
        std::string str;

        String();
        String(const std::string& str);
        String(const char* data, unsigned size);
    };

    struct Error : public GarbageCollected
    {
        std::string errorString;

        Error();
        Error(const std::string& str);
        Error(const char* data, unsigned size);
    };

    struct Array : public GarbageCollected
    {
        std::vector<Value> elements;

        Array();
    };

    struct Object : public GarbageCollected
    {
        struct Member
        {
            unsigned hash;
            Value value;

            Member();
            explicit Member(unsigned hash);
            Member(unsigned hash, const Value& value);
            bool operator<(const Member& o) const;
            bool operator==(const Member& o) const;
        };

        std::vector<Member> members;

        Object();
    };

    struct Box : public GarbageCollected
    {
        Value value;

        Box();
    };

    struct Function : public GarbageCollected
    {
        const CodeObject* codeObject;
        std::vector<Box*> freeVariables;
        ExecutionContext* executionContext;

        Function(const CodeObject* codeObject);
        Function(const Function* o);
    };

    struct IteratorImplementation
    {
        virtual ~IteratorImplementation() = default;
        virtual void updateGrayList(std::deque<GarbageCollected*>& grayList, GarbageCollected::State currentWhite)
        {
            (void)grayList;
            (void)currentWhite;
        };

        Value thisObjectUsed;
        Value hasNextFunction;
        Value getNextFunction;
    };

    struct Iterator : public GarbageCollected
    {
        Iterator(IteratorImplementation* implementation);
        ~Iterator();

        // The implementation has virtual methods and thus creates virtual tables.
        // For this reason it cannot be a derived struct of the 'Iterator' struct
        // because it will mess up the expected memory layout.
        IteratorImplementation* implementation;
    };

    struct ArrayIterator : public IteratorImplementation
    {
        Array* array;

        unsigned currentIndex = 0;

        ArrayIterator(Array* array);

        virtual void updateGrayList(std::deque<GarbageCollected*>& grayList, GarbageCollected::State currentWhite) override;
    };

    struct StringIterator : public IteratorImplementation
    {
        String* str;

        unsigned currentIndex = 0;

        StringIterator(String* str);

        virtual void updateGrayList(std::deque<GarbageCollected*>& grayList, GarbageCollected::State currentWhite) override;
    };

    struct ObjectIterator : public IteratorImplementation
    {
        ObjectIterator(const Value& object, const Value& hasNext, const Value& getNext);

        virtual void updateGrayList(std::deque<GarbageCollected*>& grayList, GarbageCollected::State currentWhite) override;
    };

    struct CoroutineIterator : public IteratorImplementation
    {
        CoroutineIterator(Function* coroutine);

        virtual void updateGrayList(std::deque<GarbageCollected*>& grayList, GarbageCollected::State currentWhite) override;
    };

    struct SourceCodeLine
    {
        int line;
        int instructionIndex;
    };

    struct Module
    {
        std::string filename;

        std::vector<Value> globals;
        Value result;

        std::unique_ptr<char[]> bytecode;
    };

    struct CodeObject
    {
        std::vector<Instruction> instructions;
        Module* module;
        int localVariablesCount;
        int namedParametersCount;
        std::vector<int> closureMapping;
        std::vector<SourceCodeLine> instructionLines;

        CodeObject();
        CodeObject(CodeObject&& o) = default;
        CodeObject(Instruction* instructions, unsigned instructionsSize, SourceCodeLine* lines, unsigned linesSize, int localVariablesCount, int namedParametersCount);
    };

    struct StackFrame
    {
        Function* function = nullptr;
        const Instruction* ip = nullptr;
        const Instruction* instructions = nullptr;
        std::vector<Value>* globals = nullptr;
        std::vector<Value> variables;
        Array anonymousParameters;
        Value thisObject;
    };

    struct ExecutionContext
    {
        enum State : char
        {
            CRS_NotStarted = 0,
            CRS_Started = 1,
            CRS_Finished = 2,
        };

        State state = CRS_NotStarted;
        ExecutionContext* parent = nullptr;
        Value lastObject;
        std::deque<StackFrame> stackFrames;// this needs to be a deque, because things keep references to it
        std::vector<Value> stack;
    };

    std::string bytecodeSymbolsToString(const char* bytecode);
    std::string bytecodeConstantsToString(const char* bytecode);

    class FileManager
    {
    public:
        FileManager();
        ~FileManager();

        void resetState();

        void addSearchPath(const std::string& searchPath);
        void clearSearchPaths();
        auto getSearchPaths() const -> const std::vector<std::string>&;

        std::string getExePath() const;

        // The input file name is relative to the current file being executed.
        // The output file name (if successfully resolved!) is relative to the interpreter.
        // If the file has no extension, it is as if you searched for it with ".element"
        std::string pushFileToExecute(const std::string& filename);
        void popFileToExecute();

    protected:
        std::string ResolveFile(std::string filename) const;

    private:
        std::vector<std::string> m_locationofexecfile;
        std::vector<std::string> m_searchpaths;
    };

    class Lexer
    {
        private:
            std::istream* m_instream;
            Logger& m_logger;

            char m_currch;

            Token m_currtoken;

            Location m_location;
            int m_currcolumn;

            std::string m_lastbuf;

            std::string m_lastident;
            std::string m_laststring;
            int m_lastinteger;
            int m_lastargidx;
            float m_lastfloat;
            bool m_lastbool;

            bool m_muststartover;

        public:
            Lexer(Logger& logger);
            Lexer(std::istream& input, Logger& logger);

            void setInputStream(std::istream& input);

            Token getNextToken();
            Token getNextTokenNoLF();
            Token getCurrentToken() const;

            void RewindDueToMissingElse();

            const Location& location() const;

            const std::string& getLastIdent() const;
            const std::string& GetLastString() const;
            int getLastInteger() const;
            int GetLastArgumentIndex() const;
            float getLastFloat() const;
            bool getLastBool() const;

        protected:
            void reset();

            char getNextChar();

            bool HandleCommentOrDivision();
            bool doWord();
            bool doNumber();
            bool HandleSingleChar(const char ch, const Token t);
            bool doColumn();
            bool doString();
            bool HandleSubstractOrArrow();
            bool doLessOrPush();
            bool doGreaterOrPop();
            bool doCharAndEqual(char ch, Token withoutAssign, Token withAssign);
            bool doDollarSign();


    };

    class Parser
    {
        private:
            Logger& m_logger;
            Lexer m_lexer;

        public:
            enum ExpressionType
            {
                ET_PrimaryExpression,
                ET_UnaryOperator,
                ET_BinaryOperator,
                ET_IndexOperator,
                ET_FunctionCall,
                ET_FunctionAssignment,

                ET_Unknown,
            };

            struct Operator
            {
                Token token;
                int precedence;
                ExpressionType type;
                std::shared_ptr<ast::Node> auxNode;
                Location coords;
            };

        public:
            Parser(Logger& logger);

            auto Parse(std::istream& input) -> std::unique_ptr<ast::FunctionNode>;

        protected:
            std::shared_ptr<ast::Node> ParseExpression();

            std::shared_ptr<ast::Node> ParsePrimary();
            std::shared_ptr<ast::Node> ParsePrimitive();
            std::shared_ptr<ast::Node> ParseVarialbe();
            std::shared_ptr<ast::Node> ParseParenthesis();
            std::shared_ptr<ast::Node> ParseIndexOperator();
            std::shared_ptr<ast::Node> ParseBlock();
            std::shared_ptr<ast::Node> ParseFunction();
            std::shared_ptr<ast::Node> ParseArguments();
            std::shared_ptr<ast::Node> ParseArrayOrObject();
            std::shared_ptr<ast::Node> ParseIf();
            std::shared_ptr<ast::Node> ParseWhile();
            std::shared_ptr<ast::Node> ParseFor();
            std::shared_ptr<ast::Node> ParseControlExpression();

            ExpressionType CurrentExpressionType(Token prevToken, Token token) const;

            void FoldOperatorStacks(std::vector<Operator>& operators, std::vector<std::shared_ptr<ast::Node>>& operands) const;

            bool IsExpressionTerminator(Token token) const;
    };



    class SemanticAnalyzer
    {
        public:
            enum ContextType
            {
                CXT_InGlobal,
                CXT_InFunction,
                CXT_InLoop,
                CXT_InObject,
                CXT_InArray,
                CXT_InArguments
            };

            struct BlockScope
            {
                std::map<std::string, std::shared_ptr<ast::VariableNode>> variables;
            };

            struct FunctionScope
            {
                std::shared_ptr<ast::FunctionNode> node;

                std::vector<BlockScope> blocks;
                std::vector<std::string> parameters;
                std::vector<std::string> freeVariables;

                FunctionScope(const std::shared_ptr<ast::FunctionNode>& n);
            };

        private:
            Logger& m_logger;

            std::vector<ContextType> m_context;

            std::shared_ptr<ast::FunctionNode> m_currfuncnode;

            std::vector<FunctionScope> mFunctionScopes;

            std::vector<std::string> m_globalvars;

            std::map<std::string, int> m_natfuncs;

        public:
            SemanticAnalyzer(Logger& logger);

            void Analyze(const std::shared_ptr<ast::FunctionNode>& node);

            void AddNativeFunction(const std::string& name, int index);

            void resetState();



        protected:
            bool analyzeNode(const std::shared_ptr<ast::Node>& node);
            bool analyzeBinaryOperator(const std::shared_ptr<ast::BinaryOperatorNode>& n);

            bool checkAssignable(const std::shared_ptr<ast::Node>& node) const;
            bool isBreakContinueReturn(const std::shared_ptr<ast::Node>& node) const;
            bool isBreakContinue(const std::shared_ptr<ast::Node>& node) const;
            bool isReturn(const std::shared_ptr<ast::Node>& node) const;
            bool isInLoop() const;
            bool isInFunction() const;
            bool isInConstruction() const;

            void ResolveNamesInNodes(std::vector<std::shared_ptr<ast::Node>> nodesToProcess);
            void resolveName(const std::shared_ptr<ast::VariableNode>& vn);
            bool TryToFindNameInTheEnclosingFunctions(const std::shared_ptr<ast::VariableNode>& vn);


    };

    class MemoryManager
    {
        public:
            enum GCStage : char
            {
                GCS_Ready = 0,
                GCS_MarkRoots = 1,
                GCS_Mark = 2,
                GCS_SweepHead = 3,
                GCS_SweepRest = 4,
            };

        public:
            MemoryManager();
            ~MemoryManager();

            void resetState();

            Module& GetDefaultModule();
            Module& GetModuleForFile(const std::string& filename);

            String* NewString();
            String* NewString(const std::string& str);
            String* NewString(const char* str, int size);
            Array* NewArray();
            Object* NewObject();
            Object* NewObject(const Object* other);
            Function* NewFunction(const Function* other);
            Function* NewCoroutine(const Function* other);
            Box* NewBox();
            Box* NewBox(const Value& value);
            Iterator* NewIterator(IteratorImplementation* newIterator);
            Error* NewError(const std::string& errorMessage);

            ExecutionContext* NewRootExecutionContext();
            bool DeleteRootExecutionContext(ExecutionContext* context);

            void GarbageCollect(int steps = std::numeric_limits<int>::max());

            void UpdateGcRelationship(GarbageCollected* parent, const Value& child);

            int GetHeapObjectsCount(Value::Type type) const;


        protected:
            void DeleteHeap();
            void AddToHeap(GarbageCollected* gc);
            void FreeGC(GarbageCollected* gc);
            void MakeGrayIfNeeded(GarbageCollected* gc, int* steps);

            int MarkRoots(int steps);
            int Mark(int steps);
            int SweepHead(int steps);
            int SweepRest(int steps);

        private:
            GarbageCollected* m_heaphead;

            GCStage m_gcstage;
            GarbageCollected::State m_currentwhite;
            GarbageCollected::State m_nextwhite;
            std::deque<GarbageCollected*> m_graylist;
            GarbageCollected* m_prevgc;
            GarbageCollected* m_currgc;

            // memory roots
            Module m_defmodule;
            std::unordered_map<std::string, Module> m_modules;
            std::vector<ExecutionContext*> m_excontexts;

            // statistics
            int m_heapstringscnt;
            int m_heaparrayscnt;
            int m_heapobjectscnt;
            int m_heapfunctionscnt;
            int m_heapboxescnt;
            int m_heapitercnt;
            int m_heaperrorscnt;
    };

    class VirtualMachine
    {
        public:
            VirtualMachine();

            void resetState();

            Value evalStream(std::istream& input);
            Value evalFile(const std::string& filename);

            FileManager& getFileManager();
            MemoryManager& getMemoryManager();

            void setError(const std::string& errorMessage);
            bool hasError() const;
            void clearError();

            void addNative(const std::string& name, Value::NativeFunction function);
            std::string getVersion() const;

            // value manipulation //////////////////////////////////////////////////////
            Iterator* makeIterator(const Value& value);
            unsigned GetHashFromName(const std::string& name);
            bool nameFromHash(unsigned hash, std::string* name);

            Value getMember(const Value& object, const std::string& memberName);
            Value getMember(const Value& object, unsigned memberHash) const;

            void setMember(const Value& object, const std::string& memberName, const Value& value);
            void setMember(const Value& object, unsigned memberHash, const Value& value);

            void pushElement(const Value& array, const Value& value);
            void addElement(const Value& array, int atIndex, const Value& value);

            Value callFunction(const Value& function, const std::vector<Value>& args);

            Value callMemberFunction(const Value& object, const std::string& memberFunctionName, const std::vector<Value>& args);
            Value callMemberFunction(const Value& object, unsigned functionHash, const std::vector<Value>& args);
            Value callMemberFunction(const Value& object, const Value& function, const std::vector<Value>& args);

        protected:
            Value execBytecode(const char* bytecode, Module& forModule);
            int parseBytecode(const char* bytecode, Module& forModule);

            Value commonCallFunction(const Value& thisObject, const Value& function, const std::vector<Value>& args);

            Value runCode();
            void frameRunCode(StackFrame* frame);

            void call(int argumentsCount);
            void callNative(int argumentsCount);

            void arrayPushElement(Array* array, const Value& newValue);
            bool arrayPopElement(Array* array, Value* outValue);
            void arrayLoadElement(const Array* array, int index, Value* outValue);
            void arrayStoreElement(Array* array, int index, const Value& newValue);

            void LoadMemberFromObject(Object* object, unsigned hash, Value* outValue) const;
            void objectStoreMember(Object* object, unsigned hash, const Value& newValue);

            bool doBinaryOperation(int opCode);

            void RegisterStandardUtilities();

            void logStacktraceFrom(const StackFrame* frame);
            void LocationFromFrame(const StackFrame* frame, int* currentLine, std::string* currentFile) const;

        private:
            Logger m_logger;
            Parser m_parser;
            SemanticAnalyzer m_analyzer;
            Compiler m_compiler;
            FileManager m_fileman;
            MemoryManager m_memoryman;

            std::vector<Value> m_constants;// this is the common access point for the 3 deques below
            std::deque<String> mConstantStrings;
            std::deque<Function> m_constfunctions;
            std::deque<CodeObject> m_constcodeobjects;

            std::vector<Value::NativeFunction> m_natfuncs;
            std::unordered_map<unsigned, std::string> m_symnames;

            ExecutionContext* m_execctx;
            std::vector<Value>* m_stack;

            std::string m_errmessage;
    };

    namespace nativefunctions
    {
        struct NamedFunction
        {
            std::string name;
            Value::NativeFunction function;
        };

        const std::vector<NamedFunction>& GetAllFunctions();

        Value natfn_loadelement(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
        Value natfn_addsearchpath(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
        Value natfn_getsearchpaths(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
        Value natfn_clearsearchpaths(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
        Value natfn_type(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
        Value natfn_thiscall(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
        Value natfn_garbagecollect(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
        Value natfn_memorystats(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
        Value natfn_print(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
        Value natfn_toupper(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
        Value natfn_tolower(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
        Value natfn_keys(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
        Value natfn_makeerror(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
        Value natfn_iserror(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
        Value natfn_makecoroutine(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
        Value natfn_makeiterator(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
        Value natfn_iteratorhasnext(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
        Value natfn_iteratorgetnext(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
        Value natfn_range(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
        Value natfn_each(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
        Value natfn_times(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
        Value natfn_count(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
        Value natfn_map(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
        Value natfn_filter(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
        Value natfn_reduce(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
        Value natfn_all(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
        Value natfn_any(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
        Value natfn_min(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
        Value natfn_max(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
        Value natfn_sort(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
        Value Abs(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
        Value natfn_floor(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
        Value natfn_ceil(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
        Value natfn_round(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
        Value natfn_sqrt(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
        Value natfn_sin(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
        Value natfn_cos(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
        Value natfn_tan(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
        Value natfn_chr(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);

    }// namespace nativefunctions

    struct OperatorInfo
    {
        Token token;
        bool isBinary;
        bool isRightAssociative;
        int precedence;
    };

    const OperatorInfo& GetOperatorInfo(const Token t, bool isBinary);

}// namespace element
