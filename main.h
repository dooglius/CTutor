#include "clang/AST/ASTContext.h"
#include "clang/AST/Expr.h"
#include "types.h"

extern SourceManager* src_mgr;
extern unsigned int currloc;
void do_exit(const EmuInt*);
const EmuVal* eval_rexpr(const Expr*);
const EmuVal* exec_stmt(const Stmt*);
