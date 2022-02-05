#include "element.h"

#include "element.h"

namespace element
{
    Value::Value() : type(VT_Nil), integer(0)
    {
    }

    Value::Value(int integer) : type(VT_Int), integer(integer)
    {
    }

    Value::Value(float floatingPoint) : type(VT_Float), floatingPoint(floatingPoint)
    {
    }

    Value::Value(bool boolean) : type(VT_Bool), boolean(boolean)
    {
    }

    Value::Value(unsigned hash) : type(VT_Hash), hash(hash)
    {
    }

    Value::Value(String* string) : type(VT_String), string(string)
    {
    }

    Value::Value(Array* array) : type(VT_Array), array(array)
    {
    }

    Value::Value(Object* object) : type(VT_Object), object(object)
    {
    }

    Value::Value(Function* function) : type(VT_Function), function(function)
    {
    }

    Value::Value(Box* box) : type(VT_Box), box(box)
    {
    }

    Value::Value(Iterator* iterator) : type(VT_Iterator), iterator(iterator)
    {
    }

    Value::Value(NativeFunction nativeFunction) : type(VT_NativeFunction), nativeFunction(nativeFunction)
    {
    }

    Value::Value(Error* error) : type(VT_Error), error(error)
    {
    }

    Value::Value(const Value& o) : type(o.type), function(o.function)
    {
    }

    bool Value::isManaged() const
    {
        const bool NotGC = type < VT_String || (type == VT_String && string->state == GarbageCollected::GC_Static)
                           || (type == VT_Function && function->state == GarbageCollected::GC_Static);
        return !NotGC;
    }

    bool Value::isNil() const
    {
        return type == VT_Nil;
    }

    bool Value::isFunction() const
    {
        return type == VT_Function || type == VT_NativeFunction;
    }

    bool Value::isArray() const
    {
        return type == VT_Array;
    }

    bool Value::isObject() const
    {
        return type == VT_Object;
    }

    bool Value::isString() const
    {
        return type == VT_String;
    }

    bool Value::isBoolean() const
    {
        return type == VT_Bool;
    }

    bool Value::isNumber() const
    {
        return type == VT_Int || type == VT_Float;
    }

    bool Value::isFloat() const
    {
        return type == VT_Float;
    }

    bool Value::isInt() const
    {
        return type == VT_Int;
    }

    bool Value::IsHash() const
    {
        return type == VT_Hash;
    }

    bool Value::isBox() const
    {
        return type == VT_Box;
    }

    bool Value::isIterator() const
    {
        return type == VT_Iterator;
    }

    bool Value::isError() const
    {
        return type == VT_Error;
    }

    int Value::toInt() const
    {
        return type == VT_Int ? integer : int(floatingPoint);
    }

    float Value::asFloat() const
    {
        return type == VT_Float ? floatingPoint : float(integer);
    }

    bool Value::asBool() const
    {
        if(type == VT_Bool)
            return boolean;
        if(type == VT_Nil)
            return false;
        return true;
    }

    unsigned Value::asHash() const
    {
        return hash;
    }

    std::string Value::asString() const
    {
        switch(type)
        {
            case VT_Nil:
                return "nil";
            case VT_Int:
                return std::to_string(integer);
            case VT_Float:
                return std::to_string(floatingPoint);
            case VT_Bool:
                return boolean ? "true" : "false";
            case VT_String:
                return string->str;
            case VT_Hash:
                return "<hash>";
            case VT_Function:
                return "<function>";
            case VT_Box:
                return "<box>";
            case VT_Iterator:
                return "<iterator>";
            case VT_NativeFunction:
                return "<native-function>";
            case VT_Error:
                return error->errorString;

            case VT_Array:
            {
                unsigned size = array->elements.size();

                std::string result = "[";

                if(size > 0)
                {
                    for(unsigned i = 0; i < size - 1; ++i)
                    {
                        Value& element = array->elements[i];

                        if(element.isArray())
                            result += "<array>,";
                        else if(element.isObject())
                            result += "<object>,";
                        else
                            result += element.asString() + ", ";
                    }

                    Value& element = array->elements[size - 1];

                    if(element.isArray())
                        result += "<array>";
                    else if(element.isObject())
                        result += "<object>";
                    else
                        result += element.asString();
                }

                result += "]";

                return result;
            }

            case VT_Object:
            {
                unsigned size = object->members.size();

                std::string result = "[ ";

                if(size > 1)// we always have at least the proto member
                {
                    for(unsigned i = 1; i < size - 1; ++i)
                    {
                        auto& kvp = object->members[i];
                        if(kvp.value.isArray())
                            result += std::to_string(kvp.hash) + " = <array>\n  ";
                        else if(kvp.value.isObject())
                            result += std::to_string(kvp.hash) + " = <object>\n  ";
                        else
                            result += std::to_string(kvp.hash) + " = " + kvp.value.asString() + "\n  ";
                    }

                    auto& kvp = object->members[size - 1];
                    if(kvp.value.isArray())
                        result += std::to_string(kvp.hash) + " = <array>\n";
                    else if(kvp.value.isObject())
                        result += std::to_string(kvp.hash) + " = <object>\n";
                    else
                        result += std::to_string(kvp.hash) + " = " + kvp.value.asString() + "\n";
                }
                else
                {
                    result += "=";
                }

                result += "]";

                return result;
            }
        }

        return "<[???]>";
    }

}// namespace element
