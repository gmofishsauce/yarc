# YARC Assembly Language

## Overview

YARC assembly language ("yasm", like "chasm") is a combination microassembler
and assembler. The assembly language mnemonics are not built in but rather
defined using the language. Once defined, they can be used for conventional
assembly language programming.

The syntax is simple and rigid, requiring only a lexer. It is largely but
not strictly line-oriented. Each line (really "construct") begins with a
_key symbol_. A small set of key symbols are built into the language. These
"builtins" are used to define additional symbols, including additional key
symbols, in a sort of bootstrapping process that eventually providers a full
function assembler.

The language does not support constant expressions, so it does not even require
an expression parser.

The assembler (`yarc asm`) accepts a single source file on the command line.
The source may contain one or more `.include` builtins which specify additional
code by relative (to the main source file) or absolute paths. Source files have
the extension `.yasm` by convention. There is no linker; a successful run of
`yasm` results in the generation of a single, rigidly-structured binary output
file named `yarc.bin` in the same directory as the main source file. This file
may be provided to the serial line monitor (`yarc host`) for download to YARC.

## Builtin key symbols

All builtin symbols begin with dot (`.`), and dot may not be used in symbols
defined by the programmer. Programmer-defined symbols consist of a letter or
underscore followed by letter|digit|underscore. Values may be decimal numbers,
hexadecimal (0x) numbers, or strings delimited by double quotes. Most builtins
are key symbols; a few non-key builtins are described later.

### .set

```
.set symbol value
```

Substitue `value` when `symbol` is encountered in the source text. In order
to avoid some of the complexities of generalized macro processing, severe
restrictions are placed on string values:

1. The value may not contain newlines or double quotes.
1. The value may not contain the dot character, thus no builtin symbols.

The `.set` builtin is intended to address a limited set of situations.

```
.set RSP 5
```

Allows "RSP" to appear as value in a bitfield, while

```
.set FETCH "F1=V1, F2=V2, F3=V3, F4=V4;"
```

allows "FETCH" to appear as the value of a `.slot` builtin, providing a
shorthand for an often-repeated microcode word contained in a slot.

### .include

```
.include path
```

Include the file identified by the path. Paths are relative to the main
source file's directory or may be absolute.

### .bitfield

```
.bitfield name wordsize bitrange
```

The wordsize must be a power of two. The bitrange has the form i:j where
i and j are nonnegative integers with i >= j and both less than (wordsize - 1).

Example:

```
.bitfield src1 8 7:5
```

Specifies a 3-bit field containing the most significant bits of an 8-bit value.

### .opcode

```
.opcode name opcode nargs arg0 arg1 ... argn-1
.slot ...
.endopcode
```

Define an opcode. The name becomes a key symbol, the opcode's assembly
language mnemonic. The opcode is a value between 0x80 and 0xFF
conventionally written in hex.  The arg names must have been defined in
`.bitfield` directives already seen by the assembler (no forward references).
These define the content of the low-order byte of the instruction; when the
mnemonic is later written, the actual argument values must be compatible
with the bitfield sizes.

The `.opcode` builtin is followed by 1 to 64 `.slot` directives that define
the microcode for the instruction. The slot definitions are terminated by
a `.endopcode` builtin.

### .slot

```
.slot Field1=Value1, Field2=Value2, ..., FieldN=ValueN;
```

The `.slot` builtin is used to define a single 32-bit microcode word that
occupies one "slot" in the 8k x 32 microcode RAM. The field names must
previously have been defined by `.bitfield` directives with a wordsize of 32.
Values must be compatible ith field sizes.

Example combining the features documented so far:

```
	.set r0 0
	.set r1 1
	.set r2 2
	.set r3 3
	.bitfield src1, 8, 7:5
	.bitfield src2, 8, 4:2
	.bitfield dst,  8, 1:0

	.bitfield alu_op, 32, 26:23
	.bitfield reg_specifier_from, 32, 22:22
	.set alu_add 0
	.set reg_specifier_irl  1
	.set reg_specifier_ucode 0

	.opcode ADD, 0x80, src1, src2, dst
	.slot alu_op=alu_add, reg_specifier_from=reg_specifier_irl
	.endopcode

	ADD r0, r1, r2
```
