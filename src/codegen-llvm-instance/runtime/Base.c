#include "Base.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <sys/mman.h>
#include <unistd.h>

#if __has_builtin(__builtin_trap)
#define SABLE_UNREACHABLE() __builtin_trap()
#else
#include <cstdlib>
#define SABLE_UNREACHABLE() std::abort()
#endif

#define EXPECT(condition)                                                      \
  if (!(condition)) abort()
#define INVOKE_TRAP(trap_handler, trap_code)                                   \
  {                                                                            \
    trap_handler(trap_code);                                                   \
    SABLE_UNREACHABLE();                                                       \
  }

#define WASM_PAGE_SIZE /* 64 KiB */ (64 * 1024)
#define MEMORY_GROW_FAILED ((uint32_t)-1)
#define UPPER_BOUND_UNSET ((size_t)-1)

__sable_instance_ptr __sable_instance_allocate(
    __sable_instance_getter global_getter,
    __sable_instance_getter memory_getter, __sable_instance_getter table_getter,
    __sable_instance_getter function_getter, __sable_trap_handler trap_handler,
    uint32_t num_entry) {
  __sable_instance_ptr instance =
      (__sable_instance_ptr)calloc(num_entry + 5, sizeof(void *));
  instance->global_getter = global_getter;
  instance->memory_getter = memory_getter;
  instance->table_getter = table_getter;
  instance->function_getter = function_getter;
  instance->trap_handler = trap_handler;
  return instance;
}

void __sable_instance_free(__sable_instance_ptr instance) { free(instance); }

static bool __sable_is_valid_type_char(char type) {
  switch (type) {
  case SABLE_I32:
  case SABLE_I64:
  case SABLE_F32:
  case SABLE_F64: return true;
  default: return false;
  }
}

typedef union {
  int32_t i32;
  int64_t i64;
  float f32;
  double f64;
} __sable_global_storage;
struct __sable_global {
  __sable_global_storage storage;
  char type;
};

__sable_global_ptr __sable_global_allocate(char type) {
  if (!__sable_is_valid_type_char(type)) return NULL;
  __sable_global_ptr global =
      (__sable_global_ptr)malloc(sizeof(__sable_global_t));
  memset(&global->storage, 0, sizeof(__sable_global_storage));
  global->type = type;
  return global;
}

void __sable_global_free(__sable_global_ptr global) { free(global); }

void *__sable_global_get(__sable_global_ptr global) { return &global->storage; }

char __sable_global_type(__sable_global_ptr global) { return global->type; }

struct __sable_memory_metadata {
  size_t size;
  size_t max_size;
  size_t mapped_size;
  void *mapped_address;
  void *start_address;
};
typedef struct __sable_memory_metadata *__sable_memory_metadata_ptr;
typedef struct __sable_memory_metadata __sable_memory_metadata_t;

static __sable_memory_metadata_ptr
__sable_get_metadata(__sable_memory_ptr memory) {
  assert(memory != NULL);
  size_t page_size = sysconf(_SC_PAGESIZE);
  return (__sable_memory_metadata_ptr) & ((char *)memory)[-page_size];
}

__sable_memory_ptr __sable_memory_allocate(uint32_t num_page) {
  size_t page_size = sysconf(_SC_PAGESIZE);
  EXPECT(WASM_PAGE_SIZE % page_size == 0);
  EXPECT(sizeof(__sable_memory_metadata_t) < page_size);
  size_t size = WASM_PAGE_SIZE * num_page;
  size_t mapped_size = size + page_size;
  int protect = PROT_READ | PROT_WRITE;
  int flags = MAP_PRIVATE | MAP_ANONYMOUS;
  void *allocated_mem = mmap(NULL, mapped_size, protect, flags, -1, 0);
  if (allocated_mem == MAP_FAILED) return NULL;
  void *start_address = &((char *)(allocated_mem))[page_size];
  ((__sable_memory_metadata_ptr)allocated_mem)->size = size;
  ((__sable_memory_metadata_ptr)allocated_mem)->max_size = UPPER_BOUND_UNSET;
  ((__sable_memory_metadata_ptr)allocated_mem)->mapped_size = mapped_size;
  ((__sable_memory_metadata_ptr)allocated_mem)->mapped_address = allocated_mem;
  ((__sable_memory_metadata_ptr)allocated_mem)->start_address = start_address;
  return start_address;
}

__sable_memory_ptr
__sable_memory_allocate_with_bound(uint32_t num_page, uint32_t max) {
  if (max < num_page) return NULL;
  __sable_memory_ptr memory = __sable_memory_allocate(num_page);
  if (memory == NULL) return NULL;
  __sable_memory_metadata_ptr metadata = __sable_get_metadata(memory);
  metadata->max_size = max;
  return memory;
}

void __sable_memory_free(__sable_memory_ptr memory) {
  __sable_memory_metadata_ptr metadata = __sable_get_metadata(memory);
  size_t mapped_size = metadata->mapped_size;
  void *mapped_address = metadata->mapped_address;
  munmap(mapped_address, mapped_size);
}

