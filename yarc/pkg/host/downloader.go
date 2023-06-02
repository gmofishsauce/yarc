// Copyright (c) Jeff Berkowitz 2021, 2023. All rights reserved.

package host

// Downloader for YARC.

// TODO - issue a StopCost command to start

import (
	"github.com/gmofishsauce/yarc/pkg/arduino"
	sp "github.com/gmofishsauce/yarc/pkg/proto"

	"bufio"
	"bytes"
	"fmt"
	"log"
)

// Downloader
//
// The assembler produces an absolute binary file, 70k long, having
// three sections. The sections are memory (30k bytes),  microcode
// (32k bytes), and ALU RAM (8k bytes). These can be written to and
// read back from the YARC for validation using a variety of protocol
// commands to the Nano. The code here does this download and verify.
//
// We read the entire binary into an array (slice) up front. We pass
// subslices to the handlers for each section (memory, microcode, and
// ALU RAM). Each section handler is expected to write and then verify
// the content. All transfers to the Nano over the wire are done in
// 64-byte chunkies, an unfortunate compromise (larger would be faster)
// to avoid data overrun as the serial line has no flow control.

const memorySectionBase = 0
const memorySectionSize = 30 * 1024
const microcodeSectionBase = memorySectionSize
const microcodeSectionSize = 32 * 1024
const aluSectionBase = memorySectionSize + microcodeSectionSize
const aluSectionSize = 8 * 1024
const binaryFileSize = memorySectionSize + microcodeSectionSize + aluSectionSize
const chunkSize = 64

func readFile(binary *bufio.Reader) ([]byte, error) {
	var result []byte = make([]byte, binaryFileSize, binaryFileSize)
	var err error
	for i := 0; i < binaryFileSize; i++ {
		result[i], err = binary.ReadByte()
		if err != nil {
			return nil, err
		}
	}
	return result, nil
}

func doDownload(binary *bufio.Reader, nano *arduino.Arduino) error {
	log.Println("downloading...")
	var content []byte
	var err error

	if content, err = readFile(binary); err != nil {
		return err
	}
	if err := doMemorySection(content[0:microcodeSectionBase], nano); err != nil {
		return err
	}
	if err := doMicrocodeSection(content[microcodeSectionBase:aluSectionBase], nano); err != nil {
		return err
	}
	if err := doALUSection(content[aluSectionBase:binaryFileSize], nano); err != nil {
		return err
	}

	log.Println("download complete")
	return nil
}

func doMemorySection(content []byte, nano *arduino.Arduino) error {
	var addr uint16

	// Optimization for main memory: we scan backwards from the end of the
	// memory section in the file until we hit a nonzero value. We download
	// the 64-byte chunkie containing that nonzero value and all the rest
	// down to 0. The assembler supports a directive .bes (Block Ended by
	// Symbol) to place a nonzero value that marks the end of downloadable
	// memory; very old timers will get the joke.

	var zeroes []byte = bytes.Repeat([]byte{0}, chunkSize)
	for addr = memorySectionSize - chunkSize; addr >= 0; addr -= chunkSize {
		if bytes.Compare(content[addr:addr+chunkSize], zeroes) != 0 {
			break
		}
	}

	limit := addr + chunkSize

	for addr = 0; addr < limit; addr += chunkSize {
		toWrite := content[addr : addr+chunkSize]
		if err := writeMemoryChunk(toWrite, nano, addr); err != nil {
			return err
		}
		readBack, err := readMemoryChunk(nano, addr)
		if err != nil {
			return err
		}
		if bytes.Compare(toWrite, readBack) != 0 {
			return fmt.Errorf("memory compare fail: wrote %v read %v\n", toWrite, readBack)
		}
	}
	log.Printf("wrote %d bytes of main memory\n", limit)
	return nil
}

