#include "cast.h"
#include "exit.h"
#include "external.h"
#include "mem.h"

// To update, modify NUM_EXTERNAL_FUNCTIONS in mem.h
const std::unordered_map<std::string, uint32_t> impl_ids {
	{"malloc", 0},
	{"free", 1},
	{"__builtin_va_start", 2},
};

// bools represent if it is an lvalue-based-macro
const std::vector<std::pair<external_func_t, bool> > impl_list {
	{emu_malloc,false},
	{emu_free,false},
	{emu_va_start,true},
};

const EmuFunc* get_external_func(std::string s, QualType qt){
	auto it = impl_ids.find(s);
	if(it == impl_ids.end()){
		err_exit("External function not implemented");
	}
	return new EmuFunc(it->second, qt);
}

bool is_lvalue_based_macro(uint32_t id){
	return impl_list[id].second;
}

const EmuVal* call_external(uint32_t id){
	return (impl_list[id].first)();
}

const EmuVal* emu_malloc(void){
	const auto vars = stack_vars.back();
	if(vars.size() < 1){
		err_exit("Malloc requires an argument");
	}
	const EmuVal* _arg = from_lvalue(vars[0].second);
	if(_arg->obj_type.getCanonicalType() != ULongType){
		err_exit("Malloc requires an unsigned long\n");
	}
	const EmuNum<NUM_TYPE_ULONG>* arg = (const EmuNum<NUM_TYPE_ULONG>*)_arg;
	bool def = (arg->status == STATUS_DEFINED);
	if(!def){
		err_undef();
	}
	uint64_t num_bytes = arg->val.getLimitedValue();
	mem_block *newblock = new mem_block(MEM_TYPE_HEAP, num_bytes);
	delete arg;
	return new EmuPtr(mem_ptr(newblock, 0), VoidPtrType);
}

const EmuVal* emu_free(void){
	const auto vars = stack_vars.back();
	if(vars.size() < 1){
		err_exit("Free requires an argument");
	}
	const EmuVal* _arg = from_lvalue(vars[0].second);
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

// lvalue-based here, remember not to call from_lvalue on arguments
const EmuVal* emu_va_start(void){
	const auto vars = stack_vars.back();
	if(vars.size() < 2){
		err_exit("va_start requires 2 arguments");
	}
	const lvalue _to = vars[0].second;
	const lvalue _from = vars[1].second;
	const EmuStackPos* to = new EmuStackPos(&((char*)_to.ptr.block->data)[_to.ptr.offset]);
	const EmuStackPos* from = new EmuStackPos(&((char*)_from.ptr.block->data)[_from.ptr.offset]);
	lvalue tostorage = stack_vars[to->level][to->num].second;
	if(tostorage.ptr.block->size < tostorage.ptr.offset + EMU_SIZE_STACKPOS){
		err_exit("Not enough size for va_list storage");
	}
	from->dump_repr(&((char*)tostorage.ptr.block->data)[tostorage.ptr.offset]);

	return new EmuVoid();
}