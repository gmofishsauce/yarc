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

## Implementation

Refer to the KiCad schematic title "YARC Memory" or the equivalent image
in the **img** directory.

Fundamental control signals are developed by the NAND gate U14A and the
decoder U22A. These are found at coordinates B1 in the schematic. The NAND
gate's output is `RAM/IO#`. This is low for I/O addresses (0x7800 through
0x7FFF) and high for memory addresses (0x0000 through 0x77FF).

Decoding this signal with the `READ/WRITE#` line (`SYSBUS:15`) (section B1)
produces the four signals `MEMRD#`, `MEMWR#`, `IORD#`, and `IOWR#`. These
are unclocked and as a result are potentially glitchy.

The IO read/write signals are not used by the memory subsystem, although
`RAM/IO#` and a signal called `LOW128#` (derived from the address bus in
section C1 of the schematic) are generated on-board and routed to a decoder
in the Downloader Extension.

MEMRD# and MEMWR# are mixed with the clock to create the signals `MEMRD_CLK#`
and `MEMWR_CLK#` (section B2). These are not (supposed to be) glitchy, since
the address shold have settled before the clock goes low in the second half
of each cycle. (Normally, we don't use an overbar or `#` symbol for clock
lines; maybe we shouldn't here either.)

The clocked MEMRD and MEMWR signals form the output enable (OE#) and
write enable (WE#) signals to the RAMs. The RAM/IO# line is used for the
active-high CE2 line.

The low chip-enable is used to select the correct
RAM (or RAM pair for 16-bit operations). This the result of somewhat
complex decoding performed by half a 2-4 decoder (139) U22B and by U26,
a multiplexer, found in C2 and C3 of the schematic. The results are the
four signals `SEL_xRxB` where `x` is H for Hi or L for low. The range
refers to the address (upper or lower 16k) while the byte refers to
high byte or low byte.

The 157 is switched based on the 16-bit cycle line `MEM16#` so that
two RAMs are selected when `MEM16#` is low but only one RAM when high.
The bus transceivers, in turn, are controlled by signals derived in
D1 through D3 of the schematic. The `CROSS_EN#` signal enables the high
memory bus to low system bus when a byte cycle is performed to an odd
address. `STRAIGHT_EN#` enables high-to-high and low-to-low transceivers
for both even-addressed cycle types, byte and word.

