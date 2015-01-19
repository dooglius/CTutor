#include <stdlib.h>
#include "llvm/Support/raw_ostream.h"
#include "exit.h"

void exit_premature(void){
	exit(1);
}

void cant_handle(void){
	llvm::outs() << "!The interpreter doesn't know how to handle this\n";
	exit_premature();
}

void bad_memwrite(void){
	llvm::outs() << "!Attempted write to unallocated memory\n";
	exit_premature();
}

void err_exit(const char* s){
	llvm::outs() << "!The following error was reported: "<<s<<"\n";
	exit_premature();
}

void cant_cast(void){
	llvm::outs() << "!The interpreter doesn't know how to perform this cast\n";
	exit_premature();
}

void exit_clean(void){
	exit(0);
}
