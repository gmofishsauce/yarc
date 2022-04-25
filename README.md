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

## ard - Arduino code for YARC downloader

## doc - YARC documentation (all internal - no external docs are planned).

## forth - Implementation of the FORTH language for YARC.

## ftz - Fritz sketches for YARC.

Fritzing is a useful sketchpad for breadboard-based designs but it's
incredibly quirky.  No guarantees anyone will be able to load these
sketches, not to mention modify them.

Fritzing really, *really* wants everything in "``~/Documents/Fritzing``" on
Mac, and on Mac, setting $HOME does not affect the expansion of tilde.
But I found that making "``~/Documents/Fritzing``" a symlink into this repo
works.

## go - Original host software for YARC. This directory is deprecated.

All the host software is written in Go and tested on darwin-amd64
(Intel-based Mac, not M1) only. Everything here is being replaced by
a Cobra-based CLI contained in yarc.

### go/ser - Serial download handler for Mac

Ser manages the USB serial (and in the future, possibly 2-wire serial)
port to the YARC. It reads commands from the standard input. This will
be replaced by the command "yarc host ..."

### go/protogen - Prototype generated for serial protocol. This will be
replaced by the command "yarc protogen ..."

## img - Eye candy for those who have not installed Fritzin, KiCad, etc.

Images of Fritzing sketches and KiCad schematics for perusal without
installing the apps. Images of the actual implementation.

## ki - KiCad schematics

I started using KiCad just after 6.0 came out so if you use an earlier
version, you'll have to upgrade to load the sketches.

## yarc - Second generation host software

This directory contains a Cobra CLI that contains (or will contain) all
the host software required to support YARC, including a serial line monitor,
an assembler / microassembler, prototype generator for the serial line
protocol, and all the required support for binary download files.

