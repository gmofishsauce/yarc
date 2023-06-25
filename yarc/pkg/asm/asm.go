/*
Copyright © 2022 Jeff Berkowitz (pdxjjb@gmail.com)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

// Package asm defines the YARC assembler, microassembler, and ALU specifier.
// Documentation for the language understood by asm is in the doc/ directory
// of the repo containing the source code for this tool. This comment describes
// the design criteria and the design.
//
// YARC is a 16-bit "retro computer" designed and constructed by one person.
// The goal of the YARC project is to get a piece of hardware running, not to
// create fancy software. Every effort has been made to keep both the language
// and the implementation simple. The assembler does not even support constant
// expressions. Its output is a rigidly-structured binary file, suitable for
// consumption by a downloader, with no metadata or extensibility to speak of.
//
// The language is intended to be regular, i.e. requiring only lexical scanning
// with no real parser. A small number of symbols ("builtins") are provided
// by this implementation and have complex behaviors defined in this Go code.
// Names of builtins begin with a dot, e.g. ".set", ".opcode", etc. The builtin
// symbols are used to define other symbols, such as assembly language mnemonics,
// in a sort of bootstrapping process leading to a language that can be used to
// construct code for YARC.
//
// The instruction set is defined by one or more bootstrapping file(s) written
// in the language accepted by this Go code. Bootstrap file(s) are provided
// with the assembler and enabled by a .include builtin.
//
// The output file format maps directly to YARC physical memory. YARC has a 32k
// byte address space of which 30k is RAM and the top 2k is reserved for I/O,
// so each binary file begins with a 30k chunk of RAM content. The microcode
// RAM is 8k high and 32 bits wide, so the next 32k of the file is a conceptual
// 8k x 4 byte array of data for the microcode RAM. Finally, the YARC contains
// three 8k x 8 RAMs that function as lookup tables, each simulating a 4-bit
// ALU chip. All three ALU RAMs contain the same result patterns, so only 8k
// (file bytes 62k through 70k) is reserved for their content.
//
// There is no tooling to merge or combine binary files. The 70k download must
// be generated by a single pass over source code and the only source code
// modularity feature is the include file. Various features of the builtins
// allow the programmer to set the offset within the binary file, in most cases
// indirectly. For example, the .opcode builtin takes an argument which is the
// byte value of the opcode; the 32-bit microcode words ("slots") that follow
// necessarily start at microcode RAM offset:
//   opcode_value * (4 bytes per microcode slot) * (64 slots per opcode)
// and each microcode word 0..63 in the opcode definition occupies one slot
// (4 bytes). Bytes in the binary output file not specified by source code
// are set to semimagical section-dependent "no-op" values by this tool.

// TODO: "yarc xxx" exit codes (for all yarc subcommands "xxx")

package asm

import (
	"bufio"
	"fmt"
	"log"
	"os"
	"strings"
)

func needsPlural(anInt int) string {
	if anInt == 1 {
		return ""
	}
	return "s"
}

// Assembler: open the source file, process it, and
// write some output (if there weren't any errors.)
func Assemble(sourceFile string) {
	log.SetFlags(log.Lmsgprefix | log.Lmicroseconds)
	log.SetPrefix("asm: ")
	log.Printf("main source file %s\n", sourceFile)

	f, err := os.Open(sourceFile)
	if err != nil {
		log.Fatal(err)
	}

	gs := newGlobalState(bufio.NewReader(f), sourceFile)
	if numErrors := lex(gs); numErrors != 0 {
		log.Fatalf("yasm: lex failed (%d error%s)\n", numErrors, needsPlural(numErrors))
	}

	if numErrors := applyFixups(gs); numErrors != 0 {
		log.Fatalf("yasm: fixups failed (%d error%s)\n", numErrors, needsPlural(numErrors))
	}

	if numErrors := checkSanity(gs); numErrors != 0 {
		log.Fatalf("yasm: sanity check failed (%d error%s)\n", numErrors, needsPlural(numErrors))
	}

	generateALUcontent(gs)
	if err := WriteResults(gs); err != nil {
		log.Fatalf("yasm: assembly succeeded but write results file failed: %s", err)
	}

	fmt.Println("yasm: success")
}

// Lex the source. This is the entire assembler, except for
// applying the fixups and writing out the results.
func lex(gs *globalState) int {
	inError := false
	numErrors := 0

	for {
		t := getToken(gs)
		//log.Printf("process: token %s\n", t)

		// The main loop of the assembler serves only to identify a symbol
		// at the start of a line and turn processing over to that symbol's
		// action function. When the action function is called, the tokenizer
		// is between the key symbol and whatever comes next. The action func
		// then defines the sequences of tokens it will accept, consumes them,
		// and makes some change(s) to the global state (or reports an error)
		// before returning here.

		if inError {
			switch t.tokenKind {
			case tkEnd:
				return numErrors + 1
			case tkNewline:
				numErrors++
				inError = false
			}
			continue
		}

		switch t.tokenKind {
		case tkEnd:
			return numErrors
		case tkNewline:
			break
		case tkError:
			log.Printf("error: %s\n", t.text())
			inError = true
		case tkSymbol:
			keySymbol, ok := gs.symbols[t.text()]
			if !ok {
				errMsg(gs, "expected key symbol, found %s", t)
				inError = true
				break
			}
			if keySymbol.symbolAction == nil {
				errMsg(gs, "not a key symbol: %s", keySymbol.name())
				inError = true
				break
			}
			if err := keySymbol.doAction(gs); err != nil {
				errMsg(gs, err.Error())
				inError = true
			}
		case tkLabel:
			// Label definition, e.g. LABEL:
			if err := makeLabel(gs, t.text()); err != nil {
				errMsg(gs, "error: %s\n", err.Error())
				inError = true
			}
		default:
			errMsg(gs, "unexpected: %s", t)
			inError = true
		}
	}
}

func applyFixups(gs *globalState) int {
	errors := 0
	for _, fx := range(gs.fixups) {
		// log.Printf("apply fixup %v\n", fx)
		if err := fx.fixupAction(gs, fx); err != nil {
			log.Printf("fixup error: %s\n", err)
			errors++
		}
	}
	return errors
}

func checkSanity(gs *globalState) int {
	// The YARC has a quirk: it's not possible to write into any
	// register that's being used (read from) in the same cycle.
	// We need to check the binary for such uses, but this can't
	// be done until it's completely generated so it must be a
	// post-pass. And the post-pass must know almost as much as
	// a disassembler, i.e. how to skip words in the opcode stream
	// that are actually immediate values, how to identify the
	// opcodes that are problematic (write to the dst register),
	// and how to determine a problem exists (target register is
	// actually used, rather than just identified in the RCW).
	// So I'm not actually writing this code for now, even though
	// it's not that hard, until I decide whether to implement
	// a YARC disassembler and share the implementation.
	return 0
}

func errMsg(gs *globalState, format string, args ...interface{}) {
	var sb strings.Builder
	sb.WriteString(fmt.Sprintf("error [%s]: ", gs.reader))
	sb.WriteString(fmt.Sprintf(format, args...))
	log.Println(sb.String())
}
