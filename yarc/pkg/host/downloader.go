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
//
// Since doDownload() can run for a long time, we must intersperse some
// calls to doPoll() in order to keep consuming the Nano's log. We could
// spin off a Goroutine but we'd need fancy locking; it's easier to do
// everything synchronously.

const MemorySectionBase = 0
const MemorySectionSize = 30 * 1024
const MicrocodeSectionBase = MemorySectionSize
const MicrocodeSectionSize = 32 * 1024
const AluSectionBase = MemorySectionSize + MicrocodeSectionSize
const AluSectionSize = 8 * 1024
const BinaryFileSize = MemorySectionSize + MicrocodeSectionSize + AluSectionSize
const chunkSize = 64

// Download the entire yarc.bin file (the "binary") to the Nano.
func doDownload(binary *bufio.Reader, nano *arduino.Arduino) error {
	log.Println("downloading...")
	var content []byte
	var err error

	if content, err = ReadFile(binary); err != nil {
		return err
	}
	if err := doPoll(nano); err != nil {
		return fmt.Errorf("during download: doPoll(): %s", err)
	}

	if err := doMemorySection(content[0:MicrocodeSectionBase], nano); err != nil {
		return err
	}
	if err := doPoll(nano); err != nil {
		return fmt.Errorf("during download: doPoll(): %s", err)
	}

	if err := doMicrocodeSection(content[MicrocodeSectionBase:AluSectionBase], nano); err != nil {
		return err
	}
	if err := doPoll(nano); err != nil {
		return fmt.Errorf("during download: doPoll(): %s", err)
	}

	if err := doALUSection(content[AluSectionBase:BinaryFileSize], nano); err != nil {
		return err
	}
	if err := doPoll(nano); err != nil {
		return fmt.Errorf("during download: doPoll(): %s", err)
	}

	log.Println("download complete")
	return nil
}

func ReadFile(binary *bufio.Reader) ([]byte, error) {
	var result []byte = make([]byte, BinaryFileSize, BinaryFileSize)
	var err error
	for i := 0; i < BinaryFileSize; i++ {
		result[i], err = binary.ReadByte()
		if err != nil {
			return nil, err
		}
	}
	return result, nil
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
	for addr = MemorySectionSize - chunkSize; addr >= 0; addr -= chunkSize {
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
		if addr & 0x1FF == 0 {
			if err := doPoll(nano); err != nil {
				return fmt.Errorf("during download (memory section): doPoll(): %s", err)
			}
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
	if numOpcodes != MicrocodeSectionSize/ucodePerOp {
		panic("doMicrocodeSection(): constants incorrectly defined")
	}
	if len(content) != numOpcodes*ucodePerOp {
		panic("doMicrocodeSection(): invalid content length")
	}

	var allNoops []byte = bytes.Repeat([]byte{0xFF}, ucodePerOp)
	var nWritten int

	for op := 0; op < MicrocodeSectionSize; op += ucodePerOp {
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
		if err := doPoll(nano); err != nil {
			return fmt.Errorf("during download (microcode section): doPoll(): %s", err)
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
	for addr = 0; addr < AluSectionSize; addr += chunkSize {
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

		if nWritten&3 == 0 {
			if err := doPoll(nano); err != nil {
				return fmt.Errorf("during download (microcode section): doPoll(): %s", err)
			}
		}
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
	var cmdWrSlice []byte = make([]byte, 4, 4)
	cmdWrSlice[0] = sp.CmdWrSlice
	cmdWrSlice[1] = byte(0x80 | op)
	cmdWrSlice[2] = byte(slice)
	cmdWrSlice[3] = byte(len(body))
	if result := doCountedSend(nano, cmdWrSlice, body); result != nil {
		return fmt.Errorf("microcode write failed: %s", result)
	}
	return nil
}

func writeMemoryChunk(content []byte, nano *arduino.Arduino, addr uint16) error {
	var wrMem []byte = make([]byte, 4, 4)

	if len(content) != 64 {
		fmt.Printf("warning: writeMemoryChunk(): len != 64 (%d)\n", len(content))
	}

	wrMem[0] = sp.CmdWrMem
	wrMem[1] = byte(addr >> 8)
	wrMem[2] = byte(addr & 0xFF)
	wrMem[3] = byte(len(content))

	return doCountedSend(nano, wrMem, content)
}

func readMemoryChunk(nano *arduino.Arduino, addr uint16) ([]byte, error) {
	var rdMem []byte = make([]byte, 4, 4)

	rdMem[0] = sp.CmdRdMem
	rdMem[1] = byte(addr >> 8)
	rdMem[2] = byte(addr & 0xFF)
	rdMem[3] = 64 // always

	return doCountedReceive(nano, rdMem)
}
