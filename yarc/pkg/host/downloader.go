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
// 64-byte chunkies, an unfortunate compromise to avoid data overrun
// as the serial line has absolutely no flow control of any kind.

const memorySectionBase = 0
const memorySectionSize = 30*1024
const microcodeSectionBase = memorySectionSize
const microcodeSectionSize = 32*1024
const aluRamSectionBase = memorySectionSize + microcodeSectionSize
const aluRamSectionSize = 8*1024
const binaryFileSize = memorySectionSize + microcodeSectionSize + aluRamSectionSize
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
	if err := doMicrocodeSection(content[microcodeSectionBase:aluRamSectionBase], nano); err != nil {
		return err
	}
	if err := doALUSection(content[aluRamSectionBase:binaryFileSize], nano); err != nil {
		return err
	}

	log.Println("download complete")
	return nil
}

func doMemorySection(content []byte, nano *arduino.Arduino) error {
	var addr uint16

	for addr = 0; addr < memorySectionSize; addr += chunkSize {
		toWrite := content[addr:addr+chunkSize]
		if err := writeMemoryChunk(toWrite, nano, addr); err != nil {
			return err
		}
		log.Println("-")
		readBack, err := readMemoryChunk(nano, addr)
		if err != nil {
			return err
		}
		if bytes.Compare(toWrite, readBack) != 0 {
			return fmt.Errorf("memory compare fail: wrote %v read %v\n", toWrite, readBack)
		}
		if addr % 2048 == 0 {
			log.Printf("0x%04X done\n", addr)
		}
	}
	return nil
}

// The microcode RAM is 32 bits wide and 8k high. It's arranged as
// four 8-bit slices that must be written separately. Each slice write
// has large overhead so we really want maximal (64 byte) writes, and
// this lines up nicely with the 64-byte wire chunk size. We write
// bytes 0, 4, 8, ... on the first of the four writes, bytes 1, 5, 9,
// ... on the second of the four, etc.
func doMicrocodeSection(content []byte, nano *arduino.Arduino) error {
	var body []byte = make([]byte, chunkSize, chunkSize)
	const ucodePerOp = 4 * chunkSize

	// There are 2^7 opcodes each with 2^8 bytes of microcode
	// Each 2^8 is organized as 2^2 slices of 2^6 bytes each
	// We leverage the fact that the chunk size is also 2^6
	for op := 0; op < microcodeSectionSize; op += ucodePerOp {
		for slice := 0; slice < 4; slice++ {
			var i int = 0
			for addr := op + slice; addr < op + ucodePerOp; addr += 4 {
				body[i] = content[addr]
				i++
			}
			if err := writeMicrocodeChunk(op, slice, body, nano); err != nil {
				return err
			}
		}
		if op % 16 == 0 && op > 0 {
			log.Printf("%d opcodes written\n", op)
		}
	}
	return nil
}

func doALUSection(content []byte, nano *arduino.Arduino) error {
	return nil
}

func writeMicrocodeChunk(op int, slice int, body []byte, nano *arduino.Arduino) error {
	var cmdWrSlice []byte = make([]byte, 4, 4)
	cmdWrSlice[0] = sp.CmdWrSlice
	cmdWrSlice[1] = byte(0x80 | op)
	cmdWrSlice[2] = byte(slice)
	cmdWrSlice[3] = chunkSize;
	return doCountedSend(nano, cmdWrSlice, body)
}

func writeMemoryChunk(content []byte, nano *arduino.Arduino, addr uint16) error {
	// Write the first byte (this sets various Nano registers)
	var doCycle []byte = make([]byte, 5, 5)
	var response []byte

	doCycle[0] = sp.CmdXferSingle
	doCycle[1] = byte(addr % chunkSize)	// AH
	doCycle[2] = byte(addr / chunkSize)	// AL
	doCycle[3] = 0						// DH
	doCycle[4] = content[0]				// DL
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
	cmd := make([]byte, 2, 2)
	cmd[0] = sp.CmdWritePage
	cmd[1] = byte(chunkSize - 1)
	return doCountedSend(nano, cmd, content[1:])
}

func readMemoryChunk(nano *arduino.Arduino, addr uint16) ([]byte, error) {
	var doCycle []byte = make([]byte, 5, 5)
	var result bytes.Buffer

	doCycle[0] = sp.CmdXferSingle
	doCycle[1] = byte(addr % chunkSize)	// AH
	doCycle[1] |= byte(0x80)			// Read
	doCycle[2] = byte(addr / chunkSize)	// AL
	doCycle[3] = 0						// DH
	doCycle[4] = 0						// DL
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
