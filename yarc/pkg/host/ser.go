// Copyright (c) Jeff Berkowitz 2021, 2022. All rights reserved.

package host

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
	"yarc/arduino"

	"fmt"
	"io"
	"log"
	"os"
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

func Main() {
	log.SetFlags(log.Lmsgprefix|log.Lmicroseconds)
	log.SetPrefix("host: ")
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
		msg, err := getNanoRequest(nano)
		if err != nil {
			// should session end on -any- error? Yes for now.
			return err
		}
		if len(msg) != 0 {
			if isLogRequest(msg) {
				nanoLog.Printf(msg)
			} else {
				if trouble := nanoSyscall(msg); trouble != nil {
					fmt.Printf("session: handling Nano sycall: %v\n", trouble)
				}
			}
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

func nanoSyscall(req string) error {
	if req == "$B" {
		// Breakpoint. For now we don't have to do anything here.
		// When the user types the continue command, we'll send
		// continue to the Nano.
		fmt.Printf("=== Nano is at a breakpoint ===\n")
		return nil
	}
	return fmt.Errorf("unexpected syscall from Nano: %s", req)
}

