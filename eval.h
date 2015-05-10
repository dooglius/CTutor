#include <vector>

#include "clang/AST/Expr.h"

#include "enums.h"
#include "types.h"

extern SourceManager* src_mgr;
extern int main_source;
extern SourceLocation curr_loc;
extern int curr_source;

void do_exit(const EmuNum<NUM_TYPE_INT>*);
const EmuVal* eval_rexpr(const Expr*);
const EmuVal* exec_stmt(const Stmt*);

void RunProgram(void);
void StoreFuncImpls(int);
void InitializeVars(int);
