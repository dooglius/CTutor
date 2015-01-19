#include "exit.h"
#include "mem.h"
#include "rbtree.h"
#include "types.h"

static block_id_t id_counter = BLOCK_ID_START;

static block_id_t new_block_id(void){
	block_id_t ans = id_counter;
	if(ans == 0){ // has wrapped around
		err_exit("Too many allocations");
	}
	id_counter = ans+1;
	return ans;
}

static uint32_t fid_counter = 1;
uint32_t new_fid(void){
	uint32_t ans = fid_counter;
	if(ans == 0){ // overflow
		err_exit("Too many functions");
	}
	fid_counter = ans+1;
	return ans;
}

std::unordered_map<block_id_t, mem_block*> active_mem;
std::deque<std::unordered_map<std::string, lvalue> > stack_vars;
std::unordered_map<std::string, lvalue> global_vars;
std::unordered_map<uint32_t, const void*> global_functions;
std::unordered_map<uint32_t, std::string> external_functions;


// only next isn't set immediately
mem_tag::mem_tag(size_t o, size_t s, rbnode<mem_tag>* p, QualType t)
	: offset(o),type(t),prev(p)
{
	if(t == RawType){
		typesize = 1;
		count = s;
	} else {
		typesize = s;
		count = 1;
	}
}

size_t mem_tag::sortval(void) const{
	return offset;
}

void mem_block::deal_with_end(rbnode<mem_tag>* extended, rbnode<mem_tag>* affected, size_t endpos){
	for(;affected != nullptr; affected = affected->value.next){
		size_t offset = affected->value.offset;

		if (offset >= endpos){
			if(offset == endpos && extended->value.type == affected->value.type){
				// merge
				extended->value.count += affected->value.count;
			} else {
				affected->value.prev = extended;
				break;
			}
		} else {
			size_t typesize = affected->value.typesize;
			size_t count = affected->value.count;
			size_t end = offset+typesize*count;
			if(end > endpos){
				// we cut it, now we must repair
				size_t firstvalid = (endpos-offset+typesize-1)/typesize;
				if(firstvalid == 0){ // check overflow case
					firstvalid = SIZE_MAX/typesize;
				}

				size_t invalidbytes = firstvalid*typesize;
				size_t alignbytes = offset+invalidbytes-endpos;
				affected->value.offset = endpos;
				if(alignbytes == 0){
					affected->value.count = count-firstvalid; // must be nonzero
				} else {
					size_t newcount = count-firstvalid;
					if(newcount == 0){
						//convert to raw
						affected->value.type = RawType;
						affected->value.typesize = 1;
						affected->value.count = newcount*typesize;
					} else {
						//need to insert raw, move back this
						rbnode<mem_tag>* newtag = tags.insert(mem_tag(offset+invalidbytes,newcount,affected,RawType));
						affected->value.type = RawType;
						affected->value.prev = extended;
						affected->value.next = newtag;
						affected->value.typesize = 1;
						affected->value.count = alignbytes;
					}
				}
				break;
			}
		}
		tags.remove(affected);
		delete affected;
	}
	extended->value.next = affected;
}

// called on each memory write so we can update memory tags
void mem_block::update_tag_write(const EmuVal* obj, size_t offset){
	QualType qtype = obj->obj_type;
	rbnode<mem_tag>* start = tags.findBelow(offset);
	rbnode<mem_tag>* newtag;

	if(start == nullptr){
		size_t typesize = obj->size();
		newtag = tags.insert(mem_tag(offset, typesize, nullptr, qtype));
		deal_with_end(newtag, firsttag, offset+typesize);
		firsttag = newtag;
		return;
	}
	// if we are aligned, we can avoid creating a tag for raw data
	size_t start_typesize = start->value.typesize;
	size_t start_offset = start->value.offset;
	size_t start_lastvalid = (offset-start_offset)/start_typesize;
	size_t start_validbytes = start_lastvalid*start_typesize;
	size_t alignbytes = offset-(start_offset+start_validbytes);
	if(alignbytes == 0){
		// check if the tag is already correct
		QualType start_type = start->value.type;
		if(qtype == start_type) return;
		// else see if we can combine with previous tag
		rbnode<mem_tag>* before = start->value.prev;
		if(before != nullptr && qtype == before->value.type){
			int newcount = before->value.count + 1;
			before->value.count = newcount;
			deal_with_end(before, start, offset+newcount*before->value.typesize);
			return;
		}
	}
	size_t start_endpos = start_offset + start->value.count*start_typesize;
	size_t typesize = obj->size();
	if(start_offset == offset){
		// check if we overwrite start entirely
		if(start_endpos <= offset+typesize){
			start->value.type = qtype;
			start->value.typesize = typesize;
			start->value.count = 1;
			deal_with_end(start, start->value.next, offset+typesize);
			return;
		}
		// else we just need to insert a "new" tag for what's left
		newtag = tags.insert(mem_tag(offset+typesize,start_typesize,start,start->value.type));
		newtag->value.next = start->value.next;
		start->value.type = qtype;
		start->value.next = newtag;
		start->value.typesize = typesize;
		start->value.count = 1;
		return;
	}

	start->value.count = start_lastvalid;
	if(alignbytes != 0){
		// if we are misaligned, we need to add a raw tag
		rbnode<mem_tag>* rawtag = tags.insert(mem_tag(start_offset+start_validbytes,alignbytes,start,RawType));
		start->value.next = rawtag;
		rbnode<mem_tag>* start_next = start->value.next;
		newtag = tags.insert(mem_tag(offset,typesize,rawtag,qtype));
		rawtag->value.next = newtag;
		newtag->value.next = start_next;
		deal_with_end(newtag, start_next, offset+typesize);
		return;
	}
	rbnode<mem_tag>* start_next = start->value.next;
	newtag = tags.insert(mem_tag(offset,typesize,start,qtype));
	newtag->value.next = start_next;
	start->value.next = newtag;
	deal_with_end(newtag, start_next, offset+typesize);
}

// handles internal bookkeeping of combining/splitting items
void mem_block::write(const EmuVal* obj, size_t offset){
	size_t s = size;
	if(offset > s){
		bad_memwrite();
		return;
	}
	size_t typesize = obj->size();
	if(s-offset < typesize){
		bad_memwrite();
		return;
	}

	// actually perform the write
	obj->dump_repr(&((char*)data)[offset]);

	update_tag_write(obj, offset);
}

mem_block::mem_block(mem_type_t t, size_t s)
	:id(new_block_id()),size(s),memtype(t),data(new char[s]),firsttag(nullptr),tags()
{
	active_mem.insert(std::make_pair(id,this));
}

mem_block::mem_block(mem_type_t t, const EmuVal* obj)
	:mem_block(t,obj->size())
{
	write(obj, 0);
}


mem_ptr::mem_ptr(mem_block* b, size_t o)
	: block(b), offset(o)
{
}

lvalue::lvalue(mem_block* b, QualType t, size_t o)
	: ptr(b,o),type(t)
{
}
