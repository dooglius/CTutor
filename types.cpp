#include <limits.h>
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/APInt.h"
#include "llvm/Support/raw_ostream.h"
#include "exit.h"
#include "types.h"

const llvm::APInt EMU_MAX_INT(32, INT_MAX, false);
const llvm::APInt EMU_MIN_INT(32, (uint64_t)INT_MIN, true);

QualType RawPtrType;
QualType RawType;
QualType BoolType;
QualType IntType;

EmuVal::EmuVal(status_t s, QualType t)
	:status(s),obj_type(t)
{
}

EmuVal::~EmuVal(void) {}

void EmuVal::print(void) const{
	switch(status){
	case STATUS_UNDEFINED:
		llvm::outs() << "(undefined)";
		break;
	case STATUS_UNINITIALIZED:
		llvm::outs() << "(uninitialized)";
		break;
	case STATUS_DEFINED:
		print_impl();
	}
}

EmuInt::EmuInt(status_t s)
	:EmuVal(s,IntType),repr_type_id(EMU_TYPE_INT_ID)
{
}

EmuInt::EmuInt(int32_t i)
	:EmuVal(STATUS_DEFINED,IntType),val(i),repr_type_id(EMU_TYPE_INT_ID)
{
}

EmuInt::~EmuInt(void) {}

size_t EmuInt::size(void) const{
	// 4 for type tag, 4 for value
	return sizeof(repr_type_id)+sizeof(val);
}

void EmuInt::print_impl(void) const{
	llvm::outs() << val;
}

void EmuInt::dump_repr(void* p) const{
	emu_type_id_t* type_ptr = (emu_type_id_t*)p;
	int* val_ptr = (int*)(type_ptr+1);
	switch(status){
	case STATUS_DEFINED:
	case STATUS_UNDEFINED:
		*type_ptr = repr_type_id;
		*val_ptr = val;
		break;
	case STATUS_UNINITIALIZED:
		*type_ptr = EMU_TYPE_INVALID_ID;
		*val_ptr = EMU_TYPE_INVALID_ID;
		break;
	}
}

void EmuInt::set_to_repr(const void* p) {
	const emu_type_id_t* type_ptr = (const emu_type_id_t*)p;
	const int32_t* val_ptr = (const int32_t*)(type_ptr+1);
	emu_type_id_t t = *type_ptr;
	int32_t v = *val_ptr;
	repr_type_id=t;
	val = v;
	switch(t){
	case EMU_TYPE_INT_ID:
		break;
	case EMU_TYPE_ZERO_ID:
		if(v==0) break;
	default:
		status = STATUS_UNDEFINED;
		return;
	}
	status = STATUS_DEFINED;
}


EmuPtr::EmuPtr(status_t s)
	: EmuVal(s,RawPtrType),offset(0)
{
	u.block = nullptr;
}

EmuPtr::EmuPtr(mem_ptr ptr)
	: EmuVal(STATUS_DEFINED,RawPtrType),offset(ptr.offset)
{
	u.block = ptr.block;
}

EmuPtr::~EmuPtr(void) {}

size_t EmuPtr::size(void) const{
	return sizeof(u.repr.type_id)+sizeof(u.block->id)+sizeof(offset);
}

void EmuPtr::print_impl(void) const{
	llvm::outs() << "<ptr to block " << u.block->id << " offset " << offset << ">";
}

void EmuPtr::dump_repr(void* p) const{
	emu_type_id_t* type_ptr = (emu_type_id_t*) p;
	block_id_t* bid_ptr = (block_id_t*)(type_ptr+1);
	size_t* offset_ptr = (size_t*)(bid_ptr+1);
	switch(status){
	case STATUS_DEFINED:
	{
		*type_ptr = EMU_TYPE_PTR_ID;
		mem_block *block = u.block;
		if(block == nullptr){
			*bid_ptr = 0;
		} else {
			*bid_ptr = block->id;
		}
		*offset_ptr = offset;
		break;
	}
	case STATUS_UNDEFINED:
		*type_ptr = u.repr.type_id;
		*bid_ptr = u.repr.block_id;
		*offset_ptr = offset;
		break;
	case STATUS_UNINITIALIZED:
		*type_ptr = EMU_TYPE_INVALID_ID;
		*bid_ptr = BLOCK_ID_INVALID;
		*offset_ptr = SIZE_MAX;
	}
}

