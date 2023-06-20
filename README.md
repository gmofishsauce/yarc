# YARC - Yet Another Retro Computer

## Overview

YARC is a homebrew CPU and computer design implemented in a retro style.
Work on YARC started in April 2021. The initial concept was an 8-bit
computer with a 16-bit multiplexed address/data bus and extensive
pipelining.

In December 2021, the design changed to a 16-bit computer with separate
address and data busses, minimal pipelining, four general registers, an
8-bit-wide/2-cycle ALU, and two hardware stacks. In short, a general
register machine with specific concessions to the efficient implementation
of Charles Moore's FORTH programming language.

Most recently, I decided not to build the stack hardware and to instead
focus on developing the basic instruction set using two of the four
registers for PC and SP. I plan to write a translator for a small subset
of web assembler to YARC machine language (an "AoT" in their terminology,
ahead-of-time compiler). This will allow me to program the YARC in a small
subset any language that can target WASM--like Gnu C or Golang--rather
than confining YARC programming to the Forth language.

YARC is permanently tethered to a host computer (Mac) through a module
called the "Downloader" (a misnomer, because communication is supported
in both directions between host and YARC). The Downloader is based on a
modern microprocessor (Arduino Nano) which implements serial over USB.

The YARC itself is built from 74HC-series CMOS parts, with a few
out-of-period extensions like 8kx8 static CMOS RAMs.

## NRR - It's Not Really Retro

YARC's architecture resembles the [Data General Nova](https://en.wikipedia.org/wiki/Data_General_Nova) minicomputer. The Nova was introduced in 1969. YARC, like Nova, is mostly built from 7400-series components.

But the YARC is _nothing_ like a 1970s minicomputer. Why not? Let me count the ways...

* **Sophisticated open source design software.** In 1969, "design software" meant a sharp pencil, a [plastic template](https://www.amazon.com/Alvin-TD1279-Electric-Electronic-Template/dp/B000KNLR4A/ref=asc_df_B000KNLR4A/), and an angled table. Now, it means [KiCad](https://www.kicad.org/).
* **Highly integrated, sub-100ns static RAMs.** In 1969, semiconductor memory hadn't happened yet. YARC relies on 8k x 8 static CMOS RAMs with 55ns access times - each chip contains more transistors than an _entire_ Data General Nova. And the original Nova, with core memory, had a 5-microsecond cycle time - about 10x slower than YARC can likely achieve.
* **Universal knowledge base.** In 1969, knowledge about computer design was in the hands of a few experts. Today, a quick search can bring you [this](https://www.godevtool.com/GoasmHelp/usflags.htm) or [this](https://www.righto.com/2013/09/the-z-80-has-4-bit-alu-heres-how-it.html).
* **These aren't the 7400 series components you're looking for.** YARC is built with 74HC-series components. These have better noise margins, controlled edge rates, and far lower power consumption than traditional 7400-series TTL. It's fair to say the 74HC-series wasn't introduced until it was already obsolete!

## What's here?

### [ard](https://github.com/gmofishsauce/yarc/tree/main/ard/yarc_fw) - Arduino code for YARC downloader

Nano IDE (v2.X) project holding C++ code for the Arduino that implements
the "Downloader" (which should be renamed to "System Interface" or something).

### [doc](https://github.com/gmofishsauce/yarc/tree/main/doc) - YARC documentation

Engineering documentation for YARC. No "user documentation" is planned,
but some of the documentation is tersely user-oriented, such as the
document that describes the assembly language.

### [ftz](https://github.com/gmofishsauce/yarc/tree/main/ftz) - Fritz sketches for YARC.

Fritzing is a useful sketchpad for breadboard-based designs but it's
incredibly quirky.  No guarantees anyone will be able to load these
sketches, not to mention modify them.

Fritzing really, *really* wants everything in "``~/Documents/Fritzing``" on
Mac, and on Mac, setting $HOME does not affect the expansion of tilde.
But I found that making "``~/Documents/Fritzing``" a symlink into this repo
works.

### [img](https://github.com/gmofishsauce/yarc/tree/main/img) - Eye candy

Images of Fritzing sketches, KiCad schematics, and the implementation
itself. Allows the schematics and layouts to examined without
installing the apps (KiCad and Fritzing).

### [ki](https://github.com/gmofishsauce/yarc/tree/main/ki) - KiCad schematics

I started using KiCad just after 6.0 came out so if you use an earlier
version, you'll have to upgrade to load the sketches.

### [yarc](https://github.com/gmofishsauce/yarc/tree/main/yarc) - Host software

All the host software is written in Go and tested on darwin-amd64
(Intel-based Mac, not M1) only. The host software is contained in a Cobra
CLI defined here.

### [yasm](https://github.com/gmofishsauce/yarc/tree/main/yasm) - YARC microassembler and assembler source code

The YARC assembler supports definition of the assembly language
mnemonics within the language along with the microcode for each
instruction. So the code in this directory defines the YARC instruction
set.

The assembler itself is invoked by `yarc asm ...` in the host software.

## Problems with YARC

YARC is my first-ever large digital design. It has many issues.

### Messy Control Circuitry

The microcode (writable control store) and ALU (arithmetic/logical unit) of YARC are implemented with RAMs instead of the more common EEROM/EPROM technologies. The RAMs must be "downloaded" with content from the host (Mac) through the Arduino ("downloader") before YARC can start.

This is cool, as I can fundamentally change YARC's behavior without popping ROMs in and out of circuit. But it greatly complicated the design, requiring a "back bus" for writes to ALU and Microcode RAM, additional transceivers, and control circuitry. I made matters much worse by failing to think through the control circuitry in a unified way, instead constructing a bunch of special-purpose control registers.

### Floorplanning Issues

YARC is implemented on 32 large solderless breadboards mounted to four 1' square copper clad boards. At 2' by 2', it has the potential for very long wire runs. I made this situation far worse than necessary by putting controls distribution in the center rather than centralizing data buses and bus transceivers and bringing control signals in from the edges.

### Write timing

Both the main memory RAMs and the 74HC670 devices used for the dual-ported general registers suffer from issues if their address inputs change while their write-enable signals are active (low in both cases). I conceived each bus cycle as "outputs are enabled during first half of clock with clock high, enables go low for second half of clock, and writes are latched at rising edge". Unfortunately, transceivers enables are "enables", and also drive address lines; so their settling time overlaps with the write signal going low. I had to implement delay lines for write clocks in two places. 


