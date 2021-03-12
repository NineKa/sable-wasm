#ifndef SABLE_INCLUDE_GUARD_CODEGEN_LLVM_INSTANCE_RUNTIME_BASE
#define SABLE_INCLUDE_GUARD_CODEGEN_LLVM_INSTANCE_RUNTIME_BASE

#include <stddef.h>
#include <stdint.h>

#define SABLE_TRAP_MEMORY_OUT_OF_BOUND 1
#define SABLE_TRAP_TABLE_OUT_OF_BOUND 2
#define SABLE_TRAP_TABLE_NULL 3

#define SABLE_EXPECT_OFFSET(Type, Field, Offset)                               \
  _Static_assert(offsetof(Type, Field) == Offset, "offset assertion failed")
#define SABLE_EXPECT_SIZE(Type, Size)                                          \
  _Static_assert(sizeof(Type) == Size, "size assertion failed")

#define SABLE_I32 'I'
#define SABLE_I64 'J'
#define SABLE_F32 'F'
#define SABLE_F64 'D'

struct __sable_instance;
typedef struct __sable_instance *__sable_instance_ptr;
typedef struct __sable_instance __sable_instance_t;
struct __sable_global;
typedef struct __sable_global *__sable_global_ptr;
typedef struct __sable_global __sable_global_t;
typedef void *__sable_memory_ptr;
struct __sable_table;
typedef struct __sable_table *__sable_table_ptr;

typedef void (*__sable_instance_getter)(__sable_instance_ptr, char const *);
typedef void (*__sable_trap_handler)(uint32_t trap_code);
struct __sable_instance {
  __sable_instance_getter global_getter;
  __sable_instance_getter memory_getter;
  __sable_instance_getter table_getter;
  __sable_instance_getter function_getter;
  __sable_trap_handler trap_handler;
};
SABLE_EXPECT_OFFSET(__sable_instance_t, global_getter, 0);
SABLE_EXPECT_OFFSET(__sable_instance_t, memory_getter, sizeof(void *));
SABLE_EXPECT_OFFSET(__sable_instance_t, table_getter, 2 * sizeof(void *));
SABLE_EXPECT_OFFSET(__sable_instance_t, function_getter, 3 * sizeof(void *));
SABLE_EXPECT_OFFSET(__sable_instance_t, trap_handler, 4 * sizeof(void *));

__sable_instance_ptr __sable_instance_allocate(
    __sable_instance_getter, __sable_instance_getter, __sable_instance_getter,
    __sable_instance_getter, __sable_trap_handler, uint32_t);
void __sable_instance_free(__sable_instance_ptr);

__sable_global_ptr __sable_global_allocate(char);
void __sable_global_free(__sable_global_ptr);
void *__sable_global_get(__sable_global_ptr);
char __sable_global_type(__sable_global_ptr);

__sable_memory_ptr __sable_memory_allocate(uint32_t);
__sable_memory_ptr __sable_memory_allocate_with_bound(uint32_t, uint32_t);
void __sable_memory_free(__sable_memory_ptr);
uint32_t __sable_memory_size(__sable_memory_ptr);
uint32_t __sable_memory_grow(__sable_memory_ptr *, uint32_t);
void __sable_memory_guard(__sable_instance_ptr, __sable_memory_ptr, uint32_t);

typedef void (*__sable_func_ptr)();

__sable_table_ptr __sable_table_allocate(uint32_t);
__sable_table_ptr __sable_table_allocate_with_bound(uint32_t, uint32_t);
void __sable_table_free(__sable_table_ptr);
uint32_t __sable_table_size(__sable_table_ptr);
void __sable_table_guard(__sable_instance_ptr, __sable_table_ptr, uint32_t);
__sable_func_ptr
__sable_table_set(__sable_table_ptr, uint32_t, __sable_func_ptr, char const *);
char const *__sable_table_type(__sable_table_ptr, uint32_t);
__sable_func_ptr __sable_table_get(__sable_table_ptr, uint32_t);

int32_t __sable_strcmp(char const *, char const *);
#endif
