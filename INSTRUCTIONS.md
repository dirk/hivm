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

## Exceptions

`handler DEST EXC`
:  Register an exception handler for the current stack frame at destination DEST. If the handler is invoked then the exception will be placed in register EXC (can be $null).

`raise EXC`
:  Raise an exception.

`continue`
:  Can be called from an exception handler to attempt to continue from an exception. (Warning: Dangerous!)

`reraise EXC`
:  Reraise an exception (will preserve the stack trace from the exception structure's point of origin instead of wherever the current handler is).