void EmuPtr::set_to_repr(const void* p){
	const emu_type_id_t* type_ptr = (const emu_type_id_t*) p;
	const block_id_t* bid_ptr = (const block_id_t*)(type_ptr+1);
	const size_t* offset_ptr = (const size_t*)(bid_ptr+1);

	emu_type_id_t t = *type_ptr;
	block_id_t id = *bid_ptr;
	offset = *offset_ptr;
	if(t == EMU_TYPE_PTR_ID){
		auto it = active_mem.find(id);
		if(it != active_mem.end()){
			status = STATUS_DEFINED;
			mem_block* block = it->second;
			u.block = block;
			return;
		}
	}
	status = STATUS_UNDEFINED;
	u.repr.type_id = t;
	u.repr.block_id = id;
}
/*
EmuArr::EmuArr(status_t s, unsigned int n)
	: EmuPtr(s), num(n)
{
}

EmuArr::EmuArr(mem_ptr p, unsigned int n)
	: EmuPtr(p), num(n)
{
}
*/
EmuFunc::EmuFunc(status_t s, QualType t)
	:EmuVal(s,t),repr_type_id(EMU_TYPE_FUNC_ID),func_id(0) //0 is invalid func_id
{
}

EmuFunc::EmuFunc(uint32_t f, QualType t)
	:EmuVal(STATUS_DEFINED,t),repr_type_id(EMU_TYPE_FUNC_ID),func_id(f)
{
}

EmuFunc::~EmuFunc(void){}

size_t EmuFunc::size(void) const{
	return sizeof(repr_type_id)+sizeof(func_id);
}

void EmuFunc::print_impl(void) const{
	llvm::outs() << "<function machine code>";
}

void EmuFunc::dump_repr(void* p) const {
	emu_type_id_t* type_ptr = (emu_type_id_t*)p;
	uint32_t* id_ptr = (uint32_t*)(type_ptr+1);
	*type_ptr = repr_type_id;
	*id_ptr = func_id;
}

void EmuFunc::set_to_repr(const void* p) {
	const emu_type_id_t* type_ptr = (const emu_type_id_t*)p;
	const uint32_t* id_ptr = (const uint32_t*)(type_ptr+1);
	emu_type_id_t t = *type_ptr;
	repr_type_id = t;
	uint32_t id = *id_ptr;
	func_id = id;
	bool def = (t == EMU_TYPE_FUNC_ID && global_functions.find(id) != global_functions.end());
	status = def?STATUS_DEFINED:STATUS_UNDEFINED;
}
/*
EmuVoid::EmuVoid(void) {}

size_t EmuVoid::size(void){
	err_exit("Tried to get size of void");
}

void EmuVoid::print_impl(void) const{
	llvm::outs() << "<void>";
}

void EmuVoid::dump_repr(void*){}

void EmuVoid::set_to_repr(const void*){
	err_exit("Tried to load value of void\n");
}
*/
EmuBool::EmuBool(status_t s)
	: EmuVal(s,BoolType), repr_type_id(EMU_TYPE_BOOL_ID), value(2)
{
}

EmuBool::EmuBool(bool b)
	: EmuVal(STATUS_DEFINED,BoolType), repr_type_id(EMU_TYPE_BOOL_ID), value(b?1:0)
{
}

EmuBool::~EmuBool(void){}

size_t EmuBool::size(void) const{
	return sizeof(repr_type_id)+sizeof(value);
}

void EmuBool::print_impl(void) const{
	llvm::outs() << (value?"true\n":"false\n");
}

void EmuBool::dump_repr(void* p) const{
	emu_type_id_t* type_ptr = (emu_type_id_t*)p;
	char* value_ptr = (char*)(type_ptr+1);
	*type_ptr = repr_type_id;
	*value_ptr = value;
}

void EmuBool::set_to_repr(const void* p){
	const emu_type_id_t* type_ptr = (const emu_type_id_t*)p;
	const char* value_ptr = (const char*)(type_ptr+1);
	emu_type_id_t t = *type_ptr;
	char v = *value_ptr;
	bool def = (v == 0 || v == 1) && t == EMU_TYPE_BOOL_ID;
	repr_type_id = t;
	value = v;
	status = def?STATUS_DEFINED:STATUS_UNDEFINED;
}
