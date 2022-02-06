
#include <cmath>
#include <algorithm>
#include <iterator>
#include <fstream>
#include "element.h"

namespace element
{
    VirtualMachine::VirtualMachine():
        m_logger(),
        m_parser(m_logger),
        m_analyzer(m_logger),
        m_compiler(m_logger),
        m_fileman(),
        m_memoryman(),
        m_execctx(nullptr),
        m_stack(nullptr)
    {
        registerBuiltins();
        {
            std::stringstream tmp;
            tmp << "{}";
            evalStream(tmp);
        }
    }

    void VirtualMachine::resetState()
    {
        m_logger.clearMessages();

        m_analyzer.resetState();

        m_compiler.resetState();

        m_fileman.resetState();

        m_memoryman.resetState();

        m_conststrings.clear();
        m_constfunctions.clear();
        m_constcodeobjects.clear();
        m_constants.clear();

        m_natfuncs.clear();
        m_symnames.clear();

        m_execctx = nullptr;
        m_stack = nullptr;

        m_errmessage.clear();

        registerBuiltins();
    }

    void VirtualMachine::addGlobal(const std::string& name, const Value& v)
    {
        std::cerr << "m_execctx->stackFrames.size()=" << m_execctx->stackFrames.size() << std::endl;
        m_analyzer.addGlobal(name, v);
    }

    Value VirtualMachine::evalStream(std::istream& input)
    {
        Value result;

        auto node = m_parser.parse(input);
        std::shared_ptr<ast::FunctionNode> shptr = std::move(node);

        if(!m_logger.hasMessages())
        {
            m_analyzer.Analyze(shptr);

            if(!m_logger.hasMessages())
            {
                std::unique_ptr<char[]> bytecode = m_compiler.compile(shptr);

                if(!m_logger.hasMessages())
                {
                    result = execBytecode(bytecode.get(), m_memoryman.getDefaultModule());
                }
            }
        }

        if(m_logger.hasMessages())
        {
            result = m_memoryman.makeError(m_logger.getCombined());
            m_logger.clearMessages();
        }

        clearError();

        return result;
    }

    Value VirtualMachine::evalFile(const std::string& filename)
    {
        std::string fileToExecute = m_fileman.pushFileToExecute(filename);

        if(fileToExecute.empty())
            return m_memoryman.makeError("file-not-found");

        Module& module = m_memoryman.getModuleForFile(fileToExecute);

        if(module.bytecode.get() != nullptr)
        {
            m_fileman.popFileToExecute();
            return module.result;
        }

        Value result;
        std::unique_ptr<char[]> bytecode;

        std::ifstream input(fileToExecute);

        std::shared_ptr<ast::FunctionNode> node = std::move(m_parser.parse(input));

        if(!m_logger.hasMessages())
        {
            m_analyzer.Analyze(node);

            if(!m_logger.hasMessages())
            {
                bytecode = m_compiler.compile(node);

                if(!m_logger.hasMessages())
                {
                    result = execBytecode(bytecode.get(), module);
                }
            }
        }

        if(m_logger.hasMessages())
        {
            result = m_memoryman.makeError(m_logger.getCombined());
            m_logger.clearMessages();
        }

        clearError();

        module.result = result;
        module.bytecode = std::move(bytecode);

        m_fileman.popFileToExecute();

        return result;
    }

    FileManager& VirtualMachine::getFileManager()
    {
        return m_fileman;
    }

    MemoryManager& VirtualMachine::getMemoryManager()
    {
        return m_memoryman;
    }

    void VirtualMachine::setError(const std::string& errorMessage)
    {
        m_errmessage = errorMessage;
    }

    bool VirtualMachine::hasError() const
    {
        return !m_errmessage.empty();
    }

    void VirtualMachine::clearError()
    {
        m_errmessage.clear();
    }

    void VirtualMachine::addNative(const std::string& name, Value::NativeFunction function)
    {
        int index = int(m_natfuncs.size());

        m_natfuncs.push_back(function);

        m_analyzer.addNative(name, index);
    }

    std::string VirtualMachine::getVersion() const
    {
        return "element interpreter version 0.0.5";
    }

    Iterator* VirtualMachine::makeIterator(const Value& value)
    {
        switch(value.type)
        {
            case Value::VT_Iterator:
                return value.iterator;

            case Value::VT_Array:
                return m_memoryman.makeIterator(new ArrayIterator(value.array));

            case Value::VT_String:
                return m_memoryman.makeIterator(new StringIterator(value.string));

            case Value::VT_Object:
            {
                Value hasNextMemberFunction;
                loadMemberFromObject(value.object, Symbol::HasNextHash, &hasNextMemberFunction);

                Value getNextMemberFunction;
                loadMemberFromObject(value.object, Symbol::GetNextHash, &getNextMemberFunction);

                if(hasNextMemberFunction.isNil() || getNextMemberFunction.isNil())
                    return nullptr;

                return m_memoryman.makeIterator(new ObjectIterator(value, hasNextMemberFunction, getNextMemberFunction));
            }

            case Value::VT_Function:
                if(value.function->executionContext)// only coroutines
                    return m_memoryman.makeIterator(new CoroutineIterator(value.function));

            default:
                return nullptr;
        }
    }

    unsigned VirtualMachine::hashFromName(const std::string& name)
    {
        unsigned hash = Symbol::Hash(name);
        unsigned step = Symbol::HashStep(hash);

        if(name == "proto")
            hash = Symbol::ProtoHash;

        auto it = m_symnames.find(hash);

        while(it != m_symnames.end() &&// found the hash but
              it->second != name)// didn't find the name
        {
            hash += step;
            it = m_symnames.find(hash);
        }

        if(it == m_symnames.end())
            m_symnames[hash] = name;

        return hash;
    }

    bool VirtualMachine::nameFromHash(unsigned hash, std::string* name)
    {
        auto it = m_symnames.find(hash);

        if(it != m_symnames.end())
        {
            if(name)
                *name = it->second;
            return true;
        }

        return false;
    }

    Value VirtualMachine::getMember(const Value& object, const std::string& memberName)
    {
        Value result;
        loadMemberFromObject(object.object, hashFromName(memberName), &result);
        return result;
    }

