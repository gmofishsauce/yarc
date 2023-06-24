# YARC Instruction Set Architecture

## YARC architecture overview

YARC is a 16-bit computer with a 15-bit address space. There are 30k bytes
of main memory. The top 2k of the address space is reserved for I/O registers.

YARC has four general registers r0 through r3 and a few special registers.
IR holds the currently-executing opcode. There is a flags register F containing
the traditional C, Z, N, and V (Carry, Zero, Negative, Overflow) flags.

The hardware dictates a number of restrictions. ALU operations take 2 cycles
called "phase 1" (phi1) and "phase 2" (phi2).  ALU operands may be registers,
not memory. The ALU has a transparent holding register that can be loaded from
memory in a single cycle, but that cycle may not be either an ALU phase 1 or 2
cycle. Also, no register may be used as an address or data in the same cycle
during which it is updated.

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
| 0x80:RCW | ADD | src1, src2, dst | src1 + src2 => dst, operand in LB |
| 0x81:RCW | SUB | src1, src2, dst | src1 - src2 => dst, operands in LB |
| 0x82:RCW | RSUB | src1, src2, dst | src2 - src1 => dst, operands in LB |
| 0x83:RCW | ADC | src1, src2, dst |  src1 + src2 + C => dst, operands in LB |
| 0x84:RCW | SBB | src1, src2, dst | src1 - src2 - c => dst, operands in LB |
| 0x85:RCW | RSBB | src1, src2, dst | src2 - src1 - c => dst, operands in LB |
| 0x86:RCW | NAND | src1, src2, dst | src1 & ~src2 => dst, operands in LB |
| 0x87:RCW | OR | src1, src2, dst | src1 OR src2 => dst, operands in LB |
| 0x88:RCW | XOR | src1, src2, dst | src1 XOR src2 => dst, operands in LB |
| 0x89:RCW | NOT | ----, src2, dst | ~src2 => dst, operands in LB |
| 0x8A:RCW | NEG | ----, src2, dst | -src2 => dst, operands in LB |
| 0x8B:RCW | ROT | src1, src2, dst | rotate (see "Additional Information" below) |
| 0x8C:RCW | TBD | src1, src2, dst | ALU operation 12 (TBD), operands in LB |
| 0x8D:RCW | TBD | src1, src2, dst | ALU operation 13 (TBD), operands in LB |
| 0x8E:RCW | TBD | src1, src2, dst | ALU operation 14 (TBD), operands in LB |
| 0x8F:RCW | PASS | src1, src2, dst | src2 => dst |

The low byte (LB) of the instruction is interpreted from most significant
to least significant as a 2-bit _source1_ field in the two MS bits, a 3-bit
_source2_ field, and a 3-bit _destination_ field in the 3 LS bits.

Together, these three fields are called the Register Control Word (RCW).
The RCW may come either from the low byte of the instruction register or from
the high byte of the microcode as selected by a microcode bit.

The source1 field of the RCW selects one of 4 general registers to the first
of the two ALU inputs. The source2 field selects either one of 4 general
registers (0..3) or one of four small constant registers (2, 1, -2, or -1,
encoded by the values 4..7) to the second ALU input. These values have special predefined meanings for the ROT (rotate) instruction. Finally, the 3-bit
destination field selects either a write to one of the four general register
(0..3) or a conditional write (4..7). Conditional writes are explained next.

### Conditional branches

| Opcode | Mnemonic | Operands | Notes |
| :----- | :------: | :------- | :---- |
| 0x9**F**:**OFF** | BR       | flags, offset | Branch on flag condition **F** to offset **OFF**  |

The hardware performs a conditional write to the PC enabled by one of 16
conditions selected by "F", the low-order 4 bits of the upper half of IR.
The lower byte LB contains the branch offset in 16-bit instruction words
from -64 through 63.

The assignment of flags to the opcodes 0x9F, F in 0..15 (0xF):

| Value of "F" | Interpretation ("Branch if...") |
| :----------- | :------------- |
| 0x0          | Carry set |
| 0x1          | Zero set |
| 0x2          | Negative |
| 0x3          | Signed overflow |
| 0x4          | True (always taken) |
| 0x5          | Carry or Zero (unsigned "not above"/"below or equal") |
| 0x6          | Sign and overflow differ (signed "less"/"not greater or equal")
| 0x7          | Sign and overflow differ and zero is set (signed "less or equal"/"not greater")
| 0x8          | Not carry |
| 0x9          | Not zero |
| 0xA          | Signed positive or zero (not negative) |
| 0xB          | No signed overflow |
| 0xC          | False (never taken) |
| 0xD          | Not carry and not zero (unsigned "above") |
| 0xE          | Sign and overflow match (signed greater or equal) |
| 0xF          | Sign and overflow match or zero is clear (signed greater than) |

