# YARC Instruction Set Architecture

## YARC architecture overview

YARC is a 16-bit computer with a 15-bit address space. There are 30k bytes
of main memory. The top 2k of the address space is reserved for I/O registers.

In addition to main memory, YARC has two hardware stacks called the parameter
stack and the return stack. These are located in physically separate memory.
Certain instructions make reference to these stacks while other instructions
allow programmer-specified access. The stacks only be used as stacks, not as
data or program memory.

YARC has four general registers r0 through r3 and a few special registers.
RSP and PSP are the stack pointers for the hardware stacks. IR holds the
currently-executing opcode. There is a flags register F containing the
traditional Z, N, C, and V (Zero, Negative, Carry, Overflow) flags.

The hardware dictates a number of restrictions. ALU operations take 2 cycles
called "phase 1" and "phase 2".  ALU operands may be registers, not memory.
The ALU has a transparent holding register that can be loaded from memory in
a single cycle, but that cycle may not be either an ALU phase 1 or 2 cycle. 
Also, as currently envisioned, the PSP and RSP may be referenced only on
alternating cycles, allowing a separate cycle for decrement or increment;
this may change.

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
address: opcode 0x0021 is a jump to address 0x0020, etc. CALL instructions
push the return address on the return stack, while JUMP instructions simply
load the PC with the target address.

(The astute reader may have deduced that the machine is designed to support
the classic programming language Forth. The two stacks and their names come
directly from the Forth language, and dedicating half the opcode space to
minimum-sized jump and call instructions is intended to allow the efficient
implementation of threaded code.)

Opcode values in the range 0x8000 - 0xFFFF are defined by microcode, but
the apparent large number of opcodes is an illusion. Bit 15 is always '1',
and only bits 14:8 (7 bits) are used to address a block of 64 microcode words
in the microcode RAM. Thus there are 127 distinct opcodes 0x80 - 0xFF, each
of which may execute for up to 64 clocks (one microcode word per clock).

The lower byte of each of these 127 opcodes is available for use by the
microcode (it is held in the lower half of the instruction register, IRL,
while the instructions executes, and microcoded controls may move or apply
its value in a number of ways). It is variously used to identify registers,
hold the address offset for branch instructions, or to act as a short
immediate data value.

## Instruction set details

### Absolute calls and jumps

| Opcode | Mnemonic | Operands | Notes |
| :----- | :------: | :------- | :---- |
| 0x0000 - 0x7FFE (even) | CALL | target | Call self-address |
| 0x0001 - 0x7FFF (odd) | JUMP | target | Jump to self-address - 1 |

### ALU operations

The low byte (LB) of these opcodes encodes the three operands in three
fields, each two or three bits.

| Opcode | Mnemonic | Operands | Notes |
| :----- | :------: | :------- | :---- |
| 0x80LB | ADD | src1, src2, dst | src1 + src2 => dst, operand in LB |
| 0x81LB | SUB | src1, src2, dst | src1 - src2 => dst, operands in LB |
| 0x82LB | RSUB | src1, src2, dst | src2 - src1 => dst, operands in LB |
| 0x83LB | ADC | src1, src2, dst |  src1 + src2 + C => dst, operands in LB |
| 0x84LB | SBB | src1, src2, dst | src1 - src2 - c => dst, operands in LB |
| 0x85LB | RSBB | src1, src2, dst | src2 - src1 - c => dst, operands in LB |
| 0x86LB | NAND | src1, src2, dst | src1 & ~src2 => dst, operands in LB |
| 0x87LB | OR | src1, src2, dst | src1 OR src2 => dst, operands in LB |
| 0x88LB | NOT | src1, src2, dst | ~src1 => dst, operands in LB |
| 0x89LB | XOR | src1, src2, dst | src1 ^ src2 => dst, operands in LB |
| 0x8ALB | TBD | src1, src2, dst | ALU operation 10 (TBD), operands in LB |
| 0x8BLB | TBD | src1, src2, dst | ALU operation 11 (TBD), operands in LB |
| 0x8CLB | TBD | src1, src2, dst | ALU operation 12 (TBD), operands in LB |
| 0x8DLB | TBD | src1, src2, dst | ALU operation 13 (TBD), operands in LB |
| 0x8ELB | TBD | src1, src2, dst | ALU operation 14 (TBD), operands in LB |
| 0x8FLB | TBD | src1, src2, dst | ALU operation 15 (TBD), operands in LB |

