#include <limits.h>
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/APInt.h"
#include "llvm/Support/raw_ostream.h"
#include "types.h"

const llvm::APInt EMU_MAX_INT(32, INT_MAX, false);
const llvm::APInt EMU_MIN_INT(32, (uint64_t)INT_MIN, true);

EmuVal::EmuVal(status_t s, int t)
	:status(s),obj_type(t)
{
}

void EmuVal::print(void){
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
	:EmuVal(s,EMU_TYPE_INT),repr_type(EMU_TYPE_INT)
{
}

EmuInt::EmuInt(int i)
	:EmuVal(STATUS_DEFINED,EMU_TYPE_INT),val(i),repr_type(EMU_TYPE_INT)
{
}

size_t EmuInt::size(void){
	// 4 for type tag, 4 for value
	return sizeof(obj_type)+sizeof(val);
}

void EmuInt::print_impl(void){
	llvm::outs() << val;
}

void EmuInt::dump_repr(void* p){
	emu_type_t* type_ptr = (emu_type_t*) p;
	int* val_ptr = (int*)(type_ptr+1);
	switch(status){
	case STATUS_DEFINED:
	case STATUS_UNDEFINED:
		*type_ptr = repr_type;
		*val_ptr = val;
		break;
	case STATUS_UNINITIALIZED:
		*type_ptr = EMU_TYPE_INVALID;
		*val_ptr = EMU_TYPE_INVALID;
		break;
	}
}

void EmuInt::set_to_repr(void* p){
	emu_type_t* type_ptr = (emu_type_t*)p;
	int* val_ptr = (int*)(type_ptr+1);
	emu_type_t t = *type_ptr;
	int v = *val_ptr;
	repr_type=t;
	val = v;
	switch(t){
	case EMU_TYPE_INT:
		break;
	case EMU_TYPE_ZERO:
		if(v==0) break;
	default:
		status = STATUS_UNDEFINED;
	}
}


EmuPtr::EmuPtr(status_t s)
	: EmuVal(s,EMU_TYPE_PTR),pointer(nullptr,0)
{
}

EmuPtr::EmuPtr(mem_block* b, size_t o)
	: EmuVal(STATUS_DEFINED,EMU_TYPE_PTR),pointer(b,o)
{
}

size_t EmuPtr::size(void){
	return sizeof(obj_type)+sizeof(pointer.block->id)+sizeof(pointer.offset);
}

void EmuPtr::print_impl(void){
	llvm::outs() << "<ptr to block " << pointer.block->id << " offset " << pointer.offset << ">";
}

void EmuPtr::dump_repr(void* p){
	emu_type_t* type_ptr = (emu_type_t*) p;
	block_id_t* bid_ptr = (block_id_t*)(type_ptr+1);
	size_t* offset_ptr = (size_t*)(bid_ptr+1);
	switch(status){
	case STATUS_DEFINED:
		{
		*type_ptr = EMU_TYPE_PTR;
		mem_block *block = pointer.block;
		if(block == nullptr){
			*bid_ptr = 0;
		} else {
			*bid_ptr = block->id;
		}
		*offset_ptr = pointer.offset;
		break;
		}
	case STATUS_UNDEFINED:
		*type_ptr = repr_type;
		*bid_ptr = repr_bid;
		*offset_ptr = pointer.offset;
		break;
	case STATUS_UNINITIALIZED:
		*type_ptr = EMU_TYPE_INVALID;
		*bid_ptr = BLOCK_ID_INVALID;
		*offset_ptr = SIZE_MAX;
	}
}

void EmuPtr::set_to_repr(void* p){
	emu_type_t* type_ptr = (emu_type_t*) p;
	block_id_t* bid_ptr = (block_id_t*)(type_ptr+1);
	size_t* offset_ptr = (size_t*)(bid_ptr+1);

	pointer.offset = *offset_ptr;

	emu_type_t t = *type_ptr;
	repr_type=t;
	if(t != EMU_TYPE_PTR){
		status = STATUS_UNDEFINED;
		repr_bid = *bid_ptr;
	} else {
		block_id_t id = *bid_ptr;
		size_t offset = *offset_ptr;
		pointer.offset = offset;
		rbnode<mem_block>* block_node = active_memory.findBelow(id);
		mem_block* block = &block_node->value;
		pointer.block = block;
		if(block != nullptr){
			rbnode<mem_range>* range = block->ranges.findBelow(offset);
			if(range != nullptr){
				status = STATUS_DEFINED;
				block->add_ref(this);
				return;
			}
		}
		status = STATUS_UNDEFINED;
		repr_bid = id;
	}
}

EmuFunc::EmuFunc(const char* n)
	:EmuVal(STATUS_DEFINED,EMU_TYPE_FUNC),name(n)
{
}

size_t EmuFunc::size(void){
	return 0;
}

void EmuFunc::print_impl(void){
	llvm::outs() << "<function " << name << ">";
}

void EmuFunc::dump_repr(void* p) {}
void EmuFunc::set_to_repr(void* p) {}


EmuRaw::EmuRaw(size_t l, void* b)
	:EmuVal(STATUS_DEFINED,EMU_TYPE_RAW),len(s),buf(b)
{
}

EmuRaw::~EmuRaw(void){
	delete[] buf;
}

EmuRaw::size(void){
	return len;
}

EmuRaw::print_impl(void){
	llvm::outs() << "< " << len << " bytes of unknown meaning>";
}

EmuRaw::dump_repr(void* p){
	memcpy(p,buf,len);
}

EmuRaw::set_to_repr(void* p){
	memcpy(buf,p,len);
}
