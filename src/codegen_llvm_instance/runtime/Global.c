#include "Runtime.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

union storage_t {
  int32_t i32;
  int64_t i64;
  float f32;
  double f64;
};

struct sable_global_t {
  enum sable_valuetype_t type;
  union storage_t storage;
};

static bool is_valid_valuetype(enum sable_valuetype_t type) {
  switch (type) {
  case vtI32:
  case vtI64:
  case vtF32:
  case vtF64: return true;
  default: return false;
  }
}

sable_global_ptr sable_global_create(enum sable_valuetype_t type) {
  assert(is_valid_valuetype(type) && "value type is invalid");
  struct sable_global_t *global =
      (struct sable_global_t *)malloc(sizeof(struct sable_global_t));
  global->type = type;
  memset(&global->storage, 0, sizeof(union storage_t));
  return global;
}

void sable_global_free(sable_global_ptr global) {
  assert(global != NULL);
  free(global);
}

int32_t *sable_global_as_i32(sable_global_ptr global) {
  assert((global != NULL) && (global->type == vtI32) && "type mismatch");
  return &global->storage.i32;
}

int64_t *sable_global_as_i64(sable_global_ptr global) {
  assert((global != NULL) && (global->type == vtI64) && "type mismatch");
  return &global->storage.i64;
}

float *sable_global_as_f32(sable_global_ptr global) {
  assert((global != NULL) && (global->type == vtF32) && "type mismatch");
  return &global->storage.f32;
}

double *sable_global_as_f64(sable_global_ptr global) {
  assert((global != NULL) && (global->type == vtF64) && "type mismatch");
  return &global->storage.f64;
}
