#include "clang/AST/Type.h"
#include "llvm/Support/raw_ostream.h"
#include "cast.h"
#include "eval.h"
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

size_t getSizeOf(QualType qt){
	if(qt->isIntegerType()){
		return sizeof(emu_type_id_t)+bytes_in_num_type(getNumType(qt));
	} else if(qt->isPointerType()){
		return EMU_SIZE_PTR;
	} else if(qt->isConstantArrayType()){
		const ConstantArrayType* type = (const ConstantArrayType*)qt->getAsArrayTypeUnsafe();
		size_t n = type->getSize().getLimitedValue();
		size_t sub = getSizeOf(type->getElementType());
		return n*sub+sizeof(emu_type_id_t);
	} else if(qt->isStructureType()){
		const RecordDecl* decl = qt->getAsStructureType()->getDecl();
		unsigned int n=0;
		for(auto it = decl->decls_begin(); it != decl->decls_end(); it++){
			n++;
		}
		size_t ans = sizeof(emu_type_id_t);
		for(auto it = decl->decls_begin(); it != decl->decls_end(); it++){
			ans += getSizeOf(((const ValueDecl*)*it)->getType());
		}
		return ans;
	} else if(qt->isUnionType()){
		const RecordDecl* decl = qt->getAsUnionType()->getDecl();
		unsigned int n=0;
		for(auto it = decl->decls_begin(); it != decl->decls_end(); it++){
			n++;
		}
		size_t ans = 0;
		for(auto it = decl->decls_begin(); it != decl->decls_end(); it++){
			size_t temp = getSizeOf(((const ValueDecl*)*it)->getType());
			if(temp > ans) ans=temp;
		}
		return ans;
	}
	llvm::errs() << "DOUG DEBUG: couldn't get size of unknown type:\n";
	qt.dump();
	cant_cast();
}

static const EmuPtr* newarr(QualType arrtype, const ArrayType* type, size_t num){
	llvm::errs() << "DOUG DEBUG: newarr 0\n";
	const EmuVal* sub = from_lvalue(lvalue(nullptr,type->getElementType(),0));
	llvm::errs() << "DOUG DEBUG: newarr 0.5\n";
	size_t s = sub->size();
	if(s == 0){
		err_exit("Tried to create array of zero-size objects");
	}
	size_t maxnum = SIZE_MAX/s;
	if(num > maxnum){
		err_exit("Array size overflow");
	}
	size_t size = s*num;
	llvm::errs() << "DOUG DEBUG: newarr 1\n";
	mem_block* block = new mem_block(static_init?MEM_TYPE_GLOBAL:MEM_TYPE_STACK, size);
	llvm::errs() << "DOUG DEBUG: newarr 2\n";
	for(size_t i = 0; i < size; i+=s){
		block->write(sub, i);
	}
	llvm::errs() << "DOUG DEBUG: newarr 3\n";
	return new EmuPtr(mem_ptr(block, 0), arrtype);
}

const EmuVal* zero_init(QualType qt){
	if(qt->isIntegerType()){
		num_type_t t = getNumType(qt);
		#define INITZEROCASE(T) case T: return new EmuNum<T>(llvm::APInt(bits_in_num_type(T),0,is_num_type_signed(T)));
		switch(t){
			INITZEROCASE(NUM_TYPE_BOOL)
			INITZEROCASE(NUM_TYPE_CHAR)
			INITZEROCASE(NUM_TYPE_UCHAR)
			INITZEROCASE(NUM_TYPE_SHORT)
			INITZEROCASE(NUM_TYPE_USHORT)
			INITZEROCASE(NUM_TYPE_INT)
			INITZEROCASE(NUM_TYPE_UINT)
			INITZEROCASE(NUM_TYPE_LONG)
			INITZEROCASE(NUM_TYPE_ULONG)
			INITZEROCASE(NUM_TYPE_LONGLONG)
			INITZEROCASE(NUM_TYPE_ULONGLONG)
		}
	} else if(qt->isPointerType()){
		return new EmuPtr(mem_ptr(nullptr,0),qt);
	} else if(qt->isConstantArrayType()){
		const ConstantArrayType* type = (const ConstantArrayType*)qt.getTypePtr();
		unsigned int n = type->getSize().getLimitedValue();
		const EmuVal** arr = new const EmuVal*[n];
		for(unsigned int i=0; i<n; i++){
			const EmuVal* z = zero_init(type->getElementType());
			arr[i] = z;
		}
		EmuStruct* ans = new EmuStruct(STATUS_DEFINED, qt, n, arr);
		ans->repr_type_id = 0;
		return ans;
	} else if(qt->isStructureType()){
		const RecordDecl* decl = qt->getAsStructureType()->getDecl();
		unsigned int n = 0;
		for(auto it = decl->decls_begin(); it != decl->decls_end(); it++){
			n++;
		}
		const EmuVal** arr = new const EmuVal*[n];
		unsigned int i=0;
		for(auto it = decl->decls_begin(); it != decl->decls_end(); it++){
			arr[i++] = zero_init(((const ValueDecl*)*it)->getType());
		}
		EmuStruct* ans = new EmuStruct(STATUS_DEFINED, qt, n, arr);
		ans->repr_type_id = 0;
		return ans;
	} else if(qt->isUnionType()){
		const RecordDecl* decl = qt->getAsUnionType()->getDecl();
		return zero_init(((const ValueDecl*)*decl->decls_begin())->getType());
	}
	cant_cast();
}

