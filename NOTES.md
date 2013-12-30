## Disclaimer

These are crazy-ass notes written on various combinations of low sleep, copious amounts of tea/coffee, and/or runner's high after jogs. You've been warned.

## Types

Hivm has 5+1 data types:

* Null
* Integer: signed with unlimited precision
* Float: 64-bit/double precision
* String: known-length byte sequences, *use UTF-8 because it's cool and we don't need any more Western-Latin hegemony*
* Structure: composite data type that maps symbols to data values; heavily inspired by Lua's tables and C structs; performance *must* be extremely fast
* (Symbol: just a non-negative integer mapping to a string in the VM's symbol table)

### Booleans

There are two falsy values: null and the zero integer (0x0). Everything else is true. If you want anything fancier than that then you've got to build it yourself at a higher layer.

(Also if you manage to somehow end up with a sign-negative zero (0x80000000 in big-endian 32-bit), then I will be both impressed and full of pity for you.)

### Null

Null is and will always be nothing.

## Registers

Hivm provides essentially-infinite registers. Registers are local only to the current stack frame and are not saved between calls. Registers are type-aware of the data they contain.

## Instruction set

This is pretty inspired by ARM, MIPS, and various other RISCs. For now it will probably be internally represented in 32-bit words. You should never generate these yourself; always use Hivm's generator API to interact with the code-memory of a VM instance. (However it will make it easy to extract compiled bytecode for a code region (ie. file) and cache that for reuse in the same version of the VM.) As far as any limitations imposed by this design decision goes I'm going to adopt the following philosophy: if it screams bloody murder about something you do now then it may not in the future, if it doesn't scream bloody murder about something right now then it *really shouldn't* in the future.

## Calling conventions

## Variables

Variables can either be stored in (temporary local) registers or scopes. Register variables do not interact with the garbage collection system in any way. Scopes are specialized structures and therefore interact with the GC. This is heavily inspired by old C-style memory management with a somewhat-clear usage divide between stacks (relatively primitive) and heaps (complex).

### Closures

The generator API will provide a handy utility function ("lexicalize"?) which will symbolicate all the local variables for a scope into a compact closure scope structure that can be easily embedded into a function object.
