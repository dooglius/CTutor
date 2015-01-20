#include "llvm/Support/raw_ostream.h"
#include "cast.h"
#include "debug.h"
#include "main.h"
#include "mem.h"

void debug_dump(void){
	llvm::outs() << "####### DEBUG DUMP BEGIN\n";
	llvm::outs() << " Current line: " << currloc << "\n";
	llvm::outs() << " Global variables:\n";
	for(auto it = global_vars.cbegin(); it != global_vars.cend(); ++it){
		const mem_block* block = it->second.ptr.block;
		if(block->memtype != MEM_TYPE_INVALID){
			llvm::outs() << "\t Global '" << it->first << "' of type '"<<it->second.type.getAsString()<<"' has allocation #" << it->second.ptr.block->id << "\n";
		}
	}
	for(auto stack_it = stack_vars.cbegin(); stack_it != stack_vars.cend(); ++stack_it){
		std::unordered_map<std::string, lvalue> map = *stack_it;
		auto it = map.cbegin();
		if(it == map.cend()) continue;
		llvm::outs() << " Local variable block:\n";
		do{
			llvm::outs() << "\t '" << it->first << "' of type '"<< it->second.type.getAsString() << "' points to allocation #" << it->second.ptr.block->id << ", offset "<< it->second.ptr.offset <<"\n";
		} while(++it != map.cend());
	}
	llvm::outs() << " Memory allocations:\n";
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
		llvm::outs() << "   Allocation #" << it->first<<"\n";
		llvm::outs() << "    Type: " << typedesc << "\n";

		size_t currpos = 0;
		for(rbnode<mem_tag>* curr = block->firsttag; curr != nullptr; curr=curr->value.next){
			size_t pos = curr->value.offset;
			if(pos > currpos){
				llvm::outs() << "     " << (pos - currpos) << " bytes uninitialized\n";
			}
			QualType qt = curr->value.type;
			size_t typesize = curr->value.typesize;
			llvm::outs() << "     " << qt.getAsString() << "\n";
			size_t count = curr->value.count;
			for(size_t i = 0; i < count; i++){
				const EmuVal* temp = from_lvalue(lvalue(block, qt, pos+i*typesize));
				llvm::outs() << "      ";
				temp->print();
				delete temp;
				llvm::outs() << "\n";
			}
			currpos = pos+count*typesize;
		}
		if(currpos < block->size){
			llvm::outs() << "     " << (block->size - currpos) << " bytes uninitialized\n";
		}
	}
	llvm::outs() << "+++++++ DEBUG DUMP END\n";
}
