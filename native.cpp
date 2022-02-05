#include "element.h"

#include <cmath>
#include <locale>

#include "element.h"
#include "element.h"

namespace element
{
    namespace nativefunctions
    {
        const std::vector<NamedFunction> allFunctions = {
            { "load_element", natfn_loadelement },
            { "add_search_path", natfn_addsearchpath },
            { "get_search_paths", natfn_getsearchpaths },
            { "clear_search_paths", natfn_clearsearchpaths },
            { "type", natfn_type },
            { "this_call", natfn_thiscall },
            { "garbage_collect", natfn_garbagecollect },
            { "memory_stats", natfn_memorystats },
            { "print", natfn_print },
            { "to_upper", natfn_toupper },
            { "to_lower", natfn_tolower },
            { "keys", natfn_keys },
            { "make_error", natfn_makeerror },
            { "is_error", natfn_iserror },
            { "make_coroutine", natfn_makecoroutine },
            { "make_iterator", natfn_makeiterator },
            { "iterator_has_next", natfn_iteratorhasnext },
            { "iterator_get_next", natfn_iteratorgetnext },
            { "range", natfn_range },
            { "each", natfn_each },
            { "times", natfn_times },
            { "count", natfn_count },
            { "map", natfn_map },
            { "filter", natfn_filter },
            { "reduce", natfn_reduce },
            { "all", natfn_all },
            { "any", natfn_any },
            { "min", natfn_min },
            { "max", natfn_max },
            { "sort", natfn_sort },
            { "floor", natfn_floor },
            { "ceil", natfn_ceil },
            { "round", natfn_round },
            { "sqrt", natfn_sqrt },
            { "sin", natfn_sin },
            { "cos", natfn_cos },
            { "tan", natfn_tan },
            {"chr", natfn_chr},
        };

        const std::vector<NamedFunction>& GetAllFunctions()
        {
            return allFunctions;
        }

        Value natfn_loadelement(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args)
        {
            if(args.size() != 1)
            {
                vm.setError("function 'load_element(filename)' takes exactly one argument");
                return Value();
            }

            const Value& filename = args[0];

            if(!filename.isString())
            {
                vm.setError("function 'load_element(filename)' takes a string as an argument");
                return Value();
            }

            Value result = vm.evalFile(filename.asString());

            if(result.isError())
                vm.setError(std::string("error while loading: ") + filename.asString() + "\n" + result.asString());

            return result;
        }

        Value natfn_addsearchpath(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args)
        {
            if(args.size() != 1)
            {
                vm.setError("function 'add_search_path(path)' takes exactly one argument");
                return Value();
            }

            const Value& path = args[0];

            if(!path.isString())
            {
                vm.setError("function 'add_search_path(path)' takes a string as an argument");
                return Value();
            }

            vm.getFileManager().addSearchPath(path.string->str);

            return Value();
        }

        Value natfn_getsearchpaths(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args)
        {
            if(!args.empty())
            {
                vm.setError("function 'get_search_paths()' takes no arguments");
                return Value();
            }

            MemoryManager& memoryManager = vm.getMemoryManager();

            const std::vector<std::string>& paths = vm.getFileManager().getSearchPaths();

            Value result = memoryManager.NewArray();

            result.array->elements.reserve(paths.size());

            for(const std::string& path : paths)
                vm.pushElement(result, memoryManager.NewString(path));

            return result;
        }

        Value natfn_clearsearchpaths(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args)
        {
            if(!args.empty())
            {
                vm.setError("function 'clear_search_paths()' takes no arguments");
                return Value();
            }

            vm.getFileManager().clearSearchPaths();

            return Value();
        }

