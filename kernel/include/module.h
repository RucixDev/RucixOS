#ifndef MODULE_H
#define MODULE_H

#include <types.h>
#include <list.h>

#define MODULE_NAME_LEN 64

typedef int (*module_init_t)(void);
typedef void (*module_exit_t)(void);

 
typedef enum {
    MODULE_STATE_LIVE,
    MODULE_STATE_COMING,
    MODULE_STATE_GOING,
} module_state_t;

 
typedef struct module {
    module_state_t state;
    struct list_head list;
    char name[MODULE_NAME_LEN];

     
    void *core_layout_base;
    uint64_t core_layout_size;
    
     
    module_init_t init;
    module_exit_t exit;

     
    struct kernel_symbol *syms;
    unsigned int num_syms;

     
    struct list_head dependencies; 
} module_t;

 
struct kernel_symbol {
    const char *name;
    uint64_t value;
};

 
 

#define EXPORT_SYMBOL(sym) \
    static const char __kstrtab_##sym[] __attribute__((section("__ksymtab_strings"), aligned(1))) = #sym; \
    static const struct kernel_symbol __ksymtab_##sym __attribute__((section("__ksymtab"), used)) = { __kstrtab_##sym, (uint64_t)&sym }

 
#define module_init(initfn) \
    int init_module(void) __attribute__((alias(#initfn)));

#define module_exit(exitfn) \
    void cleanup_module(void) __attribute__((alias(#exitfn)));

 
long sys_init_module(void *module_image, unsigned long len, const char *param_values);
long sys_delete_module(const char *name, unsigned int flags);

 
uint64_t module_kallsyms_lookup_name(const char *name);

void module_subsystem_init();

#endif
