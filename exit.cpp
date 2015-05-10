#include <stdlib.h>
#include "debug.h"
#include "exit.h"

void exit_premature(void){
	exit(1);
}

void cant_handle(void){
	debug_dump("The interpreter doesn't know how to handle this");
	exit_premature();
}

void bad_memwrite(void){
	debug_dump("Attempted write to unallocated memory");
	exit_premature();
}

void bad_memread(void){
	debug_dump("Attempted read larger than memory allocation");
	exit_premature();
}

void err_exit(const char* s){
	debug_dump(s);
	exit_premature();
}

void cant_cast(void){
	debug_dump("The interpreter doesn't know how to perform this cast");
	exit_premature();
}

void exit_clean(void){
	exit(0);
}

void err_undef(void){
	debug_dump("Tried to use an undefined or uninitialized value");
	exit_premature();
}
