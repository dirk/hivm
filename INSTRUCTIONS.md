# Hivm Instruction Set

## Math

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

`pow A B C`
:  A = B ^^ C

## Bitwise

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

## Control flow & subroutines

`call SUB RET`
:  Invoke the subroutine at (integer) address in SUB. RET can be a register for return or $null for no return or ignoring return.

`tailcall SUB`
:  Same as `call` but does not grow the stack. Current subroutine's return will be the return from SUB.

`return RET`
:  Return from the current subroutine to the parent. RET can be a register for returning a value or $null.

`branch DIFF`
:  Branch DIFF (integer) instructions forwards (positive) or backwards (negative).

`jump DEST`
:  Jump to DEST (non-negative integer).

`if COND DEST`
:  Jump to DeST if COND is truthy (not null and not a zero integer).

## Exceptions

`catch DEST EXC`
:  Register an exception handler for the current stack frame at destination DEST. If the handler is invoked then the exception will be placed in register EXC (can be $null).

`throw EXC`
:  Raise an exception.

`continue`
:  Can be called from an exception handler to attempt to continue from an exception. (Warning: Dangerous!)

`rethrow EXC`
:  Reraise an exception (will preserve the stack trace from the exception structure's point of origin instead of wherever the current handler is).

## Constant assignment

`setstring A S`
:  Set the string referenced by constant index S into register A.

`setinteger A I`
:  Set the integer referenced by constant index S into register A.

`setfloat A F`
:  Set the float referenced by constant index S into register A.

`setstruct A S`
:  Set the structure referenced by constant index S into register A.

`setsymbol A S`
:  At load time: look up string in constant index S, then get the symbol ID from the VM's symbol table for that string. Upon execution register A will be set to that non-negative integer ID.

`setnull A`
:  Set register A to null.

## Miscellaneous

`noop`
:  Do nothing for a wee bit of time.

`exit STATUS`
:  Exit interpreter with integer status code in STATUS.