// The microcode RAM is 32 bits wide and 8k high. It's arranged as
// four 8-bit slices that must be written separately. Each slice write
// has large overhead so we really want maximal (64 byte) writes, and
// this lines up nicely with the 64-byte wire chunk size. We write
// bytes 0, 4, 8, ... on the first of the four writes, bytes 1, 5, 9,
// ... on the second of the four, etc.
func doMicrocodeSection(content []byte, nano *arduino.Arduino) error {
	// There are 2^7 opcodes each with 2^8 bytes of microcode
	// Each 2^8 is organized as 2^2 slices of 2^6 bytes each
	// We leverage the fact that the chunk size is also 2^6

	var body []byte = make([]byte, chunkSize, chunkSize)
	const slicesPerOp = 4
	const bytesPerSlicePerOp = 64 // == chunkSize
	const ucodePerOp = slicesPerOp * bytesPerSlicePerOp
	const numOpcodes = 128

	if chunkSize != 64 {
		panic("doMicrocodeSection(): chunkSize must be 64")
	}
	if numOpcodes != microcodeSectionSize/ucodePerOp {
		panic("doMicrocodeSection(): constants incorrectly defined")
	}
	if len(content) != numOpcodes*ucodePerOp {
		panic("doMicrocodeSection(): invalid content length")
	}

	var allNoops []byte = bytes.Repeat([]byte{0xFF}, ucodePerOp)
	var nWritten int

	for op := 0; op < microcodeSectionSize; op += ucodePerOp {
		if bytes.Compare(content[op:op+ucodePerOp], allNoops) == 0 {
			continue
		}
		nWritten++
		log.Printf("write microcode for opcode 0x%02X\n", ((op >> 8)|0x80) & 0xFF)
		for slice := 0; slice < slicesPerOp; slice++ {
			var i int = 0
			for addr := op + slice; addr < op+ucodePerOp; addr += 4 {
				body[i] = content[addr]
				i++
			}
			if err := writeMicrocodeChunk(((op >> 8)|0x80) & 0xFF, slice, body, nano); err != nil {
				return err
			}
		}
	}
	log.Printf("wrote microcode for %d opcodes\n", nWritten)
	return nil
}

// The ALU section is 8k. Writes are very slow. Any 64-byte chunkie
// that is all zeroes is not written (an all-0 byte is impossible
// because if the value is 0, the zero flag (0x20) should be set,
// and otherwise the low order four bits are not zero).
func doALUSection(content []byte, nano *arduino.Arduino) error {
	var addr uint16
	var nWritten int

	var zeroes []byte = bytes.Repeat([]byte{0}, chunkSize)
	for addr = 0; addr < aluSectionSize; addr += chunkSize {
		toWrite := content[addr : addr+chunkSize]
		if bytes.Compare(toWrite, zeroes) == 0 {
			continue
		}
		nWritten++
		if err := writeAluChunk(nano, toWrite, addr); err != nil {
			return err
		}

		// As an optimization, I modified the Nano side of the
		// writeAluChunk() call (the sp.CmdWrAlu protocol function)
		// to write with verify (and panic the Nano if the verify
		// fails). So it's no longer necessary to read all three
		// RAMs separately for verification, although it works
		// just fine to do so. 5/19/2023
		//
		// There are three identical ALU RAMs. They are written
		// as a unit but must be verified separately.
		//for ram := 0; ram < 3; ram++ {
		//	readBack, err := readAluChunk(nano, addr, byte(ram))
		//	if err != nil {
		//		return err
		//	}
		//	if bytes.Compare(toWrite, readBack) != 0 {
		//		return fmt.Errorf("ALU compare fail, RAM %d: wrote %v read %v\n",
		//			ram, toWrite, readBack)
		//	}
		//}
	}
	log.Printf("wrote %d bytes of ALU RAM\n", nWritten*chunkSize)
	return nil
}

func writeAluChunk(nano *arduino.Arduino, body []byte, addr uint16) error {
	var cmdWrAlu []byte = make([]byte, 4, 4)
	cmdWrAlu[0] = sp.CmdWrAlu
	cmdWrAlu[1] = byte(addr >> 8)   // AH
	cmdWrAlu[2] = byte(addr & 0xFF) // AL
	cmdWrAlu[3] = byte(len(body))
	return doCountedSend(nano, cmdWrAlu, body)
}

func readAluChunk(nano *arduino.Arduino, addr uint16, ram byte) ([]byte, error) {
	var cmdRdAlu []byte = make([]byte, 5, 5)
	cmdRdAlu[0] = sp.CmdRdAlu
	cmdRdAlu[1] = byte(addr >> 8)   // AH
	cmdRdAlu[2] = byte(addr & 0xFF) // AL
	cmdRdAlu[3] = byte(ram)
	cmdRdAlu[4] = byte(chunkSize)
	return doCountedReceive(nano, cmdRdAlu)
}

