#include <string>
#include <unordered_map>
#include "types.h"

const EmuVal* call_external(std::string);
const EmuVal* emu_malloc(void);
const EmuVal* emu_free(void);

typedef const EmuVal* (*external_func_t)(void);

extern const std::unordered_map<std::string, external_func_t> impl_list;
