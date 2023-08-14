# YARC Instruction Set Architecture

## YARC architecture overview

YARC is a 16-bit computer with a 15-bit address space. There are 30k bytes
of main memory. The top 2k of the address space is reserved for I/O registers.

YARC has four general registers r0 through r3, four index registers ix0 through ix3,  and a few special registers. General register r3 is reserved for use as the program counter (PC). Index register ix3 is reserved for use as the stack pointer (SP). There is a flags register F containing the traditional C, Z, N, and V (Carry, Zero, Negative, Overflow) flags. There are also non-architectural registers holding an ALU input, the previous ALU output, and the currently executing instruction.

The hardware dictates a number of restrictions. ALU operations take 2 cycles
called "phase 1" (phi1) and "phase 2" (phi2).  ALU operands may be registers,
not memory. The ALU has a transparent holding register that can be loaded from
memory in a single cycle, but that cycle may not be either an ALU phase 1 or 2
cycle. Also, no register may be used in the same cycle that it is updated.

The index registers have an incrementer/decrementer capable of adding the values -4 .. +3 to any index register. Index register increments also take two cycles, the second of which is a "dead" cycle (the system address bus is not driven).

## Instruction set overview

YARC's instruction set is defined by hardware ("hard wired") decode of some
opcodes and microcoded decode of the rest. Instructions are 16 bits wide (so
there are 64k opcodes 0x0000 - 0xFFFF). Opcodes must be aligned on 16-bit
boundaries.

The opcode set is conveniently divided into lower and upper halves.

Opcodes in the range 0 to 32k-1 (0x0000 - 0x7FFF) are absolute JUMP and
CALL instructions. They are their own target address: the opcode 0x0020 is
a call to address 0x0020, etc. Since code addresses must be 16-bit aligned
(even), odd values from 0x0001 through 0x7FFF would not valid if used in
this way. Instead, these values are defined as jumps to the next lower even
address: opcode 0x0021 is a jump to address 0x0020, etc. CALL instructions
push the return address on the return stack, while JUMP instructions simply
load the PC with the target address.

Opcode values in the range 0x8000 - 0xFFFF are defined by microcode, but
the apparent large number of opcodes is an illusion. Bit 15 is always '1',
and only bits 14:8 (7 bits) are used to address a block of 64 microcode words
in the microcode RAM. Thus there are 127 distinct opcodes 0x80 - 0xFF, each
of which may execute for up to 64 clocks (one microcode word per clock).

The lower byte of each of these 127 opcodes is available for use by the
microcode (it is held in the lower half of the instruction register, IRL,
while the instructions executes, and microcoded controls may move or apply
its value in a number of ways). It is variously used as the Register Control Word (RCW) to identify registers, to hold the address offset for branch instructions, or to act as a short immediate data value.

## Instruction set details

### Absolute calls and jumps

| Opcode | Mnemonic | Operands | Notes |
| :----- | :------: | :------- | :---- |
| 0x0000 - 0x7FFE (even) | call | target | Call self-address |
| 0x0001 - 0x7FFF (odd) | jmp | target | Jump to target - 1 |

### 8-Line: ALU operations

The low byte (RCW) of these opcodes encodes the three operands in three
fields, each two or three bits.

| Opcode | Mnemonic | Operands | Notes |
| :----- | :------: | :------- | :---- |
| 0x80:RCW | add | src1, src2, dst | src1 + src2 => dst, operand in LB |
| 0x81:RCW | sub | src1, src2, dst | src1 - src2 => dst, operands in LB |
| 0x82:RCW | adc | src1, src2, dst |  src1 + src2 + C => dst, operands in LB |
| 0x83:RCW | sbb | src1, src2, dst | src1 - src2 - c => dst, operands in LB |
| 0x84:RCW | bic | src1, src2, dst | src1 & ~src2 => dst, operands in LB ("bit clear") |
| 0x85:RCW | bis | src1, src2, dst | src1 OR src2 => dst, operands in LB ("bit set") |
| 0x86:RCW | xor | src1, src2, dst | src1 XOR src2 => dst, operands in LB |
| 0x87:RCW | not | ----, src2, dst | ~src2 => dst, operands in LB |
| 0x88:RCW | neg | ----, src2, dst | -src2 => dst, operands in LB |
| 0x89:RCW | rot | src1, src2, dst | rotate (see "Additional Information" below) |
| 0x8A:RCW | cmp | src1, src2, --- | compare src1, src2 (set flags based on subtract) |
| 0x8B:RCW | asf | src1, src2, dst | shift (see "Additional Information" below) |
| 0x8C:RCW | TBD | src1, src2, dst | ALU operation 12 (TBD), operands in LB |
| 0x8D:RCW | TBD | src1, src2, dst | ALU operation 13 (TBD), operands in LB |
| 0x8E:RCW | TBD | src1, src2, dst | ALU operation 14 (TBD), operands in LB |
| 0x8F:RCW | pass | src1, src2, dst | src2 => dst |

