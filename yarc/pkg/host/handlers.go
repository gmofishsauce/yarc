// Copyright (c) Jeff Berkowitz 2021, 2022. All rights reserved.

package host

// Command handlers for Arduino Nano ("Downloader", "System
// Interface" interactions.

// Proposed design for simple macros and command files:
//
// 1. Implement semicolon as an in-line command separator.
// 2. Implement slash char as a macro definition -
//    /macro_name command1 ; command2 ; etc
//    The entire command after macro_name is simply stored
//    Macros must be contained in a single line.
// 3. Use of the macro name as a command invokes the line
// 4. Implement at char as a way to indirect to file -
//    @filename causes the content of filename to be read.
// 5. Allow a commmand as an optional command line argument:
//    ./yarc host @filename
//
// Macros can freely invoke other macros, with a modest depth
// limit to prevent infinite recursion. @-file can also invoke
// other @-files, same deal.
//
// 12/26/2022 - None of this is implemented.

import (
	"github.com/gmofishsauce/yarc/pkg/arduino"
	sp "github.com/gmofishsauce/yarc/pkg/proto"

	"bufio"
	"bytes"
	"fmt"
	"os"
	"strconv"
	"strings"
)

type protocolCommand struct {
	cmdVal          int
	shortCmd        string
	longCmd         string
	nArgBytes       int
	hasCountedBytes bool
	handler         commandHandler
}

const nostr = ""

type commandHandler func(cmd *protocolCommand, nano *arduino.Arduino, line string) (string, error)

// Because of Golang's quirky "initialization loop" restriction, we cannot
// statically initialize the protocolCommand array with the help() function
// because it refers to the array itself. The fact that I have to write
// comments like this is a severe criticism of Golang.

func seeInitBelow(notUsed1 *protocolCommand, notUsed2 *arduino.Arduino, notUsed3 string) (string, error) {
	// Never called; resolves initialization loop
	return nostr, nil
}

var commands = []protocolCommand{
	{sp.CmdBase, "h", "help", 0, false, seeInitBelow},
	{sp.CmdGetMcr, "gm", "GetMcr", 0, false, getMcr},
	{sp.CmdRunCost, "rc", "RunCost", 0, false, runCost},
	{sp.CmdStopCost, "sc", "StopCost", 0, false, stopCost},
	{sp.CmdRunYarc, "rn", "Run", 0, false, runYarc},
	{sp.CmdStopYarc, "st", "Stop", 0, false, stopYarc},
	{sp.CmdClockCtl, "cc", "Clock", 1, false, clockCtl},
	{sp.CmdRdMem, "rm", "ReadMem", 1, true, rdMem},
	{sp.CmdWrMem, "wm", "WriteMem", 1, true, wrMem},
	{sp.CmdPoll, "pl", "Poll", 0, false, notImpl},
	{sp.CmdSvcResponse, "sr", "SvcResponse", 1, true, notImpl},
	{sp.CmdGetVer, "gv", "GetVer", 0, false, notImpl},
	{sp.CmdSync, "sn", "Sync", 0, false, notImpl},
	{sp.CmdSetArh, "ah", "SetAddrHigh", 1, false, notImpl},
	{sp.CmdSetArl, "al", "SetAddrLow", 1, false, notImpl},
	{sp.CmdSetDrh, "dh", "SetDataHigh", 1, false, notImpl},
	{sp.CmdSetDrl, "dl", "SetDataLow", 1, false, notImpl},
	{sp.CmdDoCycle, "dc", "DoCycle", 0, false, doCycle},
	{sp.CmdGetResult, "gr", "GetResult", 0, false, getBir},
	{sp.CmdWrSlice, "ws", "WriteSlice", 3, true, notImpl},
	{sp.CmdRdSlice, "rs", "ReadSlice", 3, false, notImpl},
	// {sp.CmdXferSingle, "xs", "XferSingle", 5, false, notImpl},
	// {sp.CmdWritePage, "wp", "WritePage", 1, true, notImpl},
	// {sp.CmdReadPage, "rp", "ReadPage", 1, true, readState},
	{sp.CmdSetK, "sk", "SetK", 2, true, setK},
	{sp.CmdSetMcr, "sm", "SetMCR", 1, false, setMcr},
	{0, "dn", "Download", 0, false, download},
}

