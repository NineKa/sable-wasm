#ifndef SABLE_INCLUDE_GUARD_CODEGEN_LLVM_CTX_RUNTIME
#define SABLE_INCLUDE_GUARD_CODEGEN_LLVM_CTX_RUNTIME

#include <stdint.h>

enum sable_valuetype_t { vtI32, vtI64, vtF32, vtF64 };

struct sable_global_t;
typedef struct sable_global_t *sable_global_ptr;

sable_global_ptr sable_global_create(enum sable_valuetype_t type);
void sable_global_free(sable_global_ptr global);

int32_t *sable_global_as_i32(struct sable_global_t *global);
int64_t *sable_global_as_i64(struct sable_global_t *global);
float *sable_global_as_f32(struct sable_global_t *global);
double *sable_global_as_f64(struct sable_global_t *global);

void *sable_memory_create(uint32_t num_page);
void *sable_memory_create_with_limit(uint32_t num_page, uint32_t max_page);
void sable_memory_free(void *memory);
uint32_t sable_memory_size(void *memory);
uint32_t sable_memory_grow(void *memory, uint32_t delta_page);

struct sable_table_t;
typedef struct sable_table_t *sable_table_ptr;
sable_table_ptr sable_table_create(uint32_t num_entry);
sable_table_ptr
sable_table_create_with_limit(uint32_t num_entry, uint32_t max_entry);
void *sable_table_get(sable_table_ptr table, uint32_t index);
char const *sable_table_get_type(sable_table_ptr table, uint32_t index);
void sable_table_set(sable_table_ptr table, uint32_t index, void *func_ptr);

/*
 * using instance_t = void *[];
 * [ memory | table | trap_handler | function (only imported) | global ]
 *
 * instance_t *instance_create();
 * void instance_free(instance_t *instance);
 * int32_t instance_initialize(instance_t *instance);
 *
 * int32_t set_import_global(
 *   instance_t *instance, char const *module_name,
 *   char const *entity_name, sable_global_ptr global);
 * int32_t set_import_table(
 *   instance_t *instance, char const *module_name,
 *   char const *entity_name, void *memory);
 * int32_t set_import_table(
 *   instance_t *instance, char const *module_name,
 *   char const *entity_name, sable_table_t table);
 * int32_t set_import_function(
 *   instance_t *instance, char const *module_name,
 *   char const *entity_name, void *function);
 * void set_trap_handler(instance_t *instance, void *trap_handler);
 *
 * sable_global_ptr get_export_global  (instance_t *instance, char const *name)
 * void *           get_export_memory  (instance_t *instance, char const *name)
 * sable_table_ptr  get_export_table   (instance_t *instance, char const *name)
 * void *           get_export_function(instance_t *instance, char const *name)
 *
 * WASM Function: foo [i32, f32] -> []
 * Translate To : void foo(instance_t *instance, int32_t, float)
 * WASM Function: foo [i32, f32] -> [i32]
 * Translate To : int32_t foo(instance_t *instance, int32_t, float)
 * WASM Function: foo [i32, f32] -> [i32, f32]
 * Translate To :
 * struct {int32_t, float} foo(instance_t *instance, int32_t, float)
 */

// The following function is used in the generated code, user should not use
void *__sable_allocate_instance(uint32_t num_entry);
void *__sable_deallocate_instance(void *instance);
int32_t __sable_strcmp(char const *lhs, char const *rhs);
int32_t
__sable_data_copy(void *dest, uint32_t offset, void *source, uint32_t len);
int32_t
__sable_element_copy(void *table, uint32_t offset, void *source, uint32_t len);
#endif
