// THIS IS OUTDATED AS SHIT.

#ifndef HVM_OBJECT_H
#define HVM_OBJECT_H

// Objects are either primitive or composite.

typedef enum {
  HVM_NULL = 0,
  HVM_INTEGER = 1,
  HVM_FLOAT = 2,
  HVM_STRING = 3,
  HVM_STRUCTURE = 4,
  HVM_ARRAY = 5,
  HVM_SYMBOL = 6// Internally same as HVM_INTEGER
} hvm_obj_type;

typedef unsigned char byte;

/// Base reference to an object
typedef struct hvm_obj_ref {
  hvm_obj_type  type;
  uint64_t      data;// 8 bytes of data to play with
} hvm_obj_ref;

typedef struct hvm_obj_string {
  char* data;
} hvm_obj_string;

typedef struct hvm_obj_array {
  hvm_obj_ref** data;
  unsigned int length;
} hvm_obj_array;

hvm_obj_array *hvm_new_obj_array();

// PRIMITIVE
// Composed of just metadata and primitive value (integer, float, etc.)

// COMPOSITE
// Metadata and dynamic slots. Dynamic slots are by default looked up in  a
// compact hash stored inline with the object (need some very specific 
// tuning for hash growth factors/stages).

// COMPOSITE TYPED
// Metadata, static slots, and dynamic slots. Static slots are stored as a 
// constant-sized table between the metadata and dynamic slot data. Operations
// working with known types can therefore cache lookups into constant indexes
// into the static slot table.

#endif