    Value VirtualMachine::getMember(const Value& object, unsigned memberHash) const
    {
        Value result;
        loadMemberFromObject(object.object, memberHash, &result);
        return result;
    }

    void VirtualMachine::setMember(const Value& object, const std::string& memberName, const Value& value)
    {
        objectStoreMember(object.object, hashFromName(memberName), value);
    }

    void VirtualMachine::setMember(const Value& object, unsigned memberHash, const Value& value)
    {
        objectStoreMember(object.object, memberHash, value);
    }

    void VirtualMachine::pushElement(const Value& array, const Value& value)
    {
        arrayPushElement(array.array, value);
    }

    void VirtualMachine::addElement(const Value& array, int atIndex, const Value& value)
    {
        arrayStoreElement(array.array, atIndex, value);
    }

    Value VirtualMachine::callFunction(const Value& function, const std::vector<Value>& args)
    {
        return commonCallFunction(Value(), function, args);
    }

    Value VirtualMachine::callMemberFunction(const Value& object, const std::string& memberFunctionName, const std::vector<Value>& args)
    {
        Value memberFunction = getMember(object, memberFunctionName);

        return commonCallFunction(object, memberFunction, args);
    }

    Value VirtualMachine::callMemberFunction(const Value& object, unsigned functionHash, const std::vector<Value>& args)
    {
        Value memberFunction = getMember(object, functionHash);

        return commonCallFunction(object, memberFunction, args);
    }

    Value VirtualMachine::callMemberFunction(const Value& object, const Value& function, const std::vector<Value>& args)
    {
        return commonCallFunction(object, function, args);
    }

    Value VirtualMachine::execBytecode(const char* bytecode, Module& forModule)
    {
        int firstFunctionConstantIndex = parseBytecode(bytecode, forModule);

        Function* main = m_constants[firstFunctionConstantIndex].function;

        ExecutionContext dummyContext;

        if(!m_execctx)// first run
        {
            m_execctx = &dummyContext;
            m_stack = &dummyContext.stack;
        }

        return commonCallFunction(Value(), main, {});
    }

    int VirtualMachine::parseBytecode(const char* bytecode, Module& forModule)
    {
        int firstFunctionConstantIndex = -1;

        unsigned* p = (unsigned*)bytecode;

        unsigned symbolsSize = *p;
        ++p;
        unsigned symbolsCount = *p;
        ++p;
        unsigned symbolsOffset = *p;
        ++p;

        char* symbolIt = (char*)p;
        char* symbolsEnd = symbolIt + symbolsSize;

        p = (unsigned*)symbolsEnd;

        unsigned constantsSize = *p;
        ++p;
        unsigned constantsCount = *p;
        ++p;
        unsigned constantsOffset = *p;
        ++p;

        char* constantIt = (char*)p;
        char* constantsEnd = constantIt + constantsSize;

        m_symnames.reserve(symbolsCount + symbolsOffset);

        Symbol currentSymbol;

        while(symbolIt < symbolsEnd)
        {
            symbolIt = currentSymbol.readSymbol(symbolIt);

            m_symnames[currentSymbol.hash] = currentSymbol.name;
        }

        m_constants.reserve(constantsCount + constantsOffset);

        Constant currentConstant;

        while(constantIt < constantsEnd)
        {
            constantIt = currentConstant.readConst(constantIt);

            switch(currentConstant.type)
            {
                case Constant::CT_Nil:
                    m_constants.emplace_back();
                    break;

                case Constant::CT_Integer:
                    m_constants.emplace_back(currentConstant.integer);
                    break;

                case Constant::CT_Float:
                    m_constants.emplace_back(currentConstant.floatingPoint);
                    break;

                case Constant::CT_Bool:
                    m_constants.emplace_back(currentConstant.boolean);
                    break;

                case Constant::CT_String:
                {
                    m_conststrings.emplace_back(std::move(*(currentConstant.string)));

                    m_conststrings.back().state = GarbageCollected::GC_Static;

                    m_constants.emplace_back(&m_conststrings.back());
                    break;
                }

                case Constant::CT_CodeObject:
                {
                    m_constcodeobjects.emplace_back(std::move(*currentConstant.codeObject));

                    CodeObject* codeObject = &m_constcodeobjects.back();

                    codeObject->module = &forModule;

                    m_constfunctions.emplace_back(codeObject);
                    m_constfunctions.back().state = GarbageCollected::GC_Static;

                    if(firstFunctionConstantIndex == -1)
                        firstFunctionConstantIndex = int(m_constants.size());

                    m_constants.emplace_back(&m_constfunctions.back());
                    break;
                }

                default:
                    setError("Unknown constant type");
                    return firstFunctionConstantIndex;
            }
        }

        return firstFunctionConstantIndex;
    }

    Value VirtualMachine::commonCallFunction(const Value& thisObject, const Value& function, const std::vector<Value>& args)
    {
        if(function.type == Value::VT_NativeFunction)
        {
            return function.nativeFunction(*this, thisObject, args);
        }
        else// normal function
        {
            // switch context
            ExecutionContext* oldContext = m_execctx;
            m_execctx = m_memoryman.makeRootExecutionContext();
            m_stack = &m_execctx->stack;

            m_stack->reserve(args.size());
            std::copy(args.begin(), args.end(), std::back_inserter(*m_stack));
            m_stack->push_back(function);

            m_execctx->lastObject = thisObject;

            call(int(args.size()));

            Value result = runCode();

            m_memoryman.deleteRootExecutionContext(m_execctx);
            m_execctx = oldContext;
            m_stack = &oldContext->stack;

            return result;
        }
    }

    Value VirtualMachine::runCode()
    {
        StackFrame* frame = nullptr;

        // IMPORTANT: the current 'm_execctx' may change during 'frameRunCode'
        while(!m_execctx->stackFrames.empty())
        {
            frame = &m_execctx->stackFrames.back();

            frameRunCode(frame);

            if(hasError())
            {
                logStacktraceFrom(frame);

                return m_memoryman.makeError("runtime-error");
            }
        }

        // result if any
        Value result;

        if(!m_stack->empty())
        {
            result = m_stack->back();
            m_stack->pop_back();
        }

        return result;
    }