The low byte of the instruction is interpreted from most significant to least significant as a 2-bit _source1_ field in the two MS bits, a 3-bit _source2_ field, and a 3-bit _destination_ field in the 3 LS bits.

Together, these three fields are called the Register Control Word (RCW).
The RCW may come either from the low byte of the instruction register or from
the high byte of the microcode as selected by a microcode bit.

The source1 field of the RCW selects one of 4 general registers to the first
of the two ALU inputs. The source2 field selects either one of 4 general
registers (0..3) or one of four small constant registers (2, 1, -2, or -1,
encoded by the values 4..7) to the second ALU input. These values have special predefined meanings for the ROT (rotate) and SHF (shift) instructions. Finally, the 3-bit
destination field selects either a write to one of the four general register
(0..3) or a conditional write (4..7). Conditional writes are explained next.

### 9-Line: Conditional branches

| Opcode | Mnemonic | Operands | Notes |
| :----- | :------: | :------- | :---- |
| 0x9**F**:**OFF** | br       | flags, offset | Branch on flag condition **F** to offset **OFF**  |

The hardware performs a conditional write to the PC enabled by one of 16
conditions selected by "F", the low-order 4 bits of the upper half of IR.
The lower byte LB contains the branch offset in 16-bit instruction words
from -64 through 63.

| Value of "F" | Name   | Interpretation ("Branch if...") |
| :----------- | :---   | :------------- |
| 0x0          | c      | carry set |
| 0x1          | z      | zero set (after a subtract or cmp, operands were equal) |
| 0x2          | n      | negative |
| 0x3          | v      | signed overflow |
| 0x4          | always | true (always taken) |
| 0x5          | ule    | carry or zero (unsigned "not above"/"below or equal") |
| 0x6          | slt    | sign and overflow differ (signed "less"/"not greater or equal")
| 0x7          | sle    | sign and overflow differ and zero is set (signed "less or equal"/"not greater")
| 0x8          | nc     | not carry |
| 0x9          | nz     | not zero |
| 0xA          | nn     | signed positive or zero (not negative) |
| 0xB          | nv     | no signed overflow |
| 0xC          | never  | false (never taken) |
| 0xD          | ugt    | not carry and not zero (unsigned "above") |
| 0xE          | sge    | sign and overflow match (signed greater or equal) |
| 0xF          | sgt    | sign and overflow match or zero is clear (signed greater than) |

### A-Line: Move instructions

The base acronym MV is used only for register to register moves. Instructions which involve memory are named LD (load) or ST (store). Instructions make only a single memory reference except for LDDW/STDW and LDOW/STOW which read a value from code stream for use as an address in addition to the load or store. Other instructions involving immediate values are grouped separately (below).

These instructions refer to the general registers. The index registers are discussed separately (below).

| Opcode | Mnemonic | Operands     | Notes |
| :----- | :------: | :-------     | :---- |
| 0xA0:RCW | mvr    | src, dst     | register to register move |
| 0xA1:00  | TBD    | unassigned   | unassigned |
| 0xA2:00  | TBD    | unassigned   | unassigned |
| 0xA3:00  | TBD    | unassigned   | unassigned |
| 0xA4:RCW | ldrw   | @src, dst    | load register from memory word |
| 0xA5:RCW | ldrb   | @src, dst    | load register from sign-extended memory byte |
| 0xA6:RCW | strw   | src, @dst    | store register to memory word |
| 0xA7:RCW | strb   | src, @dst    | store register low byte to memory byte |
| 0xA8:RCW | lddw   | immed16, dst | load direct register *dst* from @immed16 |
| 0xA9:RCW | ldsw   | immed16, @src, dst | load register *dst* from @(src + immed16) |
| 0xAA:RCW | stdw   | src, immed16 | store direct register *src* to @immed16 |
| 0xAB:RCW | stsw   | immed16, src, @dst | store register *src* to @(dst + immed16)  |
| 0xAC:0xAF| TBD    | TBD          | unassigned (4 opcodes) |

### Opcodes B0 - BF unassigned

### C-Line: Immediate operations

The byte opcodes 0xC0..0xC7 use the low bits of the opcode word for an 8-bit immediate value, so the RCW is not available for register specification. The target register must be specified in the opcode bits. This consumes a lot of opcode space, so only r0 and r1 are supported as targets for these instructions. Byte operations on r3 generally don't make sense because it is the PC. The word operations 0xC8..0xCB each require two words but support all four registers.