        Value natfn_type(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args)
        {
            if(args.size() != 1)
            {
                vm.setError("function 'type(value)' takes exactly one argument");
                return Value();
            }

            Value result = vm.getMemoryManager().NewString();

            switch(args[0].type)
            {
                case Value::VT_Nil:
                    result.string->str = "nil";
                    break;
                case Value::VT_Int:
                    result.string->str = "int";
                    break;
                case Value::VT_Float:
                    result.string->str = "float";
                    break;
                case Value::VT_Bool:
                    result.string->str = "bool";
                    break;
                case Value::VT_String:
                    result.string->str = "string";
                    break;
                case Value::VT_Array:
                    result.string->str = "array";
                    break;
                case Value::VT_Object:
                    result.string->str = "object";
                    break;
                case Value::VT_Function:
                    result.string->str = "function";
                    break;
                case Value::VT_Iterator:
                    result.string->str = "iterator";
                    break;
                case Value::VT_NativeFunction:
                    result.string->str = "native-function";
                    break;
                case Value::VT_Error:
                    result.string->str = "error";
                    break;
                default:
                    result.string->str = "<[???]>";
                    break;
            }
            return result;
        }

        Value natfn_thiscall(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args)
        {
            if(args.size() < 2)
            {
                vm.setError("function 'this_call(function, this, args...)' takes at least two arguments");
                return Value();
            }

            const Value& function = args[0];

            if(!function.isFunction())
            {
                vm.setError("function 'this_call(function, this, args...)' takes a function as a first argument");
                return Value();
            }

            const Value& object = args[1];

            if(!object.isObject())
            {
                vm.setError("function 'this_call(function, this, args...)' takes an object as a second argument");
                return Value();
            }

            std::vector<Value> thisCallArgs(args.begin() + 2, args.end());

            // TODO: This will create a new execution context. Do we really want that?
            Value result = vm.callMemberFunction(object, function, thisCallArgs);

            return result;
        }

        Value natfn_garbagecollect(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args)
        {
            if(args.empty())
            {
                vm.getMemoryManager().GarbageCollect();
            }
            else if(args[0].isInt())
            {
                int steps = args[0].toInt();
                vm.getMemoryManager().GarbageCollect(steps);
            }
            else
            {
                vm.setError("function 'garbage_collect(steps)' takes a single integer as an argument");
            }

            return Value();
        }

        Value natfn_memorystats(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args)
        {
            MemoryManager& memoryManager = vm.getMemoryManager();

            int strings = memoryManager.GetHeapObjectsCount(Value::VT_String);
            int arrays = memoryManager.GetHeapObjectsCount(Value::VT_Array);
            int objects = memoryManager.GetHeapObjectsCount(Value::VT_Object);
            int functions = memoryManager.GetHeapObjectsCount(Value::VT_Function);
            int boxes = memoryManager.GetHeapObjectsCount(Value::VT_Box);
            int iterators = memoryManager.GetHeapObjectsCount(Value::VT_Iterator);
            int errors = memoryManager.GetHeapObjectsCount(Value::VT_Error);

            int total = strings + arrays + objects + functions + boxes + iterators + errors;

            Value data = memoryManager.NewObject();

            vm.setMember(data, "heap_strings_count", Value(strings));
            vm.setMember(data, "heap_arrays_count", Value(arrays));
            vm.setMember(data, "heap_objects_count", Value(objects));
            vm.setMember(data, "heap_functions_count", Value(functions));
            vm.setMember(data, "heap_boxes_count", Value(boxes));
            vm.setMember(data, "heap_iterators_count", Value(iterators));
            vm.setMember(data, "heap_errors_count", Value(errors));
            vm.setMember(data, "heap_total_count", Value(total));

            return data;
        }

        Value natfn_chr(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args)
        {
            std::string rt;
            rt.push_back(char(args[0].toInt()));
            return vm.getMemoryManager().NewString(rt);
        }

        Value natfn_print(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args)
        {
            for(const Value& arg : args)
            {
                auto str = arg.asString();
                auto pos = str.find('\\');

                while(pos != std::string::npos && pos + 1 < str.size())
                {
                    switch(str[pos + 1])
                    {
                        case 'n':
                            str.replace(pos, 2, "\n");
                            break;
                        case 'r':
                            str.replace(pos, 2, "\r");
                            break;
                        case 't':
                            str.replace(pos, 2, "\t");
                            break;
                        case '\\':
                            str.replace(pos, 2, "\\");
                            break;
                    }

                    pos = str.find('\\', pos + 1);
                }

                printf("%s", str.c_str());
            }
            return int(args.size());
        }

