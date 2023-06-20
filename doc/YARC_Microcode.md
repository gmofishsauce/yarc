# YARC Microcode

## Overview

YARC is a homebrew 16-bit microcoded computer. When the Arduino-based control processor ("Downloader", "the Nano") enables YARC execution, one word of microcode is clocked from the writeable control store (WCS) to the microcode pipeline register "K" on each rising edge of the system clock. The value in the K register drives control lines to all parts of YARC.

The WCS and K register are are organized as four byte-wide "slices". The transceivers that connect them to the system bus are under control of the Nano only, so YARC cannot update its own control store. The Nano typically initializes the WCS by writing values transmitted from the host (Mac) computer. Microcode is authored on the Mac and transformed into download format by an assembler. The assembler allows the YARC's instructions to be defined (by authoring the microcode for each one) and then used, i.e. it is both a microcode and instruction set assembler. It is described in a separate file in this directory. The Nano can also write directly to the K register, allowing it to control all parts of YARC for testing.

YARC has 16-bit instructions. The current instruction is stored in the dedicated register IR which is made up of high and low bytes IRH and IRL. Instructions in the range 0x0000 through 0x7FFF are jumps and calls; these are not microcoded and will be described later. Microcoded instructions range from 0x8000 through 0xFFFE. The high-order bit is always 1 (in effect it selects microcoded instructions versus hard wired jumps and calls). The 7 remaining bits of IRH form address bits 12:6 of the WCS address. A six-bit synchronous binary counter forms address bits 5:0. The counter is reset to 0 when IR is loaded and only counts up. So there are 2^7 = 128 distinct opcodes, each of which may execute for up to 2^6 = 64 clock cycles.

This describes the entire microcode controller: every instruction executes 1 to 64 microcode words in order. There are no microcode branches or loops. The way conditional flow control instructions are executed is described later. 

Each 32-bit word in the WCS is called a "slot". Within a machine instruction, each slot represents one clock of execution. Each instruction must end with the microcode steps to fetch the next instruction and clock its first word into the K register, so in effect each instruction can have only 62 useful steps. Most instructions are much shorter. The 8-bit wide ALU requires two cycles to perform a 16-bit logical or arithmetic operation, so the instruction ADD R0, R1, R2 requires four clocks: ALU phase 1, ALU phase 2, FETCH, and DECODE, after which the next instruction will execute microcode from slot identified by IRH defined by whatever was FETCHed.

The large number of slots (64) available to each instruction is mostly an artifact of the unrealistically large size of available memory chips (8k x 8 being the smallest widely-available part still in production in 2022). The large number of slots will, however, allow for some interesting instructions, such as a 16-bit multiply.

## Writing Microcode

Microcode authoring is supported by a simple single-page web application found in yarc/ued/first.htm. This file is intended to be opened locally rather than served by an HTTP server. I am neither a Javascript programmer nor a web developer. The code in `ued` is mostly copied from bits and pieces found on Stack Overflow.

As the controls are used to alter the values of fields, the commentary lines at the bottom of the page will update to display the field values. These text lines can be copied and pasted into YARC assembler (yasm) source files, typically into `.set` directives. Other plausible functionality, such as pasting microcode into the page to disassemble hex to field values, has not been implemented.

## Microcode Fields

The microcode is described as a set of bitfields in the 32-bit K register. The Nano must write it in 1-byte wide slices, however, so for sanity the design tries to break up the microcode into byte-wide chunks with somewhat meaningful field groupings.

All bits that are not decoded as fields are active LOW. Multiple bit fields either reserve the "all 1s" value as no operation or are enabled by some active LOW bit. The end result is that 0xFF_FF_FF_FF implements "no operation" if executed as microcode. The Nano initializes all of microcode memory to this FF pattern during YARC power on and writes this same pattern to the K register to make the YARC "safe" (inactive).

### K[3] Register Control Word

K3 (the most significant bits of K) are the Register Control Word (RCW). Note: the RCW was previously called the Register Specifier Word (RSW) and is still called that in many places.)

This word specifies the behavior of the two ALU source busses and the ALU destination bus.

```
.bitfield src1   31:30   # contains a gr
.bitfield src2   29:27   # contains a gr (3..0) or a constant reg (7..4)
.bitfield dst    26:24   # contains a gr (3..0) or a conditional gr (7..4)
```
The src1 field (2 bits) specifies one of the four general registers. The specified src1 register also drives the address bus.

