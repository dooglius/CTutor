#pragma once
#include <stddef.h>
#include <unistd.h>
#include "llvm/ADT/APInt.h"
#include "llvm/Support/raw_ostream.h"
#include "clang/AST/Type.h"
#include "enums.h"
#include "exit.h"
#include "mem.h"

using namespace clang;

extern const llvm::APInt EMU_MIN_INT;
extern const llvm::APInt EMU_MAX_INT;

extern QualType RawType;
extern QualType BoolType;
extern QualType CharType;
extern QualType UCharType;
extern QualType ShortType;
extern QualType UShortType;
extern QualType IntType;
extern QualType UIntType;
extern QualType LongType;
extern QualType ULongType;
extern QualType LongLongType;
extern QualType ULongLongType;
extern QualType VoidType;
extern QualType VoidPtrType;
extern QualType BuiltinVaListType;

extern const size_t EMU_SIZE_PTR;
extern const size_t EMU_SIZE_FUNC;
extern const size_t EMU_SIZE_STACKPOS;

class EmuVal{
public:
	virtual ~EmuVal(void);

	virtual size_t size(void) const = 0;
	virtual void print_impl(void) const = 0;
	virtual void dump_repr(void*) const = 0;
	virtual const EmuVal* cast_to(QualType) const = 0;
	void print(void) const;
	status_t status;
	QualType obj_type;

protected:
	EmuVal(status_t, QualType);
};

class EmuNumGeneric : public EmuVal{
public:
	EmuNumGeneric(status_t, num_type_t);
	EmuNumGeneric(status_t, num_type_t, llvm::APInt);
	
	virtual ~EmuNumGeneric(void);

	const EmuNumGeneric* add(const EmuNumGeneric*) const;
	const EmuNumGeneric* sub(const EmuNumGeneric*) const;
	const EmuNumGeneric* mul(const EmuNumGeneric*) const;
	const EmuNumGeneric* div(const EmuNumGeneric*) const;
	const EmuNumGeneric* _or(const EmuNumGeneric*) const;
	const EmuNumGeneric* _and(const EmuNumGeneric*) const;
	virtual const EmuNumGeneric* lnot(void) const = 0;
	virtual const EmuNumGeneric* neg(void) const = 0;
	bool equals(const EmuNumGeneric*) const;

	llvm::APInt val;
	emu_type_id_t repr_type_id;
};

__attribute__((unused)) static unsigned int bits_in_num_type(num_type_t t) {
        switch(t){
        case NUM_TYPE_BOOL:
                return 1;
        case NUM_TYPE_CHAR:
        case NUM_TYPE_UCHAR:
                return 8;
        case NUM_TYPE_SHORT:
        case NUM_TYPE_USHORT:
                return 16;
        case NUM_TYPE_INT:
        case NUM_TYPE_UINT:
                return 32;
        case NUM_TYPE_LONG:
        case NUM_TYPE_ULONG:
                return 32;
        case NUM_TYPE_LONGLONG:
        case NUM_TYPE_ULONGLONG:
                return 64;
        default:
                cant_cast();
        }
}

__attribute__((unused)) static unsigned int bytes_in_num_type(num_type_t t) {
	return (bits_in_num_type(t)+7)/8;
}

__attribute__((unused)) static QualType type_from_num_type(num_type_t t) {
        switch(t){
        case NUM_TYPE_BOOL:
                return BoolType;
        case NUM_TYPE_CHAR:
                return CharType;
        case NUM_TYPE_UCHAR:
                return UCharType;
        case NUM_TYPE_SHORT:
                return ShortType;
        case NUM_TYPE_USHORT:
                return UShortType;
        case NUM_TYPE_INT:
                return IntType;
        case NUM_TYPE_UINT:
                return UIntType;
        case NUM_TYPE_LONG:
                return LongType;
        case NUM_TYPE_ULONG:
                return ULongType;
        case NUM_TYPE_LONGLONG:
                return LongLongType;
        case NUM_TYPE_ULONGLONG:
                return ULongLongType;
        default:
                cant_cast();
        }
}


