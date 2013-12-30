// THIS IS OUTDATED AS SHIT.

#ifndef HVM_GC_H
#define HVM_GC_H

// Communicates with the VM, object space, and heap to mark and sweep memory
// in the object space and heap.

// Ideally this will run in a separate thread and do smart locking with the
// object space and heap to sweep and defrag/compact both with minimal
// locking with the VM.

#endif
