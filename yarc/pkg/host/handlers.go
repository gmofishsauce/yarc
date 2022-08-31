// Copyright (c) Jeff Berkowitz 2021, 2022. All rights reserved.

package host

// Command handlers for Arduino Nano ("Downloader", "System
// Interface" interactions.

import (
	"yarc/arduino"
	sp "yarc/proto"

	"fmt"
	"strings"
)

type protocolCommand struct {
	cmdVal int
	shortCmd string
	longCmd string
	nArgBytes int
	hasCountedBytes bool
	handler commandHandler
} 

type commandHandler func(cmd *protocolCommand, nano *arduino.Arduino) error

// Because of Golang's quirky "initialization loop" restriction, we cannot
// statically initialize the protocolCommand array with the help() function
// because it refers to the array itself. The fact that I have to write
// comments like this is a severe criticism of Golang.

func seeInitBelow(notUsed1 *protocolCommand, notUsed2 *arduino.Arduino) error {
	// Never called; resolves initialization loop
	return nil
}

var commands = []protocolCommand {
	{ sp.CmdBase,        "h",  "help",        0, false, seeInitBelow },
	{ sp.CmdGetMcr,      "gm", "GetMcr",      0, false, getMcr },
	{ sp.CmdEnFast,      "ef", "EnFast",      0, false, notImpl },
	{ sp.CmdDisFast,     "df", "DisFast",     0, false, notImpl },
	{ sp.CmdEnSlow,      "es", "EnSlow",      0, false, notImpl },
	{ sp.CmdDisSlow,     "ds", "DisSlow",     0, false, notImpl },
	{ sp.CmdSingle,      "ss", "SingleStep",  0, false, notImpl },
	{ sp.CmdRunYarc,     "rn", "Run",         0, false, notImpl },
	{ sp.CmdStopYarc,    "st", "Stop",        0, false, notImpl },
	{ sp.CmdPoll,        "pl", "Poll",        0, false, notImpl },
	{ sp.CmdSvcResponse, "sr", "SvcResponse", 1, true,  notImpl },
	{ sp.CmdGetVer,      "gv", "GetVer",      0, false, notImpl },
	{ sp.CmdSync,        "sn", "Sync",        0, false, notImpl },
	{ sp.CmdSetArh,      "ah", "SetAddrHigh", 1, false, notImpl },
	{ sp.CmdSetArl,      "al", "SetAddrLow",  1, false, notImpl },
	{ sp.CmdSetDrh,      "dh", "SetDataHigh", 1, false, notImpl },
	{ sp.CmdSetDrl,      "dl", "SetDataLow",  1, false, notImpl },
	{ sp.CmdDoCycle,     "do", "DoCycle",     4, false, notImpl },
	{ sp.CmdGetResult,   "gr", "GetResult",   0, false, notImpl },
	{ sp.CmdXferSingle,  "xs", "XferSingle",  5, false, notImpl },
	{ sp.CmdWritePage,   "wp", "WritePage",   1, true,  notImpl },
	{ sp.CmdReadPage,    "rp", "ReadPage",    1, true,  notImpl },
}

func init() {
	// Break initialization loop
	commands[0].handler = help
}

// Command processor

func process(line string, nano *arduino.Arduino) error {
	for _, cmd := range commands {
		if strings.HasPrefix(line, cmd.shortCmd) || strings.HasPrefix(line, cmd.longCmd) {
			if err := cmd.handler(&cmd, nano); err != nil {
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

func getMcr(cmd *protocolCommand, nano *arduino.Arduino) error {
	result, err := doCommandWithByte(nano, sp.CmdGetMcr)
	if err != nil {
		return err
	}
	fmt.Printf("0x%02x\n", result)
	return nil
}

func help(cmd *protocolCommand, nano *arduino.Arduino) error {
	fmtStr := "%-6s%-12s%-6s%-8s\n"
	fmt.Printf(fmtStr, "Short", "Long", "nArgs", "Counted")
	fmt.Printf(fmtStr, "-----", "----", "-----", "-------")

	fmtStr = "%-6s%-12s%-6d%-8t\n"
	// This mention of []commands is the cause of the initialization loop issue
	for _, c := range(commands) {
		fmt.Printf(fmtStr, c.shortCmd, c.longCmd, c.nArgBytes, c.hasCountedBytes)
	}
	return nil
}

func notImpl(cmd *protocolCommand, nano *arduino.Arduino) error {
	fmt.Printf("command 0x%x(%d, %v): not implemented\n", cmd.cmdVal, cmd.nArgBytes, cmd.hasCountedBytes)
	return nil
}