Src2 (3 bits) may be one of the four general register 0..3 or, by specifying a value in the range 4..7 may be one of the small constants -2, 1, 1, or 2. These constants support "auto increment" or "auto decrement" addressing; e.g. the instruction MOVBI (R0), +1, R2 can address memory at R0 and clock the data into R2 during the first cycle, while simultaneously performing ALU phase 1 of ADDing R0 and +1. The next cycle can perform ALU phase 2 and clock the result back to R0.

The correspondence between src2 values 4..7 and small constants is:

```
.set const_p2 4   # 4 in src2 makes the value positive 2 (p2)
.set const_p1 5   # 5 makes the value positive 1 (p1)
.set const_n2 6   # 6 makes the value negative 2 (n2)
.set const_n1 7   # 7 makes the value negative 1 (n1)
```

The dst field may specify a general register 0..3 or, by specifying a value in the range 4..7, may specify a _conditional load_. During a conditional write, four bits specify one of 16 conditions (carry set, overflow, etc.) from the ALU flags. The write occurs if the condition is satisfied, and otherwise does not occur.

Since one of the four "general" registers must act as the program counter, a microcoded ADD followed by a conditional load to the PC general register (R3) is used to implement conditional branches.

The four bits that select the branch condition are called the ALU Condition Nybble (ACN). During an ALU operation, these same four bits select one of 16 ALU operations. Both the RCW and the ACN may come from either microcode or from the instruction register (IR). The decision is made on a cycle-by-cycle basis under the control of control bits in the K[0] microcode word, described below.

Note: The register implementation can read two register and write one in each machine cycle, but the microcode programmer must not read and write the same register in any one cycle. This is due to a limitation of the components and is not detected in hardware. Because of the serendipitous decision to implement an 8-bit, 2-cycle ALU, this restriction has less impact than might be imagined. The instruction ADD R0, R1, R0 is implemented by reading R0 and R1 during the first (ALU phase 1) cycle and writing the sum to R0 at the end of the second (ALU phase 2) cycle, so it does not violate this restriction. A single-cycle like MOV (R0), R0, which would read an address from R0 and write data back to it at the end of the cycle, is forbidden.

### K[2] ALU Controls

```
.bitfield acn           23:20   # 1 of 16 alu operations
.bitfield alu_ctl       19:18   # 0=alu_phi1, 1=alu_phi2, 2=alu_in, 3=none
.bitfield alu_load_hold 17:17   # when low, clock the port 2 ALU holding register
.bitfield alu_load_flgs 16:16   # when low, clock the flags (ALU phi2 or write to flags)
```
The ALU operations (ACN described above) are defined in the instruction set document: 0 is ADD, etc. The ALU control field specifies ALU phase 1 or 2 or no operation. The value `alu_in` reverses the src2 transceivers to the data bus allowing a microcoded move the src2 holding register. This is encoded as an ALU operation to prevent combining it with either ALU phase 1 or 2 (the data would arrive too late in the cycle to allow the ALU to work). The pipeline register is "non-architectural", i.e. it is not exposed in the instruction set.

The `alu_load_hold` bit causes the ALU holding register on the src2 bus to be loaded. This must be a separate, microcode-programmer controlled bit so that the microcoder may issue ALU phase 1 with a load (for a register to register operation), or, alternatively, may do an `alu_in` cycle to load the pipeline register from memory, then an ALU phase 1 that operates on a register specified by src1 and the preloaded pipeline register.

Finally `alu_load_flgs` bit causes the flags register to be loaded. It is normally set during ALU phase 2, but would not be set for e.g. the add that updates the PC (R3) on every FETCH/DECODE. It must be set to load the flags from memory (e.g. MOV ..., F or POP F, etc.)

### K[1] Busses and Stacks

```
.bitfield sysdata_src  15:13  # 1 of 8 data bus drivers
.bitfield reg_in_mux   12:12  # register input, 1 = sysdata, 0 = alu output
.bitfield not_used     11:9   # reserved
.bitfield dst_wr_en     8:8   # enable write to GRs
```
The `sysdata_src` field identifies the subsystem that will drive the data bus in this cycle, if any. Note: the address bus is always driven by the general register selected by the src1 field.