func init() {
	// Break initialization loop
	commands[0].handler = help
}

// Command processor

func process(line string, nano *arduino.Arduino) error {
	for _, cmd := range commands {
		if strings.HasPrefix(line, cmd.shortCmd) || strings.HasPrefix(line, cmd.longCmd) {
			if _, err := cmd.handler(&cmd, nano, line); err != nil {
				return err
			}
			return nil
		}
	}
	// TODO count these and give up if too many (or maybe only if input isn't a terminal)
	fmt.Printf("input %s: unknown command\n", line)
	return nil
}

// Command handlers

// readState reads and displays the 64 bytes at 0x7700.
func readState(cmd *protocolCommand, nano *arduino.Arduino, line string) (string, error) {
	return "", process("rm 0x7700", nano);
}

func download(cmd *protocolCommand, nano *arduino.Arduino, line string) (string, error) {
	yarcbin, err := os.Open("./yarc.bin")
	if err != nil {
		yarcbin, err = os.Open("../yarc.bin")
		if err != nil {
			return nostr, err
		}
	}
	defer func() {
		yarcbin.Close()
	}()

	return nostr, doDownload(bufio.NewReader(yarcbin), nano)
}

func getMcr(cmd *protocolCommand, nano *arduino.Arduino, line string) (string, error) {
	var nanoCmd []byte = []byte{sp.CmdGetMcr}
	result, err := doFixedCommand(nano, nanoCmd, 1)
	if err != nil {
		return nostr, err
	}
	fmt.Printf("MCR 0x%02x\n", result[0])
	return nostr, nil
}

func runCost(cmd *protocolCommand, nano *arduino.Arduino, line string) (string, error) {
	err := doCommand(nano, sp.CmdRunCost)
	return nostr, err
}

func stopCost(cmd *protocolCommand, nano *arduino.Arduino, line string) (string, error) {
	err := doCommand(nano, sp.CmdStopCost)
	return nostr, err
}

func runYarc(cmd *protocolCommand, nano *arduino.Arduino, line string) (string, error) {
	err := doCommand(nano, sp.CmdRunYarc)
	return nostr, err
}

func stopYarc(cmd *protocolCommand, nano *arduino.Arduino, line string) (string, error) {
	err := doCommand(nano, sp.CmdStopYarc)
	return nostr, err
}

func clockCtl(cmd *protocolCommand, nano *arduino.Arduino, line string) (string, error) {
    words := strings.Split(line, " ")
    if len(words) != 2 {
        return nostr, fmt.Errorf("usage: cc arg")
    }

    nanoCmd := make([]byte, 2, 2)
    nanoCmd[0] = sp.CmdClockCtl
	n, err := strconv.ParseInt(words[1], 0, 16)
	if err != nil {
		return nostr, err
	}
	if n > 0xFF {
		return nostr, fmt.Errorf("illegal value")
	}
	nanoCmd[1] = byte(n)

    result, err := doFixedCommand(nano, nanoCmd, 1)
	if err != nil {
		return nostr, err
	}
	fmt.Printf("old MCR 0x%02x\n", result[0])
    return nostr, nil
}

// By-hand wrMem writes a fixed pattern to the 64 bytes at the address given on the line.
// This is mostly just for testing.
func wrMem(cmd *protocolCommand, nano *arduino.Arduino, line string) (string, error) {
	words := strings.Split(line, " ")
	if len(words) != 2 {
		return nostr, fmt.Errorf("usage: wrMem addr")
	}
	addr, err := strconv.ParseInt(words[1], 0, 16)
	if err != nil {
		return nostr, err
	}
	if (addr&0x77C0) != addr {
		return nostr, fmt.Errorf("wrMem: address must be on a 64-byte boundary < 30K")
	}

	var data []byte = bytes.Repeat([]byte{0x18}, 64) // 18 18 18 ...
	return nostr, writeMemoryChunk(data, nano, uint16(addr))
}

