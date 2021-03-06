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
// with the assembler and enabled by a .include builtin. The eventual goal is
// to bootstrap the FORTH language implementation for YARC. Toward this goal,
// advanced builtins provide FORTH-specific support such as the definion of the
// initial set of FORTH words required to transition to FORTH source code.
//
// The output file format maps directly to YARC physical memory. YARC has a 32k
// byte address space of which 30k is RAM and the top 2k is reserved for I/O,
// so each binary file begins with a 32k chunk of RAM content in which the top
// 2k is not used (not downloaded). The microcode RAM is 8k high and 32 bits
// wide, so the next 32k of the file is a conceptual 8k x 4 byte array of data
// for the microcode RAM. Finally, the YARC contains three 8k x 8 RAMs that
// function as lookup tables, each simulating a 4-bit ALU chip. All three ALU
// RAMs contain the same result patterns, so only 8k (file bytes 64k through
// 72k) is reserved for their content. YARC also has RAM for two hardware
// stacks, but these are initialized to 0 values and may not set by download.
//
// There is no tooling to merge or combine binary files. The 72k download must
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

package asm

import (
	"bufio"
	"fmt"
	"log"
	"os"
	"sort"
	"strings"
)

func Assemble(sourceFile string) {
	log.SetFlags(log.Lmsgprefix | log.Lmicroseconds)
	log.SetPrefix("asm: ")
	log.Printf("main source file %s\n", sourceFile)

	f, err := os.Open(sourceFile)
	if err != nil {
		log.Fatal(err)
	}

	gs := newGlobalState(bufio.NewReader(f), sourceFile)
	numErrors := process(gs)
	if numErrors == 0 {
		dump(gs)
		fmt.Println("yasm: success")
	} else {
		s := "s"
		if numErrors == 1 {
			s = ""
		}
		fmt.Printf("yasm: failed (%d error%s)\n", numErrors, s)
	}
}

func process(gs *globalState) int {
	inError := false
	numErrors := 0

	for {
		t := getToken(gs)
		//log.Printf("process: token %s\n", t)

		// The main loop of the assembler serves only to identify a symbol
		// at the start or a line and turn processing over to that symbol's
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
			log.Printf("error: %s\n", t.tokenText)
			inError = true
		case tkSymbol:
			keySymbol, ok := gs.symbols[t.text()]
			if !ok {
				errMsg(gs, "expected symbol, found %s", t)
				inError = true
				break
			}
			if keySymbol.symbolAction == nil {
				errMsg(gs, "not a key symbol: %s", keySymbol.name())
				inError = true
				break
			}
			if err := keySymbol.action(gs); err != nil {
				errMsg(gs, err.Error())
				inError = true
			}
		default:
			errMsg(gs, "unexpected: %s", t)
			inError = true
		}
	}
}

func dump(gs *globalState) {
	fmt.Println()
	dumpSymbols(gs)
	fmt.Println()
	dumpMem(gs)
	fmt.Println()
	dumpWCS(gs)
}

func dumpSymbols(gs *globalState) {
	sym := &gs.symbols
	names := make([]string, 0, len(*sym))
	for n := range *sym {
		names = append(names, n)
	}
	sort.Strings(names)
	fmt.Printf("%-16s %s\n", "SYMBOL", "VALUE")
	for _, n := range names {
		if !strings.HasPrefix(n, ".") {
			fmt.Printf("%-16s %v\n", n, (*sym)[n].symbolData)
		}
	}
}

const MEM_BYTES = 0x7800
const BYTES_PER_LINE = 16

func dumpMem(gs *globalState) {
	fmt.Println("ADDR   DATA")
	for m := 0; m < MEM_BYTES; m += BYTES_PER_LINE {
		printThisLine := false
		for n := 0; n < BYTES_PER_LINE; n++ {
			if gs.mem[m+n] != 0 {
				printThisLine = true
				break
			}
		}
		if printThisLine {
			fmt.Printf("0x%04X ", m)
			for n := 0; n < BYTES_PER_LINE; n++ {
				fmt.Printf("%02X", gs.mem[m+n])
				if n != 15 {
					fmt.Printf(" ")
				}
			}
			fmt.Println()
		}
	}
}

const WCS_SLOTS_PER_OPCODE = 64
const WCS_SIZE = 0x2000                             // in uint32
const WCS_OPCODES = WCS_SIZE / WCS_SLOTS_PER_OPCODE // 128
const EMPTY_SLOT = 0x00000000                       // TODO define emptiness (noop content)

func dumpWCS(gs *globalState) {
	fmt.Println("SLOT DATA")
	for opcode := 0; opcode < WCS_OPCODES; opcode += WCS_SLOTS_PER_OPCODE {
		printThisOpcode := false
		for slot := 0; slot < WCS_SLOTS_PER_OPCODE; slot++ {
			if gs.wcs[opcode+slot] != EMPTY_SLOT {
				printThisOpcode = true
				break
			}
		}
		if printThisOpcode {
			fmt.Printf("0x%02X ", opcode+0x80) // should disassemble, add is a hack, etc.
			for slot := 0; slot < WCS_SLOTS_PER_OPCODE; slot++ {
				if gs.wcs[opcode+slot] == EMPTY_SLOT {
					fmt.Println()
					break // Hmmm, cannot have an empty slot followed by non-empty slots?
				}
				fmt.Printf("%08X ", gs.wcs[opcode+slot])
			}
		}
	}
}

func errMsg(gs *globalState, format string, args ...interface{}) {
	var sb strings.Builder
	sb.WriteString(fmt.Sprintf("error [%s]: ", gs.reader))
	sb.WriteString(fmt.Sprintf(format, args...))
	log.Println(sb.String())
}
