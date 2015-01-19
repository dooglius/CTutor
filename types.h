#pragma once
#include <stddef.h>
#include <unistd.h>
#include "llvm/ADT/APInt.h"
#include "clang/AST/Type.h"
#include "enums.h"
#include "mem.h"

using namespace clang;

extern const llvm::APInt EMU_MIN_INT;
extern const llvm::APInt EMU_MAX_INT;

extern QualType RawType;
extern QualType BoolType;
extern QualType IntType;
extern QualType RawPtrType;

class EmuVal{
public:
	virtual ~EmuVal(void);

	virtual size_t size(void) const = 0;
	virtual void print_impl(void) const = 0;
	virtual void dump_repr(void*) const = 0;
	virtual void set_to_repr(const void*)=0;
	void print(void) const;
	status_t status;
	QualType obj_type;

protected:
	EmuVal(status_t, QualType);
};

class EmuInt : public EmuVal{
public:
	EmuInt(status_t);
	EmuInt(int32_t);
	~EmuInt(void);

	size_t size(void) const;
	void print_impl(void) const;
	void dump_repr(void*) const;
	void set_to_repr(const void*);

	int32_t val;

private:
	emu_type_id_t repr_type_id;
};

class EmuPtr : public EmuVal{
public:
	EmuPtr(status_t);
	EmuPtr(mem_ptr);
	~EmuPtr(void);

	size_t size(void) const;
	void print_impl(void) const;
	void dump_repr(void*) const;
	void set_to_repr(const void*);

	union{
		mem_block* block;
		struct{
			emu_type_id_t type_id;
			block_id_t block_id;
		} repr;
	} u;
	size_t offset;
};
/*
class EmuArr : public EmuPtr{
public:
	EmuArr(status_t, unsigned int);
	EmuArr(mem_ptr, unsigned int);
	~EmuInt(void);

	unsigned int num;
};
*/
class EmuFunc : public EmuVal{
public:
	EmuFunc(status_t, QualType);
	EmuFunc(uint32_t, QualType);
	~EmuFunc(void);

	size_t size(void) const;
	void print_impl(void) const;
	void dump_repr(void*) const;
	void set_to_repr(const void*);

	emu_type_id_t repr_type_id;
	uint32_t func_id;
};
/*
class EmuVoid : public EmuVal{
	EmuVoid(void);
	size_t size(void) const;
	void print_impl(void) const;
	void dump_repr(void*) const;
	void set_to_repr(const void*);
};
*/
class EmuBool : public EmuVal{
public:
	EmuBool(status_t);
	EmuBool(bool b);
	~EmuBool(void);

	size_t size(void) const;
	void print_impl(void) const;
	void dump_repr(void*) const;
	void set_to_repr(const void*);

	emu_type_id_t repr_type_id;
	char value;
};

//extern const EmuType EMU_TYPE_INT;
//extern const EmuType EMU_TYPE_FUNC;
//extern const EmuType EMU_TYPE_INVALID;
