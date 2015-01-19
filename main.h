#include "clang/AST/Expr.h"
#include "types.h"

extern unsigned int currloc;
void do_exit(const EmuInt* retval);
const EmuVal* eval_rexpr(const Expr* expr);
