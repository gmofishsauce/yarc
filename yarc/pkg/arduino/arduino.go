// Copyright (c) Jeff Berkowitz 2021. All rights reserved.

// Package arduino provides a synchronous byte I/O interface to an Arduino.
// The initial implementation uses the default USB serial port provided by
// an Arduino Nano. Opening a standard USB serial port activates the DTR signal
// which resets the Arduino, necessitating a full reconnect. The code in this
// package  is (necessarily) aware of this. A future version of the hardware
// may support a second serial connection that is USB at the host but Tx/Rx
// only at the Nano, allowing the host to reopen a connection without causing
// a reset. The code in this package will need to change when and if that
// hardware is implemented.

// Originally, the code in this file provided an _asynchronous_ interface
// to the Nano by using channels and select statements for read and write.
// When I ran Golang's data race detector, however, it fired, and the race(s)
// were in the serial port object itself - between port.Read() and port.Close(),
// for example. The Read() was by design a blocking read, meaning the Goroutine
// that issued the Read() spent all its time blocked there waiting for bytes to
// arrive from the Nano, putting them on the read channel, and then blocking
// again. So I couldn't just throw a mutex around the call; it would have been
// held all the time except for a moment each time a byte arrived. This would
// have deadlocked with the need to write on the port (since the Nano only
// responds to requests from the Mac). Bottom line, the serial port object
// isn't threadsafe and it's hard to work around it.
//
// Fortunately, when I discovered this, v1.4 of the of go.bug.st/serial.v1
// had appeared. It offers a read timeout. So it's no longer the case that
// all reads must block indefinitely. I ripped out the Goroutines and changed
// this code to do everything from the main thread (Goroutine). The struct
// arduino remains, holding just a single field (it used to have more).

package arduino

import (
    "fmt"
    "log"
    "go.bug.st/serial"
	"syscall"
	"time"
)

// Naming this package was hard. The functionality is fairly general and could
// be used (or easily adapted for use) with serial devices other than Arduino.
// Also, it's only being tested with a single Arduino model (a Nano) so it's
// a bit of an overreach to assert that this is for "Arduino". But in the end
// it's the answer that provides the most clarity.

// TODO generate shared code for Arduino specifics e.g. baud rate

const resetDelay time.Duration = 4 * time.Second

// Types

type Arduino struct {
	port serial.Port
}

type NoResponseError time.Duration

func (nre NoResponseError) Error() string {
	return fmt.Sprintf("read from Arduino: no response after %v", time.Duration(nre))
}

// Public interface

func New(deviceName string, baudRate int) (*Arduino, error) {
	var arduino Arduino
	var err error

	mode := &serial.Mode{BaudRate: baudRate, DataBits: 8, Parity: serial.NoParity, StopBits: serial.OneStopBit}
	arduino.port, err = serial.Open(deviceName, mode)
	if err != nil {
		return nil, err
	}

	// Here, the time delay is important because otherwise the Nano
	// will consume the first few bytes in an attempt to see if this
	// reset is a programming device trying to flash new firmware.
    log.Printf("serial port is open - delaying %.0f seconds for Nano reset", resetDelay.Seconds())
    time.Sleep(resetDelay)
	return &arduino, nil
}

// Read the Nano until a byte is received or a timeout occurs
func (arduino *Arduino) ReadFor(timeout time.Duration) (byte, error) {
	return arduino.readByte(timeout)
}

// Write bytes to the Arduino.
func (arduino *Arduino) Write(b []byte) error {
	return arduino.writeBytes(b)
}

// Close the connection to the Nano.
func (arduino *Arduino) Close() error {
	return arduino.closeSerialPort()
}

// Implementation

// Read a byte
func (arduino *Arduino) readByte(readTimeout time.Duration) (byte, error) {
	b := make([]byte, 1, 1)
	var n int
	var err error

	// The for-loop is -solely- to handle EINTR, which occurs constantly
	// as a result of Golang's Goroutine-level context switching mechanism.
	arduino.port.SetReadTimeout(readTimeout)
	for {
		n, err = arduino.port.Read(b)
		// Break loop unless EINTR.
		if !isRetryableSyscallError(err) {
			break
		}
		if n != 0 {
			panic("bytes returned despite EINTR")
		}
	}
	if err != nil {
		return 0, err
	}
	if n == 0 {
		return 0, NoResponseError(readTimeout)
	}
	return b[0], nil
}

// Write bytes
func (arduino *Arduino) writeBytes(toWrite []byte) error {
	var n int
	var err error

	// The for-loop is -solely- to handle EINTR, which occurs constantly
	// as a result of Golang's Goroutine-level context switching mechanism.
	for {
		n, err = arduino.port.Write(toWrite)
		// Drop out of the loop on success
		// or error, but not on EINTR.
		if !isRetryableSyscallError(err) {
			break
		}
		if n != 0 {
			panic("bytes written despite EINTR")
		}
	}
	if err != nil {
		return err
	}
	if n != len(toWrite) {
		return fmt.Errorf("write didn't consume all the bytes")
	}
	return nil
}

func (arduino *Arduino) closeSerialPort() error {
	if arduino.port == nil {
		return fmt.Errorf("internal error: close(): port not open")
	}
	if err := arduino.port.Close(); err != nil {
		log.Printf("close serial port: %s", err)
		return err
	}
	log.Println("serial port closed")
	arduino.port = nil
	return nil
}

func isRetryableSyscallError(err error) bool {
	const eIntr = 4
	if errno, ok := err.(syscall.Errno); ok {
		return errno == eIntr
	}
	return false
}