    void VirtualMachine::frameRunCode(StackFrame* frame)
    {
        while(true)
        {
            switch(frame->ip->opCode)
            {
                case OC_Pop:// pop TOS
                    m_stack->pop_back();
                    ++frame->ip;
                    break;

                case OC_PopN:// pop A values from the stack
                    for(int i = frame->ip->A; i > 0; --i)
                        m_stack->pop_back();
                    ++frame->ip;
                    break;

                case OC_Rotate2:// swap TOS and TOS1
                {
                    int tos = int(m_stack->size()) - 1;
                    Value value = m_stack->at(tos - 1);
                    m_stack->at(tos - 1) = m_stack->at(tos);
                    m_stack->at(tos) = value;
                    ++frame->ip;
                    break;
                }

                case OC_MoveToTOS2:// copy TOS over TOS2 and pop TOS
                {
                    int tos = int(m_stack->size()) - 1;
                    m_stack->at(tos - 2) = m_stack->at(tos);
                    m_stack->pop_back();
                    ++frame->ip;
                    break;
                }

                case OC_Duplicate:// make a copy of TOS and push it to the stack
                {
                    m_stack->push_back(m_stack->back());
                    ++frame->ip;
                    break;
                }

                case OC_Unpack:// A is the number of values to be produced from the TOS value
                {
                    Value valueToUnpack = m_stack->back();
                    m_stack->pop_back();

                    int expectedSize = frame->ip->A;

                    if(valueToUnpack.isArray())
                    {
                        const std::vector<Value>& elements = valueToUnpack.array->elements;
                        int arraySize = int(elements.size());

                        if(arraySize >= expectedSize)
                        {
                            for(int i = expectedSize - 1; i >= 0; --i)
                                m_stack->push_back(elements[i]);
                        }
                        else// arraySize < expectedSize
                        {
                            for(int i = expectedSize - arraySize; i > 0; --i)
                                m_stack->emplace_back();// nil

                            for(int i = arraySize - 1; i >= 0; --i)
                                m_stack->push_back(elements[i]);
                        }
                    }
                    else
                    {
                        for(int i = expectedSize - 1; i > 0; --i)
                            m_stack->emplace_back();// nil

                        m_stack->push_back(valueToUnpack);
                    }

                    ++frame->ip;
                    break;
                }

                case OC_LoadConstant:// A is the index in the constants vector
                    m_stack->push_back(m_constants[frame->ip->A]);
                    ++frame->ip;
                    break;

                case OC_LoadLocal:// A is the index in the function scope
                    m_stack->push_back(frame->variables[frame->ip->A]);
                    ++frame->ip;
                    break;

                case OC_LoadGlobal:// A is the index in the global scope
                {
                    unsigned index = unsigned(frame->ip->A);
                    m_stack->push_back(index < frame->globals->size() ? frame->globals->at(index) : Value());
                    ++frame->ip;
                    break;
                }

                case OC_LoadNative:// A is the index in the native functions
                    m_stack->push_back(m_natfuncs[frame->ip->A]);
                    ++frame->ip;
                    break;

                case OC_LoadArgument:// A is the index in the arguments array
                    if(int(frame->anonymousParameters.elements.size()) > frame->ip->A)
                        m_stack->push_back(frame->anonymousParameters.elements[frame->ip->A]);
                    else
                        m_stack->emplace_back();
                    ++frame->ip;
                    break;

                case OC_LoadArgsArray:// load the current frame's arguments array
                    m_stack->emplace_back();
                    m_stack->back().type = Value::VT_Array;
                    m_stack->back().array = &frame->anonymousParameters;
                    ++frame->ip;
                    break;

                case OC_LoadThis:// load the current frame's this object
                    m_stack->emplace_back(frame->thisObject);
                    ++frame->ip;
                    break;

                case OC_StoreLocal:// A is the index in the function scope
                    frame->variables[frame->ip->A] = m_stack->back();
                    ++frame->ip;
                    break;

                case OC_StoreGlobal:// A is the index in the global scope
                {
                    unsigned index = unsigned(frame->ip->A);
                    if(index >= frame->globals->size())
                        frame->globals->resize(index + 1);
                    frame->globals->at(index) = m_stack->back();
                    ++frame->ip;
                    break;
                }

                case OC_PopStoreLocal:// A is the index in the function scope
                    frame->variables[frame->ip->A] = m_stack->back();
                    m_stack->pop_back();
                    ++frame->ip;
                    break;

                case OC_PopStoreGlobal:// A is the index in the global scope
                {
                    unsigned index = unsigned(frame->ip->A);
                    if(index >= frame->globals->size())
                        frame->globals->resize(index + 1);
                    frame->globals->at(index) = m_stack->back();
                    m_stack->pop_back();
                    ++frame->ip;
                    break;
                }

                case OC_MakeArray:// A is number of elements to be taken from the stack
                {
                    int elementsCount = frame->ip->A;

                    Array* array = m_memoryman.makeArray();
                    array->elements.resize(elementsCount);

                    for(int i = elementsCount - 1; i >= 0; --i)
                    {
                        array->elements[i] = m_stack->back();
                        m_stack->pop_back();
                    }

                    m_stack->emplace_back(array);

                    ++frame->ip;
                    break;
                }

                case OC_LoadElement:// TOS is the index in the TOS1 array or object
                {
                    Value index = m_stack->back();
                    m_stack->pop_back();

                    Value container = m_stack->back();
                    m_stack->pop_back();

                    if(container.isArray())
                    {
                        if(!index.isInt())
                        {
                            setError("Array index must be an integer");
                            return;
                        }

                        m_stack->emplace_back();// the value to get
                        arrayLoadElement(container.array, index.toInt(), &m_stack->back());

                        if(hasError())
                            return;
                    }
                    else if(container.isObject())
                    {
                        if(!index.isString())
                        {
                            setError("Object index must be a string");
                            return;
                        }

                        m_stack->emplace_back();// the value to get
                        loadMemberFromObject(container.object, hashFromName(index.asString()), &m_stack->back());
                    }
                    else// error
                    {
                        setError("The indexing operator only operates on arrays and objects");
                        return;
                    }

                    ++frame->ip;
                    break;
                }

                case OC_StoreElement:// TOS index, TOS1 array or object, TOS2 new value
                {
                    Value index = m_stack->back();
                    m_stack->pop_back();

                    Value container = m_stack->back();
                    m_stack->pop_back();

                    if(container.isArray())
                    {
                        if(!index.isInt())
                        {
                            setError("Array index must be an integer");
                            return;
                        }

                        arrayStoreElement(container.array, index.toInt(), m_stack->back());

                        if(hasError())
                            return;
                    }
                    else if(container.isObject())
                    {
                        if(!index.isString())
                        {
                            setError("Object index must be a string");
                            return;
                        }

                        objectStoreMember(container.object, hashFromName(index.asString()), m_stack->back());
                    }
                    else// error
                    {
                        setError("The indexing operator only operates on arrays and objects");
                        return;
                    }

                    ++frame->ip;
                    break;
                }

                case OC_PopStoreElement:// TOS index, TOS1 array or object, TOS2 new value
                {
                    Value index = m_stack->back();
                    m_stack->pop_back();

                    Value container = m_stack->back();
                    m_stack->pop_back();

                    if(container.isArray())
                    {
                        if(!index.isInt())
                        {
                            setError("Array index must be an integer");
                            return;
                        }

                        arrayStoreElement(container.array, index.toInt(), m_stack->back());

                        if(hasError())
                            return;

                        m_stack->pop_back();
                    }
                    else if(container.isObject())
                    {
                        if(!index.isString())
                        {
                            setError("Object index must be a string");
                            return;
                        }

                        objectStoreMember(container.object, hashFromName(index.asString()), m_stack->back());
                        m_stack->pop_back();
                    }
                    else// error
                    {
                        setError("The indexing operator only operates on arrays and objects");
                        return;
                    }

                    ++frame->ip;
                    break;
                }

                case OC_ArrayPushBack:
                {
                    Value newValue = m_stack->back();
                    m_stack->pop_back();

                    if(m_stack->back().isArray())
                    {
                        arrayPushElement(m_stack->back().array, newValue);
                        m_stack->pop_back();
                        m_stack->push_back(newValue);
                    }
                    else
                    {
                        setError("Invalid arguments for operator <<");
                        return;
                    }

                    ++frame->ip;
                    break;
                }

                case OC_ArrayPopBack:
                {
                    if(m_stack->back().isArray())
                    {
                        Array* array = m_stack->back().array;
                        m_stack->pop_back();

                        Value popped;
                        if(arrayPopElement(array, &popped))
                            m_stack->push_back(popped);
                        else
                            m_stack->push_back(m_memoryman.makeError("empty-array"));
                    }
                    else
                    {
                        setError("Invalid arguments for operator >>");
                        return;
                    }

                    ++frame->ip;
                    break;
                }

                case OC_MakeObject:// A is number of key-value pairs to be taken from the stack
                {
                    int membersCount = frame->ip->A;

                    Object* object = m_memoryman.makeObject();
                    object->members.resize(membersCount);

                    for(int i = membersCount - 1; i >= 0; --i)
                    {
                        Object::Member& member = object->members[i];

                        member.value = m_stack->back();
                        m_stack->pop_back();

                        member.hash = m_stack->back().asHash();
                        m_stack->pop_back();
                    }

                    std::sort(object->members.begin(), object->members.end());

                    m_stack->emplace_back(object);

                    ++frame->ip;
                    break;
                }

                case OC_MakeEmptyObject:// make an object with just the proto member value
                {
                    Object* object = m_memoryman.makeObject();
                    object->members.resize(1);

                    object->members[0].hash = Symbol::ProtoHash;

                    m_stack->emplace_back(object);

                    ++frame->ip;
                    break;
                }

                case OC_LoadHash:// H is the hash to load on the stack
                    m_stack->emplace_back(frame->ip->H);
                    ++frame->ip;
                    break;

                case OC_LoadMember:// TOS is the member hash in the TOS1 object
                {
                    unsigned hash = m_stack->back().asHash();
                    m_stack->pop_back();

                    if(!m_stack->back().isObject())
                    {
                        setError("Attempt to access a member of a non-object value");
                        return;
                    }

                    m_execctx->lastObject = m_stack->back();
                    m_stack->pop_back();
                    m_stack->emplace_back();// the value to get
                    loadMemberFromObject(m_execctx->lastObject.object, hash, &m_stack->back());
                    ++frame->ip;
                    break;
                }

                case OC_StoreMember:// TOS member hash, TOS1 object, TOS2 new value
                {
                    unsigned hash = m_stack->back().asHash();
                    m_stack->pop_back();

                    if(!m_stack->back().isObject())
                    {
                        setError("Attempt to access a member of a non-object value");
                        return;
                    }

                    Object* object = m_stack->back().object;
                    m_stack->pop_back();
                    objectStoreMember(object, hash, m_stack->back());
                    ++frame->ip;
                    break;
                }

                case OC_PopStoreMember:// TOS member hash, TOS1 object, TOS2 new value
                {
                    unsigned hash = m_stack->back().asHash();
                    m_stack->pop_back();

                    if(!m_stack->back().isObject())
                    {
                        setError("Attempt to access a member of a non-object value");
                        return;
                    }

                    Object* object = m_stack->back().object;
                    m_stack->pop_back();
                    objectStoreMember(object, hash, m_stack->back());
                    m_stack->pop_back();
                    ++frame->ip;
                    break;
                }

                case OC_MakeIterator:// make an iterator object from TOS and replace it at TOS
                {
                    Iterator* iterator = makeIterator(m_stack->back());

                    if(iterator)
                    {
                        m_stack->pop_back();
                        m_stack->emplace_back(iterator);
                        ++frame->ip;
                        break;
                    }
                    else// error
                    {
                        if(m_stack->back().type == Value::VT_Function && m_stack->back().function->executionContext == nullptr)
                        {
                            setError("Cannot iterate a function. Only coroutine instances are iterable.");
                        }
                        else
                        {
                            setError("Value not iterable.");
                        }
                        return;
                    }
                }

                case OC_IteratorHasNext:// call 'has_next' from the TOS object
                {
                    if(m_stack->back().isIterator())
                    {
                        IteratorImplementation* ii = m_stack->back().iterator->implementation;

                        m_execctx->lastObject = ii->thisObjectUsed;

                        m_stack->push_back(ii->hasNextFunction);

                        if(m_stack->back().type == Value::VT_NativeFunction)
                        {
                            callNative(0);

                            if(hasError())
                                return;

                            ++frame->ip;
                        }
                        else// normal function
                        {
                            call(0);

                            ++frame->ip;
                            return;
                        }
                    }
                    else
                    {
                        setError("Value is not an iterator");
                        return;
                    }
                    break;
                }

                case OC_IteratorGetNext:// call 'get_next' from the TOS object
                {
                    if(m_stack->back().isIterator())
                    {
                        IteratorImplementation* ii = m_stack->back().iterator->implementation;

                        m_execctx->lastObject = ii->thisObjectUsed;

                        m_stack->push_back(ii->getNextFunction);

                        if(m_stack->back().type == Value::VT_NativeFunction)
                        {
                            callNative(0);

                            if(hasError())
                                return;

                            ++frame->ip;
                        }
                        else// normal function
                        {
                            call(0);

                            ++frame->ip;
                            return;
                        }
                    }
                    else
                    {
                        setError("Value is not an iterator");
                        return;
                    }
                    break;
                }

                case OC_MakeBox:// A is the index of the box that needs to be created
                {
                    Value& variable = frame->variables[frame->ip->A];
                    variable = m_memoryman.makeBox(variable);
                    ++frame->ip;
                    break;
                }

                case OC_LoadFromBox:// load the value stored in the box at index A
                    m_stack->emplace_back(frame->variables[frame->ip->A].box->value);
                    ++frame->ip;
                    break;

                case OC_StoreToBox:// A is the index of the box that holds the value
                {
                    Box* box = frame->variables[frame->ip->A].box;
                    Value& newValue = m_stack->back();

                    box->value = newValue;

                    m_memoryman.updateGCRelationship(box, newValue);

                    ++frame->ip;
                    break;
                }

                case OC_PopStoreToBox:// A is the index of the box that holds the value
                {
                    Box* box = frame->variables[frame->ip->A].box;
                    Value& newValue = m_stack->back();

                    box->value = newValue;

                    m_memoryman.updateGCRelationship(box, newValue);

                    m_stack->pop_back();
                    ++frame->ip;
                    break;
                }

                case OC_MakeClosure:// Create a closure from the function object at TOS and replace it
                {
                    Function* newFunction = m_memoryman.makeFunction(m_stack->back().function);

                    const std::vector<int>& closureMapping = newFunction->codeObject->closureMapping;

                    newFunction->freeVariables.reserve(closureMapping.size());

                    for(int indexToBox : closureMapping)
                    {
                        if(indexToBox >= 0)
                            newFunction->freeVariables.push_back(frame->variables[indexToBox].box);
                        else// from a free variable
                            newFunction->freeVariables.push_back(frame->function->freeVariables[-indexToBox - 1]);
                    }

                    m_stack->back() = Value(newFunction);

                    ++frame->ip;
                    break;
                }

                case OC_LoadFromClosure:// load the value of the free variable inside the closure at index A
                    m_stack->emplace_back(frame->function->freeVariables[frame->ip->A]->value);
                    ++frame->ip;
                    break;

                case OC_StoreToClosure:// A is the index of the free variable inside the closure
                {
                    Box* box = frame->function->freeVariables[frame->ip->A];
                    Value& newValue = m_stack->back();

                    box->value = newValue;

                    m_memoryman.updateGCRelationship(box, newValue);

                    ++frame->ip;
                    break;
                }

                case OC_PopStoreToClosure:// A is the index of the free variable inside the closure
                {
                    Box* box = frame->function->freeVariables[frame->ip->A];
                    Value& newValue = m_stack->back();

                    box->value = newValue;

                    m_memoryman.updateGCRelationship(box, newValue);

                    m_stack->pop_back();
                    ++frame->ip;
                    break;
                }

                case OC_Jump:// jump to A
                    frame->ip = &frame->instructions[frame->ip->A];
                    break;

                case OC_JumpIfFalse:// jump to A, if TOS is false
                    if(m_stack->back().asBool())
                        ++frame->ip;
                    else
                        frame->ip = &frame->instructions[frame->ip->A];
                    break;

                case OC_PopJumpIfFalse:// jump to A, if TOS is false, pop TOS either way
                    if(m_stack->back().asBool())
                        ++frame->ip;
                    else
                        frame->ip = &frame->instructions[frame->ip->A];
                    m_stack->pop_back();
                    break;

                case OC_JumpIfFalseOrPop:// jump to A, if TOS is false, otherwise pop TOS (and-op)
                    if(m_stack->back().asBool())
                    {
                        m_stack->pop_back();
                        ++frame->ip;
                    }
                    else
                    {
                        frame->ip = &frame->instructions[frame->ip->A];
                    }
                    break;

                case OC_JumpIfTrueOrPop:// jump to A, if TOS is true, otherwise pop TOS (or-op)
                    if(m_stack->back().asBool())
                    {
                        frame->ip = &frame->instructions[frame->ip->A];
                    }
                    else
                    {
                        m_stack->pop_back();
                        ++frame->ip;
                    }
                    break;

                case OC_FunctionCall:// function to call and arguments are on stack, A is arguments count
                    if(!m_stack->back().isFunction())
                    {
                        setError("Attempt to call a non-function value");
                        return;
                    }

                    if(m_stack->back().type == Value::VT_NativeFunction)
                    {
                        callNative(frame->ip->A);

                        if(hasError())
                            return;

                        ++frame->ip;
                    }
                    else// normal function
                    {
                        call(frame->ip->A);

                        ++frame->ip;
                        return;
                    }
                    break;

                case OC_Yield:// yield the value from TOS to the parent execution context
                {
                    if(!m_execctx->parent)
                    {
                        setError("Attempt to yield while not in a coroutine");
                        return;
                    }

                    Value yieldValue = m_stack->back();
                    m_stack->pop_back();

                    // switch context
                    m_execctx = m_execctx->parent;
                    m_stack = &m_execctx->stack;

                    m_stack->push_back(yieldValue);

                    ++frame->ip;
                    return;
                }

                case OC_EndFunction:// end function sentinel
                    m_execctx->stackFrames.pop_back();

                    if(m_execctx->stackFrames.empty())
                    {
                        m_execctx->state = ExecutionContext::CRS_Finished;

                        if(m_execctx->parent)
                        {
                            Value yieldValue = m_stack->back();
                            m_stack->pop_back();

                            // switch context
                            m_execctx = m_execctx->parent;
                            m_stack = &m_execctx->stack;

                            m_stack->push_back(yieldValue);
                        }
                    }
                    return;

                case OC_Add:
                case OC_Subtract:
                case OC_Multiply:
                case OC_Divide:
                case OC_Power:
                case OC_Modulo:
                case OC_Concatenate:
                case OC_Xor:

                case OC_Equal:
                case OC_NotEqual:
                case OC_Less:
                case OC_Greater:
                case OC_LessEqual:
                case OC_GreaterEqual:
                    if(!doBinaryOperation(frame->ip->opCode))
                        return;

                    ++frame->ip;
                    break;

                case OC_UnaryPlus:
                    if(!m_stack->back().isNumber())
                    {
                        setError("Unary plus used on a value that is not an integer or float");
                        return;
                    }

                    ++frame->ip;// do nothing (:
                    break;

                case OC_UnaryMinus:
                    if(m_stack->back().isInt())
                    {
                        int i = m_stack->back().toInt();
                        m_stack->pop_back();
                        m_stack->emplace_back(-i);
                    }
                    else if(m_stack->back().isFloat())
                    {
                        float f = m_stack->back().asFloat();
                        m_stack->pop_back();
                        m_stack->emplace_back(-f);
                    }
                    else
                    {
                        setError("Unary minus used on a value that is not an integer or float");
                        return;
                    }

                    ++frame->ip;
                    break;

                case OC_UnaryNot:
                {
                    bool b = m_stack->back().asBool();// anything can be turned into a bool
                    m_stack->pop_back();
                    m_stack->emplace_back(!b);
                    ++frame->ip;
                    break;
                }

                case OC_UnaryConcatenate:
                {
                    std::string str = m_stack->back().asString();// anything can be turned into a string
                    m_stack->pop_back();
                    m_stack->emplace_back(m_memoryman.makeString(str));
                    ++frame->ip;
                    break;
                }

                case OC_UnarySizeOf:
                {
                    const Value& value = m_stack->back();
                    int size = 0;

                    if(value.isArray())
                        size = int(value.array->elements.size());
                    else if(value.isObject())
                        size = int(value.object->members.size());
                    else if(value.isString())
                        size = int(value.string->str.size());
                    else
                    {
                        setError("Attempt to get the size of a value that is not an array, object or string");
                        return;
                    }

                    m_stack->pop_back();
                    m_stack->emplace_back(size);

                    ++frame->ip;
                    break;
                }

                default:
                    setError("Invalid OpCode!");
                    return;
            }
        }
    }

