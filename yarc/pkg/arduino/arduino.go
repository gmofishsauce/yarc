// Copyright (c) Jeff Berkowitz 2021. All rights reserved.

// Package arduino provides an aynchronous byte I/O interface to an Arduino.
// The initial implementation uses the default USB serial port provided by
// an Arduino Nano. Opening a standard USB serial port activates the DTR signal
// which resets the Arduino, necessitating a full reconnect. The code in this
// package  is (necessarily) aware of this. A future version of the hardware
// may support a second serial connection that is USB at the host but Tx/Rx
// only at the Nano, allowing the host to reopen a connection without causing
// a reset. The code in this package will need to change when and if that
// hardware is implemented.

package arduino

import (
    "fmt"
    "log"
    "go.bug.st/serial.v1"
	"reflect"
	"syscall"
	"time"
)

// Naming this package was hard. The functionality is fairly general and could
// be used (or easily adapted for use) with serial devices other than Arduino.
// Also, it's only being tested with a single Arduino model (a Nano) so it's
// a bit of an overreach to assert that this is for "Arduino". But in the end
// it's the answer that provides the most clarity.

// TODO generate shared code for Arduino specifics e.g. baud rate

const resetDelay = 3 * time.Second

var debug bool = false

func setDebug(setting bool) {
	debug = setting
}

// Types

type Arduino struct {
	port serial.Port
	mode *serial.Mode
	fromNano chan byte
	toNano chan byte
}

type NoResponseError time.Duration

func (nre NoResponseError) Error() string {
	return fmt.Sprintf("read from Arduino: no response after %v", time.Duration(nre))
}

type WriteWouldBlockError byte

func (wwb WriteWouldBlockError) Error() string {
	return fmt.Sprintf("write to Arduino: write 0x%02X would block", byte(wwb))
}

// Public interface

func New(deviceName string, baudRate int) (*Arduino, error) {
	var arduino Arduino
	var err error

	mode := &serial.Mode{BaudRate: baudRate, DataBits: 8, Parity: serial.NoParity, StopBits: serial.OneStopBit}
	arduino.port, err = openStandardPort(deviceName, mode)
	if err != nil {
		return nil, err
	}

	arduino.mode = mode
	arduino.fromNano = make(chan byte)
	arduino.toNano = make(chan byte)

	go arduino.reader()
	go arduino.writer()

	return &arduino, nil
}

// Read the Nano until a byte is received or a timeout occurs
func (arduino *Arduino) ReadFor(timeout time.Duration) (byte, error) {
	select {
		case b := <-arduino.fromNano:
			if debug {
				log.Printf("readFor: got 0x%X", b)
			}
			return b, nil

		case <-time.After(timeout):
			return 0, NoResponseError(timeout)
	}
	panic("ReadFrom: not reachable")
}

// Write a byte to the Arduino. Return error if the write would block.
func (arduino *Arduino) WriteTo(b byte, timeout time.Duration) error {
	select {
		case arduino.toNano <- b:
			if debug {
				log.Printf("WriteTo: sent 0x%02X", b)
			}
			return nil
		case <-time.After(timeout):
			return WriteWouldBlockError(b)
	}
	panic("WriteTo: not reachable")
}

// Close the connection to the Nano.
func (arduino *Arduino) Close() error {
	return arduino.closeSerialPort()
}

// Implementation

// Loop reading forever or until an error occurs
func (arduino *Arduino) reader() {
	for {
		b, err := arduino.blockingReadByte()
		if err != nil {
			log.Printf("Nano reader: aborting: %s (%d): %s\n", reflect.TypeOf(err), err, err.Error())
			return
		}
		arduino.fromNano <- b
	}
}

// Loop writing forever or until an error occurs
func (arduino *Arduino) writer() {
	for {
		if err := arduino.blockingWriteByte(<-arduino.toNano); err != nil {
			log.Printf("Nano writer: aborting: %s (%d): %s\n", reflect.TypeOf(err), err, err.Error())
			return
		}
	}
}

// Open the "standard" port, which resets the Arduino device
func openStandardPort(arduinoNanoDevice string, mode *serial.Mode) (serial.Port, error) {
    port, err := serial.Open(arduinoNanoDevice, mode)
    if err != nil {
        return nil, err
    }

	// Here, the time delay is important because otherwise the Nano
	// will consume the first few bytes in an attempt to see if this
	// reset is a programming device trying to flash new firmware.
    log.Println("serial port is open - delaying for Nano reset")
    time.Sleep(resetDelay)
	return port, nil
}

// Read a byte. Errors are this level are serious and mean the
// the protocol has broken down or is about to.
func (arduino *Arduino) blockingReadByte() (byte, error) {
	b := make([]byte, 1, 1)
	var n int
	var err error

	// The for-loop is -solely- to handle EINTR, which occurs constantly
	// as a result of Golang's Goroutine-level context switching mechanism.
	for {
		n, err = arduino.port.Read(b)
		// Break loop on success or error,
		// but not on EINTR.
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
	if n != 1 {
		return 0, fmt.Errorf("blocking read return 0 bytes")
	}
	if debug {
		log.Printf("blockingReadByte: return 0x%X\n", b[0])
	}
	return b[0], nil
}

// Write a byte. Errors are this level are serious and mean the
// the protocol has broken down or is about to.
func (arduino *Arduino) blockingWriteByte(toWrite byte) error {
	if debug {
		log.Printf("blockingWriteByte: write 0x%X\n", toWrite)
	}
	b := []byte { toWrite }
	var n int
	var err error

	// The for-loop is -solely- to handle EINTR, which occurs constantly
	// as a result of Golang's Goroutine-level context switching mechanism.
	for {
		n, err = arduino.port.Write(b)
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
	if n != 1 {
		return fmt.Errorf("blocking write consumed 0 bytes")
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

