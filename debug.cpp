#include "llvm/Support/raw_ostream.h"
#include "cast.h"
#include "debug.h"
#include "eval.h"
#include "main.h"
#include "mem.h"

bool firsttime = true;

unsigned int lastline = 0;

void debug_dump(void){
	debug_dump(nullptr);
}

void debug_dump(const char* exc){
	int ms = main_source;
	int s = curr_source;
	if(exc != nullptr) llvm::errs() << "DOUG DEBUG: ending with exception " << exc << "\n";
	llvm::errs() << "DOUG DEBUG file " << sources[s]->getSourceManager().getFilename(curr_loc) << " line " << sources[s]->getSourceManager().getSpellingLineNumber(curr_loc) << "\n";
	if(lastline != 0x42424242) return;

	if(exc == nullptr && s != ms) return;

	unsigned int loc = sources[s]->getSourceManager().getSpellingLineNumber(curr_loc);
	if(exc == nullptr && loc == lastline) return;
	lastline = loc;

	if(firsttime){
		llvm::outs() << "[\n";
		firsttime = false;
	} else {
		llvm::outs() << ",\n";
	}
	llvm::outs() << "{\n\"ordered_globals\": [\n";
	bool first = true;
	for(auto it = local_vars[ms].cbegin(); it != local_vars[ms].cend(); ++it){
		const mem_block* block = it->second.ptr.block;
		if(block->memtype != MEM_TYPE_INVALID){
			if(first){
				first = false;
			} else {
				llvm::outs() << ",\n";
			}
			llvm::outs() << "\"" << it->first << "\"";
		}
	}
	llvm::outs() << "\n],\n\"stdout\": \"\",\n";
	if(exc != nullptr){
		llvm::outs() << "\"exception_msg\": \""<<exc<<"\", ";
	}
	llvm::outs() << "\"func_name\": \"unnamed_func\",\n\"stack_to_render\": [";
	first = true;
	int frame_id = 1;
	for(auto stack_it = stack_vars.cbegin(); stack_it != stack_vars.cend(); ++stack_it){
		std::vector<std::pair<std::string, lvalue> > map = *stack_it;
		auto it = map.cbegin();
		if(it == map.cend()) continue;
		if(first){
			first = false;
		} else {
			llvm::outs() << ",\n";
		}
		llvm::outs() << "{\n";
		llvm::outs() << "\"frame_id\": "<< (frame_id++) << ",\n";
		llvm::outs() << "\"encoded_locals\": {\n";
		bool innerfirst = true;
		do{
			if(innerfirst){
				innerfirst = false;
			} else {
				llvm::outs() << ",\n";
			}
			llvm::outs() << "\"" << it->second.type.getAsString() << " " << it->first << "\": [\"REF\", " << it->second.ptr.block->id << "]";
		} while(++it != map.cend());
		llvm::outs() << "\n},\n\"is_highlighted\": false,\n\"is_parent\": false,\n\"func_name\": \"unnamed\",\n\"is_zombie\": false,\n\"parent_frame_id_list\": [],\n\"unique_hash\": \"func_" << frame_id << "\",\n\"ordered_varnames\": [\n";
		it = map.cbegin();
		innerfirst = true;
		do{
			if(innerfirst){
				innerfirst = false;
			} else {
				llvm::outs() << ",\n";
			}
			llvm::outs() << "\"" << it->second.type.getAsString() << " " << it->first << "\"";
		} while(++it != map.cend());
		llvm::outs() << "\n]\n}";
	}
	llvm::outs() << "\n],\n\"globals\": {\n";
	first = true;
	for(auto it = local_vars[ms].cbegin(); it != local_vars[ms].cend(); ++it){
		const mem_block* block = it->second.ptr.block;
		if(block->memtype != MEM_TYPE_INVALID){
			if(first){
				first = false;
			} else {
				llvm::outs() << ",\n";
			}
			llvm::outs() << "\"" << it->first << "\": [\"REF\", " << block->id << "]";
		}
	}
	llvm::outs() << "\n},\n\"heap\": {\n";
	first = true;
	for(auto it = active_mem.cbegin(); it != active_mem.cend(); it++){
		std::string typedesc;
		mem_block* block = it->second;
		switch(block->memtype){
		case MEM_TYPE_STATIC: typedesc = "STATIC"; break;
		case MEM_TYPE_GLOBAL: typedesc = "GLOBAL"; break;
		case MEM_TYPE_HEAP:   typedesc = "HEAP"; break;
		case MEM_TYPE_STACK:  typedesc = "STACK"; break;
		case MEM_TYPE_EXTERN: typedesc = "EXTERN"; break;
		case MEM_TYPE_INVALID:
		default:
			continue;
		}
		if(first){
			first = false;
		} else {
			llvm::outs() << ",\n";
		}
		llvm::outs() << "\"" << block->id << "\": [\"CLASS\", \""<<typedesc<<"\", []";

		size_t currpos = 0;
		for(rbnode<mem_tag>* curr = block->firsttag; curr != nullptr; curr=curr->value.next){
			size_t pos = curr->value.offset;
			if(pos > currpos){
				llvm::outs() << ",[\"\",\"" << (pos - currpos) << " bytes uninitialized\"]\n";
			}
			llvm::outs() << ",[\"\",[\"REF\",\""<< block->id << "O" << pos << "\"]]\n";
			currpos += curr->value.typesize*curr->value.count;
		}
		if(currpos < block->size){
			llvm::outs() << ",[\"\",\"" << (block->size - currpos) << " bytes uninitialized\"]\n";
		}
		llvm::outs() << "] ";
	}
	for(auto it = active_mem.cbegin(); it != active_mem.cend(); it++){
		std::string typedesc;
		mem_block* block = it->second;
		switch(block->memtype){
		case MEM_TYPE_STATIC: typedesc = "STATIC"; break;
		case MEM_TYPE_GLOBAL: typedesc = "GLOBAL"; break;
		case MEM_TYPE_HEAP:   typedesc = "HEAP"; break;
		case MEM_TYPE_STACK:  typedesc = "STACK"; break;
		case MEM_TYPE_EXTERN: typedesc = "EXTERN"; break;
		case MEM_TYPE_INVALID:
		default:
			continue;
		}

		for(rbnode<mem_tag>* curr = block->firsttag; curr != nullptr; curr=curr->value.next){
			size_t pos = curr->value.offset;
			size_t typesize = curr->value.typesize;
			QualType qt = curr->value.type;
			llvm::outs() << ", \"" << block->id << "O" << pos << "\": [\"LIST\"";
			size_t count = curr->value.count;
			for(size_t i = 0; i < count; i++){
				const EmuVal* temp = from_lvalue(lvalue(block, qt, pos+i*typesize));
				if((temp->obj_type->isPointerType() || temp->obj_type->isArrayType()) && temp->status == STATUS_DEFINED){
					const EmuPtr* ptr = (const EmuPtr*)temp;
					llvm::outs() << ", [\"REF\",\"" << ptr->u.block->id << "O" << ptr->offset << "\"]";
				} else {
					llvm::outs() << ", \"";
					temp->print();
					llvm::outs() << "\"";
				}
				delete temp;
			}
			llvm::outs() << "]";
		}
	}


	llvm::outs() << "}, \"line\": " << loc <<", \"event\": \"";
	if(exc == nullptr){
		llvm::outs() << "step_line\"}";
	} else {
		llvm::outs() << "exception\"}]";
	}
}