    void VirtualMachine::call(int argumentsCount)
    {
        Function* function = m_stack->back().function;
        m_stack->pop_back();

        std::vector<Value>* sourceStack = m_stack;

        if(function->executionContext)
        {
            function->executionContext->parent = m_execctx;

            if(function->executionContext->state == ExecutionContext::CRS_Started)
            {
                Value valueToSend;

                if(argumentsCount == 1)
                {
                    valueToSend = m_stack->back();
                    m_stack->pop_back();
                }
                else if(argumentsCount > 1)// we need to pack them into an array
                {
                    Array* array = m_memoryman.makeArray();
                    array->elements.resize(argumentsCount);

                    std::copy(m_stack->end() - argumentsCount, m_stack->end(), array->elements.begin());
                    m_stack->resize(m_stack->size() - argumentsCount);

                    valueToSend = Value(array);
                }

                // switch context
                m_execctx = function->executionContext;
                m_stack = &m_execctx->stack;

                m_stack->push_back(valueToSend);
                return;
            }
            if(function->executionContext->state == ExecutionContext::CRS_Finished)
            {
                m_stack->resize(m_stack->size() - argumentsCount);
                m_stack->push_back(m_memoryman.makeError("dead-coroutine"));
                return;
            }
            if(function->executionContext->state == ExecutionContext::CRS_NotStarted)
            {
                // we will extract the arguments from the stack of the old context
                sourceStack = m_stack;

                // switch context
                m_execctx = function->executionContext;
                m_execctx->state = ExecutionContext::CRS_Started;
                m_stack = &m_execctx->stack;
            }
        }

        const CodeObject* codeObject = function->codeObject;

        // create a new stack frame ////////////////////////////////////////////////
        m_execctx->stackFrames.emplace_back();
        StackFrame* newFrame = &m_execctx->stackFrames.back();

        newFrame->function = function;
        newFrame->instructions = codeObject->instructions.data();
        newFrame->ip = newFrame->instructions;
        newFrame->thisObject = m_execctx->lastObject;
        newFrame->globals = &codeObject->module->globals;

        newFrame->variables.resize(codeObject->localVariablesCount);

        // bind parameters to local variables //////////////////////////////////////
        int anonymousCount = argumentsCount - codeObject->namedParametersCount;

        if(anonymousCount <= 0)
        {
            std::copy(sourceStack->end() - argumentsCount, sourceStack->end(), newFrame->variables.begin());
        }
        else// we have some anonymous arguments
        {
            newFrame->anonymousParameters.elements.resize(anonymousCount);

            std::copy(sourceStack->end() - argumentsCount, sourceStack->end() - anonymousCount, newFrame->variables.begin());
            std::copy(sourceStack->end() - anonymousCount, sourceStack->end(), newFrame->anonymousParameters.elements.begin());
        }

        sourceStack->resize(sourceStack->size() - argumentsCount);
    }

