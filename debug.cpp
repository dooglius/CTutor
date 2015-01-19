#include "llvm/Support/raw_ostream.h"
#include "debug.h"
#include "main.h"
#include "mem.h"

void debug_dump(void){
	llvm::outs() << "####### DEBUG DUMP BEGIN\n";
	llvm::outs() << " Current line: " << currloc << "\n";
	llvm::outs() << " Global variables:\n";
	for ( auto it = global_vars.cbegin(); it != global_vars.cend(); ++it ){
		llvm::outs() << "\t Global '" << it->first << "' has block " << it->second.ptr.block << "\n";
	}
	llvm::outs() << "+++++++ DEBUG DUMP END\n";
}
