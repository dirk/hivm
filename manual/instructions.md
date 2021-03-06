### Instruction set

##### Document syntax guide

- Instruction names are `lowercase`.
- Register names are `UPPERCASE`.
- Values in an instruction are prefixed with a `#HASH`.
- Reserved data in an instruction is in `(parentheses)`.

##### Reserved data

Some instructions provide an area region of data in their fields for the virtual machine to use for internal purposes. This region is required; the bytecode generation toolchain will automatically include these regions for you.

#### Array instructions

`arraypush A B`
:  Push B onto the end of array A.

`arrayunshift A B`
:  Push B onto the front of array A.

`arraypop B A`
:  Pop B off of the end of array A.

`arrayshift B A`
:  Pop B off the front of array A.

`arrayset A I V`
:  Set value V at index I in array A.

`arrayget V A I`
:  Get value at index I from array A and store it in V.

`arrayremove V A I`
:  Remove value at index I from array A and store it in V.

`arraynew A L`
:  Create a new array in A with length of non-negative integer L ($zero is allowed).

`arraylen B A`
:  Get the length of A and store it in B as an integer.

#### Structure operations

`structset S K V`
:  Set key symbol (non-negative integer) K in struct S to value V.

`structget V S K`
:  Get value from S by key symbol K and store the value in V.

`structdelete V S K`
:  Same as `structget`, however the key-value will be removed from S.

`structhas B S K`
:  Set boolean in B depending on whether or not struct S has a value for symbol key K. (Boolean will be integer 0 or 1.)

`structnew S`
:  Create a new struct in register S.

#### Subroutines

Subroutines may be executed either statically (address or symbolic name known ahead of time) or dynamically (address or name not known until execution). Static execution is referred to as a call, whereas dynamic execution is referred to as an invocation.

Non-primitive subroutine execution instructions include a reserved data section called the `(tag)`. The tag provides a space for the virtual machine to store information relating to the execution of that subroutine. Tags are currently 3 bytes in size.

##### Calls

Calls operate directly via hardcoded destinations. They are intended to be used within a common compilation block for fast subroutine invocation and tail-call recursion.

`call (tag) SUB RET`
:  Call the subroutine at address SUB (8-byte). RET can be a register for return or $null for no return or ignoring return.

`callprimitive CONST RET`
:  Look up a symbol ID from the constant table (4 bytes constant index), then call the primitive identified by that symbol ID.

`tailcall (tag) SUB`
:  Same as `call` but does not grow the stack. Current subroutine's return will be the return from SUB.

`callsymbolic (tag) CONST RET`
:  Look up a symbol ID from the constant table, then call the subroutine identified by that symbol ID.

##### Invocations

Invocations use symbols and addresses passed via registers to control which subroutine/primitive is invoked.

`invokesymbolic (tag) SYM RET`
:  Invoke subroutine identified by the symbol ID in register SYM.

`invokeaddress (tag) ADDR RET`
:  Invoke subroutine at address in register ADDR.

`invokeprimitive SYM RET`
:  Invoke the primitive with symbol ID in SYM.

##### Header

**Note:** This is currently being (re)considered and is not currently implemented.

Subroutines may have an optional header. This is a special no-op instruction that the virtual machine uses to store optimization metadata (tracking hot-ness, trampolining to/from the JIT, etc.). Using `hvm_gen_sub` and similar generator functions will automatically insert this header for you. The header may only appear as the first instruction in a subroutine.

`subheader (data)`
:  Header at the beginning of the subroutine.

#### Control flow

`return RET`
:  Return from the current subroutine to the parent. RET can be a register for returning a value or $null.

`if COND DEST`
:  Jump to DEST if COND is truthy (not null and not a zero integer).

##### Jumps and gotos

`jump DIFF`
:  Jump DIFF (integer) instructions forwards (positive) or backwards (negative).

`goto DEST`
:  Go to DEST (non-negative integer).

`gotoaddress DEST`
:  Go to address in register DEST.

#### Exceptions

`catch DEST EXC`
:  Register an exception handler for the current stack frame at destination DEST. If the handler is invoked then the exception will be placed in register EXC (can be $null).

`getexceptiondata DATA EXC`
:  Get the data object from exception EXC and store it in DATA.

`clearcatch`
:  Clear the current stack frame's exception handler.

`throw DATA`
:  Raise an exception. DATA is an object to be attached to the exception (can be $null).

`clearexception`
:  Clear the current exception and continue execution.

`continue`
:  **Note**: Currently not implemented and likely to be removed.
   Can be called from an exception handler to attempt to continue from an exception. (Warning: Dangerous!)

`rethrow EXC`
:  Reraise an exception (will preserve the stack trace from the exception structure's point of origin instead of wherever the current handler is).

`setexception EXC`
:  Set the current exception into register EXC.

#### Constant and literal assignment

##### Constants

Constants are stored in a constant-substitution section of an `hvm_chunk`. At load time these constants are copied from the chunk into the global constant pool (`vm->const_pool`) and the indexes in the instructions are updated to point the corresponding value in the global pool.

NOTE: May want to make a `setconstant` instruction available.

`setstring A #S`
:  Set the string referenced by constant index S into register A.

`setinteger A #I`
:  Set the integer referenced by constant index I into register A.

`setfloat A #F`
:  Set the float referenced by constant index F into register A.

`setstruct A #S`
:  Set the structure referenced by constant index S into register A.

`setsymbol A #S`
:  At load time: look up string in constant index S, then get the symbol ID from the VM's symbol table for that string. Upon execution register A will be set to that non-negative integer ID.

`setnull A`
:  Set register A to null.

##### Literals

Literals are directly stored in the instruction. For example `litinteger` takes 10 bytes: byte 1 is the instruction itself, byte 2 is the destination register, and bytes 3-10 are used for the 64-bit integer.

`litinteger A #I`
:  Set A to literal integer I.

#### Miscellaneous

`noop`
:  Do nothing for a wee bit of time.

`exit STATUS`
:  Exit interpreter with integer status code in STATUS.

`symbolicate SYM STR`
:  Look up the symbol ID for the string in STR and update SYM with that value.

`move A B`
:  A = B.

#### Local/global variables

`setlocal N V`
:  Sets a local by symbol name N with value V.

`getlocal V N`
:  Gets a local by symbol name N into V.

`setglobal N V`
:  Sets a global by symbol name N with value V.

`getglobal V N`
:  Gets a global by symbol name N into V.

`findlexical V N`
:  **Slow stack search operation.** Climb the stack searching for a local by symbol name N; if found store in V.

##### Closures

`getclosure A`
:   Get the current scope as a closure-structure.  
    **Warning**: These will probably be not-very-performant since it will (in
    the unoptimized case) probably end up compacting and copying the stack.

#### Math

`add A B C`
:  A = B + C

`sub A B C`
:  A = B - C

`mul A B C`
:  A = B * C

`div A B C`
:  A = B / C

`mod A B C`
:  A = B % C

#### Bitwise

`and A B C`
:  A = B & C

`or A B C`
:  A = B | C

`not A B`
:  A = ~B

`xor A B C`
:  A = B ^ C

`rshift A B C`
:  A = B >> C

`lshift A B C`
:  A = B << C