| Opcode | Mnemonic | Operands     | Notes |
| :----- | :------: | :-------     | :---- |
| 0xC0:IMM | ldib   | immed8, r0   | load immediate sign extended byte to r0 |
| 0xC1:IMM | ldib   | immed8, r1   | load immediate sign extended byte to r1 |
| 0xC2:IMM | addib  | immed8, r0   | add immediate sign extended byte to r0  |
| 0xC3:IMM | addib  | immed8, r1   | add immediate sign extended byte to r1  |
| 0xC4:IMM | bicib  | immed8, r0   | clear bits in low byte of r0 that are set in immed8 |
| 0xC5:IMM | bicib  | immed8, r1   | clear bits in low byte of r1 that are set in immed8 |
| 0xC6:IMM | bisib  | immed8, r0   | set bits in low byte of r0 that are set in immed8 |
| 0xC7:IMM | bisib  | immed8, r1   | set bits in low byte of r1 that are set in immed8 |
| 0xC8:RCW | ldiw   | immed16, dst   | load immediate value to register |
| 0xC9:RCW | addiw  | immed16, dst   | add immediate value to register |
| 0xCA:RCW | biciw  | immed16, dst   | clear bits in dst that are set in immed16 |
| 0xCB:RCW | bisiw  | immed16, dst   | set bits in dst that are set in immed16 |
| 0xCC:RCW | cmpiw  | immed16, src2  | set flags based on immed16 - src2 |
| 0xCD - 0xCF | unassigned | TBD     | unassigned (3 opcodes) |

### Index register operations

For these operations, the index register 0..3 is encoded in the LS bits of the upper byte of the instruction word. Instructions D0, D4, D8, and DC refer to index register 0. D1, D5, ... refer to index register 1, etc. Note that index register 3 is the stack pointer. These operations treat the stack pointer exactly like the other index registers. Separate push and pop operations provide a shorthand for some operations on ix3 (SP).

| Opcode     | Mnemonic | Operands    | Notes  |
| :-----     | :------: | :-------    | :----  |
| 0xD0..0xD3 | ldxb  | ix, dst, incr   | load sign-extended byte to general register *dst* from the address in index register ix and add *incr* to the index register value. |
| 0xD4..0xD7 | stxb  | src, ix, incr   | store low order byte from general register *src* to the byte address in index register ix and add *incr* to the index register value.  |
| 0xD8..0xDB | ldxw | ix, incr, dst | load word to general register *dst* from the address in index register ix and add *incr* to the index register value |
| 0xDC..0xDF | stxw | src, ix, incr | store word from general register *src* from the address in index register ix and add *incr* to the index register value |
| 0xE0..0xE3 | mvrx | src, dst        | move general register *src* to index register *dst* |
| 0xE4..0xE7 | mvxr | src, dst        | move index register *src* to general register *dst* |
| 0xE8..0xEB | ldxi | immed16, ix     | load immediate value to index register |
| 0xEC..0xEF | unassigned | TBD | unassigned (4 opcodes) |

### Special instructions

Interrupt instructions are reserved,  but interrupts are not implemented.

| Opcode   | Mnemonic | Operands | Notes  |
| :-----   | :------: | :------- | :----  |
| 0xF0:IM8 | ldif     | imm8     | load immediate flags from imm8 3:0 (V N Z C) |
| 0xF1:RCW | mvf      | dst      | move flags to register dst |
| 0xF200   |          |          |        |
| 0xF300   |          |          |        |
| 0xF400   |          |          |        |
| 0xF500   |          |          |        |
| 0xF6NN   |          |          |        |
| 0xF700   |          |          |        |
| 0xF800   | nop2     | none     | no operation, 2 machine cycles |
| 0xF900   | nop3     | none     | no operation, 3 machine cycles |
| 0xFA00   | ret      | none     | return |
| 0xFB00   | TBD      | TBD      | unassigned |
| 0xFC00   | TBD      |          | Reserved for interrupt support |
| 0xFD00   | TBD      |          | Reserved for interrupt support |
| 0xFE00   | TBD      |          | Hardwired as implementation of CALL |
| 0xFF00   | TBD      |          | Hardwired as implementation of JMP |

### Additional Information

The rotate instruction rotates the register specified by src1 and places the result in dst (often, src1 and dst will be the same). The rotation is specified by src2, which is usually one of the small constant registers 4..7. The operations are:

| src2 value | Small constant value | Meaning |
|:---------- | :------------------- | :------ |
| 4          | 2                    | Rotate left. The LS bit and carry flag in the dst are set to the value of the MS bit from src1. Other bits are shifted left one position. |
| 5          | 1                    | Rotate left through carry. The LS bit is set to the value of the carry flag. The carry flag is set to the value of the MS bit. Other bits are shifted left one position. |
| 6          | -2                   | Rotate right. The MS bit and carry flag are set to the value of the LS bit. Other bits are shifted right one position. |
| 7          | -1                   | Rotate right through carry. The MS bit is set to the value of the carry flag. The carry flag is set to the value of the LS bit. Other bits are shifted right one position. |

For asf (arithmetic shift), the carry flag is not affected. The src2 operand specifies the amount of the shift, -1 or -2 positions (right) or 1 or 2 positions (left). For right shifts, the MS bit of the word is duplicated. For left shifts, zeroes are introduced.

### Additional Opcodes

Opcodes 0xC0 - 0xFB were reserved for FORTH primitives in a previous iteration of the design. The intent now is to implement a Wasm-to-YARC translator and not implement the FORTH environment.

Opcodes 0xFC and 0xFD are reserved for a possible implementation of hardware and software interrupts.

Opcodes 0xFE and 0xFF are reserved by hardware as the implementation of CALL and JMP.

