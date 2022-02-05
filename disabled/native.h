#ifndef _NATIVE_INCLUDED_
#define _NATIVE_INCLUDED_

#include "value.h"

namespace element
{

namespace nativefunctions
{

struct NamedFunction
{
	std::string				name;
	Value::NativeFunction	function;
};

const std::vector<NamedFunction>& GetAllFunctions();

	
Value natfn_loadelement		(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
Value natfn_addsearchpath		(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
Value natfn_getsearchpaths	(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
Value natfn_clearsearchpaths	(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
Value natfn_type				(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
Value natfn_thiscall			(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
Value natfn_garbagecollect	(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
Value natfn_memorystats		(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
Value natfn_print				(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
Value natfn_toupper			(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
Value natfn_tolower			(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
Value natfn_keys				(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
Value natfn_makeerror			(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
Value natfn_iserror			(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
Value natfn_makecoroutine		(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
Value natfn_makeiterator		(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
Value natfn_iteratorhasnext	(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
Value natfn_iteratorgetnext	(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
Value natfn_range				(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
Value natfn_each				(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
Value natfn_times				(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
Value natfn_count				(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
Value natfn_map				(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
Value natfn_filter			(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
Value natfn_reduce			(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
Value natfn_all				(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
Value natfn_any				(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
Value natfn_min				(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
Value natfn_max				(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
Value natfn_sort				(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
Value Abs				(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
Value natfn_floor				(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
Value natfn_ceil				(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
Value natfn_round				(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
Value natfn_sqrt				(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
Value natfn_sin				(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
Value natfn_cos				(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);
Value natfn_tan				(VirtualMachine& vm, const Value& thisObject, const std::vector<Value>& args);

}

}

#endif // _NATIVE_INCLUDED_
