#include <limits.h>
#include "clang/AST/Decl.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/APInt.h"
#include "llvm/Support/raw_ostream.h"
#include "cast.h"
#include "exit.h"
#include "types.h"

const llvm::APInt EMU_MAX_INT(32, INT_MAX, false);
const llvm::APInt EMU_MIN_INT(32, (uint64_t)INT_MIN, true);

QualType RawType;
QualType BoolType;
QualType CharType;
QualType UCharType;
QualType ShortType;
QualType UShortType;
QualType IntType;
QualType UIntType;
QualType LongType;
QualType ULongType;
QualType LongLongType;
QualType ULongLongType;
QualType VoidType;
QualType VoidPtrType;
QualType BuiltinVaListType;

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

static BuiltinType::Kind get_gen_kind(const EmuNumGeneric* a, const EmuNumGeneric* b){
	QualType qa = a->obj_type.getCanonicalType();
	QualType qb = b->obj_type.getCanonicalType();
	if(!isa<BuiltinType>(qa) || !isa<BuiltinType>(qb)){
		cant_cast();
	}
	const BuiltinType* ba = (const BuiltinType*)qa.getTypePtr();
	if(!ba->isInteger()){
		cant_cast();
	}
	const BuiltinType* bb = (const BuiltinType*)qb.getTypePtr();
	BuiltinType::Kind ans = ba->getKind();
	if(ans != bb->getKind()){
		cant_cast();
	}
	return ans;
}

EmuNumGeneric::EmuNumGeneric(status_t s, num_type_t T)
	: EmuVal(s, type_from_num_type(T)), repr_type_id(id_from_num_type(T))
{
}

EmuNumGeneric::EmuNumGeneric(status_t s, num_type_t T, llvm::APInt v)
	: EmuVal(s, type_from_num_type(T)), val(v), repr_type_id(id_from_num_type(T))
{
}

EmuNumGeneric::~EmuNumGeneric() {}

bool EmuNumGeneric::equals(const EmuNumGeneric* other) const {
	llvm::errs() << "DOUG DEBUG: comparing two numbers:\n";
	val.dump();
	other->val.dump();
	return val == other->val;
}

#define DOOP(str) \
const EmuNumGeneric* EmuNumGeneric::str (const EmuNumGeneric* other) const{ \
	switch(get_gen_kind(this,other)){ \
	case BuiltinType::Bool:      return ((const EmuNum<NUM_TYPE_BOOL>*      )this)->str((const EmuNum<NUM_TYPE_BOOL>*     )other); \
	case BuiltinType::SChar:     return ((const EmuNum<NUM_TYPE_CHAR>*      )this)->str((const EmuNum<NUM_TYPE_CHAR>*     )other); \
	case BuiltinType::UChar:     return ((const EmuNum<NUM_TYPE_UCHAR>*     )this)->str((const EmuNum<NUM_TYPE_UCHAR>*    )other); \
	case BuiltinType::Short:     return ((const EmuNum<NUM_TYPE_SHORT>*     )this)->str((const EmuNum<NUM_TYPE_SHORT>*    )other); \
	case BuiltinType::UShort:    return ((const EmuNum<NUM_TYPE_USHORT>*    )this)->str((const EmuNum<NUM_TYPE_USHORT>*   )other); \
	case BuiltinType::Int:       return ((const EmuNum<NUM_TYPE_INT>*       )this)->str((const EmuNum<NUM_TYPE_INT>*      )other); \
	case BuiltinType::UInt:      return ((const EmuNum<NUM_TYPE_UINT>*      )this)->str((const EmuNum<NUM_TYPE_UINT>*     )other); \
	case BuiltinType::Long:      return ((const EmuNum<NUM_TYPE_LONG>*      )this)->str((const EmuNum<NUM_TYPE_LONG>*     )other); \
	case BuiltinType::ULong:     return ((const EmuNum<NUM_TYPE_ULONG>*     )this)->str((const EmuNum<NUM_TYPE_ULONG>*    )other); \
	case BuiltinType::LongLong:  return ((const EmuNum<NUM_TYPE_LONGLONG>*  )this)->str((const EmuNum<NUM_TYPE_LONGLONG>* )other); \
	case BuiltinType::ULongLong: return ((const EmuNum<NUM_TYPE_ULONGLONG>* )this)->str((const EmuNum<NUM_TYPE_ULONGLONG>*)other); \
	default: \
		cant_cast(); \
	} \
} \

DOOP(add)
DOOP(sub)
DOOP(mul)
DOOP(div)
DOOP(_or)
DOOP(_and)

// Uses little endian

