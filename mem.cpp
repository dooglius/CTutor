#include "llvm/Support/raw_ostream.h"
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

static uint32_t fid_counter = NUM_EXTERNAL_FUNCTIONS;
uint32_t new_fid(void){
	uint32_t ans = fid_counter;
	if(ans == 0){ // overflow
		err_exit("Too many functions");
	}
	fid_counter = ans+1;
	return ans;
}

std::unordered_map<block_id_t, mem_block*> active_mem;
std::unordered_map<std::string, std::deque<std::pair<int,int> > > stack_var_map;
std::vector<std::vector<std::pair<std::string, lvalue> > > stack_vars;
std::unordered_map<std::string, lvalue>* local_vars;
std::unordered_map<std::string, int> global_vars;
std::unordered_map<uint32_t, std::pair<int, const void*> > global_functions;

void add_stack_var(std::string name, lvalue loc){
	int n = stack_vars.size()-1;
	int i = stack_vars[n].size();
	llvm::errs() << "DOUG MEM DEBUG: adding "<<name<<" at stack location "<<n<<","<<i<<" with block at "<<((void*)loc.ptr.block)<<"\n";
	stack_vars.back().push_back(std::pair<std::string, lvalue> (name, loc));
	auto list = stack_var_map.find(name);
	if(list == stack_var_map.end()){
		std::deque<std::pair<int, int> > newq = std::deque<std::pair<int, int> >();
		newq.push_back(std::pair<int, int>(n, i));
		stack_var_map.insert(std::make_pair(name, newq));
	} else {
		list->second.push_back(std::pair<int, int>(n,i));
	}
}

void add_stack_frame(void){
	stack_vars.push_back(std::vector<std::pair<std::string, lvalue>>());
	llvm::errs() << "DOUG MEM DEBUG: adding stack frame, there are now "<<stack_vars.size()<<"\n";
}

void pop_stack_frame(void){
	auto frame = stack_vars.back();
	stack_vars.pop_back();
	for(auto it = frame.cbegin(); it != frame.cend(); it++){
		std::string name = it->first;
		auto list = &stack_var_map.find(name)->second;
//		llvm::errs() << "DOUG MEM DEBUG: while popping stack frame, deleting local var "<<name<<" from list of "<<list->size()<<" with that name\n";
//		llvm::errs() << "DOUG MEM DEBUG: most recent list element has loc "<<list->back().first<<","<<list->back().second<<"\n";
//		llvm::errs() << "DOUG MEM DEBUG: oldest list element has loc "<<list->front().first<<","<<list->front().second<<"\n";
		bool erase = (list->size() == 1);
		list->pop_back();
		if(erase){
			stack_var_map.erase(name);
		}
		it->second.ptr.block->free();
	}
	llvm::errs() << "DOUG MEM DEBUG: popping stack frame, there are now "<<stack_vars.size()<<"\n";
}

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
	size_t start_typesize = start->value.typesize;
	size_t start_offset = start->value.offset;
	size_t start_endpos = start_offset + start->value.count*start_typesize;
	// check if we are right after the previous tag
	if(offset == start_endpos){
		// check for combining with that
		QualType start_type = start->value.type;
		if(qtype == start_type){
			size_t newcount = start->value.count+1;
			start->value.count = newcount;
			deal_with_end(start, start->value.next, start_endpos+start_typesize);
			return;
		}
	}
	// if we start in an uninit range, just stick the new one in
	if(offset >= start_endpos){
		size_t typesize = obj->size();
		newtag = tags.insert(mem_tag(offset, typesize, start, qtype));
		deal_with_end(newtag, start->value.next, offset+typesize);
		start->value.next = newtag;
		return;
	}
	// else, this tag cuts off part or all of start

	// if we are aligned, we can avoid creating a tag for raw data from fragmentation
	size_t start_lastvalid = (offset-start_offset)/start_typesize;
	size_t start_validbytes = start_lastvalid*start_typesize;
	size_t alignbytes = offset-(start_offset+start_validbytes);
	if(alignbytes == 0){
		// check if the tag is already correct
		QualType start_type = start->value.type;
		if(qtype == start_type) return;
	}
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

void mem_block::free(void){
	memtype = MEM_TYPE_FREED;
	delete ((char*)data);
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

mem_block::~mem_block(void){
	delete[] ((char*)data);
	active_mem.erase(id);
}

mem_ptr::mem_ptr(mem_block* b, size_t o)
	: block(b), offset(o)
{
}

lvalue::lvalue(mem_block* b, QualType t, size_t o)
	: ptr(b,o),type(t)
{
}
