// Copyright (c) Jeff Berkowitz 2021, 2023. All rights reserved.

package host

// Downloader for YARC.

// TODO - centralize constants
// TODO - centralize definition of file layout
// TODO - possibly read the whole file up front
// TODO - issue a StopCost command to start

import (
	"github.com/gmofishsauce/yarc/pkg/arduino"
    sp "github.com/gmofishsauce/yarc/pkg/proto"

	"bufio"
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

const MemorySize = 0x7800	// 32k - 2k of I/O register space

func doDownload(binary *bufio.Reader, nano *arduino.Arduino) error {
	log.Println("downloading...")
	if err := doMemorySection(binary, nano); err != nil {
		return err
	}
	if err := doMicrocodeSection(binary, nano); err != nil {
		return err
	}
	if err := doALUSection(binary, nano); err != nil {
		return err
	}
	fmt.Println("download complete")
	return nil
}

// Read a fixed set of bytes into the slice at offset through offset + n -1.
// If an error is returned, the state of the slice into[] is undefined.
func readBytes(binary *bufio.Reader, into []byte, offset int, n int) error {
	for i := 0; i < n; i++ {
		b, err := binary.ReadByte()
		if err != nil {
			return err
		}
		into[offset] = b;
		offset++;
	}
	return nil
}

func writeMainMemoryChunk(binary *bufio.Reader, nano *arduino.Arduino, addr uint16, size uint16) error {
	if size > 256 {
		return fmt.Errorf("illegal size: %d", size)
	}

	// write the first byte
	var doCycle[] byte = make([]byte, 5, 5)
	var response[] byte

	b, err := binary.ReadByte()
	if err != nil {
		return err
	}
	doCycle[0] = sp.CmdXferSingle
	doCycle[1] = byte(addr % size)		// AH
	doCycle[2] = byte(addr / size)		// AL
	doCycle[3] = 0						// DH
	doCycle[4] = b						// DL
	response, err = doFixedCommand(nano, doCycle, 1)
	if err != nil {
		return err
	}
	// On a write transfer, the returned value should
	// merely echo the data value.
	if response[0] != doCycle[4] {
		return fmt.Errorf("invalid response 0x%02X to CmdXferSingle(0x%02X)",
			response[0], doCycle[4])
	}

	// Now with the addressing registers set in the Nano, read the
	// rest of the chunk from the binary file. We use ReadByte()
	// because it will never return a short read so we avoid having
	// fill logic.
	writePage := make([]byte, size-1, size-1)
	for i := range(writePage) {
		if writePage[i], err = binary.ReadByte(); err != nil {
			return err
		}
	}
	cmd := make([]byte, 2, 2)
	cmd[0] = sp.CmdWritePage
	cmd[1] = byte(len(writePage))
	return doCountedSend(nano, cmd, writePage)
}

func doMemorySection(binary *bufio.Reader, nano *arduino.Arduino) error {
	const chunkSize = 64
	var addr uint16

	// For now this will only work if ChunkSize is a multiple of memory
	// size (basically a power of 2).
	for addr = 0; addr < MemorySize; addr += chunkSize {
		if err := writeMainMemoryChunk(binary, nano, addr, chunkSize); err != nil {
			return err
		}
		if addr % 2048 == 0 {
			log.Printf("0x%04X done\n", addr)
		}
	}
	return nil
}

func doMicrocodeSection(binary *bufio.Reader, nano *arduino.Arduino) error {
	return nil
}

func doALUSection(binary *bufio.Reader, nano *arduino.Arduino) error {
	return nil
}
