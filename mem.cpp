#include "exit.h"
#include "mem.h"
#include "rbtree.h"
#include "types.h"

rbtree<mem_block> active_memory();
std::stack<mem_block> the_stack();
std::unordered_map<mem_block> global_mem();

static block_id_t id_counter = BLOCK_ID_START;

static block_id_t new_block_id(void){
	block_id_t ans = id_counter;
	if(ans == 0){
		exit_premature();
	}
	id_counter = ans+1;
	return ans;
}

// only next isn't set immediately
mem_item::mem_item(mem_block* b, EmuVal* obj, mem_item* p, mem_item* n, size_t o, size_t s)
	: offset(o),values(obj),parent(b),prev(p),next(n),size(s)
{
}


// handles internal bookkeeping of combining/splitting items
void mem_block::write(EmuVal* obj, size_t offset){
	size_t s = size;
	if(offset > s || s-offset < typesize){
		llvm::outs() << "!ERROR: write to unallocated memory\n";
		exit_premature();
	}

	rbnode<mem_item>* node = place_item(obj,offset);
	size_t typesize = obj->size();
	size_t elem = node->values.size();
	size_t nsize = node->size();
	if(typesize*elem <= nsize) return;
	
}

rbnode<mem_item>* mem_block::place_item(EmuVal* obj, size_t offset){
	mem_item* before;
	size_t typesize = obj->size();
	rbnode<mem_item>* node_before = items.findBelow(offset);
	if(node_before == nullptr){
		rbnode<mem_item>* node_after = items.findAbove(offset);
		return new rbnode<mem_item>(mem_item(this,obj,nullptr,&node_after.value,offset,typesize));
	}

	before = &node_before->value;
	size_t before_offset = before->offset;
	size_t offset_diff = o - before_offset;
	if(offset_diff == 0){
		mem_item* over = before;
		before = before->prev;
		EmuVal* before_obj = before->values[0];
		if(before_obj->obj_type == obj->obj_type){
			size_t before_typesize = before_obj->size();
			size_t before_num = before->values.size();
			size_t before_size = before->size;
			if(before_typesize*before_num == before_size){
				// we only need to extend the last one
				rbnode<mem_item>* ans = before;
				ans->values.add(obj);
				ans->size += typesize;
				return ans;
			}
		}
		rbnode<mem_item>* ans = new rbnode<mem_item> (mem_item(this,obj,before,over,offset,typesize));
		before->next = &ans->value;
		return ans;
	}

//	rbnode<mem_item>* ans = new rbnode<mem_item>* (mem_item(this,obj,before,before,offset,typesize));
	size_t before_typesize = before->values[0]->size();
	size_t num = before->values.size();
	size_t before_used = num*before_typesize;

	size_t newnum = offset_diff / before_typesize;
	size_t cutstart = newnum*before_typesize;
	size_t cutbytes = offset_diff - cutstart;
	if(offset_diff+typesize < before_used){
		// we are right in the middle of before
		size_t start_saved = (offset_diff+typesize)/before_typesize;
		size_t num_saved = num-start_saved;
		size_t tempsize = num_saved*before_typesize
		char* temp = new char[tempsize];
		for(size_t i = 0; i < num_saved; i++){
			before->values[start_saved+i]->dump_repr(&temp[i*before_typesize]);
		}
		size_t bufsize = before_used-offset_diff-typesize;
		char* buf = new char[bufsize];
		memcpy(&buf[tempsize-bufsize],temp,tempsize-bufsize);
		delete temp;
		EmuRaw* saved = new EmuRaw(saved_bytes, buf);
	}




	if(offset_diff >= before_used){
		// we've overrun the rest of before
		before->size = offset_diff;
		size_t newnum = offset_diff / before_typesize;
		size_t cutstart = newnum*before_typesize;
		size_t cutbytes = offset_diff - cutstart;
	} else {
		size_t newnum = offset_diff / before_typesize;
		size_t cutstart = newnum*before_typesize;
		size_t cutbytes = offset_diff - cutstart;
		if(cutbytes == 0){
			before->next = this;
		} else {
			char* buf = new char[before_typesize];
			before->values[newnum]->dump_repr(temp);
			char* raw = new char[cutbytes];
			memcpy(raw,buf,cutbytes);
			before = p->items.insert(mem_item(cutstart,new EmuRaw(cutbytes,raw),p));
			delete buf;
		}
	}
}


###
	size_t before_unused = before->uninitialized_bytes_after;
	if(s-before_offset > 
	if(type
	mem_item* after = before;
	
	{

		end = curr->next;
		if(end == nullptr){
			if(
		}
	}
	size_t b_offset = before->offset;
	
}

int mem_item::sortval(void) const {
	return offset;
}

mem_block::mem_block(mem_type type, EmuVal* obj)
	:id(new_block_id()),memtype(type),items(mem_item(obj->size())),refs()
{
}

int mem_block::sortval(void) const {
	return id;
}

mem_item* by_index(size_t index) const{
	return &items.findBelow(index)->value;
}

mem_ptr::mem_ptr(mem_block* b, size_t o)
	: block(b), offset(o)
{
}
