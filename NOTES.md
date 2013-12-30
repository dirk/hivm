## Disclaimer

These are crazy-ass notes written on various combinations of low sleep, copious amounts of tea/coffee, and/or runner's high after jogs. You've been warned.

## Types

Hivm supports a few different data types:

* Integer (signed with unlimited precision)
* Float (64-bit/double precision)
* String (known-length byte sequences, *use UTF-8 because it's cool and we don't need any more Western-Latin hegemony*)
* Probably more down the road.

## Registers

## Instruction set

This is pretty inspired by ARM, MIPS, and various other RISCs. For now it will probably be internally represented in 32-bit words. You should never generate these yourself; always use Hivm's generator API to interact with the code-memory of a VM instance. (However it will make it easy to extract compiled bytecode for a code region (ie. file) and cache that for reuse in the same version of the VM.) As far as any limitations imposed by this design decision goes I'm going to adopt the following philosophy: if it screams bloody murder about something you do now then it may not in the future, if it doesn't scream bloody murder about something right now then it *really shouldn't* in the future.

## Calling conventions

## Variables

### Closures

The generator API will provide a handy utility function ("lexicalize"?) which will symbolicate all the local variables for a scope into a compact closure scope object that can be easily embedded into a function object. (Possibly symbolicate all the local variable names into a sorted array that you can then easily do a binary search through for lookups. Also makes it easy to inline cache those indexes in the inner function during first invocation. Or maybe down the road make some sweet little fast table/hash thing for general use beyond this.)
