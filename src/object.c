#include <stdlib.h>

#include "object.h"

// TODO: Flywheelize this.
hvm_obj_string *hvm_new_obj_string() {
  hvm_obj_string *str = malloc(sizeof(hvm_obj_string));
  str->data = NULL;
  return str;
}

hvm_obj_array *hvm_new_obj_array() {
  hvm_obj_array *arr = malloc(sizeof(hvm_obj_array));
  arr->data = malloc(sizeof(hvm_obj_ref*) * 1);
  arr->data[0] = NULL;
  arr->length = 0;
  return arr;
}

hvm_obj_struct *hvm_new_obj_struct() {
  hvm_obj_struct *strct = malloc(sizeof(hvm_obj_struct));
  strct->heap_size = HVM_STRUCT_INITIAL_HEAP_SIZE;
  strct->heap = malloc(HVM_STRUCT_HEAP_MEMORY_SIZE(strct->heap_size));
  strct->heap[0] = NULL;
  return strct;
}
