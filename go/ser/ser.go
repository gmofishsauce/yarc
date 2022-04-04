// Copyright (c) Jeff Berkowitz 2021, 2022. All rights reserved.

package main

// Serial port communications for Arduino Nano.
// This program implements a simple byte-oriented protocol used to
// communicate with an Arduino Nano. The Nano runs firmware that
// implements the downloader for the YARC sort-of-retro-computer.

// Consider this if it turns out to be useful to have nonblocking
// I/O on the serial port:
// "github.com/albenik/go-serial"
// This implementation uses blocking serial I/O, goroutines, and
// the Golang-level select statement to multiplex input and output.

import (
	"local.pkg/arduino"
	sp "local.pkg/serial_protocol"

	"fmt"
	"io"
	"log"
	"os"
	"strings"
	"time"
)

var debug = false

const responseDelay = 20 * time.Millisecond
const commandDelay = 50 * time.Millisecond
const interSessionDelay = 3000 * time.Millisecond

const arduinoNanoDevice = "/dev/cu.usbserial-AQ0169PT"
const baudRate = 115200 // Note: change requires updating the Arduino firmware

// When the Arduino (the "Nano") is connected by USB-serial, opening the port
// from the Mac side forces a hard reset to the device (the Arduino restarts).
// If the Nano is connected by two-wire serial, it doesn't. Both possibilities
// have pros and cons. During the first part of development of the YARC, USB-
// serial was used. But changing to 2-wire serial is contemplated. (The 2-wire
// connection would also be USB at the host, but would not connect the DTR line
// to the Nano so would not cause a reset when opened.)

// About calls to time.Sleep() in this code: sleeps occur only during session
// setup and teardown, and they are long (seconds). There are no millisecond
// delays imposed by code in this file. As of March 2022, the only millisecond
// sleep is in the terminal input code.

func main() {
	log.SetFlags(log.Lmsgprefix|log.Lmicroseconds)
	log.SetPrefix("ser: ")
	log.Println("firing up")

	nanoLogFile, err := os.OpenFile("Nano.log", os.O_APPEND|os.O_CREATE|os.O_WRONLY, 0644)
	if err != nil {
		log.Fatal("opening Nano.log: ", err)
	}
	nanoLog := log.New(nanoLogFile, "", log.Lmsgprefix|log.Lmicroseconds)

	for {
		log.Println("starting a session")
		err := session(nanoLog)
		if err == io.EOF {
			break
		}
		// Report interesting errors
		log.Printf("session aborted: %v\n", err)
		time.Sleep(interSessionDelay)
	}
}

// Conduct a session with the Nano. Ideally, this function is
// called once per execution of this program and never returns.
//
// Errors:
// No such file or directory: the Nano is probably not plugged in
// (The USB device doesn't exist in /dev unless Nano is connected.)
//
// Connection not established: device open, but protocol broke down
//
func session (nanoLog *log.Logger) error {
	nano, err := arduino.New(arduinoNanoDevice, baudRate)
	if err != nil {
		return err
	}
	defer nano.Close()

	// Session creation
	tries := 3
	for i := 0; i < tries; i++ {
		log.Println("creating connection")
		if err = establishConnection(nano, i == 0); err == nil {
			break
		}

		log.Printf("connection setup failed: %v: sync retry %d\n", err, i + 1)
		time.Sleep(interSessionDelay)
	}
	if err != nil {
		return err	
	}

	log.Println("session in progress")
	input := NewInput()

	for {
		if err := getNanoLogs(nano, nanoLog); err != nil {
			// should session end on -any- error? Yes for now.
			return err
		}
		line := input.get()
		if len(line) > 0 {
			if line == "EOF" {
				return io.EOF
			}
			if err := process(line[:len(line)-1], nano); err != nil {
				return err
			}
		}
	}
}

// Nano command handling. Commands are received on the standard input.

type protocolCommand struct {
	cmdVal int
	shortCmd string
	longCmd string
	nArgBytes int
	hasCountedBytes bool
	handler commandHandler
} 

// TODO cmd *protocolCommand I think
type commandHandler func(cmd protocolCommand, nano *arduino.Arduino) error

// Because of Golang's quirky "initialization loop" restriction, we cannot
// statically initialize the protocolCommand array with the help() function
// because it refers to the array itself. The fact that I have to write
// comments like this is a severe criticism of Golang.

func seeInitBelow(notUsed1 protocolCommand, notUsed2 *arduino.Arduino) error {
	// Never called; resolves initialization loop
	return nil
}

var commands = []protocolCommand {
	{ sp.CmdBase,        "h",  "help",        0, false, seeInitBelow },
	{ sp.CmdGetMcr,      "gm", "GetMcr",      0, false, notImpl },
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

// Command handlers

func help(cmd protocolCommand, nano *arduino.Arduino) error {
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

func notImpl(cmd protocolCommand, nano *arduino.Arduino) error {
	fmt.Printf("command 0x%x(%d, %v): not implemented\n", cmd.cmdVal, cmd.nArgBytes, cmd.hasCountedBytes)
	return nil
}

// Handler for single byte commands, no arguments, ack or nak only
func doSingle(cmd protocolCommand, nano *arduino.Arduino) error {
	return notImpl(cmd, nano)
}

// Command processor

func process(line string, nano *arduino.Arduino) error {
	for _, cmd := range commands {
		if strings.HasPrefix(line, cmd.shortCmd) || strings.HasPrefix(line, cmd.longCmd) {
			if err := cmd.handler(cmd, nano); err != nil {
				return err
			}
			return nil
		}
	}
	// TODO count these and give up if too many (or maybe only if input isn't a terminal)
	fmt.Printf("input %s: unknown command\n", line)
	return nil
}
