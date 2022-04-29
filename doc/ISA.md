# YARC Instruction Set Architecture

## YARC architecture overview

YARC is a 16-bit computer with a 15-bit address space. There are 30k bytes
of main memory. The top 2k of the address space is reserved for I/O registers.

In addition to main memory, YARC has two hardware stacks called the parameter
stack and the return stack. These are located in physically separate memory.
Certain instructions make reference to these stacks while other instructions
allow programmer-specified access.

YARC has four general registers r0 through r3 and a few special registers.
RSP and PSP are the stack pointers for the hardware stacks. IR holds the
currently-executing opcode. There is a flags register F containing the
traditional Z, N, C, and V (Zero, Negative, Carry, Overflow) flags.

## Instruction set overview

YARC's instruction set is defined by hardware ("hard wired") decode of some
opcodes and microcoded decode of the rest. Instructions are 16 bits wide (so
there are 64k opcodes 0x0000 - 0xFFFF). Opcodes must be aligned on 16-bit
boundaries.

The opcode set is conveniently divided into lower and upper halves.

Opcodes in the range 0 to 32k-1 (0x0000 - 0x7FFF) are absolute JUMP and
CALL instructions. They are their own target address: the opcode 0x0020 is
a call to address 0x0020, etc. Since code addresses must be 16-bit aligned
(even), odd values from 0x0000 through 0x7FFF would not valid if used in
this way. Instead, these values are defined as jumps to the next lower even
address. Opcode 0x0021 is a jump to address 0x0020, etc. CALL instructions
push the return address on the return stack, while JUMP instructions simply
load the PC with the target address.

(The astute reader may have deduced that the machine is designed to support
the classic programming language FORTH. The two stacks and their names come
directly from the FORTH language, and dedicating half the opcode space to
minimum-sized jump and call instructions is intended to allow the efficient
implementation of threaded code.)

Opcode values in the range 0x8000 - 0xFFFF are defined by microcode, but
the apparent large number of opcodes is an illusion. Bit 15 is always '1',
and only bits 14:8 (7 bits) are used to address a block of 64 microcode words
in the microcode RAM. Thus there are 127 distinct opcodes 0x80 - 0xFF, each
of which may execute for up to 64 clocks (one microcode word per clock).

The lower half of each of these 127 opcodes is available for use by the
microcode (it is held in the lower half of the instruction register, IRL,
while the instructions executes, and microcoded controls may move or apply
its value in a number of ways). It is variously used to identify registers,
hold the address offset for branch instructions, act as a short immediate
data value, etc.

## Instruction set details

| Opcode | Mnemonic | Operands | Notes |
| :----- | :------: | :------- | :---- |
| 0x0000 - 0x7FFE, even) | CALL | target | Call self-address |
| 0x0001 - 0x7FFF, odd) | JUMP | target | Jump to self-address |
| 0x80 | ALU | src1, src2, dst | ALU operation 0 (TBD) |
| 0x81 | ALU | src1, src2, dst | ALU operation 1 (TBD) |
| 0x82 | ALU | src1, src2, dst | ALU operation 2 (TBD) |
| 0x83 | ALU | src1, src2, dst | ALU operation 3 (TBD) |
| 0x84 | ALU | src1, src2, dst | ALU operation 4 (TBD) |
| 0x85 | ALU | src1, src2, dst | ALU operation 5 (TBD) |
| 0x86 | ALU | src1, src2, dst | ALU operation 6 (TBD) |
| 0x87 | ALU | src1, src2, dst | ALU operation 7 (TBD) |
| 0x88 | ALU | src1, src2, dst | ALU operation 8 (TBD) |
| 0x89 | ALU | src1, src2, dst | ALU operation 9 (TBD) |
| 0x8A | ALU | src1, src2, dst | ALU operation 10 (TBD) |
| 0x8B | ALU | src1, src2, dst | ALU operation 11 (TBD) |
| 0x8C | ALU | src1, src2, dst | ALU operation 12 (TBD) |
| 0x8D | ALU | src1, src2, dst | ALU operation 13 (TBD) |
| 0x8E | ALU | src1, src2, dst | ALU operation 14 (TBD) |
| 0x8F | ALU | src1, src2, dst | ALU operation 15 (TBD) |