/*
EmuInt::~EmuInt(void)
{
}

// 4 for type tag, 4 for value
const size_t EMU_SIZE_INT = sizeof(emu_type_id_t)+sizeof(EmuInt::val);

size_t EmuInt::size(void) const{
	return EMU_SIZE_INT;
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
		*type_ptr = EMU_TYPE_INT_ID | EMU_TYPE_UNINIT_MASK;
		*val_ptr = EMU_TYPE_INVALID_ID;
		break;
	}
}

const EmuInt* EmuInt::add(const EmuInt* other) const{
	status_t s1 = status;
	status_t s2 = other->status;
	if(s1 == STATUS_UNDEFINED || s2 == STATUS_UNDEFINED){
		return new EmuInt(STATUS_UNDEFINED);
	}
	if(s1 == STATUS_UNINITIALIZED || s2 == STATUS_UNINITIALIZED){
		return new EmuInt(STATUS_UNINITIALIZED);
	}
	return new EmuInt(val+other->val);
}

const EmuInt* EmuInt::subtract(const EmuInt* other) const{
	status_t s1 = status;
	status_t s2 = other->status;
	if(s1 == STATUS_UNDEFINED || s2 == STATUS_UNDEFINED){
		return new EmuInt(STATUS_UNDEFINED);
	}
	if(s1 == STATUS_UNINITIALIZED || s2 == STATUS_UNINITIALIZED){
		return new EmuInt(STATUS_UNINITIALIZED);
	}
	return new EmuInt(val-other->val);
}

// assumes STATUS_DEFINED
const EmuVal* EmuInt::cast_to(QualType t) const{
	const Type* canon = t.getCanonicalType().getTypePtr();
	if(!isa<BuiltinType>(canon)){
		cant_cast();
	}

	const BuiltinType* ty = (const BuiltinType*)canon;
	switch(ty->getKind()){
	case BuiltinType::Int:
		return new EmuInt(val);
	case BuiltinType::ULong:
		if(val < 0){
			return new EmuULong(STATUS_UNDEFINED);
		} else {
			return new EmuULong((uint32_t)val);
		}
	case BuiltinType::Bool:
	case BuiltinType::Void:
	case BuiltinType::UChar:
	case BuiltinType::Char16:
	case BuiltinType::UShort:
	case BuiltinType::UInt:
	case BuiltinType::ULongLong:
	case BuiltinType::UInt128:
	case BuiltinType::Short:
	case BuiltinType::Long:
	case BuiltinType::LongLong:
	case BuiltinType::Int128:
	case BuiltinType::Half:
	case BuiltinType::Float:
	case BuiltinType::Double:
	case BuiltinType::LongDouble:
	case BuiltinType::NullPtr:
	case BuiltinType::ObjCId:
	case BuiltinType::ObjCClass:
	case BuiltinType::ObjCSel:
	case BuiltinType::OCLImage1d:
	case BuiltinType::OCLImage1dArray:
	case BuiltinType::OCLImage1dBuffer:
	case BuiltinType::OCLImage2d:
	case BuiltinType::OCLImage2dArray:
	case BuiltinType::OCLImage3d:
	case BuiltinType::OCLSampler:
	case BuiltinType::OCLEvent:
	case BuiltinType::Dependent:
	case BuiltinType::Overload:
	case BuiltinType::BoundMember:
	case BuiltinType::PseudoObject:
	case BuiltinType::UnknownAny:
	case BuiltinType::BuiltinFn:
	case BuiltinType::ARCUnbridgedCast:
	default:
		cant_cast();
	}
}


EmuULong::EmuULong(status_t s)
	:EmuVal(s,ULongType),repr_type_id(EMU_TYPE_ULONG_ID)
{
}

EmuULong::EmuULong(uint32_t i)
	:EmuVal(STATUS_DEFINED,ULongType),val(i),repr_type_id(EMU_TYPE_ULONG_ID)
{
}

EmuULong::EmuULong(const void* p)
	: EmuVal(STATUS_UNDEFINED, ULongType)
{
	const emu_type_id_t* type_ptr = (const emu_type_id_t*)p;
	const uint32_t* val_ptr = (const uint32_t*)(type_ptr+1);
	emu_type_id_t t = *type_ptr;
	uint32_t v = *val_ptr;
	repr_type_id=t;
	val = v;
	switch(t){
	case EMU_TYPE_ULONG_ID | EMU_TYPE_UNINIT_MASK:
		status = STATUS_UNINITIALIZED;
	case EMU_TYPE_ULONG_ID:
		break;
	case EMU_TYPE_ZERO_ID:
		if(v==0) break;
	default:
		status = STATUS_UNDEFINED;
		return;
	}
	status = STATUS_DEFINED;
}

EmuULong::~EmuULong(void)
{
}

const size_t EMU_SIZE_ULONG = sizeof(emu_type_id_t)+sizeof(EmuULong::val);

size_t EmuULong::size(void) const{
	// 4 for type tag, 4 for value
	return sizeof(repr_type_id)+sizeof(val);
}

void EmuULong::print_impl(void) const{
	llvm::outs() << val;
}

void EmuULong::dump_repr(void* p) const{
	emu_type_id_t* type_ptr = (emu_type_id_t*)p;
	uint32_t* val_ptr = (uint32_t*)(type_ptr+1);
	switch(status){
	case STATUS_DEFINED:
	case STATUS_UNDEFINED:
		*type_ptr = repr_type_id;
		*val_ptr = val;
		break;
	case STATUS_UNINITIALIZED:
		*type_ptr = EMU_TYPE_ULONG_ID | EMU_TYPE_UNINIT_MASK;
		*val_ptr = EMU_TYPE_INVALID_ID;
		break;
	}
}

// assumes STATUS_DEFINED
const EmuVal* EmuULong::cast_to(QualType t) const{
	if(t.getCanonicalType() == ULongType){
		return new EmuULong(val);
	}
	cant_cast();
}

const EmuULong* EmuULong::multiply(const EmuULong* other) const{
	status_t s1 = status;
	status_t s2 = other->status;
	if(s1 == STATUS_UNDEFINED || s2 == STATUS_UNDEFINED){
		return new EmuULong(STATUS_UNDEFINED);
	}
	if(s1 == STATUS_UNINITIALIZED || s2 == STATUS_UNINITIALIZED){
		return new EmuULong(STATUS_UNINITIALIZED);
	}
	return new EmuULong(val*other->val);
}
*/

