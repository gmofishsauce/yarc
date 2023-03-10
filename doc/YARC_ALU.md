# YARC ALU

## Overview

The YARC is a 16-bit retrocomputer implemented with 74HC-series CMOS versions of 1970s style components but using more modern 8k x 8 RAM chips (because 1970s RAMs have become serious collector's items). It has a 16-bit data bus, 16-bit-wide main memory, and a small 32k address space. 30k of the address space is RAM (0 - 0x77FF) and the top 2k is reserved for I/O registers.

In order to reduce part count, YARC's ALU is only 8 bits wide. Two clock cycles are required for each ALU operation, including the program counter increment following an instruction fetch.

There is lots of precedent in early computers for ALUs that are narrower than the system data bus or memory width. The Z-80 had an ALU only 4 bits wide. Digital's 12-bit PDP 8-I had a 1-bit ALU; it required 12 clock cycles for an add, a subtract, etc. The RCA 1802 microprocessor also had a 1-bit ALU and required 8 clock cycles for a byte operation. Early calculator chips generally operated on one BCD digit at a time.

This document describes the YARC ALU in detail. Refer to sheet 15 (YARC ALU) of the YARC's KiCad schematic for the wiring of the ALU buses. Refer to sheet 17 (YARC ALU Controls) for the flag logic (how the zero and carry flags are set) and to sheet 11 (YARC Flag Logic) for how the CPU flags are interpreted by conditional branch instructions.

## ALU Control

The ALU is controlled by a decoded two-bit microcode field. When the field's value is 3 (0b11), the ALU performs no operation. When 0, the ALU performs a first-cycle ("phi1") operation and when it's 1, a second-cycle ("phi2") operation. The effect of the value 2 (0b10) is explained later.

The ALU has two 16-bit input holding registers, one for each "side". The holding register on ALU port 1 is clocked on any phi1 cycle (i.e. at the start of any ALU operation). Separate bits control the loading of the holding register on ALU port 2 and loading of the ALU flags register. A separate 4-bit field selects one of 16 possible ALU operations. This field must be set to the desired operation during both phi1 and phi2. The operations are typical ALU operations (ADD, SUBTRACT, bitwise operators). They are defined in the accompanying instruction set document.

## ALU Input

The holding registers, which are shown on sheet 11 (YARC Flag Logic), are "fed" from the YARC's general registers via buses S1BUS15:0 and S2BUS15:0. The YARC has 4 16-bit general registers, one of which must be reserved as the PC. The registers support two independent reads per clock cycle. One bank feeds the S1BUS and the other bank feeds S2BUS. The registers to read are called out by separate microcode fields. The content of S1BUS is also driven to the system address bus in all cycles.

The input holding registers drive *byte wide* inputs to the ALU core called A and B. These must each supply one operand byte per cycle: the low operand bytes during phi1 and the high bytes during phi2. Thus the input registers thus act as 16:8 multiplexers. Each holding register is implemented as two 8-bit halves with separate enables. Both low bytes are enabled to the ALU "A" and "B" inputs during phi1, both high bytes during phi2.

The lower byte of both holding registers are implemented as transparent registers (like the popular 74LS373; actually CMOS 74HC573s which have nicer "straight across" pinouts). During the cycle, their outputs follow their inputs with a delay of about 25ns. At the end of the cycle, they "freeze" (latch) the value. The high byte of both holding registers is implemented with clocked D registers (HC574s). Both parts have 3-state outputs which are used to accomplish the multiplexing function.

This may be clarified by an example: instruction fetch. During a fetch cycle, the PC (say, general register 3) is selected on S1BUS. The microcode does not select a general register for S2BUS; instead, it selects one of the four "small constant" registers, the value "+2". The S1BUS is driven to the system address bus fairly early in the cycle. The microcode calls out phi1, so the low-order bits of R3 and "+2" are added during the cycle. This is possible because the low byte of the S1BUS/"A" holding register is transparent (it "flows through" to ALU's "A" input) and the S2BUS/"B" holding register does the same.

At the end of the phi1 cycle, the low transparent low byte byte registers freeze and the high byte registers, which are edge-triggered "D" latches, each capture the upper byte of S1BUS and S2BUS, respectively. In addition, the ALU output register captures the low byte of the sum of R3 and +2. At the same time, the result of the fetch is captured by clocking the special register IR (Instruction Register) which begins the decode of the next instruction.

In the next (instruction decode) machine cycle, the microcode calls out phi2, which gates the high-order D registers onto the A and B inputs of the ALU. The ALU does another 8-bit add and, at the end of the cycle, provides the result through a transparent '573 latch. The phi1 register and the phi2 transparent latch can be clocked into any one of the four general registers; in this case, of course, the result is clocked into the PC (R3 in this example). During this machine cycle, the decode (microcode lookup) operation took place, and at the end of the cycle the control signals corresponding to the first cycle of the newly-fetched instruction are clocked from microcode memory into the microcode control register K.

## ALU Flags

At this point, the astute reader may be thinking: you've completely botched this. What about carry in? And carry out? And for that matter, carry from the low half operation to the high half? And how are you going to compute the zero flag across the two cycles of a 16-bit ALU operation?

Good questions. In fact this isn't easy. After a little background, we'll take it one case at a time.

The ALU includes a conventional flags register, with bits 0:3 holding C, Z, N, and V (carry, zero, negative, and overflow). The register is clocked by a microcode bit, which is normally set during phi2 cycles (but need not be, allowing for ALU operations that don't alter the flags). The register is diretly connected to the inputs of the flag interpretation circuitry described later. It may be gated to the data bus to read the flags as data. Finally, the flags register may be set from the data bus, allowing for e.g. a POP FLAGS instruction and also for instructions to set and clear the flags.

### Carry In

Because the ALU takes 2 cycles to perform an operation, carry must be propagated unconditionally from the 8-bit phi1 partial operation into phi2. On the other hand, the carry into phi1 from the flags register must be conditional, based on a microcode bit, in order to allow both ADD (with no carry in) and ADC (with carry in) as specified in the instruction set document. Without this important programmer convenience, it's necessary to explicitly clear the carry flag to perform a "stand alone" add operation.

These operations are performed by the logic and flip-flop in the lower left corner of the ALU Controls schematic. The registered carry flag is first ANDed with PHI_1_PLS (pulse) which is high during phi1. The result is then inverted and ANDed with the microcode bit K3, carry enable. The result becomes phi1 CARRY\_IN. At the end of phi1, the 8-bit half carry from the ALU is clocked into a flip-flop by the rising edge of PHI\_1\_CLK. This becomes the CARRY\_IN signal during phi2.

### Carry Out

At the end of phi2, the carry value from the 16-bit operation is clocked into the flags register if enabled by microcode. The value comes directly from the ALU devices as explained later.

### Zero Flag

The ALU devices compute zero over their four-bit A and B inputs and provide it as an output as explained later. In each cycle phi1 and phi2, these outputs are ANDed to produce an 8-bit zero flag. This half zero is clocked into the other half of a 74HC74 flip-flop at the end of phi1 and this value is ANDed with the phi2 half zero flag during phi2, producing a result that is conditionally clocked into the flags register along with the carry out.

### Sign (Negative, N) and Overflow (V) flags

These are produced by the ALU devices and stored (or not) along with the carry and zero flags.

## ALU Core

### RAMs

The "ALU chips" are 8k x 8 static RAMs.

Let me say that again: the ALU chips are 8k x 8 static RAMs. How? Why? Let's do "why" first.

The CPUs of many "retrocomputers" (and of course the CPUs of many computers of the 1970s) used the ubiquitous 74181 4-bit ALU chip. But these devices haven't been manufactured in a long time and are becoming collector's items. Some are no doubt reaching the rising end of their reliability bathtub curves as well. I didn't want to go there.

Instead, I use RAMs. I happen to have selected 8k x 8 55ns static RAMs for the memory subsystems in YARC, because they are the smallest and simplest RAMs still in regular production. These devices contain about 375,000 transistors and become producible in the early 90s, long after computers designed the like the YARC had ceased to be viable. I use these RAMs for ALU chips.

How is this possible? Well, consider: a 74181 has four bits of "A" input, 4 bits of "B" input, 5 bits of control lines to specify the operation, and a 1-bit carry input. That's 14 input signals. An 8k x 8 static RAM has 13 address lines. I observed that the 5 control lines allow for 32 distinct operations, most of which are never used (and some follow-on parts, like the 74381, had fewer operation controls).

By allowing only 16 operations selected by four control lines, an 8k static RAM loaded with the results of 4-bit operations can now emulate a 74181, which contains about 75 logic gates. Better, it has 8 outputs, so it can produce a four-bit result and all four traditional flags for every input combination. The 74181 produces carry, an equality signals (A = B), and two additional signals called carry generate and carry propagate.

Of course, using RAMs has a cost. They have to be loaded with the correct content before the YARC has an ALU. Until that time, YARC cannot even execute instructions because it cannot increment the program counter! This is addressed by the system downloader, which can take control of YARC's bus and gain write access to the ALU RAMs (in addition to the microcode RAMs and main memory). The YARC's functional RAM (microcode and ALU) must be properly initialized before the downloader, an Arduino Nano, can release the YARC to execute instructions. The requirement for writing into the ALU significantly complicates the design.

### Carry Select Addition

My lookup-based ALUs doesn't produce carry generate or carry propagate outputs. This means I can't do what is called "lookahead addition" with these devices. So each device (i.e. the RAM for the high-order bits) must wait for the lower order RAM to produce a carry output, which is input as an address line to the high-order device. This is combinational and occurs within each of the phi1 and phi2 clock cycles, so it's likely to become a critical path in system timing.

This can be partially addressed by a scheme I called "Cheesy Lookahead Carry" (CLC) when I (re-)invented it in 2021. I had no illusions that I was the first to think of it at the time, and it turns out it was invented by an IBM engineer 60 years earlier. "My" design is now known as a carry select adder. It's based on the observation that selecting one of two possible results is faster than computing the result.

Instead of having one ALU RAM for the upper four bits, there are two. The first has its carry input (address line 8) wired to "0", and the second has its carry input wired to "1". They are both presented with the same operation and A/B input data (the upper nybbles of the operands) at the same time that the low-order ALU RAM begins working on the low nybble.

The carry **output** of the low order ALU RAM is connected to the output enable of one high-order RAM, inverted, and then connected to the other. The output enable delay is about 30ns shorter than the RAM access time, which reduces the time taken by the entire ALU path by more than 25%.

All three ALU RAMs, one low-order and two high-order, have the same content and are written in parallel by the downloader.

## ALU Output

The low-order byte of output is registered at the end of phi1. The high order byte passes through a transparent driver to the ALU input multiplexer, allowing the result to be stored in a general register at the end of the phi2 operation cycle. The result could be captured on a later cycle if not overwritten (I think).

## ALU Operations on Memory

In YARC, the design goal speed of 4MHz to 6MHz will probably not be realized because of signal quality issues associated with long buses and breadboards. Despite this fact, these goal clock rates are still used to condition the design. The clock period does not allow for data to flow in from memory, through the ALU during phi1, and be latched in the lower-half of the ALU result register.

Instead, a separate cycle must be spent fetching data from memory into the S2BUS ("B" side) ALU holding register. This register is clocked under the control of a separate bit. The S2BUS transceivers are reversed (inbound) when when ALU control field is 2 (0b10) to allow in the data from memory. This operation (loading the B side ALU holding register) is encoded into the ALU control field to prevent a simultaneous phi1 or phi2 ALU cycle (that would not get the correct result).

## ALU Initialization

The downloader (Nano) can take control of the YARC's buses and write data, typically data downloaded from the host (Mac), into the ALU RAMs. There is an ALU Control Register (ACR) with 5 defined bits 0..4 accessible to the Nano (but not YARC; as with microcode RAM, the YARC cannot update itself).

ACR:0 enables the outputs of the high-order, carry = 1 ALU RAM for reading. ACR:1 enables both the low-order RAM and the high-order, carry = 0 RAM to separate 8-bit buses. The buses each have a transceiver to sysbus, and these transceivers are enabled by ACR:2 and ACR:3, respectively. So the pattern 0b1010 will read the high-order, carry = 1 RAM, 0b1001 the high-order, carry = 0 RAM, and the pattern 0b0110 the low-order RAM. Finally, with the Nano in control, ACR:4 can be used to set the carry in address bit (bit 8) of all three RAMs. The low-order address bits must be clocked into the ALU holding registers and the high-order address bits supplied as the ALU operation (from either the instruction register or K register).

The direction signal on the transcievers comes from the Nano's READ/WRITE# bit (address bit 15) when the Nano has control. So the same logic can be used to write the RAMs with the output enables false (high). The write signal is provided by a pulse output from the Nano.

## Summary

The YARC ALU emulates a conventional 1970s minicomputer ALU using unconventional techniques. A possible follow-on project is the construction of an ALU from discrete N- and P-channel MOSFETs or even bipolar transistors.
