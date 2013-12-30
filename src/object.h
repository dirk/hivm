// THIS IS OUTDATED AS SHIT.

#ifndef HVM_OBJECT_H
#define HVM_OBJECT_H

// Objects are either primitive or composite.

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
