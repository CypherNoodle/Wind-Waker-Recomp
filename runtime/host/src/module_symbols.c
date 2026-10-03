#include "module_symbols.h"

#include <stddef.h>
#include <string.h>

#if BLUEWAKE_STATIC_MODULE

// Keep these declarations deliberately untyped. The callers already own the
// exact ABI typedefs and cast the result just as they do for dlsym. Optional
// option exports are weak because a composite without BetterWW has none.
#define BLUEWAKE_EXPORT(name) extern void name(void)
#define BLUEWAKE_WEAK_EXPORT(name) \
    extern void name(void) __attribute__((weak))

BLUEWAKE_EXPORT(staticrecomp_get_module);
BLUEWAKE_EXPORT(staticrecomp_get_rel_data);
BLUEWAKE_EXPORT(bluewake_set_mem_write_journal);
BLUEWAKE_EXPORT(bluewake_set_edge_service);
BLUEWAKE_EXPORT(bluewake_composite_mod_count);
BLUEWAKE_EXPORT(bluewake_composite_mod_name);
BLUEWAKE_EXPORT(bluewake_composite_apply_mods);
BLUEWAKE_EXPORT(bluewake_composite_mod_writes);
BLUEWAKE_WEAK_EXPORT(bluewake_composite_option_flags);
BLUEWAKE_WEAK_EXPORT(bluewake_composite_set_native_hook);
BLUEWAKE_WEAK_EXPORT(bluewake_composite_option_count);
BLUEWAKE_WEAK_EXPORT(bluewake_composite_option);
BLUEWAKE_WEAK_EXPORT(bluewake_composite_option_writes);

typedef struct BluewakeStaticSymbol {
    const char* name;
    void (*address)(void);
} BluewakeStaticSymbol;

static const BluewakeStaticSymbol k_symbols[] = {
    {"staticrecomp_get_module", staticrecomp_get_module},
    {"staticrecomp_get_rel_data", staticrecomp_get_rel_data},
    {"bluewake_set_mem_write_journal", bluewake_set_mem_write_journal},
    {"bluewake_set_edge_service", bluewake_set_edge_service},
    {"bluewake_composite_mod_count", bluewake_composite_mod_count},
    {"bluewake_composite_mod_name", bluewake_composite_mod_name},
    {"bluewake_composite_apply_mods", bluewake_composite_apply_mods},
    {"bluewake_composite_mod_writes", bluewake_composite_mod_writes},
    {"bluewake_composite_option_flags", bluewake_composite_option_flags},
    {"bluewake_composite_set_native_hook", bluewake_composite_set_native_hook},
    {"bluewake_composite_option_count", bluewake_composite_option_count},
    {"bluewake_composite_option", bluewake_composite_option},
    {"bluewake_composite_option_writes", bluewake_composite_option_writes},
};

void* bluewake_module_open(const char* path) {
    (void)path;
    return (void*)&k_symbols;
}

void* bluewake_module_symbol(void* module, const char* name) {
    if (module == NULL || name == NULL)
        return NULL;
    for (size_t i = 0; i < sizeof k_symbols / sizeof k_symbols[0]; ++i) {
        if (strcmp(name, k_symbols[i].name) == 0) {
            union {
                void (*function)(void);
                void* object;
            } address = {k_symbols[i].address};
            return address.object;
        }
    }
    return NULL;
}

const char* bluewake_module_error(void) {
    return "translated composite symbol is not linked into this executable";
}

void bluewake_module_close(void* module) { (void)module; }

#else

#include <dlfcn.h>

void* bluewake_module_open(const char* path) {
    return dlopen(path, RTLD_NOW | RTLD_LOCAL);
}

void* bluewake_module_symbol(void* module, const char* name) {
    return dlsym(module, name);
}

const char* bluewake_module_error(void) { return dlerror(); }

void bluewake_module_close(void* module) {
    if (module != NULL)
        dlclose(module);
}

#endif
