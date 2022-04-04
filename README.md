# YARC - Yet Another Retro Computer

YARC is a homebrew CPU and computer design implemented in a retro style.
Work on YARC started in April 2021. The initial concept was an 8-bit
computer with a 16-bit multiplexed address/data bus and extensive
pipelining.

In December 2021, the design changed to a 16-bit computer with separate
address and data busses, minimal pipelining, an 8-bit/2 cycle ALU, and
two hardware stacks. In short, a general register machine with specific
concessions to the efficient implementation of Charles Moore's FORTH
programming language.

YARC is permanently tethered to a host computer (Mac) through a module
called the "Downloader" (a misnomer, because communication is supported
in both directions between host and YARC).

## 4th - Eventual Forth implementation for YARC.

## ard - Arduino code for YARC downloader

## ftz - Fritz sketches for YARC.

Fritzing is a useful sketchpad for breadboard-based designs but it's
incredibly quirky.  No guarantees anyone will be able to load these
sketches, not to mention modify them.

Fritzing really, *really* wants everything in "~/Documents/Fritzing" on
Mac, and on Mac, setting $HOME does not affect the expansion of tilde.
But I found that making "~/Documents/Fritzing" a symlink into this repo
works.

## doc - YARC documentation (well, eventually...)

## go - Host software for YARC.

All the host software is written in Go and tested on darwin-amd64
(Intel-based Mac, not M1) only.

### go/isa - ISAcode PEG parser-based simulator and assembler generator

### go/ser - Serial download handler for Mac

Ser manages the USB serial (and in the future, possibly 2-wire serial)
port to the YARC. It reads commands from the standard input.

### go/rdr - Reader for YARC binaries

YARC binaries contain microcde and ALU tables in addition to code and
static data.  The reader (rdr) consumes these files and drives ser to
download them to the YARC.

## img - Eventual images

Images of Fritzing sketches and KiCad schematics for perusal without
installing the apps. Images of the actual implementation.

## ki - KiCad schematics

I started using KiCad just after 6.0 came out so if you use an earlier
version, you'll have to upgrade to load the sketches.

