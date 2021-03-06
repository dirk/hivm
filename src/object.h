#ifndef HVM_OBJECT_H
#define HVM_OBJECT_H

///@relates hvm_obj_struct
#define HVM_STRUCT_INITIAL_HEAP_SIZE 8
///@relates hvm_obj_struct
#define HVM_STRUCT_HEAP_MEMORY_SIZE(S) (S * (sizeof(hvm_symbol_id) + sizeof(void*)))
#define HVM_STRUCT_HEAP_GROWTH_RATE 2

// Objects are either primitive or composite.

typedef enum {
  HVM_NULL = 0,
  HVM_INTEGER = 1,
  HVM_FLOAT = 2,
  HVM_STRING = 3,
  HVM_STRUCTURE = 4,
  HVM_ARRAY = 5,
  HVM_SYMBOL = 6,// Internally same as HVM_INTEGER
  HVM_INTERNAL = 7,
  HVM_EXCEPTION = 8
} hvm_obj_type;

/// @brief Union of types for the data field in hvm_obj_ref.
union hvm_obj_ref_data {
  /// Signed integer (used by HVM_INTEGER)
  int64_t  i64;
  /// Unsigned integer (used by HVM_SYMBOL)
  uint64_t u64;
  /// Void pointer
  void*    v;
};

/// Exempts an object from garbage collection (and should eventually
/// auto-promote to ancient generation).
#define HVM_OBJ_FLAG_CONSTANT 0x2
/// Flags an object as being tracked by the garbage collector.
#define HVM_OBJ_FLAG_GC_TRACKED 0x4
/// Instructs the GC not to follow objects referenced by this object.
/// For INTERNAL objects this tells the GC to not to try to free the memory
/// at `.data.v`.
#define HVM_OBJ_FLAG_NO_FOLLOW 0x8

/// Base reference to an object.
typedef struct hvm_obj_ref {
  /// Type of data stored in/pointed to by the reference.
  hvm_obj_type type;
  /// 8 bytes of data to play with; can be used as pointer or literal.
  union hvm_obj_ref_data data;
  /// Internal flags for the reference.
  byte flags;
  /// Pointer to its entry in the GC heap
  void *entry;
} hvm_obj_ref;


/// Each zone will hold 65,536 `hvm_obj_ref`
#define HVM_OBJ_REF_POOL_ZONE_SIZE 65536
#define HVM_OBJ_REF_POOL_ZONE_FULL HVM_OBJ_REF_POOL_ZONE_SIZE

typedef struct hvm_obj_ref_pool_zone {
  hvm_obj_ref *refs;
  /// Index of the first free slot in this zone, set this to
  /// HVM_OBJ_REF_POOL_ZONE_FULL to indicate that this zone is full
  unsigned int earliest_free;
  // Doubly-linked-list to preceding and following zones
  void *prev;
  void *next;
} hvm_obj_ref_pool_zone;

typedef struct hvm_obj_ref_pool {
  hvm_obj_ref_pool_zone *head;
  hvm_obj_ref_pool_zone *tail;
  /// First zone with empty slots
  hvm_obj_ref_pool_zone *earliest_free;
} hvm_obj_ref_pool;

// TYPES ----------------------------------------------------------------------

typedef struct hvm_obj_string {
  /// Raw string data (NULL-terminated)
  char* data;
} hvm_obj_string;

/// Dynamic array complex data type.
typedef struct hvm_obj_array {
#ifdef GLIB_MAJOR_VERSION
  /// Pointer to the internal glib GArray instance.
  GArray *array;
#else
  /// @cond
  void *array;
  /// @endcond
#endif
  // hvm_obj_ref** data;
  // unsigned int length;
} hvm_obj_array;

/// Pair of symbol ID and object references in a hvm_obj_struct's heap.
typedef struct hvm_obj_struct_heap_pair {
  /// Symbol key
  hvm_symbol_id id;
  /// Object value
  hvm_obj_ref*  obj;
} hvm_obj_struct_heap_pair;

/// @brief   Structure/table complex data type.
/// @details `memory size (bytes) = heap_size (pairs) * 2 (words/pair) * 8 (bytes/word)`
typedef struct hvm_obj_struct {
  /// Binary heap of symbol-integer-and-pointer pairs.
  hvm_obj_struct_heap_pair** heap;
  /// Number of pairs allocated for the binary heap.
  unsigned int heap_size;
  /// Number of pairs in the binary heap.
  unsigned int heap_length;
} hvm_obj_struct;