    void VirtualMachine::callNative(int argumentsCount)
    {
        Value::NativeFunction function = m_stack->back().nativeFunction;
        m_stack->pop_back();

        std::vector<Value> arguments;
        arguments.resize(argumentsCount);

        for(int i = argumentsCount - 1; i >= 0; --i)
        {
            arguments[i] = m_stack->back();
            m_stack->pop_back();
        }

        Value result = function(*this, m_execctx->lastObject, arguments);

        m_stack->push_back(result);
    }

    void VirtualMachine::arrayPushElement(Array* array, const Value& newValue)
    {
        array->elements.push_back(newValue);

        m_memoryman.updateGCRelationship(array, newValue);
    }

    bool VirtualMachine::arrayPopElement(Array* array, Value* outValue)
    {
        if(!array->elements.empty())
        {
            *outValue = array->elements.back();
            array->elements.pop_back();
            return true;
        }

        return false;
    }

    void VirtualMachine::arrayLoadElement(const Array* array, int index, Value* outValue)
    {
        int size = int(array->elements.size());

        if(index < 0)
            index += size;

        if(index < 0 || index >= size)
        {
            setError("Array index out of range");
            return;
        }

        *outValue = array->elements[index];
    }

    void VirtualMachine::arrayStoreElement(Array* array, int index, const Value& newValue)
    {
        int size = int(array->elements.size());

        if(index < 0)
            index += size;

        if(index < 0 || index >= size)
        {
            setError("Array index out of range");
            return;
        }

        array->elements[index] = newValue;

        m_memoryman.updateGCRelationship(array, newValue);
    }

