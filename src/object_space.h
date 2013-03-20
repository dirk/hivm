#ifndef HVM_OBJECT_SPACE_H
#define HVM_OBJECT_SPACE_H

// Object space stores mappings between object references (constant pointers)
// and heap references (variable pointers).

// Although this means a worst-case scenario of two lookups for every object
// reference from the VM, with inline caching and other optimizations it
// should be possible to average closer to 1 distant lookup.

#endif