        Value natfn_toupper(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args)
        {
            if(args.size() != 1)
            {
                vm.setError("function 'to_upper(string)' takes exactly one argument");
                return Value();
            }

            if(!args[0].isString())
            {
                vm.setError("function 'to_upper(string)' takes a string as argument");
                return Value();
            }

            std::locale locale;
            std::string str = args[0].string->str;

            unsigned size = str.size();

            for(unsigned i = 0; i < size; ++i)
                str[i] = std::toupper(str[i], locale);

            return vm.getMemoryManager().NewString(str);
        }

        Value natfn_tolower(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args)
        {
            if(args.size() != 1)
            {
                vm.setError("function 'to_lower(string)' takes exactly one argument");
                return Value();
            }

            if(!args[0].isString())
            {
                vm.setError("function 'to_lower(string)' takes a string as argument");
                return Value();
            }

            std::locale locale;
            std::string str = args[0].string->str;

            unsigned size = str.size();

            for(unsigned i = 0; i < size; ++i)
                str[i] = std::tolower(str[i], locale);

            return vm.getMemoryManager().NewString(str);
        }

        Value natfn_keys(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args)
        {
            if(args.size() != 1)
            {
                vm.setError("function 'keys(object)' takes exactly one argument");
                return Value();
            }

            if(!args[0].isObject())
            {
                vm.setError("function 'keys(object)' takes an object as argument");
                return Value();
            }

            MemoryManager& memoryManager = vm.getMemoryManager();

            Object* object = args[0].object;

            Value keys = memoryManager.NewArray();
            std::string name;

            keys.array->elements.reserve(object->members.size());

            for(const Object::Member& member : object->members)
            {
                if(vm.nameFromHash(member.hash, &name))
                    vm.pushElement(keys, memoryManager.NewString(name));

                name.clear();
            }

            return keys;
        }

        Value natfn_makeerror(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args)
        {
            if(args.size() != 1)
            {
                vm.setError("function 'make_error(message)' takes exactly one argument");
                return Value();
            }

            Value::Type type = args[0].type;

            if(type != Value::VT_String)
            {
                vm.setError("function 'make_error(message)' takes only a string as an argument");
                return Value();
            }

            const std::string& str = args[0].string->str;

            return vm.getMemoryManager().NewError(str);
        }

        Value natfn_iserror(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args)
        {
            if(args.size() != 1)
            {
                vm.setError("function 'is_error(value)' takes exactly one argument");
                return Value();
            }

            return args[0].isError();
        }

        Value natfn_makecoroutine(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args)
        {
            if(args.size() != 1)
            {
                vm.setError("function 'make_coroutine(function)' takes exactly one argument");
                return Value();
            }

            Value::Type type = args[0].type;

            if(type != Value::VT_Function)
            {
                vm.setError("function 'make_coroutine(function)' takes only a function as an argument");
                return Value();
            }

            return vm.getMemoryManager().NewCoroutine(args[0].function);
        }

        Value natfn_makeiterator(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args)
        {
            if(args.size() != 1)
            {
                vm.setError("function 'make_iterator(value)' takes exactly one argument");
                return Value();
            }

            Iterator* iterator = vm.makeIterator(args[0]);

            if(iterator)
                return iterator;

            if(args[0].type == Value::VT_Function && args[0].function->executionContext == nullptr)
            {
                vm.setError("function 'make_iterator(value)': Cannot iterate a function. Only coroutine instances are iterable.");
            }
            else
            {
                vm.setError("function 'make_iterator(value)': Value is not iterable.");
            }

            return Value();
        }

        Value natfn_iteratorhasnext(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args)
        {
            if(args.size() != 1)
            {
                vm.setError("function 'iterator_has_next(iterator)' takes exactly one argument");
                return Value();
            }

            if(args[0].type != Value::VT_Iterator)
            {
                vm.setError("function 'iterator_get_next(iterator)' takes an iterator as a first argument");
                return Value();
            }

            IteratorImplementation* ii = args[0].iterator->implementation;

            Value result = vm.callMemberFunction(ii->thisObjectUsed, ii->hasNextFunction, {});

            if(vm.hasError())
                return Value();

            return result;
        }

