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


## Register & memory

## Control flow & subroutines

`call SUB RET`
:  Invoke the subroutine at (integer) address in SUB. RET can be a register for return or $null for no return or ignoring return.