// By-hand rdMem reads and displays the 64 bytes at the address given on the line.
func rdMem(cmd *protocolCommand, nano *arduino.Arduino, line string) (string, error) {
	words := strings.Split(line, " ")
	if len(words) != 2 {
		return nostr, fmt.Errorf("usage: wrMem addr")
	}
	addr, err := strconv.ParseInt(words[1], 0, 16)
	if err != nil {
		return nostr, err
	}
	if (addr&0x77C0) != addr {
		return nostr, fmt.Errorf("wrMem: address must be on a 64-byte boundary < 30K")
	}
	data, err := readMemoryChunk(nano, uint16(addr))
	if err != nil {
		return nostr, err
	}

	// Display 64 bytes as a 4 lines of 8 hex words
	for i := 0; i < 4; i++ {
		fmt.Printf("0x%04X: ", int(addr) + i*16)
		for j := 0; j < 8; j++ {
			index := 16*i + 2*j
			word := (data[index+1] << 8) | (data[index]&0xFF)
			fmt.Printf("%04X ", word)
		}
		for j := 0; j < 16; j++ {
			white := ' '
			if j == 15 {
				white = '\n'
			}
			ch := data[16*i + j]
			if ch < 0x20 || ch > 0x7F {
				ch = '?'
			}
			fmt.Printf("%c%c", ch, white)
		}
	}
	return nostr, nil
}

func doCycle(cmd *protocolCommand, nano *arduino.Arduino, line string) (string, error) {
	var nanoCmd []byte = []byte{sp.CmdDoCycle}
	result, err := doFixedCommand(nano, nanoCmd, 1)
	if err != nil {
		return nostr, err
	}
	fmt.Printf("BIR 0x%02x\n", result[0])
	return nostr, nil
}

func getBir(cmd *protocolCommand, nano *arduino.Arduino, line string) (string, error) {
	var nanoCmd []byte = []byte{sp.CmdGetResult}
	result, err := doFixedCommand(nano, nanoCmd, 1)
	if err != nil {
		return nostr, err
	}
	fmt.Printf("BIR 0x%02x\n", result[0])
	return nostr, nil
}

func setK(pc *protocolCommand, nano *arduino.Arduino, line string) (string, error) {
	words := strings.Split(line, " ")
	if len(words) != 5 {
		return nostr, fmt.Errorf("usage: sk k3 k2 k1 k0")
	}

	cmd := make([]byte, 5, 5)
	cmd[0] = sp.CmdSetK
	for i := 1; i < 5; i++ {
		n, err := strconv.ParseInt(words[i], 0, 16)
		if err != nil {
			return nostr, err
		}
		if n > 0xFF {
			return nostr, fmt.Errorf("illegal value")
		}
		cmd[i] = byte(n)
	}

	_, err := doFixedCommand(nano, cmd, 0)
	return nostr, err
}

func setMcr(cmd *protocolCommand, nano *arduino.Arduino, line string) (string, error) {
	words := strings.Split(line, " ")
	if len(words) != 2 {
		return nostr, fmt.Errorf("usage: sm mcrValue")
	}
	n, err := strconv.ParseInt(words[1], 0, 16)
	if err != nil {
		return nostr, err
	}
	_, err = doFixedCommand(nano, []byte{sp.CmdSetMcr, byte(n)}, 0)
	return nostr, err
}

func help(cmd *protocolCommand, nano *arduino.Arduino, line string) (string, error) {
	fmtStr := "%-6s%-12s%-6s%-8s\n"
	fmt.Printf(fmtStr, "Short", "Long", "nArgs", "Counted")
	fmt.Printf(fmtStr, "-----", "----", "-----", "-------")

	fmtStr = "%-6s%-12s%-6d%-8t\n"
	// This mention of []commands is the cause of the initialization loop issue
	for _, c := range commands {
		fmt.Printf(fmtStr, c.shortCmd, c.longCmd, c.nArgBytes, c.hasCountedBytes)
	}
	return nostr, nil
}

func notImpl(cmd *protocolCommand, nano *arduino.Arduino, line string) (string, error) {
	fmt.Printf("command 0x%x(%d, %v): not implemented\n", cmd.cmdVal, cmd.nArgBytes, cmd.hasCountedBytes)
	return nostr, nil
}