EmuPtr::EmuPtr(status_t s, QualType t)
	: EmuVal(s,t),offset(0)
{
}

EmuPtr::EmuPtr(mem_ptr ptr, QualType t)
	: EmuVal(STATUS_DEFINED, t),offset(ptr.offset)
{
	u.block = ptr.block;
}

EmuPtr::EmuPtr(const void* p, QualType qt)
	: EmuVal(STATUS_UNDEFINED, qt)
{
	const emu_type_id_t* type_ptr = (const emu_type_id_t*) p;
	const block_id_t* bid_ptr = (const block_id_t*)(type_ptr+1);
	const size_t* offset_ptr = (const size_t*)(bid_ptr+1);

	emu_type_id_t t = *type_ptr;
	block_id_t id = *bid_ptr;
	offset = *offset_ptr;
	switch(t){
	case EMU_TYPE_PTR_ID | EMU_TYPE_UNINIT_MASK:
		status = STATUS_UNINITIALIZED;
		break;
	case EMU_TYPE_PTR_ID:
	{
		llvm::errs() << "DOUG DEBUG: looking at mem for id "<<id<<" [offset="<<offset<<"]\n";
		auto it = active_mem.find(id);
		if(it != active_mem.end()){
			status = STATUS_DEFINED;
			mem_block* block = it->second;
			u.block = block;
			return;
		}
		// else fall-through
	}
	default:
		status = STATUS_UNDEFINED;
	}
	u.repr.type_id = t;
	u.repr.block_id = id;
}

EmuPtr::~EmuPtr(void) {}

const size_t EMU_SIZE_PTR = sizeof(emu_type_id_t)+sizeof(mem_block::id)+sizeof(EmuPtr::offset);

size_t EmuPtr::size(void) const{
	return EMU_SIZE_PTR;
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
			*bid_ptr = BLOCK_ID_NULL;
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
		*type_ptr = EMU_TYPE_PTR_ID | EMU_TYPE_UNINIT_MASK;
		*bid_ptr = BLOCK_ID_INVALID;
		*offset_ptr = SIZE_MAX;
	}
}