func writeMicrocodeChunk(op int, slice int, body []byte, nano *arduino.Arduino) error {
	// log.Printf("writeMicrocode op 0x%02x slice %d\n", op, slice)
	// var cmdRdSlice []byte = make([]byte, 4, 4)
	// cmdRdSlice[0] = sp.CmdRdSlice
	// cmdRdSlice[1] = byte(0x80 | op)
	// cmdRdSlice[2] = byte(slice)
	// cmdRdSlice[3] = byte(len(body))
	// before, err := doCountedReceive(nano, cmdRdSlice)
	// if err != nil {
	// 	return fmt.Errorf("BEFORE read error: %s", err)
	// }
	// log.Printf("BEFORE: 0x%02X 0x%02X 0x%02X 0x%02X\n", before[0], before[1], before[2], before[3])

	var cmdWrSlice []byte = make([]byte, 4, 4)
	cmdWrSlice[0] = sp.CmdWrSlice
	cmdWrSlice[1] = byte(0x80 | op)
	cmdWrSlice[2] = byte(slice)
	cmdWrSlice[3] = byte(len(body))
	if result := doCountedSend(nano, cmdWrSlice, body); result != nil {
		return fmt.Errorf("microcode write failed: %s", result)
	}

    // cmdRdSlice[0] = sp.CmdRdSlice
    // cmdRdSlice[1] = byte(0x80 | op)
    // cmdRdSlice[2] = byte(slice)
    // cmdRdSlice[3] = byte(len(body))
    // after, err := doCountedReceive(nano, cmdRdSlice)
    // if err != nil {
	// 	return fmt.Errorf("AFTER read error: %s", err)
    // }
	// log.Printf("AFTER: 0x%02X 0x%02X 0x%02X 0x%02X\n", after[0], after[1], after[2], after[3])

	return nil
}

func writeMemoryChunk(content []byte, nano *arduino.Arduino, addr uint16) error {
	// Write the first byte (this sets various Nano registers)
	var doCycle []byte = make([]byte, 5, 5)
	var response []byte

	doCycle[0] = sp.CmdXferSingle
	doCycle[1] = byte(addr >> 8)   // AH
	doCycle[2] = byte(addr & 0xFF) // AL
	doCycle[3] = 0                 // DH
	doCycle[4] = content[0]        // DL
	response, err := doFixedCommand(nano, doCycle, 1)
	if err != nil {
		return err
	}
	// On a write transfer, the returned value should
	// merely echo the data value. It does not verify
	// that the memory write succeeded.
	if response[0] != doCycle[4] {
		return fmt.Errorf("invalid response 0x%02X to CmdXferSingle(0x%02X)",
			response[0], doCycle[4])
	}

	// Now with the addressing registers set in the Nano, send
	// the rest of the chunk with single transfer
	theRest := content[1:]
	cmd := make([]byte, 2, 2)
	cmd[0] = sp.CmdWritePage
	cmd[1] = byte(len(theRest))
	return doCountedSend(nano, cmd, theRest)
}

func readMemoryChunk(nano *arduino.Arduino, addr uint16) ([]byte, error) {
	var doCycle []byte = make([]byte, 5, 5)
	var result bytes.Buffer

	doCycle[0] = sp.CmdXferSingle
	doCycle[1] = byte(addr >> 8)   // AH
	doCycle[1] |= byte(0x80)       // Read
	doCycle[2] = byte(addr & 0xFF) // AL
	doCycle[3] = 0                 // DH
	doCycle[4] = 0                 // DL
	response, err := doFixedCommand(nano, doCycle, 1)
	if err != nil {
		return nil, err
	}
	result.WriteByte(response[0])

	// Now with the addressing registers set in the Nano, send
	// the rest of the chunk with single transfer
	cmd := make([]byte, 2, 2)
	cmd[0] = sp.CmdReadPage
	cmd[1] = byte(chunkSize - 1)
	response, err = doCountedReceive(nano, cmd)
	if err != nil {
		return nil, err
	}
	result.Write(response)
	return result.Bytes(), nil
}
