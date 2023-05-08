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
	"fmt"
	"os"
	"strconv"
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

var commands = []protocolCommand {
	{ sp.CmdBase,        "h",  "help",        0, false, seeInitBelow },
	{ sp.CmdGetMcr,      "gm", "GetMcr",      0, false, getMcr   },
	{ sp.CmdRunCost,     "rc", "RunCost",     0, false, runCost  },
	{ sp.CmdStopCost,    "sc", "StopCost",    0, false, stopCost },
	{ sp.CmdRunYarc,     "rn", "Run",         0, false, notImpl  },
	{ sp.CmdStopYarc,    "st", "Stop",        0, false, notImpl  },
	{ sp.CmdPoll,        "pl", "Poll",        0, false, notImpl  },
	{ sp.CmdSvcResponse, "sr", "SvcResponse", 1, true,  notImpl  },
	{ sp.CmdGetVer,      "gv", "GetVer",      0, false, notImpl  },
	{ sp.CmdSync,        "sn", "Sync",        0, false, notImpl  },
	{ sp.CmdSetArh,      "ah", "SetAddrHigh", 1, false, notImpl  },
	{ sp.CmdSetArl,      "al", "SetAddrLow",  1, false, notImpl  },
	{ sp.CmdSetDrh,      "dh", "SetDataHigh", 1, false, notImpl  },
	{ sp.CmdSetDrl,      "dl", "SetDataLow",  1, false, notImpl  },
	{ sp.CmdDoCycle,     "do", "DoCycle",     4, false, notImpl  },
	{ sp.CmdGetResult,   "gr", "GetResult",   0, false, notImpl  },
	{ sp.CmdWrSlice,     "ws", "WriteSlice",  3, true,  notImpl  },
	{ sp.CmdRdSlice,     "rs", "ReadSlice",   3, false, notImpl  },
	{ sp.CmdXferSingle,  "xs", "XferSingle",  5, false, notImpl  },
	{ sp.CmdWritePage,   "wp", "WritePage",   1, true,  notImpl  },
	{ sp.CmdReadPage,    "rp", "ReadPage",    1, true,  notImpl  },
	{ sp.CmdSetK,        "sk", "SetK",        2, true,  setK     },
	{ sp.CmdSetMcr,      "sm", "SetMCR",      1, false, setMcr   },
	{ 0,                 "dn", "Download",    0, false, download },
}

func init() {
	// Break initialization loop
	commands[0].handler = help
}

// Command processor

func process(line string, nano *arduino.Arduino) error {
	for _, cmd := range commands {
		if strings.HasPrefix(line, cmd.shortCmd) || strings.HasPrefix(line, cmd.longCmd) {
			if str, err := cmd.handler(&cmd, nano, line); err != nil {
				if (debug) {
					fmt.Printf("ensure variable str is used for now: %s\n", str)
				}
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

func download(cmd *protocolCommand, nano *arduino.Arduino, line string) (string, error) {
    yarcbin, err := os.Open("./yarc.bin")
    if err != nil {
		yarcbin, err = os.Open("../yarc.bin")
		if err != nil {
			return nostr, err;
		}
    }
    defer func() {
        yarcbin.Close()
    }()

	fmt.Println("Downloading...")
    return nostr, doDownload(bufio.NewReader(yarcbin), nano)
}

func getMcr(cmd *protocolCommand, nano *arduino.Arduino, line string) (string, error) {
	var nanoCmd []byte = []byte{sp.CmdGetMcr}
	result, err := doFixedCommand(nano, nanoCmd, 1)
	if err != nil {
		return nostr, err
	}
	fmt.Printf("0x%02x\n", result[0])
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

func setK(pc *protocolCommand, nano *arduino.Arduino, line string) (string, error) {
	words := strings.Split(line, " ");
	if len(words) != 5 {
		return nostr, fmt.Errorf("usage: sk k3 k2 k1 k0");
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
		cmd[i] = byte(n);
	}

	_, err := doFixedCommand(nano, cmd, 0)
	return nostr, err
}

func setMcr(cmd *protocolCommand, nano *arduino.Arduino, line string) (string, error) {
	words := strings.Split(line, " ");
	if len(words) != 2 {
		return nostr, fmt.Errorf("usage: sm mcrValue");
	}
	n, err := strconv.ParseInt(words[1], 0, 16)
	if err != nil {
		return nostr, err
	}
	_, err = doFixedCommand(nano, []byte { sp.CmdSetMcr, byte(n) }, 0)
	return nostr, err
}

func help(cmd *protocolCommand, nano *arduino.Arduino, line string) (string, error) {
	fmtStr := "%-6s%-12s%-6s%-8s\n"
	fmt.Printf(fmtStr, "Short", "Long", "nArgs", "Counted")
	fmt.Printf(fmtStr, "-----", "----", "-----", "-------")

	fmtStr = "%-6s%-12s%-6d%-8t\n"
	// This mention of []commands is the cause of the initialization loop issue
	for _, c := range(commands) {
		fmt.Printf(fmtStr, c.shortCmd, c.longCmd, c.nArgBytes, c.hasCountedBytes)
	}
	return nostr, nil
}

func notImpl(cmd *protocolCommand, nano *arduino.Arduino, line string) (string, error) {
	fmt.Printf("command 0x%x(%d, %v): not implemented\n", cmd.cmdVal, cmd.nArgBytes, cmd.hasCountedBytes)
	return nostr, nil
}