        Value natfn_iteratorgetnext(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args)
        {
            if(args.size() != 1)
            {
                vm.setError("function 'iterator_get_next(iterator)' takes exactly one argument");
                return Value();
            }

            if(args[0].type != Value::VT_Iterator)
            {
                vm.setError("function 'iterator_get_next(iterator)' takes an iterator as a first argument");
                return Value();
            }

            IteratorImplementation* ii = args[0].iterator->implementation;

            Value result = vm.callMemberFunction(ii->thisObjectUsed, ii->getNextFunction, {});

            if(vm.hasError())
                return Value();

            return result;
        }

        struct RangeIterator : public IteratorImplementation
        {
            int from = 0;
            int to = 0;
            int step = 1;

            RangeIterator()
            {
                hasNextFunction = Value(
                [](VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args) -> Value
                {
                    RangeIterator* self = static_cast<RangeIterator*>(thisObject.iterator->implementation);

                    return self->from < self->to;
                });

                getNextFunction = Value(
                [](VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args) -> Value
                {
                    RangeIterator* self = static_cast<RangeIterator*>(thisObject.iterator->implementation);

                    int result = self->from;
                    self->from += self->step;
                    return result;
                });
            }
        };

        Value natfn_range(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args)// TODO check for reversed ranges like 'range(10, 0)'
        {
            if(args.size() == 1)
            {
                if(!args[0].isInt())
                {
                    vm.setError("function 'range(max)' takes an integer as argument");
                    return Value();
                }

                RangeIterator* rangeIterator = new RangeIterator();

                rangeIterator->to = args[0].toInt();

                return vm.getMemoryManager().NewIterator(rangeIterator);
            }
            else if(args.size() == 2)
            {
                if(!args[0].isInt() || !args[1].isInt())
                {
                    vm.setError("function 'range(min,max)' takes integers as arguments");
                    return Value();
                }

                RangeIterator* rangeIterator = new RangeIterator();

                rangeIterator->from = args[0].toInt();
                rangeIterator->to = args[1].toInt();

                return vm.getMemoryManager().NewIterator(rangeIterator);
            }
            else if(args.size() == 3)
            {
                if(!args[0].isInt() || !args[1].isInt() || !args[2].isInt())
                {
                    vm.setError("function 'range(min,max,step)' takes integers as arguments");
                    return Value();
                }

                RangeIterator* rangeIterator = new RangeIterator();

                rangeIterator->from = args[0].toInt();
                rangeIterator->to = args[1].toInt();
                rangeIterator->step = args[2].toInt();

                return vm.getMemoryManager().NewIterator(rangeIterator);
            }

            vm.setError("'range' can only be 'range(max)', 'range(min,max)' or 'range(min,max,step)'");
            return Value();
        }

        Value natfn_each(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args)
        {
            if(args.size() != 2)
            {
                vm.setError("function 'each(iterable, function)' takes exactly two arguments");
                return Value();
            }

            const Value& function = args[1];

            if(!function.isFunction())
            {
                vm.setError("function 'each(iterable, function)': second argument is not a function");
                return Value();
            }

            if(Iterator* iterator = vm.makeIterator(args[0]))
            {
                Value result;
                std::vector<Value> noArgs;
                Value& objectUsed = iterator->implementation->thisObjectUsed;
                Value& hasNext = iterator->implementation->hasNextFunction;
                Value& getNext = iterator->implementation->getNextFunction;

                while(true)
                {
                    result = vm.callMemberFunction(objectUsed, hasNext, noArgs);

                    if(vm.hasError())
                        return Value();

                    if(!result.asBool())
                        break;

                    result = vm.callMemberFunction(objectUsed, getNext, noArgs);

                    if(vm.hasError())
                        return Value();

                    vm.callFunction(function, { result });

                    if(vm.hasError())
                        return Value();
                }
            }
            else
            {
                vm.setError("function 'each(iterable, function)': first argument not interable");
            }

            return Value();
        }