// caller must free returned pointer
const EmuVal* from_lvalue(lvalue l){
	QualType qt = l.type.getCanonicalType();
	llvm::errs() << "DOUG DEBUG: from_lvalue called, type is ";
	qt.dump();
	llvm::errs() << "\n";
	void* loc;
	size_t space;

	if(l.ptr.block == nullptr){
		loc = nullptr;
	} else {
		if(l.ptr.block->memtype == MEM_TYPE_FREED){
			err_exit("Tried to load from freed memory\n");
		}
		size_t s = l.ptr.block->size;
		size_t o = l.ptr.offset;
		if(o > s){
			llvm::errs() << "\n\nDEBUG: " << o << ">" << s << "\n";
			err_exit("Tried to read from invalid location\n");
		}
		loc = (void*)&((char*)l.ptr.block->data)[l.ptr.offset];
		space = s-o;
	}

	llvm::errs() << "DOUG DEBUG: from_lvalue at 1\n";

	if(qt->isIntegerType()){
		#define DOCASE(N) case N: { \
			if(loc == nullptr) return new EmuNum<N>(STATUS_UNINITIALIZED); \
			if(space < bytes_in_num_type(N)) bad_memread(); \
			return new EmuNum<N>(loc); \
		}

		num_type_t t = getNumType(qt);
		switch(t){
			DOCASE(NUM_TYPE_BOOL)
			DOCASE(NUM_TYPE_CHAR)
			DOCASE(NUM_TYPE_UCHAR)
			DOCASE(NUM_TYPE_SHORT)
			DOCASE(NUM_TYPE_USHORT)
			DOCASE(NUM_TYPE_INT)
			DOCASE(NUM_TYPE_UINT)
			DOCASE(NUM_TYPE_LONG)
			DOCASE(NUM_TYPE_ULONG)
			DOCASE(NUM_TYPE_LONGLONG)
			DOCASE(NUM_TYPE_ULONGLONG)
		default:
			llvm::errs() << "DOUG DEBUG: arith type not supported?\n";
			cant_cast();
		}
	}

	const Type* ty = qt.getTypePtr();
	llvm::errs() << "DOUG DEBUG: from_lvalue at 2:";
	ty->dump();
	llvm::errs() << "\n";
	if (ty->isPointerType()){
		if(loc == nullptr) return new EmuPtr(STATUS_UNINITIALIZED, l.type);
		if(space < EMU_SIZE_PTR) bad_memread();
		llvm::errs() << "DOUG DEBUG: returning EmuPtr with storage at block id="<< l.ptr.block->id <<" and offset "<<l.ptr.offset<<"\n";
		return new EmuPtr(loc, l.type);
	} else if (ty->isConstantArrayType()){
		if(loc == nullptr){
			const ConstantArrayType *type = (const ConstantArrayType*)ty;
			return newarr(qt, type, type->getSize().getLimitedValue());
		}
		if(space < EMU_SIZE_PTR){
			llvm::errs() << "DOUG DEBUG: "<<space<<" vs "<<EMU_SIZE_PTR<<"\n";
			bad_memread();
		}
		return new EmuPtr(loc, l.type);
	} else if (ty->isVariableArrayType()){
		if(loc == nullptr){
			const VariableArrayType *type = (const VariableArrayType*)ty;
			const EmuVal* temp = eval_rexpr(type->getSizeExpr());
			const Type* t = temp->obj_type.getCanonicalType().getTypePtr();
			if(!isa<BuiltinType>(t) || (((const BuiltinType*)t)->getKind() != BuiltinType::Int)){
				cant_handle();
			}
			const llvm::APInt* size = &((const EmuNum<NUM_TYPE_INT>*)temp)->val;
			if(size->isNegative()){
				err_exit("Tried to create negative length array\n");
			}
			if(size->ugt(SIZE_MAX)){
				err_exit("Requested array size too big\n");
			}
			delete temp;
			return newarr(qt, type, (size_t)size->getLimitedValue());
		}
		if(space < EMU_SIZE_PTR) bad_memread();
		return new EmuPtr(loc, l.type);
	} else if (ty->isFunctionType()){
		if(loc == nullptr) return new EmuFunc(STATUS_UNINITIALIZED, l.type);
		if(space < EMU_SIZE_FUNC){
			bad_memread();
		}
		return new EmuFunc(loc, l.type);
	} else if (ty->isRecordType()){
		// null specially handled by the struct impl, just pass the lvalue
		return new EmuStruct(l);
	}
	llvm::errs() << "\n\nDEBUG: Couldn't get lvalue from type:\n";
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
		return ((const EmuNum<NUM_TYPE_INT>*)v)->val == 0;
	}
	cant_handle();
}

