#include "clang/AST/Type.h"
#include "types.h"

EmuVal* cast_to(EmuVal*, QualType);
const EmuVal* cast_to(const EmuVal*, QualType);
EmuVal* blank_from_type(QualType);
emu_type_id_t type_id_from_type(QualType);
EmuVal* from_lvalue(lvalue);
bool is_scalar_zero(const EmuVal*);
