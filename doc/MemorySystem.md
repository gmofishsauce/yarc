# YARC 30k RAM

## System Context

The YARC data and address busses have 16 wires. There are 16 data bits and
15 address bits. The high-order "bit" on the address bus is the READ/WRITE#
signal. The only other global control signal is the clock; unlike other
parts of YARC, the memory system's behavior is does not vary between init
and runtime (i.e. the YARC/NANO# signal plays no part in memory control).

The 32k address space is divided into 30k bytes of memory and 2k bytes of
I/O space. The memory is implemented with 4 8kx8 CMOS static RAMs. These
are not "period" parts ("It's Not Really Retro!"). Each RAM contains about
400,000 transistors. Such parts became available in the 90s as geometries
fell below 1 micron (1000nm).

The high-order address bit is ignored, so the address 0x8000 folds back
to address 0, 0x8001 to 1, ... and 0xFFFF to 0x7FFF.

Control logic is implemented using 74HC-series MSI like the rest of the YARC
design. 

## Functionality

Memory is both byte and word addressable. Three types of memory cycles are
supported: byte on even address, byte on odd address, and word (16 bits)
on even address. The behavior of word cycle on odd address is undefined.

Memory responds to the addresses 0x0000 through 0x77FF ("30k - 1"). Addresses
from 0x7800 to 0x7FFF do not select any of the RAM chips nor open the
memory bus transceivers. If the I/O subsystem doesn't decode the address,
the data bus will be undriven. The resulting behavior is undefined but in
practice the implementation returns 0xFFFF for reads and discards writes.

## Implementation (refer to KiCad schematic title "YARC Memory")

Fundamental control signals are developed by the NAND gate U14A and the
decoder U22A. These are found at coordinates B1 in the schematic. The NAND
gate's output is RAM/IO#. This is low for I/O address (0x7800 through 0x7FFF) 
and high for memory.

Decodingthis signal with the READ/WRITE# line (SYSBUS:15) the produces the four
(unclocked and therefore potentially glitchy) signals MEMRD#, MEMWR#, IORD#,
and IOWR#.

The IO read/write signals are not used by the memory subsystem. The MEMRD#
signal is used directly as the direction control on the bus transceivers;
since the transceiver enables are gated by the clock as explained later,
glitches on the direction line are believed to irrelevant.

MEMRD# is also used directly as the OE# (output enable) for the RAMs.
