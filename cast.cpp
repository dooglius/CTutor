#include "clang/AST/Type.h"
#include "llvm/Support/raw_ostream.h"
#include "cast.h"
#include "exit.h"
#include "main.h"
#include "types.h"

using namespace clang;

// caller responsible for freeing the result
// note that returned could be equal to original
const EmuVal* cast_to(const EmuVal* orig, QualType t){
	const Type* to = t.getCanonicalType().getTypePtr();
	const Type* from = orig->obj_type.getCanonicalType().getTypePtr();
	if(to == from){
		return orig;
	}
	if(isa<PointerType>(to)){
		if(isa<PointerType>(from)){
			
		}
	}
	llvm::outs() << "DEBUG: const cast_to failed between types:\n";
	to->dump();
	from->dump();
	cant_handle();
}

EmuVal* cast_to(EmuVal* orig, QualType t){
	const Type* to = t.getCanonicalType().getTypePtr();
	const Type* from = orig->obj_type.getCanonicalType().getTypePtr();
	if(to == from){
		return orig;
	}
	llvm::outs() << "DEBUG: cast_to failed between types:\n";
	to->dump();
	from->dump();
	cant_handle();
}

static EmuPtr* newarr(const ArrayType* type, size_t num){
	EmuVal* sub = blank_from_type(type->getElementType());
	size_t s = sub->size();
	if(s == 0){
		err_exit("Tried to create array of zero-size objects");
	}
	size_t maxnum = SIZE_MAX/s;
	if(num > maxnum){
		err_exit("Array size overflow");
	}
	size_t size = s*num;
	mem_block* block = new mem_block(MEM_TYPE_STACK, size);
	for(size_t i = 0; i < size; i+=s){
		block->write(sub, i);
	}
	return new EmuPtr(mem_ptr(block, 0));
}

// caller must free
EmuVal* blank_from_type(QualType t){
	const Type *ty = t.getCanonicalType().getTypePtr();
	if(isa<BuiltinType>(ty)){
		const BuiltinType *type = (const BuiltinType*)ty;
		switch(type->getKind()){
		case BuiltinType::Int:
			return new EmuInt(STATUS_UNINITIALIZED);
		default:
			type->dump();
			cant_handle();
		}
	} else if (isa<PointerType>(ty)){
		return new EmuPtr(STATUS_UNINITIALIZED);
	} else if (isa<ConstantArrayType>(ty)){
		const ConstantArrayType *type = (const ConstantArrayType*)ty;
		return newarr(type, type->getSize().getLimitedValue());
	} else if (isa<VariableArrayType>(ty)){
		const VariableArrayType *type = (const VariableArrayType*)ty;
		const EmuVal* temp = eval_rexpr(type->getSizeExpr());
		const Type* t = temp->obj_type.getCanonicalType().getTypePtr();
		if(!isa<BuiltinType>(t) || (((const BuiltinType*)t)->getKind() != BuiltinType::Int)){
			cant_handle();
		}
		size_t size = ((const EmuInt*)temp)->val;
		delete temp;
		return newarr(type, size);
	}
	ty->dump();
	cant_handle();
}

emu_type_id_t type_id_from_type(QualType t){
	const Type *ty = t.getCanonicalType().getTypePtr();
	if(isa<BuiltinType>(ty)){
		const BuiltinType *type = (const BuiltinType*)ty;
		switch(type->getKind()){
		case BuiltinType::Int:
			return EMU_TYPE_INT_ID;
		default:
			type->dump();
			cant_handle();
		}
	} else if (isa<PointerType>(ty) || isa<ArrayType>(ty)){
		return EMU_TYPE_PTR_ID;
	}
	ty->dump();
	cant_handle();
}

// caller must free
EmuVal* from_lvalue(lvalue l){
	void* raw_ptr = (void*)&((char*)l.ptr.block->data)[l.ptr.offset];
	EmuVal* ans = blank_from_type(l.type);
	ans->set_to_repr(raw_ptr);
	return ans;
}

bool is_scalar_zero(const EmuVal* v){
	const Type* t = v->obj_type.getCanonicalType().getTypePtr();
	if(isa<BuiltinType>(t) && (((const BuiltinType*)t)->getKind() == BuiltinType::Int)){
		return ((const EmuInt*)v)->val != 0;
	}
	cant_handle();
}
