#pragma once
#include <inttypes.h>

enum status_t{
	STATUS_UNDEFINED,
	STATUS_UNINITIALIZED,
	STATUS_DEFINED
};

typedef uint32_t emu_type_id_t;

static const uint32_t EMU_TYPE_ZERO_ID    = 0x00000000;
static const uint32_t EMU_TYPE_INVALID_ID = 0x01010101;
static const uint32_t EMU_TYPE_FUNC_ID    = 0x02020202;
static const uint32_t EMU_TYPE_VOID_ID    = 0x03030303;
static const uint32_t EMU_TYPE_STRUCT_ID  = 0x04040404;
static const uint32_t EMU_TYPE_STACKPOS_ID= 0x05050505;

static const uint32_t EMU_TYPE_BOOL_ID     = 0x40404040;
static const uint32_t EMU_TYPE_CHAR_ID     = 0x41414141;
static const uint32_t EMU_TYPE_SHORT_ID     = 0x42424242;
static const uint32_t EMU_TYPE_INT_ID     = 0x43434343;
static const uint32_t EMU_TYPE_LONG_ID     = 0x44444444;
static const uint32_t EMU_TYPE_LONGLONG_ID   = 0x45454545;

static const uint32_t EMU_TYPE_PTR_ID     = 0x60606060;


// masks:
static const uint32_t EMU_TYPE_NUM_MASK = 0x40404040;
static const uint32_t EMU_TYPE_PTR_MASK = EMU_TYPE_PTR_ID;
static const uint32_t EMU_TYPE_UNINIT_MASK = 0x80808080;

enum mem_type_t{
	MEM_TYPE_STATIC,
	MEM_TYPE_GLOBAL,
	MEM_TYPE_STACK,
	MEM_TYPE_HEAP,
	MEM_TYPE_EXTERN,
	MEM_TYPE_INVALID,
	MEM_TYPE_FREED,
};

enum num_type_t{
	NUM_TYPE_BOOL,
	NUM_TYPE_CHAR,
	NUM_TYPE_UCHAR,
	NUM_TYPE_SHORT,
	NUM_TYPE_USHORT,
	NUM_TYPE_INT,
	NUM_TYPE_UINT,
	NUM_TYPE_LONG,
	NUM_TYPE_ULONG,
	NUM_TYPE_LONGLONG,
	NUM_TYPE_ULONGLONG,
};