__attribute__((unused)) static emu_type_id_t id_from_num_type(num_type_t t) {
        switch(t){
        case NUM_TYPE_BOOL:
                return EMU_TYPE_BOOL_ID;
        case NUM_TYPE_CHAR:
        case NUM_TYPE_UCHAR:
                return EMU_TYPE_CHAR_ID;
        case NUM_TYPE_SHORT:
        case NUM_TYPE_USHORT:
                return EMU_TYPE_SHORT_ID;
        case NUM_TYPE_INT:
        case NUM_TYPE_UINT:
                return EMU_TYPE_INT_ID;
        case NUM_TYPE_LONG:
        case NUM_TYPE_ULONG:
                return EMU_TYPE_LONG_ID;
        case NUM_TYPE_LONGLONG:
        case NUM_TYPE_ULONGLONG:
                return EMU_TYPE_LONGLONG_ID;
        default:
                cant_cast();
        }
}

__attribute__((unused)) static bool is_num_type_signed(num_type_t t) {
        switch(t){
        case NUM_TYPE_BOOL:
        case NUM_TYPE_UCHAR:
        case NUM_TYPE_USHORT:
        case NUM_TYPE_UINT:
        case NUM_TYPE_ULONG:
        case NUM_TYPE_ULONGLONG:
                return false;
        case NUM_TYPE_CHAR:
        case NUM_TYPE_SHORT:
        case NUM_TYPE_INT:
        case NUM_TYPE_LONG:
        case NUM_TYPE_LONGLONG:
                return true;
        default:
                cant_cast();
        }
}

__attribute__((unused)) static num_type_t getNumType(QualType numtype) {
        QualType qt = numtype.getCanonicalType();
        if(qt->isBuiltinType()){
                const BuiltinType* ty = (const BuiltinType*)qt.getTypePtr();
                switch(ty->getKind()){
                        case BuiltinType::Bool:
                                return NUM_TYPE_BOOL;
                        case BuiltinType::Char_S:
                        case BuiltinType::SChar:
                                return NUM_TYPE_CHAR;
                        case BuiltinType::Char_U:
                        case BuiltinType::UChar:
                                return NUM_TYPE_UCHAR;
                        case BuiltinType::Short:
                                return NUM_TYPE_SHORT;
                        case BuiltinType::UShort:
                                return NUM_TYPE_USHORT;
                        case BuiltinType::Int:
                                return NUM_TYPE_INT;
                        case BuiltinType::UInt:
                                return NUM_TYPE_UINT;
                        case BuiltinType::Long:
                                return NUM_TYPE_LONG;
                        case BuiltinType::ULong:
                                return NUM_TYPE_ULONG;
                        case BuiltinType::LongLong:
                                return NUM_TYPE_LONGLONG;
                        case BuiltinType::ULongLong:
                                return NUM_TYPE_ULONGLONG;
                        default:
                                break;
                }
		llvm::errs() << "DOUG DEBUG: could not get type of ";
		ty->dump();
		llvm::errs() << "\n";
        }
        cant_cast();
}

