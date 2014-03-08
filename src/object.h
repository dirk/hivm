#ifndef HVM_OBJECT_H
#define HVM_OBJECT_H

///@relates hvm_obj_struct
#define HVM_STRUCT_INITIAL_HEAP_SIZE 8
///@relates hvm_obj_struct
#define HVM_STRUCT_HEAP_MEMORY_SIZE(S) (S * (sizeof(uint64_t) + sizeof(void)))

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

/// Base reference to an object
typedef struct hvm_obj_ref {
  hvm_obj_type  type;
  /// 8 bytes of data to play with; can be used as pointer or literal.
  uint64_t      data;
} hvm_obj_ref;

// TYPES
typedef struct hvm_obj_string {
  char* data;
} hvm_obj_string;

typedef struct hvm_obj_array {
  hvm_obj_ref** data;
  unsigned int length;
} hvm_obj_array;

///@details `memory size (bytes) = heap_size (pairs) * 2 (words/pair) * 8 (bytes/word)`
typedef struct hvm_obj_struct {
  /// Binary heap of symbol-integer-and-pointer pairs.
  void** heap;
  /// Number of pairs in the binary heap.
  unsigned int heap_size;
} hvm_obj_struct;

// CONSTRUCTORS
hvm_obj_string *hvm_new_obj_string();
hvm_obj_array *hvm_new_obj_array();
/// Construct a new structure.
/// @memberof hvm_obj_struct
hvm_obj_struct *hvm_new_obj_struct();

hvm_obj_ref *hvm_new_obj_ref();
void hvm_obj_ref_set_string(hvm_obj_ref*, hvm_obj_string*);

// PRIMITIVE
// Composed of just metadata and primitive value.
// Types: null, integer, float, (symbol)

// INDIRECT PRIMITIVE
// Types: string

// COMPOSITE
// Types: structure, array


// OLD ------------------------------------------------------------------------

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