The low byte (LB) of the instruction is interpreted from most significant
to least significant as a 2-bit _source1_ field in the two MS bits, a 3-bit
_source2_ field, and a 3-bit _destination_ field in the 3 LS bits.

Together, these three fields are called the Register Specifier Word (RSW).
The RSW may come either from the low byte of the instruction register or from
the high byte of the microcode as selected by a microcode bit.

The source1 field of the RSW selects one of 4 general registers to the first
of the two ALU inputs. The source2 field selects either one of 4 general
registers (0..3) or one of four small constant registers (-2, -1, 1, or 2 as
encoded by the values 4..7) to the second ALU input. Finally, the 3-bit
destination field selects either a write to one of the four general register
(0..3) or a conditional write (4..7). Conditional writes are explained next.

### Conditional branches

| Opcode | Mnemonic | Operands | Notes |
| :----- | :------: | :------- | :---- |
| 0x9FLB | BR       | flags, offset | Branch on flags matching F to offset LB  |

The hardware performs a conditional write to the PC enabled by one of 16
conditions selected by "F", the low-order 4 bits of the upper half of IR.
The lower byte LB contains the branch offset in 16-bit instruction words
from -64 through 63. This may change to -128..127 in the future.

### Move and push instructions

| Opcode | Mnemonic | Operands     | Notes |
| :----- | :------: | :-------     | :---- |
| 0xA0xx | MV       | s1, --, d    | register to register move |
| 0xA1xx | MV       | (s1), --, d  | memory to register move, memory address in s1, s1 in 0..3 |
| 0xA2xx | MV       | s1, --, (d)  | register to memory move |
| 0xA3xx | TBD      | none         | unassigend |
| 0xA4xx | PUSHPSP  | s1, --, --   | push s1 to PSP |
| 0xA5xx | POPPSP   | --, --, d    | pop PSP to d (d is a general register only) |
| 0xA6xx | PUSHRSP  | s1, --, --   | push s1 to RSP |
| 0xA7xx | POPRSP   | --, --, d    | pop RSP to d (d is a general register only) |
| 0xANIM - 0xANIM | MV immed8, r(N - 8)  | immediate value, register | sign extended byte IM to r0 - r3 low byte |
| 0xAN00 - 0xAN00 0xIMIM | MV immed16, r(N - 12) | immediate value, register | 16-bit word to r0 - r3 |

### Special instructions

| Opcode | Mnemonic | Operands     | Notes |
| :----- | :------: | :-------     | :---- |
| 0xB000 | PUSHF    |              | Push flags to RSP|
| 0xB100 | POPF     |              | Pop flags from RSP |
| 0xB200 | RET      |              | Return (PC from RSP) |
| 0xB300 | RTI      |              | Return from interrupt |
| 0xB400 | EI       |              | Enable interrupts |
| 0xB500 | DI       |              | Disable interrupts |
| 0xB6NN | INT      | NN - value   | R0 := value; interrupt |
| 0xB700 | TBD      | unknown      | unassigned |
| 0xB800 | TBD      |              |       |
| 0xB900 | TBD      |              |       |
| 0xBA00 | TBD      |              |       |
| 0xBB00 | TBD      |              |       |
| 0xBC00 | TBD      |              |       |
| 0xBD00 | TBD      |              |       |
| 0xBE00 | TBD      |              |       |
| 0xBF00 | TBD      |              |       |

### Microcoded FORTH primitive instructions

| Opcode | Mnemonic | Operands     | Notes |
| :----- | :------: | :-------     | :---- |
| 0xC0BB - | TBD   | BB - unknown  | unassigned |
| 0xFFBB   | TBD   | BB - unknown  | unassigned