        Value natfn_times(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args)
        {
            if(args.size() != 2)
            {
                vm.setError("function 'times(n, function)' takes exactly two arguments");
                return Value();
            }

            if(!args[0].isInt())
            {
                vm.setError("function 'times(n, function)': first argument is not an integer");
                return Value();
            }

            int times = args[0].toInt();
            const Value& function = args[1];

            if(!function.isFunction())
            {
                vm.setError("function 'times(n, function)': second argument is not a function");
                return Value();
            }

            for(int i = 0; i < times; ++i)
            {
                vm.callFunction(function, { i });

                if(vm.hasError())
                    return Value();
            }

            return Value();
        }

        Value natfn_count(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args)
        {
            if(args.size() != 2)
            {
                vm.setError("function 'count(iterable, function)' takes exactly two arguments");
                return Value();
            }

            const Value& function = args[1];

            if(!function.isFunction())
            {
                vm.setError("function 'count(iterable, function)': second argument is not a function");
                return Value();
            }

            if(Iterator* iterator = vm.makeIterator(args[0]))
            {
                Value result;
                std::vector<Value> noArgs;
                Value& objectUsed = iterator->implementation->thisObjectUsed;
                Value& hasNext = iterator->implementation->hasNextFunction;
                Value& getNext = iterator->implementation->getNextFunction;

                int counter = 0;

                while(true)
                {
                    result = vm.callMemberFunction(objectUsed, hasNext, noArgs);

                    if(vm.hasError())
                        return Value();

                    if(!result.asBool())
                        break;

                    result = vm.callMemberFunction(objectUsed, getNext, noArgs);

                    if(vm.hasError())
                        return Value();

                    result = vm.callFunction(function, { result });

                    if(vm.hasError())
                        return Value();

                    if(result.asBool())
                        ++counter;
                }

                return counter;
            }

            vm.setError("function 'count(iterable, function)': first argument not interable");
            return Value();
        }

        Value natfn_map(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args)
        {
            if(args.size() != 2)
            {
                vm.setError("function 'map(iterable, function)' takes exactly two arguments");
                return Value();
            }

            const Value& function = args[1];

            if(!function.isFunction())
            {
                vm.setError("function 'map(iterable, function)': second argument is not a function");
                return Value();
            }

            if(Iterator* iterator = vm.makeIterator(args[0]))
            {
                Value result;
                std::vector<Value> noArgs;
                Value& objectUsed = iterator->implementation->thisObjectUsed;
                Value& hasNext = iterator->implementation->hasNextFunction;
                Value& getNext = iterator->implementation->getNextFunction;

                Value mapped = vm.getMemoryManager().NewArray();

                while(true)
                {
                    result = vm.callMemberFunction(objectUsed, hasNext, noArgs);

                    if(vm.hasError())
                        return Value();

                    if(!result.asBool())
                        break;

                    result = vm.callMemberFunction(objectUsed, getNext, noArgs);

                    if(vm.hasError())
                        return Value();

                    result = vm.callFunction(function, { result });

                    if(vm.hasError())
                        return Value();

                    vm.pushElement(mapped, result);
                }

                return mapped;
            }

            vm.setError("function 'map(iterable, function)': first argument not interable");
            return Value();
        }

        Value natfn_filter(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args)
        {
            if(args.size() != 2)
            {
                vm.setError("function 'filter(iterable, predicate)' takes exactly two arguments");
                return Value();
            }

            const Value& function = args[1];

            if(!function.isFunction())
            {
                vm.setError("function 'filter(iterable, predicate)': second argument is not a function");
                return Value();
            }

            if(Iterator* iterator = vm.makeIterator(args[0]))
            {
                Value result;
                Value item;
                std::vector<Value> noArgs;
                Value& objectUsed = iterator->implementation->thisObjectUsed;
                Value& hasNext = iterator->implementation->hasNextFunction;
                Value& getNext = iterator->implementation->getNextFunction;

                Value filtered = vm.getMemoryManager().NewArray();

                while(true)
                {
                    result = vm.callMemberFunction(objectUsed, hasNext, noArgs);

                    if(vm.hasError())
                        return Value();

                    if(!result.asBool())
                        break;

                    item = vm.callMemberFunction(objectUsed, getNext, noArgs);

                    if(vm.hasError())
                        return Value();

                    result = vm.callFunction(function, { item });

                    if(vm.hasError())
                        return Value();

                    if(result.asBool())
                        vm.pushElement(filtered, item);
                }

                return filtered;
            }

            vm.setError("function 'filter(iterable, function)': first argument not interable");
            return Value();
        }