```
# data bus drivers for memory writes
.set bus_gr    0        # general register drives bus
.set bus_sp    1        # not used (was: hardware stacks)
.set bus_ir    2        # instruction register drives bus
.set bus_f     3        # flags register drives bus
.set bus_mem   4        # memory drives bus (memory read)
.set bus_tbd_5 5        # unused
.set bus_tbd_6 6        # unused
.set bus_none  7        # "none" - permanently reserved
```
The next bit determines whether data for a register write, if any, comes from the ALU outputs or the system data bus. The next three bits are reserved. Finally, the LS bit of K1 enables writes (including conditional writes) to the general registers. If the bit is high, no write will occur. If the bit is low and the dst bits in K[3] specify a register, that register loaded. If low and dst specifies a conditional load, that register will be loaded if the condition specified by the ACN is true. The mapping of conditions to ACN values can be found in the instruction set document.

### K[0] Miscellaneous Controls

```
.bitfield rw            32    7:7   # read/write# line for all memory and I/O transfers
.bitfield m16_en        32    6:6   # enable 16-bit memory or I/O cycle (default 8 bit)
.bitfield load_ir       32    5:5   # clock the IR
.bitfield rcw_ir_uc     32    4:4   # RCW from IRL7:0 when low, from K31:24 when high
.bitfield carry_en      32    3:3   # Force carry-in low when low (for add vs adc)
.bitfield load_flgs_mux 32    2:2   # From sysdata when high else from ALU result flags
.bitfield acn_ir_uc     32    1:1   # ACN from IRH11:8 when low, from K23:30 when high
.bitfield ir0_en        32    0:0   # Force IR:0 to sysdata low when low (for jumps)

```
The MS bit specifies read or write for transfers to the memory and I/O space only; if the instruction specifies (e.g.) a load of the instruction register IR, the bit is not regarded. The `m16_en` bit enables 16-bit memory accesses; when high, all memory access are bytes. Note: byte accesses may be to any address, but 16-bit words may be read or written from even addresses only. The effect of a 16-bit access to an odd address is not defined. The error is not detected by hardware.

The `ir_clk` bit enables writes to the instruction register. It must be low during a cycle that fetches the next instruction. It could be used to support a hypothetical `execute` instruction that would execute an instruction out-of-line.

The `rcw_ir_uc` bit selects the source of the RCW, from K[3] when high else from IRL.

Review: in any given cycle, both the Register Control Word (RCW) and the ALU Condition Nybble (ACN; 12 bits total) can come from one of two places: (1) from the microcode bits in K3 and the upper bits of K2, or (2) from the IR bits 11:0. This design is an expensive compromise (3 multiplexer chips and a bunch of wires) between (1) having a relatively small number of instruction bits and (2) wanting to enable complex, multiple-step microcode instructions that may perform arithmetic. The first requires the that low byte of the instruction register be able to specify registers, and the second requires that the microcode be able to do so (since the instruction bits won't change during a complex instruction that may need to do various register and arithmetic operations).

The four low-order bits of K[0] are described in their comments.

## Jumps and Calls

The low-numbered even opcodes 0x0000 through 0x7FFE are CALL instructions that target themselves: the opcode 0x0020 is CALL 0x0020. Since instructions are 16-bit values and all 16-bit values must be read from even addresses, the odd numbers 0x0001 through 0x7FFF would not be useful as similar instructions. So the hardware maps the patterns as JUMPs (rather than CALLs) to next-lower opcode value: the opcode 0x0021 is JUMP 0x0020.

The high-order bit of IR is connected to the control line of a multiplexer. When it's 1 (instruction 0x8000 through 0xFFFF), the multiplexer gates the 7 low-order bits of IRH through as microcode address bits 12:6. When it's a 0, the multiplexer gates through a fixed pattern, which is 0b111_111X with X = 0 for calls and 1 for jumps. The effect is that all jumps and calls are transformed in the opcodes 0xFFxx and 0xFExx. Microcode at these locations pushes the PC on the stack (for calls) and then loads the instruction register into the PC, masking the low order to ensure the next fetch is from an even address. When loading the PC, bit K[0]:0 should be low to force the low order bit of the instruction, which is its own target address, to be 0.