// CONSTRUCTORS
hvm_obj_string *hvm_new_obj_string();
hvm_obj_array *hvm_new_obj_array();
hvm_obj_array *hvm_new_obj_array_with_length(hvm_obj_ref*);
/// Construct a new structure.
/// @memberof hvm_obj_struct
hvm_obj_struct *hvm_new_obj_struct();

// DESTRUCTORS
void hvm_obj_free(hvm_obj_ref *ref);
void hvm_obj_struct_free(hvm_obj_struct*);

bool hvm_obj_is_falsey(hvm_obj_ref *ref);
bool hvm_obj_is_truthy(hvm_obj_ref *ref);

// Internal struct manipulation
hvm_obj_ref *hvm_obj_struct_internal_get(hvm_obj_struct*, hvm_symbol_id);
void hvm_obj_struct_internal_set(hvm_obj_struct*, hvm_symbol_id, hvm_obj_ref*);
// External manipulation (via object refs)
void hvm_obj_struct_set(hvm_obj_ref*, hvm_obj_ref*, hvm_obj_ref*);
hvm_obj_ref* hvm_obj_struct_get(hvm_obj_ref*, hvm_obj_ref*);
hvm_obj_ref* hvm_obj_struct_delete(hvm_obj_ref*, hvm_obj_ref*);
void hvm_obj_print_structure(hvm_vm *vm, hvm_obj_struct *strct);

hvm_obj_ref *hvm_new_obj_ref();
void hvm_obj_ref_set_string(hvm_obj_ref*, hvm_obj_string*);

/// New pool-based `hvm_obj_ref` allocator
hvm_obj_ref *hvm_obj_ref_new_from_pool(hvm_vm*);
hvm_obj_ref_pool *hvm_obj_ref_pool_new();
void hvm_obj_ref_free(hvm_vm*, hvm_obj_ref*);

hvm_obj_ref *hvm_new_obj_int(hvm_vm*);
hvm_obj_ref *hvm_obj_int_add(hvm_vm*, hvm_obj_ref*, hvm_obj_ref*);
hvm_obj_ref *hvm_obj_int_sub(hvm_vm*, hvm_obj_ref*, hvm_obj_ref*);
hvm_obj_ref *hvm_obj_int_mul(hvm_vm*, hvm_obj_ref*, hvm_obj_ref*);
hvm_obj_ref *hvm_obj_int_div(hvm_vm*, hvm_obj_ref*, hvm_obj_ref*);
hvm_obj_ref *hvm_obj_int_mod(hvm_vm*, hvm_obj_ref*, hvm_obj_ref*);
hvm_obj_ref *hvm_obj_int_lt (hvm_vm*, hvm_obj_ref*, hvm_obj_ref*);
hvm_obj_ref *hvm_obj_int_gt (hvm_vm*, hvm_obj_ref*, hvm_obj_ref*);
hvm_obj_ref *hvm_obj_int_lte(hvm_vm*, hvm_obj_ref*, hvm_obj_ref*);
hvm_obj_ref *hvm_obj_int_gte(hvm_vm*, hvm_obj_ref*, hvm_obj_ref*);
hvm_obj_ref *hvm_obj_int_eq (hvm_vm*, hvm_obj_ref*, hvm_obj_ref*);

hvm_obj_ref *hvm_obj_cmp_and(hvm_vm*, hvm_obj_ref*, hvm_obj_ref*);

void hvm_obj_array_push(hvm_obj_ref*, hvm_obj_ref*);
void hvm_obj_array_unshift(hvm_obj_ref*, hvm_obj_ref*);
hvm_obj_ref* hvm_obj_array_shift(hvm_obj_ref*);
hvm_obj_ref* hvm_obj_array_pop(hvm_obj_ref*);
hvm_obj_ref* hvm_obj_array_get(hvm_obj_ref*, hvm_obj_ref*);
void hvm_obj_array_set(hvm_obj_ref*, hvm_obj_ref*, hvm_obj_ref*);
hvm_obj_ref* hvm_obj_array_remove(hvm_obj_ref*, hvm_obj_ref*);
hvm_obj_ref* hvm_obj_array_len(hvm_vm*, hvm_obj_ref*);

uint64_t hvm_array_len(hvm_obj_array *arr);
hvm_obj_ref* hvm_obj_array_internal_get(hvm_obj_array*, uint64_t);

// UTILITIES ------------------------------------------------------------------
hvm_obj_ref *hvm_new_obj_ref_string_data(char *data);
const char *hvm_human_name_for_obj_type(hvm_obj_type type);

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
