#pragma once
#include <inttypes.h>
#include <deque>
#include <stddef.h>
#include <string>
#include <unordered_map>
#include "clang/AST/Type.h"
#include "clang/AST/Stmt.h"
#include "enums.h"
#include "rbtree.h"

using namespace clang;

class EmuVal;
class EmuPtr;

//might want to change to uint64_t later if we have too many stack vars
// NOTE: block ID 0 to 2 (inclusive) are not used
typedef uint32_t block_id_t;

uint32_t new_fid(void);

#define BLOCK_ID_NULL 1
#define BLOCK_ID_INVALID 2
#define BLOCK_ID_START 3

// represents a list of objects of the same type and size
class mem_tag{
public:
	size_t offset; //relative to containing mem_block
	QualType type;
	rbnode<mem_tag> *prev;
	rbnode<mem_tag> *next;
	size_t typesize; //size of stored object
	size_t count;

	mem_tag(size_t, size_t, rbnode<mem_tag>*, QualType);
	size_t sortval(void) const;
};

class mem_block{
friend class mem_tag;
public:
	mem_block(mem_type_t, const EmuVal*);
	mem_block(mem_type_t, size_t);
	~mem_block(void);

	size_t sortval(void) const;
	void write(const EmuVal*, size_t);

	const block_id_t id;
	const size_t size; // extra space is uninit
	mem_type_t memtype;
	void* data;
	rbnode<mem_tag>* firsttag;

private:
	rbtree<mem_tag> tags;
	void remove_tag(mem_tag*);
	void update_tag_write(const EmuVal*, size_t);
	void deal_with_end(rbnode<mem_tag>*, rbnode<mem_tag>*, size_t);
};

class mem_ptr{
public:
	mem_block* block;
	size_t offset;

	mem_ptr(mem_block*, size_t);
};

class lvalue{
public:
	mem_ptr ptr;
	QualType type;

	lvalue(mem_block*, QualType, size_t);
};

extern std::unordered_map<block_id_t, mem_block*> active_mem;
extern std::deque<std::unordered_map<std::string, lvalue> > stack_vars;
extern std::unordered_map<std::string, lvalue> global_vars;
extern std::unordered_map<uint32_t, const void*> global_functions;
extern std::unordered_map<uint32_t, std::string> external_functions;
