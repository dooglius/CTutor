#pragma once
#include <unistd.h>
#include "llvm/ADT/APInt.h"
#include "mem.h"

extern const llvm::APInt EMU_MIN_INT;
extern const llvm::APInt EMU_MAX_INT;

enum status_t{
	STATUS_UNDEFINED,
	STATUS_UNINITIALIZED,
	STATUS_DEFINED
};

enum emu_type_t{
	EMU_TYPE_ZERO=0, // zero is a special case due to calloc and such
	EMU_TYPE_INVALID,
	EMU_TYPE_RAW,
	EMU_TYPE_INT,
	EMU_TYPE_PTR,
	EMU_TYPE_FUNC
};

class EmuVal{
public:
	virtual size_t size(void) const = 0;
	virtual void print_impl(void) const = 0;
	virtual void dump_repr(void* ptr) const = 0;
	virtual void set_to_repr(void* ptr)=0;
	void print(void);
	status_t status;
	int obj_type;

protected:
	EmuVal(status_t s, int t);
};

class LValue{
public:
	EmuVal* value;
	mem_block* block;
	mem_range* loc;
}

class EmuInt : public EmuVal{
public:
	EmuInt(status_t s);
	EmuInt(int i);

	size_t size(void);
	void print_impl(void);
	void dump_repr(void*);
	void set_to_repr(void*);

private:
	int val;
	emu_type_t repr_type;
};

class EmuPtr : public EmuVal{
public:
	EmuPtr(status_t s);
	EmuPtr(mem_block* b, size_t o);

	size_t size(void);
	void print_impl(void);
	void dump_repr(void*);
	void set_to_repr(void*);

private:
	emu_type_t repr_type;
	mem_ptr pointer;
	int repr_bid;
};

class EmuFunc : public EmuVal{
public:
	EmuFunc(const char*);
	size_t size(void);
	void print_impl(void);
	void dump_repr(void*);
	void set_to_repr(void*);

private:
	const char* name;
};

class EmuRaw : public EmuVal{
public:
	// assumes pointer is caller allocated, will delete in destructor
	EmuRaw(size_t, void*);
	~EmuRaw(void);
	size_t size(void);
	void print_impl(void);
	void dump_repr(void*);
	void set_to_repr(void*);

private:
	size_t len;
	void* buf;
};
