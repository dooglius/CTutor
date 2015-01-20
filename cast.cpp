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
	if(orig->status != STATUS_DEFINED){
		err_exit("Attempted to cast invalid value");
	}
	return orig->cast_to(t);
}

static const EmuPtr* newarr(QualType arrtype, const ArrayType* type, size_t num){
	const EmuVal* sub = from_lvalue(lvalue(nullptr,type->getElementType(),0));
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
	return new EmuPtr(mem_ptr(block, 0), arrtype);
}

// caller must free
const EmuVal* from_lvalue(lvalue l){
	QualType qt = l.type.getCanonicalType();
	void* loc;
	if(l.ptr.block == nullptr) loc = nullptr;
	else loc = (void*)&((char*)l.ptr.block->data)[l.ptr.offset];

	if(qt == IntType){
		if(loc == nullptr) return new EmuInt(STATUS_UNINITIALIZED);
		else return new EmuInt(loc);
	} else if(qt == ULongType){
		if(loc == nullptr) return new EmuULong(STATUS_UNINITIALIZED);
		else return new EmuULong(loc);
	}

	const Type* ty = qt.getTypePtr();
	if (ty->isPointerType()){
		if(loc == nullptr) return new EmuPtr(STATUS_UNINITIALIZED, l.type);
		else return new EmuPtr(loc, l.type);
	} else if (ty->isConstantArrayType()){
		if(loc == nullptr){
			const ConstantArrayType *type = (const ConstantArrayType*)ty;
			return newarr(qt, type, type->getSize().getLimitedValue());
		} else return new EmuPtr(loc, l.type);
	} else if (ty->isVariableArrayType()){
		if(loc == nullptr){
			const VariableArrayType *type = (const VariableArrayType*)ty;
			const EmuVal* temp = eval_rexpr(type->getSizeExpr());
			const Type* t = temp->obj_type.getCanonicalType().getTypePtr();
			if(!isa<BuiltinType>(t) || (((const BuiltinType*)t)->getKind() != BuiltinType::Int)){
				cant_handle();
			}
			size_t size = ((const EmuInt*)temp)->val;
			delete temp;
			return newarr(qt, type, size);
		} else return new EmuPtr(loc, l.type);
	} else if (ty->isFunctionType()){
		if(loc == nullptr) return new EmuFunc(STATUS_UNINITIALIZED, l.type);
		else return new EmuFunc(loc, l.type);
	}
	ty->dump();
	cant_handle();
}
/*
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
*/
// caller must free

bool is_scalar_zero(const EmuVal* v){
	QualType t = v->obj_type.getCanonicalType();
	if(t == IntType){
		return ((const EmuInt*)v)->val == 0;
	}
	cant_handle();
}
