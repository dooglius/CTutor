#pragma once
#include <inttypes.h>
#include <vector>
#include "rbtree.h"
#include "types.h"

class EmuVal;
class EmuPtr;

//might want to change to uint64_t later if we have too many stack vars
// NOTE: block ID 0 to 2 (inclusive) are not used
typedef uint32_t block_id_t;

#define BLOCK_ID_NULL 1
#define BLOCK_ID_INVALID 2
#define BLOCK_ID_START 3

class mem_tag{
public:
	const size_t offset; //relative to containing mem_block
	const emu_type_t type;
	mem_item *prev;
	mem_item *next;
	const size_t size; //size of stored object

	mem_item(size_t, EmuVal*);
	size_t sortval(void) const;
};

class mem_block{
friend class mem_item;
public:
	mem_block(mem_type,EmuVal*);
	int sortval(void) const;
	//mem_item* by_index(size_t index) const;

	const block_id_t id;
	size_t size; // extra space is uninit
	void* data;

private:
	rbtree<mem_tag> tags;
};

class mem_ptr{
public:
	mem_ptr(mem_block*, size_t);

	const mem_block* block; // null if null pointer
	const size_t offset;
};

extern rbtree<mem_block> active_heap;
extern std::stack<mem_block> the_stack;
extern std::unordered_map<std::string, mem_block> global_mem;
