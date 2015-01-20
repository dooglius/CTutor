#include "clang/AST/Type.h"
#include "types.h"
#include "mem.h"

const EmuVal* cast_to(const EmuVal*, QualType);
const EmuVal* from_lvalue(lvalue);
bool is_scalar_zero(const EmuVal*);