        Value natfn_reduce(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args)
        {
            if(args.size() != 2)
            {
                vm.setError("function 'reduce(iterable, function)' takes exactly two arguments");
                return Value();
            }

            const Value& function = args[1];

            if(!function.isFunction())
            {
                vm.setError("function 'reduce(iterable, function)': second argument is not a function");
                return Value();
            }

            if(Iterator* iterator = vm.makeIterator(args[0]))
            {
                Value result;
                std::vector<Value> noArgs;
                Value& objectUsed = iterator->implementation->thisObjectUsed;
                Value& hasNext = iterator->implementation->hasNextFunction;
                Value& getNext = iterator->implementation->getNextFunction;

                Value reduced;
                result = vm.callMemberFunction(objectUsed, hasNext, noArgs);

                if(vm.hasError())
                    return Value();

                if(result.asBool())
                {
                    reduced = vm.callMemberFunction(objectUsed, getNext, noArgs);

                    while(true)
                    {
                        result = vm.callMemberFunction(objectUsed, hasNext, noArgs);

                        if(vm.hasError())
                            return Value();

                        if(!result.asBool())
                            break;

                        result = vm.callMemberFunction(objectUsed, getNext, noArgs);

                        if(vm.hasError())
                            return Value();

                        reduced = vm.callFunction(function, { reduced, result });

                        if(vm.hasError())
                            return Value();
                    }
                }

                return reduced;
            }

            vm.setError("function 'reduce(iterable, function)': first argument not interable");
            return Value();
        }

        Value natfn_all(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args)
        {
            unsigned argsSize = args.size();

            if(argsSize < 1 || argsSize > 2)
            {
                vm.setError("function 'all(iterable, [predicate])' takes one or two arguments");
                return Value();
            }

            if(Iterator* iterator = vm.makeIterator(args[0]))
            {
                Value result;
                std::vector<Value> noArgs;
                Value& objectUsed = iterator->implementation->thisObjectUsed;
                Value& hasNext = iterator->implementation->hasNextFunction;
                Value& getNext = iterator->implementation->getNextFunction;

                if(argsSize == 1)
                {
                    while(true)
                    {
                        result = vm.callMemberFunction(objectUsed, hasNext, noArgs);

                        if(vm.hasError())
                            return Value();

                        if(!result.asBool())
                            break;

                        result = vm.callMemberFunction(objectUsed, getNext, noArgs);

                        if(vm.hasError())
                            return Value();

                        if(!result.asBool())
                            return false;
                    }

                    return true;
                }
                else
                {
                    const Value& function = args[1];

                    if(!function.isFunction())
                    {
                        vm.setError("function 'all(iterable, [predicate])': second argument is not a function");
                        return Value();
                    }

                    while(true)
                    {
                        result = vm.callMemberFunction(objectUsed, hasNext, noArgs);

                        if(vm.hasError())
                            return Value();

                        if(!result.asBool())
                            break;

                        result = vm.callMemberFunction(objectUsed, getNext, noArgs);

                        if(vm.hasError())
                            return Value();

                        result = vm.callFunction(function, { result });

                        if(vm.hasError())
                            return Value();

                        if(!result.asBool())
                            return false;
                    }

                    return true;
                }
            }

            vm.setError("function 'all(iterable, [predicate])': first argument not interable");
            return Value();
        }

