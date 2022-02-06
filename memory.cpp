#include "element.h"

#include <algorithm>

namespace element
{
    MemoryManager::MemoryManager()
    : m_heaphead(nullptr), m_gcstage(GCS_Ready), m_currentwhite(GarbageCollected::GC_White0),
      m_nextwhite(GarbageCollected::GC_White1), m_prevgc(nullptr), m_currgc(nullptr), m_heapstringscnt(0),
      m_heaparrayscnt(0), m_heapobjectscnt(0), m_heapfunctionscnt(0), m_heapboxescnt(0), m_heapitercnt(0),
      m_heaperrorscnt(0)
    {
    }

    MemoryManager::~MemoryManager()
    {
        deleteHeap();
    }

    void MemoryManager::resetState()
    {
        m_defmodule = Module();

        m_modules.clear();
        m_excontexts.clear();

        deleteHeap();

        m_gcstage = GCS_Ready;
        m_currentwhite = GarbageCollected::GC_White0;
        m_nextwhite = GarbageCollected::GC_White1;
        m_prevgc = nullptr;
        m_currgc = nullptr;

        m_heapstringscnt = 0;
        m_heaparrayscnt = 0;
        m_heapobjectscnt = 0;
        m_heapfunctionscnt = 0;
        m_heapboxescnt = 0;
        m_heapitercnt = 0;
        m_heaperrorscnt = 0;
    }

    Module& MemoryManager::getDefaultModule()
    {
        return m_defmodule;
    }

    Module& MemoryManager::getModuleForFile(const std::string& filename)
    {
        Module& module = m_modules[filename];

        if(module.filename.empty())
            module.filename = filename;

        return module;
    }

    String* MemoryManager::makeString()
    {
        String* newString = new String();

        addToHeap(newString);

        ++m_heapstringscnt;

        return newString;
    }

    String* MemoryManager::makeString(const std::string& str)
    {
        String* newString = new String(str);

        addToHeap(newString);

        ++m_heapstringscnt;

        return newString;
    }

    String* MemoryManager::makeString(const char* str, int size)
    {
        String* newString = new String(str, size);

        addToHeap(newString);

        ++m_heapstringscnt;

        return newString;
    }

    Array* MemoryManager::makeArray()
    {
        Array* newArray = new Array();

        addToHeap(newArray);

        ++m_heaparrayscnt;

        return newArray;
    }

    Object* MemoryManager::makeObject()
    {
        Object* newObject = new Object();

        addToHeap(newObject);

        ++m_heapobjectscnt;

        return newObject;
    }

    Object* MemoryManager::makeObject(const Object* other)
    {
        Object* newObject = new Object();
        *newObject = *other;

        addToHeap(newObject);

        ++m_heapobjectscnt;

        return newObject;
    }

    Function* MemoryManager::makeFunction(const Function* other)
    {
        Function* newFunction = new Function(other);

        addToHeap(newFunction);

        ++m_heapfunctionscnt;

        return newFunction;
    }

    Function* MemoryManager::makeCoroutine(const Function* other)
    {
        Function* newFunction = makeFunction(other);

        newFunction->executionContext = new ExecutionContext();

        return newFunction;
    }

    Box* MemoryManager::makeBox()
    {
        Box* newBox = new Box();

        addToHeap(newBox);

        ++m_heapboxescnt;

        return newBox;
    }

    Box* MemoryManager::makeBox(const Value& value)
    {
        Box* newBox = new Box();

        newBox->value = value;

        addToHeap(newBox);

        ++m_heapboxescnt;

        return newBox;
    }

    Iterator* MemoryManager::makeIterator(IteratorImplementation* newIterator)
    {
        Iterator* iterator = new Iterator(newIterator);

        addToHeap(iterator);

        ++m_heapitercnt;

        return iterator;
    }

    Error* MemoryManager::makeError(const std::string& errorMessage)
    {
        Error* newError = new Error(errorMessage);

        addToHeap(newError);

        ++m_heaperrorscnt;

        return newError;
    }

