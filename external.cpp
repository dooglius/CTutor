#include "cast.h"
#include "exit.h"
#include "external.h"
#include "mem.h"

const std::unordered_map<std::string, external_func_t> impl_list {
	{"malloc", emu_malloc},
	{"free", emu_free},
//	std::pair<std::string, external_func_t>("malloc", emu_malloc),
//	std::pair<std::string, external_func_t>("free", emu_free)
};

const EmuVal* call_external(std::string s){
	auto it = impl_list.find(s);
	if(it == impl_list.end()){
		err_exit("External function not implemented");
	}
	return (it->second)();
}

const EmuVal* emu_malloc(void){
	const auto it = stack_vars.front().find("arg 1");
	if(it == stack_vars.front().end()){
		err_exit("Malloc requires an argument");
	}
	const EmuVal* _arg = from_lvalue(it->second);
	if(_arg->obj_type.getCanonicalType() != ULongType){
		err_exit("Malloc requires an unsigned long\n");
	}
	const EmuULong* arg = (const EmuULong*)_arg;
	bool def = (arg->status == STATUS_DEFINED);
	if(!def){
		err_undef();
	}
	uint32_t num_bytes = arg->val;
	mem_block *newblock = new mem_block(MEM_TYPE_HEAP, num_bytes);
	delete arg;
	return new EmuPtr(mem_ptr(newblock, 0), VoidPtrType);
}

const EmuVal* emu_free(void){
	auto it = stack_vars.front().find("arg 1");
	if(it == stack_vars.front().end()){
		err_exit("Free requires an argument");
	}
	const EmuVal* _arg = from_lvalue(it->second);
	if(!_arg->obj_type.getTypePtr()->isPointerType()){
		err_exit("Free requires a pointer");
	}
	const EmuPtr* arg = (const EmuPtr*)_arg;
	bool def = (arg->status == STATUS_DEFINED);
	if(!def){
		err_undef();
	}
	mem_block* block = arg->u.block;
	delete arg;
	delete block;
	return new EmuVoid();
}
