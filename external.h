#include <string>
#include <unordered_map>
#include "types.h"

bool is_lvalue_based_macro(uint32_t id);
const EmuFunc* get_external_func(std::string, QualType);
const EmuVal* call_external(uint32_t);
const EmuVal* emu_malloc(void);
const EmuVal* emu_free(void);
const EmuVal* emu_va_start(void);

typedef const EmuVal* (*external_func_t)(void);
