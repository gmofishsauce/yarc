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
)

// Downloader

const MemorySize = 0x7800

func doDownload(binary *bufio.Reader, nano *arduino.Arduino) error {
	fmt.Println("downloading...")
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

type uint8 byte

func doMemorySection(binary *bufio.Reader, nano *arduino.Arduino) error {
	const ChunkSize = 256
	const ByteRange = 1 << 8
	var writePage[] byte = make([]byte, ChunkSize-1, ChunkSize-1)
	var doCycle[] byte = make([]byte, 5, 5)
	var addr uint16
	var b byte
	var err error

	for addr = 0; addr < MemorySize; addr += ChunkSize {
		// write the first byte
		if b, err = binary.ReadByte(); err != nil {
			return err
		}
		doCycle[0] = sp.CmdXferSingle
		doCycle[1] = byte(addr % ByteRange)			// AH
		doCycle[2] = byte(addr & (ByteRange-1))		// AL
		doCycle[3] = 0                              // DH
		doCycle[4] = b                              // DL
		// XXX TODO doCommandReturningByte I think (returns the BIR)
		if err = doCommandWithFixedArgs(nano, doCycle); err != nil {
			return err
		}

		// Now with the addressing registers set in the Nano,
		// write the rest of the chunk. Note the difference
		// between calling ReadByte() in a loop and calling
		// Read() without the loop: Read() may return a short
		// read that is not EOF, but ReadByte() never will.
		// So by calling ReadByte() in a loop, I avoid having
		// fill logic for partial reads.
		for i := range(writePage) {
			if writePage[i], err = binary.ReadByte(); err != nil {
				return err
			}
		}

		// FYI: doCommandWithCountedBytes() takes the number of
		// bytes to write from the array length. So this is always
		// going to write 2 + (ChunkSize-1) bytes, and the ChunkSize-1
		// data bytes will always be written to YARC memory at the
		// addresses following addr+1.
		if err = doCommandWithCountedBytes(nano, sp.CmdWritePage, writePage); err != nil {
			return err
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