// assumes STATUS_DEFINED
const EmuVal* EmuPtr::cast_to(QualType t) const{
	const Type* ty = t.getTypePtr();
	if(ty->isPointerType() || ty->isArrayType()){
		return new EmuPtr(mem_ptr(u.block, offset), t);
	}
	cant_cast();
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
	:EmuVal(s,t),repr_type_id(EMU_TYPE_FUNC_ID | ((s==STATUS_UNINITIALIZED)?EMU_TYPE_UNINIT_MASK:0)),func_id(0) //0 is invalid func_id
{
}

EmuFunc::EmuFunc(uint32_t f, QualType t)
	:EmuVal(STATUS_DEFINED,t),repr_type_id(EMU_TYPE_FUNC_ID),func_id(f)
{
}

EmuFunc::EmuFunc(const void* p, QualType qt)
	:EmuVal(STATUS_UNDEFINED, qt)
{
	const emu_type_id_t* type_ptr = (const emu_type_id_t*)p;
	const uint32_t* id_ptr = (const uint32_t*)(type_ptr+1);
	emu_type_id_t t = *type_ptr;
	repr_type_id = t;
	uint32_t id = *id_ptr;
	func_id = id;
	switch(t){
		case EMU_TYPE_FUNC_ID | EMU_TYPE_UNINIT_MASK:
			status = STATUS_UNINITIALIZED;
			break;
		case EMU_TYPE_FUNC_ID:
			if(global_functions.find(id) != global_functions.end()){
				status = STATUS_DEFINED;
				break;
			}
			// else fall-through
		default:
		status = STATUS_UNDEFINED;
	}
}

EmuFunc::EmuFunc(const void* p)
	: EmuFunc(p,RawType)
{
}

EmuFunc::~EmuFunc(void){}

const size_t EMU_SIZE_FUNC = sizeof(emu_type_id_t)+sizeof(EmuFunc::func_id);

size_t EmuFunc::size(void) const{
	return EMU_SIZE_FUNC;
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

// assumes STATUS_DEFINED
const EmuVal* EmuFunc::cast_to(QualType t) const{
	if(t.getTypePtr()->isFunctionType()){
		return new EmuFunc(func_id, t);
	}
	cant_cast();
}


EmuVoid::EmuVoid(void)
	: EmuVal(STATUS_DEFINED, VoidType)
{
}

size_t EmuVoid::size(void) const{
	err_exit("Tried to get size of void");
}

void EmuVoid::print_impl(void) const{
	llvm::outs() << "<void>";
}

void EmuVoid::dump_repr(void*) const{
	err_exit("Tried to write void to memory");
}

const EmuVal* EmuVoid::cast_to(QualType) const{
	err_exit("Tried to cast void");
}

const size_t EMU_SIZE_STACKPOS = sizeof(EmuStackPos::repr_type_id)+sizeof(EmuStackPos::level)+sizeof(EmuStackPos::num);

EmuStackPos::EmuStackPos(unsigned int l, unsigned int n)
	: EmuVal(STATUS_DEFINED,BuiltinVaListType), repr_type_id(EMU_TYPE_STACKPOS_ID), level(l), num(n)
{
}

EmuStackPos::EmuStackPos(const void* p)
	: EmuVal(STATUS_UNDEFINED,BuiltinVaListType)
{
	const emu_type_id_t* type = (const emu_type_id_t*)p;
	const int* lptr = (const int*)(type+1);
	const int* nptr = (const int*)(lptr+1);
	
	emu_type_id_t t = *type;
	level = *lptr;
	num = *nptr;
	repr_type_id = t;
	switch(t){
	case EMU_TYPE_STACKPOS_ID | EMU_TYPE_UNINIT_MASK:
		status = STATUS_UNINITIALIZED;
		break;
	case EMU_TYPE_STACKPOS_ID:
		status = STATUS_DEFINED;
		break;
	default:
		status = STATUS_UNDEFINED;
		break;
	}
}

EmuStackPos::~EmuStackPos(void){}

size_t EmuStackPos::size(void) const{
	return EMU_SIZE_STACKPOS;
}

void EmuStackPos::print_impl(void) const{
	llvm::outs() << "(reference to variable at stack frame "<<level<<" number "<<num<<"\n";
}

void EmuStackPos::dump_repr(void* p) const{
	emu_type_id_t* type = (emu_type_id_t*)p;
	int* intout = (int*)(type+1);
	*type = repr_type_id;
	intout[0] = level;
	intout[1] = num;
}

const EmuVal* EmuStackPos::cast_to(QualType t) const{
	cant_cast();
}

EmuStruct::EmuStruct(status_t s, QualType qt, unsigned int n, const EmuVal** m)
	: EmuVal(s, qt), members(m), num(n)
{
	switch(s){
	case STATUS_DEFINED:
		repr_type_id = EMU_TYPE_STRUCT_ID;
		break;
	case STATUS_UNINITIALIZED:
		repr_type_id = EMU_TYPE_STRUCT_ID | EMU_TYPE_UNINIT_MASK;
		break;
	case STATUS_UNDEFINED:
		repr_type_id = EMU_TYPE_INVALID_ID;
		break;
	}
	llvm::errs() << "DOUG DEBUG: new emustruct at "<<((void*)this)<<"\n";
}

// special case: if null pointer, just makes them up
EmuStruct::EmuStruct(lvalue l)
	: EmuVal(STATUS_DEFINED,l.type), members()
{
	status_t tempstatus;

	if(l.ptr.block != nullptr){
		size_t space = l.ptr.block->size - l.ptr.offset;
		if(space < sizeof(emu_type_id_t)) err_exit("Tried to read undefined memory");

		const emu_type_id_t* type_ptr = (const emu_type_id_t*)l.ptr.block->data;
		emu_type_id_t t = *type_ptr;
		repr_type_id = t;

		switch(t){
			case EMU_TYPE_STRUCT_ID | EMU_TYPE_UNINIT_MASK:
				tempstatus = STATUS_UNINITIALIZED;
				break;
			case EMU_TYPE_STRUCT_ID:
				tempstatus = STATUS_DEFINED;
				break;
			default:
				tempstatus = STATUS_UNDEFINED;
		}
	} else {
		tempstatus = STATUS_UNINITIALIZED;
		repr_type_id = EMU_TYPE_STRUCT_ID | EMU_TYPE_UNINIT_MASK;
	}
	size_t o = sizeof(emu_type_id_t);

	const TagDecl* defn = ((const RecordType*)l.type.getCanonicalType().getTypePtr())->getDecl()->getDefinition();
	if(defn == nullptr){
		llvm::outs() << "\n\n";
		l.type.getTypePtr()->dump();
		llvm::outs() << "\n\n";
		l.type.getCanonicalType().getTypePtr()->dump();
		llvm::outs() << "\n\n";
		const RecordType* r = (const RecordType*)l.type.getCanonicalType().getTypePtr();
		r->dump();
		llvm::outs() << "\n\n";
		r->getDecl()->dump();
		llvm::outs() << "\n\n";
		err_exit("Dealing with undefined record");
	}
	unsigned int fields = 0;
	for(auto it = defn->decls_begin(); it != defn->decls_end(); it++){
		fields++;
	}
	const EmuVal** temp_members = new const EmuVal*[fields];
	auto it = defn->decls_begin();
	for(unsigned int i=0; i<fields; i++){
		const Decl* d = *it;
		if(!isa<ValueDecl>(d)){
			err_exit("Error with record member");
		}
		const ValueDecl* vd = (const ValueDecl*)d;
		llvm::errs() << "DOUG DEBUG: trying to interpret field of decl ";
		vd->dump();
		llvm::errs() << " and offset " << (l.ptr.offset+o) << "\n";
		const EmuVal* temp = from_lvalue(lvalue(l.ptr.block, vd->getType(), l.ptr.offset+o));
		temp_members[i] = temp;
		status_t s = temp->status;
		if(s != tempstatus){
			if(s == STATUS_UNDEFINED){
				tempstatus = s;
			} else if(tempstatus != STATUS_UNDEFINED && s != STATUS_DEFINED){
				tempstatus = s;
			}
		}
	}
	members = temp_members;
	num = fields;
	if(tempstatus != STATUS_DEFINED) status = tempstatus;
}

EmuStruct::~EmuStruct(void) {
	if(STATUS_DEFINED){
		for(unsigned int i = 0; i < num; i++){
			delete members[i];
		}
		delete members;
	}
}

size_t EmuStruct::size(void) const {
	size_t ans = sizeof(repr_type_id);
	for(unsigned int i = 0; i < num; i++){
		ans += members[i]->size();
	}
	return ans;
}

void EmuStruct::print_impl(void) const{
	llvm::outs() << "<struct>";
}

void EmuStruct::dump_repr(void* p) const{
	emu_type_id_t* type_ptr = (emu_type_id_t*)p;
	*type_ptr = repr_type_id;
	size_t offset = sizeof(repr_type_id);
	char* ptr = (char*)p;
	for(unsigned int i = 0; i < num; i++){
		members[i]->dump_repr((void*)(ptr+offset));
		offset += members[i]->size();
	}
}

const EmuVal* EmuStruct::cast_to(QualType qt) const {
	if(qt.getCanonicalType() != obj_type.getCanonicalType()){
		cant_cast();
	}
	const EmuVal** newmemb = new const EmuVal*[num];
	for(unsigned int i = 0; i < num; i++){
		llvm::errs() << "DOUG DEBUG: struct attempting to copy member "<<i<<" of type\n";
		members[i]->obj_type.dump();
		newmemb[i] = members[i]->cast_to(members[i]->obj_type);
	}
	return new EmuStruct(status, qt, num, newmemb);
}