    ExecutionContext* MemoryManager::makeRootExecutionContext()
    {
        ExecutionContext* newContext = new ExecutionContext();

        m_excontexts.push_back(newContext);

        return newContext;
    }

    bool MemoryManager::deleteRootExecutionContext(ExecutionContext* context)
    {
        auto it = std::find(m_excontexts.begin(), m_excontexts.end(), context);

        if(it != m_excontexts.end())
        {
            m_excontexts.erase(it);
            delete context;
            return true;
        }

        return false;
    }

    void MemoryManager::collectGarbage(int steps)
    {
        switch(m_gcstage)
        {
            case GCS_Ready:
                m_graylist.clear();
                std::swap(m_currentwhite, m_nextwhite);// White0 <-> White1
                m_gcstage = GCS_MarkRoots;

            case GCS_MarkRoots:
                steps = markRoots(steps);
                if(steps <= 0)
                    return;
                m_gcstage = GCS_Mark;

            case GCS_Mark:
                steps = mark(steps);
                if(steps <= 0)
                    return;
                m_gcstage = GCS_SweepHead;

            case GCS_SweepHead:
                steps = sweepHead(steps);
                if(steps <= 0)
                    return;
                m_gcstage = GCS_SweepRest;

            case GCS_SweepRest:
                steps = sweepRest(steps);
                if(steps <= 0)
                    return;
                m_gcstage = GCS_Ready;
        }
    }

    void MemoryManager::updateGCRelationship(GarbageCollected* parent, const Value& child)
    {
        // the tri-color invariant states that at no point shall
        // a black node be directly connected to a white node
        if(parent->state == GarbageCollected::GC_Black && child.isManaged() && child.garbageCollected->state == m_currentwhite)
        {
            child.garbageCollected->state = GarbageCollected::State::GC_Gray;
            m_graylist.push_back(child.garbageCollected);
        }
    }

    int MemoryManager::heapObjectsCount(Value::Type type) const
    {
        switch(type)
        {
            case Value::VT_String:
                return m_heapstringscnt;
            case Value::VT_Array:
                return m_heaparrayscnt;
            case Value::VT_Object:
                return m_heapobjectscnt;
            case Value::VT_Function:
                return m_heapfunctionscnt;
            case Value::VT_Box:
                return m_heapboxescnt;
            case Value::VT_Iterator:
                return m_heapitercnt;
            case Value::VT_Error:
                return m_heaperrorscnt;
            default:
                return 0;
        }
    }

    void MemoryManager::deleteHeap()
    {
        while(m_heaphead)
        {
            GarbageCollected* next = m_heaphead->next;
            ;
            freeGC(m_heaphead);
            m_heaphead = next;
        }
    }

    void MemoryManager::addToHeap(GarbageCollected* gc)
    {
        gc->state = m_nextwhite;

        if(m_heaphead)
            gc->next = m_heaphead;

        m_heaphead = gc;
    }

    void MemoryManager::freeGC(GarbageCollected* gc)
    {
        switch(gc->type)
        {
            case Value::VT_String:
                delete(String*)gc;
                --m_heapstringscnt;
                break;

            case Value::VT_Array:
                delete(Array*)gc;
                --m_heaparrayscnt;
                break;

            case Value::VT_Object:
                delete(Object*)gc;
                --m_heapobjectscnt;
                break;

            case Value::VT_Function:
            {
                Function* f = (Function*)gc;
                if(f->executionContext)
                    delete f->executionContext;
                delete f;
                --m_heapfunctionscnt;
                break;
            }
            case Value::VT_Box:
                delete(Box*)gc;
                --m_heapboxescnt;
                break;

            case Value::VT_Iterator:
                delete(Iterator*)gc;// virtual call
                --m_heapitercnt;
                break;

            case Value::VT_Error:
                delete(Error*)gc;
                --m_heaperrorscnt;
                break;

            default:
                break;
        }
    }

    void MemoryManager::makeGrayIfNeeded(GarbageCollected* gc, int* steps)
    {
        if(gc->state == m_currentwhite)
        {
            gc->state = GarbageCollected::GC_Gray;
            m_graylist.push_back(gc);

            *steps -= 1;
        }
    }

