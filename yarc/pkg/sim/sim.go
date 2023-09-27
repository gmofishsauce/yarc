/*
Copyright © 2023 Jeff Berkowitz (pdxjjb@gmail.com)

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

// Package sim defines the YARC simulator.

package sim

import (
	"bufio"
	"fmt"
	"log"
	"os"

	"github.com/gmofishsauce/yarc/pkg/host"	
)

type Mem []byte

type Reg struct {
	val uint32
	size uint16		// in bits
}
func NewReg(size uint16) Reg {
	return Reg{val: 0, size: size};
}

type Node struct {
	val uint32
	size uint16		// in bits
}

type Engine struct {
	// physical memories
	ram Mem						// memory 30k
	microcode Mem				// memory 32k
	alu Mem						// alu RAM 8k

	// physical registers
	ir Reg						// instruction register
	aluPort1InputHold Reg		// alu port 1 holding reg
	aluPort2InputHold Reg		// alu port 2 holding reg
	aluOutput Reg				// alu output holding reg
	microcodeOffset Reg			// microcode counter (6 bits)
	k Reg						// k (microcode control) register 
	ix [4]Reg					// four index registers,   [3] == SP
	gr [4]Reg					// four general registers, [3] == PC
	
	// dynamic nodes - active second half of clock
	aluA uint8					// ALU A side input, 8 bits
	aluB uint8					// ALU B side input, 8 bits
	sysaddr uint16				// system address
	sysdata uint16				// system data
	addrIncr uint16				// index incrementer
}

func NewEngine(m []byte, uc []byte, a []byte) *Engine {
	return &Engine {
		ram: m,
		microcode: uc,
		alu: a,
	}
}

// Simulator: open the binary file, read it, and
// then execute it.
func Simulate(args []string) error {
	log.SetFlags(log.Lmsgprefix | log.Lmicroseconds)
	log.SetPrefix("sim: ")

	var t1 = []int{}
	var t2 = [...]int{}

	fmt.Printf("TYPES %T %T\n", t1, t2);

    yarcbin, err := os.Open("./yarc.bin")
    if err != nil {
        yarcbin, err = os.Open("../yarc.bin")
        if err != nil {
            return err
        }
    }
    defer func() {
        yarcbin.Close()
    }()
	
	var content []byte
    if content, err = host.ReadFile(bufio.NewReader(yarcbin)); err != nil {
        return err
    }

	var memory Mem = content[host.MemorySectionBase:host.MemorySectionSize]
	var microcode Mem = content[host.MicrocodeSectionBase:host.MicrocodeSectionBase+host.MicrocodeSectionSize]
	var alu Mem = content[host.AluSectionBase:host.AluSectionBase+host.AluSectionSize]

	if len(memory) + len(microcode) + len(alu) != host.BinaryFileSize {
		log.Fatal("internal error: file and section sizes")
	}
	
	return run(NewEngine(memory, microcode, alu))
}

func run(e *Engine) error {
	return nil
}
