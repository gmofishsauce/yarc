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
		if err = doCommandWithFixedArgs(nano, doCycle); err != nil {
			return err
		}

		// Now with the addressing registers set in the Nano, read the
		// rest of the chunk from the binary file. We use ReadByte()
		// because it will never return a short read so we avoid having
		// fill logic.
		writePage := make([]byte, ChunkSize-1, ChunkSize-1)
		for i := range(writePage) {
			if writePage[i], err = binary.ReadByte(); err != nil {
				return err
			}
		}

		cmd := make([]byte, 1, 1)
	 	cmd[0] = sp.CmdWritePage
		if err = doCountedSend(nano, cmd, writePage); err != nil {
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
