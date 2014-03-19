# Notes

## Disclaimer

These are crazy-ass notes that probably are not very intelligent. You've been warned.

## Types

Hivm has 6+1 data types:

* Null
* Integer: signed with unlimited precision
* Float: 64-bit/double precision
* String: known-length byte sequences, *use UTF-8 because it's cool and we don't need any more Western-Latin hegemony*
* Structure: composite data type that maps symbols to data values; heavily inspired by Lua's tables and C structs; performance *must* be extremely fast
* Array: dynamic array; can contain any type as values
* (Symbol: just a non-negative integer mapping to a string in the VM's symbol table)

### Booleans

There are two falsy values: null and the zero integer (0x0). Everything else is true. If you want anything fancier than that then you've got to build it yourself at a higher layer.

(Also if you manage to somehow end up with a sign-negative zero (0x80000000 in big-endian 32-bit), then I will be both impressed and full of pity for you.)

### Null

Null is and will always be nothing.

### Simple data types

Null, integer, float, and string are all "simple" data types. They are immutable.

### Complex data types

Complex data types are mutable.

Arrays are fast dynamic arrays that grow and shrink as you need them. They also support stack and queue operations (push, pop, shift, unshift).

Structures are designed to support objects. There will likely be some form of type assignment/hinting so that you can build a relative-high-performance static/dynamic-yet-consistent type system on top of them. The optimizer/JIT compiler will also carefully watch structure interaction (and aforementioned type notations) when generating type-guarded native/optimized code.

## Registers

Hivm provides essentially-infinite registers. Registers are type-aware of the data they contain.

### General (temporary) registers: $r0, $r1, ...

General-purpose registers; local only to the current stack frame and are not saved between calls.

### Argument registers: $a0, $a1, ...

Write-only. Used for arguments when calling subroutines. Contain null by default.

### Parameter registers: $pn, $p0, $p1, ...

Read-only. Used by subroutines to read arguments. $pn is a special integer register that contains the total number of parameters passed (allows varargs). Like argument registers they are null by default.

### Virtual registers: $zero, $null

The $null register will always be null. Writing any value in the $null register will not raise an exception.

$zero will always be a zero integer.

## Calling conventions

Invoking subroutines should normally follow the following process:

1. Set argument registers with values to be passed to the subroutine.
2. Use the `call SUB RET` instruction to call the subroutine. SUB should be an integer-valued register with the address. RET can be a register for the return value of the subroutine to be placed in. If the subroutine is not expected to return a value or you don't want to use the value returned then use the $null register as RET.

## Instruction set

This is pretty inspired by ARM, MIPS, and various other RISCs. For now it will probably be internally represented in 32-bit words. You should never generate these yourself; always use Hivm's generator API to interact with the code-memory of a VM instance. (However it will make it easy to extract compiled bytecode for a code region (ie. file) and cache that for reuse in the same version of the VM.) As far as any limitations imposed by this design decision goes I'm going to adopt the following philosophy: if it screams bloody murder about something you do now then it may not in the future, if it doesn't scream bloody murder about something right now then it *really shouldn't* in the future.

See [INSTRUCTIONS](INSTRUCTIONS.md) for detailed documentation of instructions.

## Variables

Variables can either be stored in (temporary/frame-local) registers or scopes. Register variables do not interact with the garbage collection system in any way. Scopes are specialized structures and therefore interact with the GC. This is heavily inspired by old C-style memory management with a somewhat-clear usage divide between stacks (relatively primitive) and heaps (complex).

### Closures

The generator API will provide a handy utility function ("lexicalize"?) which will symbolicate all the local variables for a scope into a compact closure scope structure that can be easily embedded into a function object.

## Constants

Bytecode chunks may include a constant pool for any necessary values. Instructions reference constants locally to their chunks. These chunk-relative references are resolved to VM-relative references when the chunk is loaded.

## Examples

### Anonymous functions

The following is pseudo-JS and pseudo-ASM for implementing an anonymous function.

```js
var a = function() { ... }
// (stuff)
a()
```

```ruby
# @_anonymous_123 will be resolved to a relative address by the generator
# and will be marked for absolute address resolution in the chunk created
# by the generator.
SETCONSTANT $r0, @_anonymous_123
CALL _js_new_function, $r1, "(anonymous)", $r0
SETLOCAL :a, $r1 # :a is in the constant pool and resolved ahead-of-time
# (stuff)
GETLOCAL $r2, :a # Get the function struct created by _js_new_function
STRUCTGET $r3, $r2, :_js_function_addr # Get the internal address
CALLDYNAMIC $r3, $null # Invoke the code at that address (_anonymous_123)

_anonymous_123:
  ...
  RETURN $null
```
