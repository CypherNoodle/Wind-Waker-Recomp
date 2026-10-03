#ifndef BLUEWAKE_MODULE_SYMBOLS_H
#define BLUEWAKE_MODULE_SYMBOLS_H

// The desktop host loads the translated composite from a dylib. Platforms
// without a dynamic loader (notably Horizon/libnx) link the same exports into
// the executable and resolve them by name through this small common ABI.
void* bluewake_module_open(const char* path);
void* bluewake_module_symbol(void* module, const char* name);
const char* bluewake_module_error(void);
void bluewake_module_close(void* module);

#endif
