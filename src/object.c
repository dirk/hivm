#include <stdlib.h>

#include "object.h"

hvm_obj_array *hvm_new_obj_array() {
  hvm_obj_array *arr = malloc(sizeof(hvm_obj_array));
  arr->data = malloc(sizeof(hvm_obj_ref*) * 1);
  arr->data[0] = NULL;
  arr->length = 0;
  return arr;
}