template <num_type_t NumType> class EmuNum : public EmuNumGeneric{
public:
	EmuNum(status_t s)
		: EmuNumGeneric(s,NumType)
	{
	}

	EmuNum(llvm::APInt i)
		: EmuNumGeneric(STATUS_DEFINED,NumType,i)
	{
	}

	EmuNum(const void* p)
        : EmuNumGeneric(STATUS_UNDEFINED,NumType)
	{
		const emu_type_id_t* type_ptr = (const emu_type_id_t*)p;
		unsigned int bits = bits_in_num_type(NumType);

		// val is stored big-endian to check for errors
		const unsigned char* val_ptr = (const unsigned char*)(type_ptr+1);
		uint64_t v = 0;
		for(unsigned int bitsleft = bits; bitsleft > 0;){
				unsigned char nextbyte = *val_ptr;
				if(bitsleft < 8){
						nextbyte &= 0xFF >> (8-bitsleft);
						bitsleft = 0;
				} else {
						bitsleft -= 8;
				}
				v = 0x100 * v + nextbyte;
				val_ptr++;
		}
		emu_type_id_t t = *type_ptr;
		val = llvm::APInt(bits, v, is_num_type_signed(NumType));

		repr_type_id=t;
		val = v;

		switch(t){
		case EMU_TYPE_INT_ID | EMU_TYPE_UNINIT_MASK:
				status = STATUS_UNINITIALIZED;
				return;
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

	~EmuNum(void) {}

	size_t size(void) const{
		int vbits = bits_in_num_type(NumType);
		int vbytes = (vbits+7)/8;
		return sizeof(emu_type_id_t)+vbytes;
	}

	void print_impl(void) const{
		llvm::outs() << val;
	}

	void dump_repr(void* p) const{
		emu_type_id_t* type_ptr = (emu_type_id_t*)p;
		char* val_ptr = (char*)(type_ptr+1);

		int vbits = bits_in_num_type(NumType);
		int vbytes = (vbits+7)/8;
		switch(status){
		case STATUS_DEFINED:
		case STATUS_UNDEFINED:
		{
				*type_ptr = repr_type_id;
				// actually output
				llvm::APInt t = val;
				for(int i = 0; i < vbytes; i++){
						val_ptr[vbytes-i-1] = t.getZExtValue() & 0xFF;
						t = t.lshr(8);
				}
				break;
		}
		case STATUS_UNINITIALIZED:
				*type_ptr = id_from_num_type(NumType) | EMU_TYPE_UNINIT_MASK;
				for(int i = 0; i < vbytes; i++){
						val_ptr[i] = (char)EMU_TYPE_INVALID_ID;
				}
				break;
		}
	}

	const EmuVal* cast_to(QualType qt) const{
		num_type_t t = getNumType(qt);
		bool invalid = false;
		if(is_num_type_signed(NumType) && !is_num_type_signed(t)){
				invalid = true;
		}
		#define CASTTODOCASE(T) case T: {\
				if(invalid){ \
						return new EmuNum<T>(STATUS_UNDEFINED); \
				} else { \
						return new EmuNum<T>(llvm::APInt(bits_in_num_type(T),val.getLimitedValue(),is_num_type_signed(T))); \
				} \
		} \

		switch(t){
				CASTTODOCASE(NUM_TYPE_BOOL)
				CASTTODOCASE(NUM_TYPE_CHAR)
				CASTTODOCASE(NUM_TYPE_UCHAR)
				CASTTODOCASE(NUM_TYPE_SHORT)
				CASTTODOCASE(NUM_TYPE_USHORT)
				CASTTODOCASE(NUM_TYPE_INT)
				CASTTODOCASE(NUM_TYPE_UINT)
				CASTTODOCASE(NUM_TYPE_LONG)
				CASTTODOCASE(NUM_TYPE_ULONG)
				CASTTODOCASE(NUM_TYPE_LONGLONG)
				CASTTODOCASE(NUM_TYPE_ULONGLONG)
		}
		cant_cast();
	}

	const EmuNum<NumType>* add(const EmuNum<NumType>* other) const {
		status_t s1 = status;
		status_t s2 = other->status;
		if(s1 == STATUS_UNDEFINED || s2 == STATUS_UNDEFINED){
				return new EmuNum<NumType>(STATUS_UNDEFINED);
		}
		if(s1 == STATUS_UNINITIALIZED || s2 == STATUS_UNINITIALIZED){
				return new EmuNum<NumType>(STATUS_UNINITIALIZED);
		}
		return new EmuNum<NumType>(val+other->val);
	}

	const EmuNum<NumType>* sub(const EmuNum<NumType>* other) const{
		status_t s1 = status;
		status_t s2 = other->status;
		if(s1 == STATUS_UNDEFINED || s2 == STATUS_UNDEFINED){
				return new EmuNum<NumType>(STATUS_UNDEFINED);
		}
		if(s1 == STATUS_UNINITIALIZED || s2 == STATUS_UNINITIALIZED){
				return new EmuNum<NumType>(STATUS_UNINITIALIZED);
		}
		return new EmuNum<NumType>(val-other->val);
	}
	const EmuNum<NumType>* mul(const EmuNum<NumType>* other) const{
		status_t s1 = status;
		status_t s2 = other->status;
		if(s1 == STATUS_UNDEFINED || s2 == STATUS_UNDEFINED){
				return new EmuNum<NumType>(STATUS_UNDEFINED);
		}
		if(s1 == STATUS_UNINITIALIZED || s2 == STATUS_UNINITIALIZED){
				return new EmuNum<NumType>(STATUS_UNINITIALIZED);
		}
		return new EmuNum<NumType>(val*other->val);
	}

	const EmuNum<NumType>* div(const EmuNum<NumType>* other) const{
		status_t s1 = status;
		status_t s2 = other->status;
		if(s1 == STATUS_UNDEFINED || s2 == STATUS_UNDEFINED){
				return new EmuNum<NumType>(STATUS_UNDEFINED);
		}
		if(s1 == STATUS_UNINITIALIZED || s2 == STATUS_UNINITIALIZED){
				return new EmuNum<NumType>(STATUS_UNINITIALIZED);
		}
		if(other->val==0){
				return new EmuNum<NumType>(STATUS_UNDEFINED);
		}
		if(is_num_type_signed(NumType)){
				return new EmuNum<NumType>(val.sdiv(other->val));
		} else {
				return new EmuNum<NumType>(val.udiv(other->val));
		}
	}
	const EmuNum<NumType>* _or(const EmuNum<NumType>* other) const{
		status_t s1 = status;
		status_t s2 = other->status;
		if(s1 == STATUS_UNDEFINED || s2 == STATUS_UNDEFINED){
				return new EmuNum<NumType>(STATUS_UNDEFINED);
		}
		if(s1 == STATUS_UNINITIALIZED || s2 == STATUS_UNINITIALIZED){
				return new EmuNum<NumType>(STATUS_UNINITIALIZED);
		}
		return new EmuNum<NumType>(val | other->val);
	}
	const EmuNum<NumType>* _and(const EmuNum<NumType>* other) const{
		status_t s1 = status;
		status_t s2 = other->status;
		if(s1 == STATUS_UNDEFINED || s2 == STATUS_UNDEFINED){
				return new EmuNum<NumType>(STATUS_UNDEFINED);
		}
		if(s1 == STATUS_UNINITIALIZED || s2 == STATUS_UNINITIALIZED){
				return new EmuNum<NumType>(STATUS_UNINITIALIZED);
		}
		return new EmuNum<NumType>(val & other->val);
	}
	const EmuNum<NumType>* lnot(void) const{
		status_t s1 = status;
		if(s1 == STATUS_UNDEFINED){
				return new EmuNum<NumType>(STATUS_UNDEFINED);
		}
		if(s1 == STATUS_UNINITIALIZED){
				return new EmuNum<NumType>(STATUS_UNINITIALIZED);
		}
		bool b = !val;
		return new EmuNum<NumType> (llvm::APInt(bits_in_num_type(NumType), b, is_num_type_signed(NumType)));
	}
	const EmuNum<NumType>* neg(void) const{
		status_t s1 = status;
		if(s1 == STATUS_UNDEFINED){
				return new EmuNum<NumType>(STATUS_UNDEFINED);
		}
		if(s1 == STATUS_UNINITIALIZED){
				return new EmuNum<NumType>(STATUS_UNINITIALIZED);
		}
		return new EmuNum<NumType> (llvm::APInt(bits_in_num_type(NumType), - val.getLimitedValue(), is_num_type_signed(NumType)));
	}
};

class EmuPtr : public EmuVal{
public:
	EmuPtr(status_t, QualType);
	EmuPtr(mem_ptr, QualType);
	EmuPtr(const void*, QualType);
	~EmuPtr(void);

	size_t size(void) const;
	void print_impl(void) const;
	void dump_repr(void*) const;
	const EmuVal* cast_to(QualType) const;

	// points to a block if STATUS_DEFINED
	//  has a type_id and block_id if STATUS_UNDEFINED
	union{
		mem_block* block;
		struct{
			emu_type_id_t type_id;
			block_id_t block_id;
		} repr;
	} u;
	size_t offset;
};

class EmuFunc : public EmuVal{
public:
	EmuFunc(status_t, QualType);
	EmuFunc(uint32_t, QualType);
	EmuFunc(const void*);
	EmuFunc(const void*, QualType);
	~EmuFunc(void);

	size_t size(void) const;
	void print_impl(void) const;
	void dump_repr(void*) const;
	const EmuVal* cast_to(QualType) const;

	emu_type_id_t repr_type_id;
	uint32_t func_id;
};

class EmuVoid : public EmuVal{
public:
	EmuVoid(void);
	size_t size(void) const;
	void print_impl(void) const;
	void dump_repr(void*) const;
	const EmuVal* cast_to(QualType) const;
};

class EmuStackPos : public EmuVal{
public:
	EmuStackPos(unsigned int, unsigned int);
	EmuStackPos(const void*);
	~EmuStackPos(void);

	size_t size(void) const;
	void print_impl(void) const;
	void dump_repr(void*) const;
	const EmuVal* cast_to(QualType) const;

	emu_type_id_t repr_type_id;
	unsigned int level, num;
};

class EmuStruct : public EmuVal{
public:
	EmuStruct(status_t, QualType, unsigned int, const EmuVal**);
	EmuStruct(lvalue);
	~EmuStruct(void);

	size_t size(void) const;
	void print_impl(void) const;
	void dump_repr(void*) const;
	const EmuVal* cast_to(QualType) const;

	emu_type_id_t repr_type_id;
private:
	const EmuVal** members;
	unsigned int num;
};

