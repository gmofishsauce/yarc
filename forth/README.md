# Forth

The goal of YARC is to eventually bootstrap Forth on the YARC,
using the approach called "subroutine threaded code with in-line
expansion" but with 30 or more basic Forth primitives implemented
as machine instructions, i.e. in microcode.

This is similar to https://www.jhuapl.edu/Content/techdigest/pdf/V10-N03/10-03-Lee.pdf.

A beautifully-commented Forth implementation for x86 Linux can be
found in the jonesforth directory. Of course I won't need to define
the primitives (DUP, SWAP, and about 30 others) in assembly language,
because they'll be single machine instructions implemented in yarc/yasm.
And I won't need e.g. **next** as a primitive either; it will simply
be a YARC instruction fetch, as the Forth "inner interpreter" will be
the instruction fetch hardware of YARC.

But Jonesforth will guide the set of words that have to be implemented
in assembler (there will be some, such as the words that handle console input)
and the Forth file may be consumable directly if I can fully duplicate the
functionality of the jonesforth.s file in YARC.

Nevertheless, YARC will remain a general register computer with a
standard assembly language and have only certain concessions to the
Forth implementation. The concessions include the two hardware stack
and the microcoded primitives.

YARC is similar to http://users.ece.cmu.edu/~koopman/stack_computers/sec4_2.html.