        Value natfn_any(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args)
        {
            unsigned argsSize = args.size();

            if(argsSize < 1 || argsSize > 2)
            {
                vm.setError("function 'any(iterable, [predicate])' takes one or two arguments");
                return Value();
            }

            if(Iterator* iterator = vm.makeIterator(args[0]))
            {
                Value result;
                std::vector<Value> noArgs;
                Value& objectUsed = iterator->implementation->thisObjectUsed;
                Value& hasNext = iterator->implementation->hasNextFunction;
                Value& getNext = iterator->implementation->getNextFunction;

                if(argsSize == 1)
                {
                    while(true)
                    {
                        result = vm.callMemberFunction(objectUsed, hasNext, noArgs);

                        if(vm.hasError())
                            return Value();

                        if(!result.asBool())
                            break;

                        result = vm.callMemberFunction(objectUsed, getNext, noArgs);

                        if(vm.hasError())
                            return Value();

                        if(result.asBool())
                            return true;
                    }

                    return false;
                }
                else// argsSize == 2
                {
                    const Value& function = args[1];

                    if(!function.isFunction())
                    {
                        vm.setError("function 'any(iterable, [predicate])': second argument is not a function");
                        return Value();
                    }

                    while(true)
                    {
                        result = vm.callMemberFunction(objectUsed, hasNext, noArgs);

                        if(vm.hasError())
                            return Value();

                        if(!result.asBool())
                            break;

                        result = vm.callMemberFunction(objectUsed, getNext, noArgs);

                        if(vm.hasError())
                            return Value();

                        result = vm.callFunction(function, { result });

                        if(vm.hasError())
                            return Value();

                        if(result.asBool())
                            return true;
                    }

                    return false;
                }
            }

            vm.setError("function 'any(iterable, [predicate])': first argument not interable");
            return Value();
        }

        Value natfn_min(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args)
        {
            vm.setError("function 'min' is not implemented");
            return Value();
        }

        Value natfn_max(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args)
        {
            vm.setError("function 'max' is not implemented");
            return Value();
        }

        Value natfn_sort(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args)
        {
            vm.setError("function 'sort' is not implemented");
            return Value();
        }

        Value Abs(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args)
        {
            if(args.size() != 1)
            {
                vm.setError("function 'abs(number)' takes exactly one argument");
                return Value();
            }

            if(!args[0].isNumber())
            {
                vm.setError("function 'abs(number)': number must be int or float");
            }

            if(args[0].isInt())
                return std::abs(args[0].toInt());
            return std::abs(args[0].asFloat());
        }

        Value natfn_floor(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args)
        {
            if(args.size() != 1)
            {
                vm.setError("function 'floor(number)' takes exactly one argument");
                return Value();
            }

            if(!args[0].isNumber())
            {
                vm.setError("function 'floor(number)': number must be int or float");
            }

            return std::floor(args[0].asFloat());
        }

        Value natfn_ceil(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args)
        {
            if(args.size() != 1)
            {
                vm.setError("function 'ceil(number)' takes exactly one argument");
                return Value();
            }

            if(!args[0].isNumber())
            {
                vm.setError("function 'ceil(number)': number must be int or float");
            }

            return std::ceil(args[0].asFloat());
        }

        Value natfn_round(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args)
        {
            if(args.size() != 1)
            {
                vm.setError("function 'round(number)' takes exactly one argument");
                return Value();
            }

            if(!args[0].isNumber())
            {
                vm.setError("function 'round(number)': number must be int or float");
            }

            return std::round(args[0].asFloat());
        }

        Value natfn_sqrt(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args)
        {
            if(args.size() != 1)
            {
                vm.setError("function 'sqrt(number)' takes exactly one argument");
                return Value();
            }

            if(!args[0].isNumber())
            {
                vm.setError("function 'sqrt(number)': number must be int or float");
            }

            return std::sqrt(args[0].asFloat());
        }

        Value natfn_sin(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args)
        {
            if(args.size() != 1)
            {
                vm.setError("function 'sin(number)' takes exactly one argument");
                return Value();
            }

            if(!args[0].isNumber())
            {
                vm.setError("function 'sin(number)': number must be int or float");
            }

            return std::sin(args[0].asFloat());
        }

        Value natfn_cos(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args)
        {
            if(args.size() != 1)
            {
                vm.setError("function 'cos(number)' takes exactly one argument");
                return Value();
            }

            if(!args[0].isNumber())
            {
                vm.setError("function 'cos(number)': number must be int or float");
            }

            return std::cos(args[0].asFloat());
        }

        Value natfn_tan(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args)
        {
            if(args.size() != 1)
            {
                vm.setError("function 'tan(number)' takes exactly one argument");
                return Value();
            }

            if(!args[0].isNumber())
            {
                vm.setError("function 'tan(number)': number must be int or float");
            }

            return std::tan(args[0].asFloat());
        }

    }// namespace nativefunctions

}// namespace element