    void VirtualMachine::loadMemberFromObject(Object* object, unsigned hash, Value* outValue) const
    {
        Object::Member member(hash);

        auto it = std::lower_bound(object->members.begin(), object->members.end(), member);

        if(it == object->members.end() || it->hash != hash)
        {
            const Value* proto = &object->members[0].value;

            while(proto->type == Value::VT_Object &&// it has a proto object
                  proto->object != object)// and it is not the first object
            {
                std::vector<Object::Member>& members = proto->object->members;

                auto it = std::lower_bound(members.begin(), members.end(), member);

                if(it == members.end() || it->hash != hash)
                {
                    proto = &members[0].value;
                }
                else// found in one of the proto objects
                {
                    *outValue = it->value;
                    return;
                }
            }

            // not found, out value shall stay nil
        }
        else// found the value corresponding to this hash
        {
            *outValue = it->value;
        }
    }

    void VirtualMachine::objectStoreMember(Object* object, unsigned hash, const Value& newValue)
    {
        Object::Member member(hash);

        auto it = std::lower_bound(object->members.begin(), object->members.end(), member);

        if(it == object->members.end() || it->hash != hash)
        {
            bool found = false;
            const Value* proto = &object->members[0].value;

            while(proto->type == Value::VT_Object &&// it has a proto object
                  proto->object != object)// and it is not the first object
            {
                std::vector<Object::Member>& members = proto->object->members;

                auto it = std::lower_bound(members.begin(), members.end(), member);

                if(it == members.end() || it->hash != hash)
                {
                    proto = &members[0].value;
                }
                else// found in one of the proto objects
                {
                    it->value = newValue;
                    found = true;
                    break;
                }
            }

            if(!found)// create a new one
                object->members.insert(it, Object::Member(hash, newValue));
        }
        else// found the value corresponding to this hash
        {
            it->value = newValue;
        }

        m_memoryman.updateGCRelationship(object, newValue);
    }

