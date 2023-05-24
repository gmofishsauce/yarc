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

package asm

import (
	"bufio"
	"encoding/binary"
	"fmt"
	"os"
	"sort"
	"strings"
)

// Whether to dump the various memory contents to the screen.
// For now, yes. Maybe make this a command line option someday.
const dumpSections = true

const MEM_BYTES = 0x7800
const BYTES_PER_LINE = 16

const WCS_SLOTS_PER_OPCODE = 64
const WCS_SIZE = 0x2000                             // in uint32
const WCS_OPCODES = WCS_SIZE / WCS_SLOTS_PER_OPCODE // 128
const SLOT_NOOP = 0xFFFFFFFF

// Write the results of a successful run of the assembler.
// The contents of the symbol table are dumped to the screen.
// The binary file yarc.bin is always written. The contents
// are optionally dumped to the screen.
func WriteResults(gs *globalState) error {
	writeSymbols(gs)

	yarcbin, err := os.Create("yarc.bin")
	if err != nil {
		return err
	}
	defer func() {
		yarcbin.Close()
	}()

	return writeBinary(bufio.NewWriter(yarcbin), gs)
}

func writeSymbols(gs *globalState) {
	fmt.Println()
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

func writeBinary(yarcbin *bufio.Writer, gs *globalState) error {
	fmt.Println("")
	if err := writeMem(yarcbin, gs); err != nil {
		return err
	}
	if dumpSections {
		dumpMem(gs)
	}

	fmt.Println("")
	if err := writeWCS(yarcbin, gs); err != nil {
		return err
	}
	if dumpSections {
		dumpWCS(gs)
	}

	fmt.Println("")
	if err := writeALU(yarcbin, gs); err != nil {
		return err
	}
	if dumpSections {
		dumpALU(gs)
	}
	return nil
}

func writeMem(yarcbin *bufio.Writer, gs *globalState) error {
	n, err := yarcbin.Write(gs.mem)
	fmt.Printf("Wrote %d bytes to mem section\n", n)
	return err
}

func dumpMem(gs *globalState) {
	dumpBytes("Memory", gs.mem)
}

func dumpBytes(label string, bytes []byte) {
	fmt.Printf("%s\nADDR   DATA\n", label)
	for m := 0; m < len(bytes); m += BYTES_PER_LINE {
		printThisLine := false
		for n := 0; n < BYTES_PER_LINE; n++ {
			if bytes[m+n] != 0 {
				printThisLine = true
				break
			}
		}
		if printThisLine {
			fmt.Printf("0x%04X ", m)
			for n := 0; n < BYTES_PER_LINE; n++ {
				fmt.Printf("%02X", bytes[m+n])
				if n != 15 {
					fmt.Printf(" ")
				}
			}
			fmt.Println()
		}
	}
}

func writeWCS(yarcbin *bufio.Writer, gs *globalState) error {
	err := binary.Write(yarcbin, binary.LittleEndian, gs.wcs)
	fmt.Printf("Wrote %d bytes to wcs section\n", 4*len(gs.wcs))
	return err
}

func dumpWCS(gs *globalState) {
	fmt.Println("Microcode")
	for opcode := 0; opcode < WCS_OPCODES; opcode++ {
		printThisOpcode := false
		for slot := 0; slot < WCS_SLOTS_PER_OPCODE; slot++ {
			if gs.wcs[WCS_SLOTS_PER_OPCODE*opcode+slot] != SLOT_NOOP {
				printThisOpcode = true
				break
			}
		}
		if printThisOpcode {
			fmt.Printf("Opcode 0x%02X\n", opcode+0x80) // should disassemble, add is a hack, etc.
			for slot := 0; slot < WCS_SLOTS_PER_OPCODE; slot++ {
				fmt.Printf("%08X ", gs.wcs[WCS_SLOTS_PER_OPCODE*opcode+slot])
				if slot&0x07 == 0x07 {
					fmt.Println()
				}
			}
		}
	}
}

func writeALU(yarcbin *bufio.Writer, gs *globalState) error {
	n, err := yarcbin.Write(gs.alu)
	fmt.Printf("Wrote %d bytes to alu section\n", n)
	return err
}

func dumpALU(gs *globalState) {
	dumpBytes("ALU", gs.alu)
}
