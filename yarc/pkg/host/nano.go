// Copyright (c) Jeff Berkowitz 2021, 2022. All rights reserved.

package host

// Serial port communications for Arduino Nano.

// About calls to time.Sleep(): sleeps occur only during connection
// establishment, and they are long, like 1 - 3 seconds. There are
// no millisecond delays imposed by code in this file.

import (
    "github.com/gmofishsauce/yarc/pkg/arduino"
    sp "github.com/gmofishsauce/yarc/pkg/proto"

    "fmt"
    "log"
    "time"
)

func establishConnection(nano *arduino.Arduino, wasReset bool) error {
	if wasReset {
		time.Sleep(3 * time.Second)
	} else {
		if err := drain(nano); err != nil {
			return err
		}
	}
	if err := getSyncResponse(nano); err != nil {
		return err
	}
	if err := checkProtocolVersion(nano); err != nil {
		return err
	}
	return nil
}

// The Nano is not supposed to ever send more than an ack, a byte count < 256,
// and that many data bytes in response to any command. Also, it shouldn't go
// for more than a millisecond or so (really much less) without transmitting
// if it has anything to transmit. So we read up to 257 + fudge factor bytes,
// giving it 10mS each. If it's still transmitting, return an error.
func drain(nano *arduino.Arduino) error {
	for i := 0; i < 300; i++ {
		if _, err := nano.ReadFor(responseDelay); err != nil {
			log.Println("Nano is drained")
			return nil
		}
	}
	return fmt.Errorf("Nano is transmitting continuously")
}

// Slowly send some syncs until we see one sync ack. If one is seen,
// return success after trying to consume any delayed acks.
func getSyncResponse(nano *arduino.Arduino) error {
	nSent := 0
	tries := 3

	for i := 0; i < tries; i += 1 {
		err := doCommand(nano, sp.CmdSync)
		nSent++
		if err != nil {
			log.Printf("sync command failed: %s\n", err.Error())
		} else {
			for nSent--; nSent > 0; nSent-- {
				nano.ReadFor(responseDelay)
			}
			return nil // success
		}
		time.Sleep(1 * time.Second)
	}

	return fmt.Errorf("failed to synchronize")
}

func checkProtocolVersion(nano *arduino.Arduino) error {
	b, err := doCommandReturningByte(nano, sp.CmdGetVer)
	if err != nil {
		return err
	}
	if b != sp.ProtocolVersion {
		return fmt.Errorf("protocol version mismatch: host 0x%02X, Arduino 0x%02X\n",
			sp.ProtocolVersion, b)
	}
	return nil // ok
}

func getNanoRequest(nano *arduino.Arduino) (string, error) {
	b, err := doCommandReturningByte(nano, sp.CmdPoll)
	if err != nil {
		return nostr, err
	}
	if b == 0 {
		return nostr, nil
	}
	if debug {
		log.Printf("reading %d byte request from Nano\n", b)
	}

	var msg string
	msg, err = getCountedStringFromNano(nano, b);
	if err != nil {
		return nostr, fmt.Errorf("getNanoRequest(): reading %d byte request string: %v", b, err)
	}
	if len(msg) == 0 {
		return nostr, fmt.Errorf("getNanoRequest(): read 0 of %d bytes", b)
	}
	return msg, nil
}

// There are two general kinds of requests from the Nano: "log a message" and
// "everything else". Everything else is system call requests, etc. Requests
// other than log messages have a punctuation mark in the first column, while
// log requests never do. This is all somewhat historical; originally, the Nano
// was going to return log messages only, and the system call mechanism was
// added only later.
func isLogRequest(req string) bool {
	if req[0] >= '#' && req[0] <= '&' {
		// syscall request of some type: '#', '$', '%', and '&' reserved
		return false
	}
	// anything else we just log
	return true
}

// Get a counted number of bytes from the Nano and return them as a string.
// The protocol only allows the Nano to send the characters 0x20 through 0x7F
// in the body of a string (no control characters or 8-bit characters). We
// check this and declare a serious error if we see an offending character.
func getCountedStringFromNano(nano *arduino.Arduino, count byte) (string, error) {
	bytes, err := getCountedBytesFromNano(nano, count)
	if err != nil {
		return "", fmt.Errorf("while getting string from Nano: %v", err)
	}
	badByte := false
	for i := range(bytes) {
		if bytes[i] < 0x20 || bytes[i] > 0x7F {
			log.Printf("while getting string from Nano: bad byte 0x%02X\n", bytes[i])
			badByte = true
		}
	}

	if badByte {
		return "", fmt.Errorf("invalid string from Nano");
	}

	return string(bytes[:]), nil
}

func getCountedBytesFromNano(nano *arduino.Arduino, count byte) ([]byte, error) {
	result := make([]byte, count, count)
	n := int(count)
	for i := 0; i < n; i++ {
		b, err := nano.ReadFor(responseDelay)
		if err != nil {
			return nil, err
		}
		result[i] = b
	}
	return result, nil
}

type UnexpectedResponseError struct {
	Command byte
	Response byte
}

func (u *UnexpectedResponseError) Error() string {
	return fmt.Sprintf("command 0x%X: unexpected response 0x%X", u.Command, u.Response)
}

// Do a command, returning error if no ack or bad ack.
func doCommand(nano *arduino.Arduino, cmd byte) error {
	b := []byte { cmd }
	return doCommandWithFixedArgs(nano, b)
}

func doCommandWithFixedArgs(nano *arduino.Arduino, cmd []byte) error {
	if len(cmd) < 1 || len(cmd) > 8 {
		return fmt.Errorf("invalid command length")
	}
	if debug {
		log.Printf("doCommand: write 0x%X with %d argument bytes\n", cmd[0], len(cmd) - 1)
	}
	if err := nano.Write(cmd); err != nil {
		// typical error here: "write would block"
		return err
	}
	return getAck(nano, cmd[0])
}

func getAck(nano *arduino.Arduino, cmd byte) error {
    b, err := nano.ReadFor(responseDelay)
    if err != nil {
        return err
    }
    if b != sp.Ack(cmd) {
        return &UnexpectedResponseError {cmd, b}
    }
    return nil
}

func doCommandReturningByte(nano *arduino.Arduino, cmd byte) (byte, error) {
    if err := doCommand(nano, cmd); err != nil {
        return 0, err
    }
    b, err := nano.ReadFor(responseDelay)
    if err != nil {
        return 0, fmt.Errorf("no response byte after command 0x%02X %v", cmd, err)
    }
	return b, nil
}

// Issue a command with a counted string argument
func doCommandWithString(nano *arduino.Arduino, cmd byte, body string) error {
	return doCommandWithCountedBytes(nano, cmd, []byte(body))
}

// Issue a command with an array of byte arguments.
// Do not include the byte count in the bytes argument.
// TODO FIXME - get the ack BEFORE sending the counted bytes
func doCommandWithCountedBytes(nano *arduino.Arduino, cmd byte, bytes []byte) error {
	if len(bytes) == 0 || len(bytes) > 255 {
		return fmt.Errorf("doCommandWithCountedBytes(): invalid body length %d", len(bytes))
	}
	msg := make([]byte, 2 + len(bytes))
	msg[0] = cmd
	msg[1] = byte(len(bytes))
	for i := range(bytes) {
		msg[2 + i] = bytes[i]
	}
	nano.Write(msg)
    return getAck(nano, cmd)
}