    bool VirtualMachine::doBinaryOperation(int opCode)
    {
        unsigned last = m_stack->size() - 1;
        Value& lhs = m_stack->at(last - 1);
        Value& rhs = m_stack->at(last);
        Value result;

        switch(opCode)
        {
            case OpCode::OC_Add:
            {
                if(lhs.isNumber() && rhs.isNumber())
                {
                    if(lhs.isFloat() || rhs.isFloat())
                        result = Value(lhs.asFloat() + rhs.asFloat());
                    else
                        result = Value(lhs.toInt() + rhs.toInt());
                }
                else if(lhs.isArray() && rhs.isArray())
                {
                    Array* newArray = m_memoryman.makeArray();
                    auto& elements = newArray->elements;
                    elements.reserve(lhs.array->elements.size() + rhs.array->elements.size());

                    for(const auto& v : lhs.array->elements)
                        elements.push_back(v);
                    for(const auto& v : rhs.array->elements)
                        elements.push_back(v);

                    result = newArray;
                }
                else if(lhs.isObject() && rhs.isObject())
                {
                    Object* newObject = m_memoryman.makeObject();
                    auto& members = newObject->members;
                    members.reserve(lhs.object->members.size() + rhs.object->members.size());

                    for(const auto& v : lhs.object->members)
                        members.push_back(v);
                    for(const auto& v : rhs.object->members)
                        members.push_back(v);

                    std::sort(members.begin(), members.end());

                    result = newObject;
                }
                else
                {
                    setError("Invalid arguments for operator +");
                    return false;
                }
                break;
            }

            case OpCode::OC_Subtract:
            {
                if(lhs.isNumber() && rhs.isNumber())
                {
                    if(lhs.isFloat() || rhs.isFloat())
                        result = Value(lhs.asFloat() - rhs.asFloat());
                    else
                        result = Value(lhs.toInt() - rhs.toInt());
                }
                else
                {
                    setError("Invalid arguments for operator -");
                    return false;
                }
                break;
            }

            case OpCode::OC_Multiply:
            {
                if(lhs.isNumber() && rhs.isNumber())
                {
                    if(lhs.isFloat() || rhs.isFloat())
                        result = Value(lhs.asFloat() * rhs.asFloat());
                    else
                        result = Value(lhs.toInt() * rhs.toInt());
                }
                else
                {
                    setError("Invalid arguments for operator *");
                    return false;
                }
                break;
            }

            case OpCode::OC_Divide:
            {
                if(lhs.isNumber() && rhs.isNumber())
                {
                    if(rhs.asFloat() == 0)
                    {
                        setError("Division by 0");
                    }
                    else if(lhs.isFloat() || rhs.isFloat())
                        result = Value(lhs.asFloat() / rhs.asFloat());
                    else
                        result = Value(lhs.toInt() / rhs.toInt());
                }
                else
                {
                    setError("Invalid arguments for operator /");
                    return false;
                }
                break;
            }

            case OC_Power:
            {
                if(lhs.isNumber() && rhs.isNumber())
                {
                    if(lhs.isFloat())
                        result = float(std::pow(lhs.asFloat(), rhs.asFloat()));
                    else
                        result = int(std::pow(lhs.toInt(), rhs.asFloat()));
                }
                else
                {
                    setError("Invalid arguments for operator ^");
                    return false;
                }
                break;
            }

            case OC_Modulo:
            {
                if(lhs.isNumber() && rhs.isNumber())
                {
                    if(lhs.isInt() && rhs.isInt())
                        result = Value(lhs.toInt() % rhs.toInt());
                    else
                        result = Value(std::fmod(lhs.asFloat(), rhs.asFloat()));
                }
                else
                {
                    setError("Invalid arguments for operator %%");
                    return false;
                }
                break;
            }

            case OC_Concatenate:
            {
                if(lhs.isString() && rhs.isString())
                    result = m_memoryman.makeString(lhs.string->str + rhs.string->str);
                else// anything can be turned into a string
                    result = m_memoryman.makeString(lhs.asString() + rhs.asString());
                break;
            }

            case OC_Xor:
            {
                result = !lhs.asBool() != !rhs.asBool();// anything can be turned into a bool
                break;
            }

            case OC_Equal:
            {
                if(lhs.isInt() && rhs.isInt())
                {
                    result = lhs.toInt() == rhs.toInt();
                }
                else if(lhs.isNumber() && rhs.isNumber())
                {
                    result = lhs.asFloat() == rhs.asFloat();
                }
                else if(lhs.type == rhs.type)
                {
                    if(lhs.isNil())
                        result = true;
                    else if(lhs.isBoolean())
                        result = lhs.asBool() == rhs.asBool();
                    else if(lhs.IsHash())
                        result = lhs.hash == rhs.hash;
                    else if(lhs.isString() || lhs.isError())
                        result = lhs.asString() == rhs.asString();
                    else
                        result = lhs.object == rhs.object;// compare any pointers
                }
                else
                {
                    result = false;
                }
                break;
            }

            case OC_NotEqual:
            {
                if(lhs.isInt() && rhs.isInt())
                {
                    result = lhs.toInt() != rhs.toInt();
                }
                else if(lhs.isNumber() && rhs.isNumber())
                {
                    result = lhs.asFloat() != rhs.asFloat();
                }
                else if(lhs.type == rhs.type)
                {
                    if(lhs.isNil())
                        result = false;
                    else if(lhs.isBoolean())
                        result = lhs.asBool() != rhs.asBool();
                    else if(lhs.IsHash())
                        result = lhs.hash != rhs.hash;
                    else if(lhs.isString() || lhs.isError())
                        result = lhs.asString() != rhs.asString();
                    else
                        result = lhs.object != rhs.object;// compare any pointers
                }
                else
                {
                    result = true;
                }
                break;
            }

            case OC_Less:
            {
                if(lhs.isNumber() && rhs.isNumber())
                {
                    if(lhs.isFloat() || rhs.isFloat())
                        result = lhs.asFloat() < rhs.asFloat();
                    else
                        result = lhs.toInt() < rhs.toInt();
                }
                else
                {
                    setError("Invalid arguments for operator <");
                    return false;
                }
                break;
            }

            case OC_Greater:
            {
                if(lhs.isNumber() && rhs.isNumber())
                {
                    if(lhs.isFloat() || rhs.isFloat())
                        result = lhs.asFloat() > rhs.asFloat();
                    else
                        result = lhs.toInt() > rhs.toInt();
                }
                else
                {
                    setError("Invalid arguments for operator >");
                    return false;
                }
                break;
            }

            case OC_LessEqual:
            {
                if(lhs.isNumber() && rhs.isNumber())
                {
                    if(lhs.isFloat() || rhs.isFloat())
                        result = lhs.asFloat() <= rhs.asFloat();
                    else
                        result = lhs.toInt() <= rhs.toInt();
                }
                else
                {
                    setError("Invalid arguments for operator <=");
                    return false;
                }
                break;
            }

            case OC_GreaterEqual:
            {
                if(lhs.isNumber() && rhs.isNumber())
                {
                    if(lhs.isFloat() || rhs.isFloat())
                        result = lhs.asFloat() >= rhs.asFloat();
                    else
                        result = lhs.toInt() >= rhs.toInt();
                }
                else
                {
                    setError("Invalid arguments for operator >=");
                    return false;
                }
                break;
            }
        }

        m_stack->pop_back();
        m_stack->pop_back();
        m_stack->push_back(result);

        return true;
    }