uint32_t __sable_memory_size(__sable_memory_ptr memory) {
  __sable_memory_metadata_ptr metadata = __sable_get_metadata(memory);
  return metadata->size / WASM_PAGE_SIZE;
}

uint32_t __sable_memory_grow(__sable_memory_ptr *memory, uint32_t delta) {
  size_t page_size = sysconf(_SC_PAGESIZE);
  __sable_memory_metadata_ptr metadata = __sable_get_metadata(*memory);
  uint32_t num_page = __sable_memory_size(*memory) + delta;
  if (num_page > 0x10000) return MEMORY_GROW_FAILED;
  if ((metadata->max_size != UPPER_BOUND_UNSET) &&
      (metadata->max_size < num_page))
    return MEMORY_GROW_FAILED;
  size_t new_size = num_page * WASM_PAGE_SIZE;
  size_t new_mapped_size = new_size + page_size;
  void *new_mapped_address = mremap(
      metadata->mapped_address, metadata->mapped_size, new_mapped_size,
      MREMAP_MAYMOVE);
  if (new_mapped_address == MAP_FAILED) return MEMORY_GROW_FAILED;
  *memory = (__sable_memory_ptr)(&((char *)new_mapped_address)[page_size]);
  metadata = __sable_get_metadata(*memory);
  metadata->size = new_size;
  metadata->mapped_size = new_mapped_size;
  metadata->mapped_address = new_mapped_address;
  metadata->start_address = *memory;
  return num_page;
}

void __sable_memory_guard(
    __sable_instance_ptr instance, __sable_memory_ptr memory,
    uint32_t address) {
  __sable_memory_metadata_ptr metadata = __sable_get_metadata(memory);
  if (!(address < metadata->size))
    INVOKE_TRAP(instance->trap_handler, SABLE_TRAP_MEMORY_OUT_OF_BOUND)
}

struct __sable_table_entry {
  void (*func_ptr)();
  char *type;
};
typedef struct __sable_table_entry __sable_table_entry_t;
typedef struct __sable_table_entry *__sable_table_entry_ptr;
struct __sable_table {
  __sable_table_entry_ptr storage;
  size_t size;
  size_t max;
};
typedef struct __sable_table __sable_table_t;
typedef struct __sable_table *__sable_ptr;

__sable_table_ptr __sable_table_allocate(uint32_t num_entry) {
  __sable_table_ptr table = (__sable_table_ptr)malloc(sizeof(__sable_table_t));
  if (table == NULL) return NULL;
  table->storage =
      (__sable_table_entry_ptr)calloc(sizeof(__sable_table_entry_t), num_entry);
  table->size = num_entry;
  table->max = UPPER_BOUND_UNSET;
  if (table->storage == NULL) {
    free(table);
    return NULL;
  }
  return table;
}

__sable_table_ptr
__sable_table_allocate_with_bound(uint32_t num_entry, uint32_t max) {
  if (max < num_entry) return NULL;
  __sable_table_ptr table = __sable_table_allocate(num_entry);
  if (table == NULL) return NULL;
  table->max = max;
  return table;
}

void __sable_table_free(__sable_table_ptr table) {
  for (size_t i = 0; i < table->size; ++i) free(table->storage[i].type);
  free(table->storage);
  free(table);
}

uint32_t __sable_table_size(__sable_table_ptr table) { return table->size; }

void __sable_table_guard(
    __sable_instance_ptr instance, __sable_table_ptr table, uint32_t index) {
  if (!(index < table->size))
    INVOKE_TRAP(instance->trap_handler, SABLE_TRAP_TABLE_OUT_OF_BOUND)
  if ((table->storage[index].func_ptr == NULL) ||
      (table->storage[index].type == NULL))
    INVOKE_TRAP(instance->trap_handler, SABLE_TRAP_TABLE_NULL)
}

static bool __sable_is_valid_type_string(char const *str) {
  for (; (*str != ':') && (*str != '\0'); ++str)
    if (!__sable_is_valid_type_char(*str)) return false;
  if (*str == '\0') return false;
  for (str = str + 1; *str != '\0'; ++str)
    if (!__sable_is_valid_type_char(*str)) return false;
  return true;
}

__sable_func_ptr __sable_table_set(
    __sable_table_ptr table, uint32_t index, __sable_func_ptr func_ptr,
    char const *type) {
  if (func_ptr == NULL) {
    assert(type == NULL);
    table->storage[index].func_ptr = NULL;
    table->storage[index].type = NULL;
  } else {
    assert(type != NULL);
    if (!__sable_is_valid_type_string(type)) return NULL;
    char *dup_type = strdup(type);
    if (dup_type == NULL) return NULL;
    table->storage[index].func_ptr = func_ptr;
    table->storage[index].type = dup_type;
  }
  return table->storage[index].func_ptr;
}

__sable_func_ptr __sable_table_get(__sable_table_ptr table, uint32_t index) {
  return table->storage[index].func_ptr;
}

char const *__sable_table_type(__sable_table_ptr table, uint32_t index) {
  return table->storage[index].type;
}

int32_t __sable_strcmp(char const *lhs, char const *rhs) {
  return strcmp(lhs, rhs);
}