### Move instructions

| Opcode | Mnemonic | Operands     | Notes |
| :----- | :------: | :-------     | :---- |
| 0xA0:RCW | MVR    | src, dst     | register to register move |
| 0xA1:IMM | MVF    | immed4       | move bits 3:0 to flags VNZC |
| 0xA2:RCW | LDRW   | @src, dst    | load register from memory word |
| 0xA3:RCW | LDRB   | @src, dst    | load register from sign extended memory byte |
| 0xA4:RCW | STRW   | src, @dst    | store register to memory word |
| 0xA5:RCW | STRB   | src, @dst    | store register to memory byte |
| 0xA6:RCW | LDIW   | immed16, dst | move immediate word to register |
| 0xA7:TBD | TBD    | TBD          | TBD |
| 0xA8:RCW | LDDW   | immed16, dst | load register from @immed16 (load direct) |
| 0xA9:RCW | LDOW   | immed16, @src, dst | load register from @(src + immed16) |
| 0xAA:RCW | STDW   | src, immed16 | store register to @immed16 (store direct) |
| 0xAB:RCW | STOW   | immed16, src, @dst | store register to @(dst + immed16)  |
| 0xAC:TBD - 0xAF:TBD | TBD  | TBD   | TBD |

The opcodes from 0xA8 through 0xAB require the addition of a pending hardware feature.

### Unassigned opcode block

Opcodes from 0xB000 through 0xEF00 are unassigned. The opcodes 0xB000 through 0xBF00 might be implemented as byte immediate loads and stores similar to LDDW, LDOW, STDW, STOW. Byte immediates take the RCW as the immediate value and as a result require a lot of opcode space.

### Special instructions

Interrupt instructions are reserved,  but interrupts are not implemented.

| Opcode | Mnemonic | Operands     | Notes |
| :----- | :------: | :-------     | :---- |
| 0xF000 | PUSHF    |              | Push flags|
| 0xF100 | POPF     |              | Pop flags |
| 0xF200 | RET      |              | Return |
| 0xF300 | RTI      |              | Return from interrupt |
| 0xF400 | EI       |              | Enable interrupts |
| 0xF500 | DI       |              | Disable interrupts |
| 0xF6NN | INT      | NN - value   | R0 := value; interrupt |
| 0xF700 | TBD      | unknown      | unassigned |
| 0xF800 | TBD      |              |       |
| 0xF900 | TBD      |              |       |
| 0xFA00 | TBD      |              |       |
| 0xFB00 | TBD      |              |       |
| 0xFC00 | TBD      |              | Reserved for interrupt support |
| 0xFD00 | TBD      |              | Reserved for interrupt support |
| 0xFE00 | TBD      |              | Hardwired as implementation of CALL |
| 0xFF00 | TBD      |              | Hardwired as implementation of JMP |

### Additional Information

The rotate instruction rotates the register specified by src1 and places the result in dst (often, src1 and dst will be the same). The rotation is specified by src2, which is usually one of the small constant registers 4..7. The operations are:

| src2 value | Small constant value | Meaning |
|:---------- | :------------------- | :------ |
| 4          | 2                    | Rotate left. The LS bit and carry flag in the dst are set to the value of the MS bit from src1. Other bits are shifted left one position. |
| 5          | 1                    | Rotate left through carry. The LS bit is set to the value of the carry flag. The carry flag is set to the value of the MS bit. Other bits are shifted left one position. |
| 6          | -2                   | Rotate right. The MS bit and carry flag are set to the value of the LS bit. Other bits are shifted right one position. |
| 7          | -1                   | Rotate right through carry. The MS bit is set to the value of the carry flag. The carry flag is set to the value of the LS bit. Other bits are shifted right one position. |

### Additional Opcodes

Opcodes 0xC0 - 0xFB were reserved for FORTH primitives in a previous iteration of the design. The intent now is to implement a Wasm-to-YARC translator and not implement the FORTH environment.

Opcodes 0xFC and 0xFD are reserved for a possible implementation of hardware and software interrupts.

Opcodes 0xFE and 0xFF are reserved by hardware as the implementation of CALL and JMP.