    int MemoryManager::markRoots(int steps)
    {
        for(Value& global : m_defmodule.globals)
            if(global.isManaged())
                makeGrayIfNeeded(global.garbageCollected, &steps);

        for(auto& kvp : m_modules)
            for(Value& global : kvp.second.globals)
                if(global.isManaged())
                    makeGrayIfNeeded(global.garbageCollected, &steps);

        for(ExecutionContext* context : m_excontexts)
        {
            for(StackFrame& frame : context->stackFrames)
            {
                for(Value& local : frame.variables)
                    if(local.isManaged())
                        makeGrayIfNeeded(local.garbageCollected, &steps);

                for(Value& anonymousParameter : frame.anonymousParameters.elements)
                    if(anonymousParameter.isManaged())
                        makeGrayIfNeeded(anonymousParameter.garbageCollected, &steps);
            }

            for(Value& value : context->stack)
                if(value.isManaged())
                    makeGrayIfNeeded(value.garbageCollected, &steps);
        }

        return steps;
    }

    int MemoryManager::mark(int steps)
    {
        GarbageCollected* currentObject = nullptr;

        while(!m_graylist.empty() && steps > 0)
        {
            currentObject = m_graylist.back();
            currentObject->state = GarbageCollected::GC_Black;
            m_graylist.pop_back();
            steps -= 1;

            switch(currentObject->type)
            {
                case Value::VT_Array:
                    for(Value& element : ((Array*)currentObject)->elements)
                        if(element.isManaged())
                            makeGrayIfNeeded(element.garbageCollected, &steps);
                    break;

                case Value::VT_Object:
                    for(Object::Member& member : ((Object*)currentObject)->members)
                        if(member.value.isManaged())
                            makeGrayIfNeeded(member.value.garbageCollected, &steps);
                    break;

                case Value::VT_Function:
                {
                    Function* function = ((Function*)currentObject);
                    for(Box* box : function->freeVariables)
                        if(box)
                            makeGrayIfNeeded(box, &steps);

                    if(function->executionContext)
                    {
                        for(StackFrame& frame : function->executionContext->stackFrames)
                        {
                            for(Value& local : frame.variables)
                                if(local.isManaged())
                                    makeGrayIfNeeded(local.garbageCollected, &steps);

                            for(Value& anonymousParameter : frame.anonymousParameters.elements)
                                if(anonymousParameter.isManaged())
                                    makeGrayIfNeeded(anonymousParameter.garbageCollected, &steps);
                        }

                        for(Value& value : function->executionContext->stack)
                            if(value.isManaged())
                                makeGrayIfNeeded(value.garbageCollected, &steps);
                    }
                    break;
                }

                case Value::VT_Box:
                {
                    Value& value = ((Box*)currentObject)->value;
                    if(value.isManaged())
                        makeGrayIfNeeded(value.garbageCollected, &steps);
                    break;
                }

                case Value::VT_Iterator:
                    ((Iterator*)currentObject)->implementation->updateGrayList(m_graylist, m_currentwhite);// virtual call
                    break;

                default:
                    break;
            }
        }

        return steps;
    }

    int MemoryManager::sweepHead(int steps)
    {
        while(m_heaphead && steps > 0)
        {
            if(m_heaphead->state == m_currentwhite)
            {
                GarbageCollected* next = m_heaphead->next;
                freeGC(m_heaphead);
                m_heaphead = next;
            }
            else
            {
                m_heaphead->state = m_nextwhite;
                m_prevgc = m_heaphead;
                m_currgc = m_heaphead->next;
                break;
            }

            steps -= 1;
        }

        return steps;
    }

    int MemoryManager::sweepRest(int steps)
    {
        while(m_currgc && steps > 0)
        {
            if(m_currgc->state == m_currentwhite)
            {
                m_prevgc->next = m_currgc->next;
                freeGC(m_currgc);
                m_currgc = m_prevgc->next;
            }
            else
            {
                m_currgc->state = m_nextwhite;
                m_prevgc = m_currgc;
                m_currgc = m_currgc->next;
            }

            steps -= 1;
        }

        return steps;
    }

}// namespace element
