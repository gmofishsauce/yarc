# YARC Assembly Language

## Overview

YARC assembly language ("yasm", like "chasm") is a combination microassembler
and assembler. The assembly language mnemonics are not built-in, but rather
defined, along with the microcode that implements them, using the language.
Once defined, they can be used for conventional assembly language programming.

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
file named `yarc.bin` in the current directory. This file
may be provided to the serial line monitor (`yarc host`) for download to YARC.

The assembler makes one full pass over the source code. Its design ensures it knows the absolute offset in the code stream at every point in time; as as result it can assign correct values to all labels during the source code pass. It cannot generate the code for forward branches to symbols it hasn't seen yet, so instead it generates _fixups_ which are placed in a _fixup list_. A lightweight second pass then processes the fixup list in order, correcting the opcodes and immediate values as required. This process is described in more detail under **Code Generation**, below.

## Builtin key symbols

All builtin symbols begin with dot (`.`), and dot may not be used in symbols
defined by the programmer. Programmer-defined symbols consist of a letter or
underscore followed by letter|digit|underscore. Values may be decimal numbers,
hexadecimal (0x) numbers, or strings delimited by double quotes. All currently-
defined builtins are key symbols.

### .set

```
.set symbol value
```

Substitue `value` when `symbol` is encountered in the source text. In order
to avoid some of the complexities of generalized macro processing, the value
may not contain a dot ("."). This prevents builtins in the expansion text.

The values may be numbers or strings. Initially, string values are intended
for one purpose only: sequences of "bitfield = value" expressions that will
be expanded within the `.slot` builtin described later. Examples:

```
.set RSP 5
```

Allows "RSP" to appear as value in a bitfield, while

```
.set FETCH "F1=1 F2=7 F3=0 F4=RSP"
```

allows "FETCH" to appear as the value or partial value of a `.slot` builtin,
providing a shorthand for often-repeated microcode field settings in the slot.

### .include

```
.include "path"
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
.slot Field1=Value1 Field2=Value2 ...
      FieldN=ValueN ... ;
```

The `.slot` builtin is used to define a single 32-bit microcode word that
occupies one "slot" in the 8k x 32 microcode RAM. The field names must
previously have been defined by `.bitfield` directives with a wordsize of 32.
Values must be compatible with field sizes. The `field=value` expressions
may span lines. The slot must be terminated with a semicolon.

### .endopcode

```
.endopcode
```
Ends an opcode definition (sequence of slot definitions)

### .dup

Duplicate the microcode for an existing instruction as another (new) instruction with a new name and opcode. This is particularly useful for instructions that take control bits from the instruction register, e.g. ALU operations.

```
.opcode add 0x80 3 .src1 .src2 .dst
.slot "K3_NONE ALU_PHI1_K2 K1_NONE ALU_PHI1_K0_NO_CARRY" ;
.slot "K3_NONE ALU_PHI2_K2 ALU_PHI2_K1 ALU_PHI2_K0" ;
.slot FETCH ;
.slot DECODE ;
.endopcode

.dup sub  0x81 add
```

Duplicate the microcode for `ADD src1, src2, dst` as `SUB src1, src2, dst` in the microcode slots for opcode 0x81. Since the source, target, and destination come from the instruction rather than the microcode and the ACN is the `1` in `0x81`, this is all that's required to specify `SUB`.

## Labels

Labels are valid symbols terminated with a colon character `:`. They are equivalent to `.set theSymbol <location>` where location is the current offset in the code being assembled. 

## Code Generation Metasymbols

The assembler must know how to generate the bits for various instruction formats. This is accomplished by using builtin symbols in `.opcode` definitions; an example (`.src1`, etc.) can be seen in the code just above.

Each of these builtins results in creation of a _fixup_ that is placed in the _fixup list_ during the lexer pass. The fixup's code tells the assembler how to align a bitfield when generating an instruction. When an `ADD` instruction is used, for example, the assembler code behind `.src1` will cause the value to be collected, range checked (it must fit in a 2-bit field), and then aligned appropriately in bits 7:6 of the instructions low byte (which will be the RCW, assuming the microcode correctly specifies that the RCW should come from the instruction register in that cycle).

The available metasymbols are:

| Symbol | Compatible value | Meaning |
|--------|------------------|---------|
|.abs | Label | the entire opcode is replaced by the 16-bit value of the label |
| .rel | Label | the low byte of the opcode is replaced by offset from the instruction to the label |
| .immb | 8-bit value | the low byte of the instruction is replaced by the value |
| .immw | 16-bit value | the word following the instruction will contain the value |
| .rt | value in 0..3 | bits 1:0 of the high order byte of the opcode are replaced by the value |
| .src1 | value in 0..3 | bits 7:6 of the low order byte of the opcode are replaced by the value |
| .src2 | value in 0..7 | bits 5:3 of the low order byte of the opcode are replaced by the value |
| .dst | value in 0..7 | bits 2:0 of the low order byte of the opcode are replaced by the value |