    void VirtualMachine::registerBuiltins()
    {
        std::string executablePath = m_fileman.getExePath();
        m_fileman.addSearchPath(executablePath + "../stdlib");

        for(const auto& native : Builtins::GetAllFunctions())
        {
            addNative(native.name, native.function);
        }
    }

    void VirtualMachine::logStacktraceFrom(const StackFrame* frame)
    {
        using namespace std::string_literals;

        int line = -1;
        std::string oldFilename;
        std::string newFilename;
        locationFromFrame(frame, &line, &oldFilename);

        m_logger.pushError(line, m_errmessage);

        m_errmessage = "called from here";

        if(m_execctx)
        {
            m_execctx->stackFrames.pop_back();

            ExecutionContext* currentContext = m_execctx;

            while(currentContext)
            {
                std::deque<StackFrame>& stackFrames = currentContext->stackFrames;

                while(!stackFrames.empty())
                {
                    locationFromFrame(&stackFrames.back(), &line, &newFilename);

                    if(oldFilename != newFilename)
                    {
                        m_logger.pushError("file: "s + oldFilename);
                        oldFilename = newFilename;
                    }

                    m_logger.pushError(line, m_errmessage);

                    stackFrames.pop_back();
                }

                currentContext = currentContext->parent;
            }
        }
    }

    void VirtualMachine::locationFromFrame(const StackFrame* frame, int* currentLine, std::string* currentFile) const
    {
        const CodeObject* codeObject = frame->function->codeObject;

        *currentLine = -1;
        *currentFile = codeObject->module->filename;

        const auto& lines = codeObject->instructionLines;

        if(lines.empty())
            return;

        if(lines.size() == 1)
        {
            *currentLine = lines.back().line;
            return;
        }

        int instructionIndex = frame->ip - codeObject->instructions.data();

        int lineIndex = -1;

        for(const SourceCodeLine& line : lines)
        {
            if(instructionIndex >= line.instructionIndex)
            {
                lineIndex = line.line;
            }
            else
            {
                *currentLine = lineIndex;
                return;
            }
        }

        *currentLine = lineIndex;
    }

}// namespace element
