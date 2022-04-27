// Copyright (c) Jeff Berkowitz 2021, 2022. All rights reserved.

package host

// Serial port communications for Arduino Nano.

// About calls to time.Sleep(): sleeps occur only during connection
// establishment, and they are long, like 1 - 3 seconds. There are
// no millisecond delays imposed by code in this file.

import (
    "yarc/arduino"
    sp "yarc/proto"

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
	b, err := doCommandWithByte(nano, sp.CmdGetVer)
	if err != nil {
		return err
	}
	if b != sp.ProtocolVersion {
		return fmt.Errorf("protocol version mismatch: host 0x%02X, Arduino 0x%02X\n",
			sp.ProtocolVersion, b)
	}
	return nil // ok
}

// TODO - this has to become pollNano, which can return either logs
// or a service request from YARC.

func getNanoLogs(nano *arduino.Arduino, nanoLog *log.Logger) error {
	b, err := doCommandWithByte(nano, sp.CmdPoll)
	if err != nil {
		return err
	}
	if debug {
		log.Printf("reading %d byte log message\n", b)
	}
	var msg string
	msg, err = getCountedStringFromNano(nano, b);
	if err != nil {
		return fmt.Errorf("reading string after GET_MSG: %v", err)
	}
	if len(msg) == 0 {
		return nil
	}

	nanoLog.Printf(msg)
	return nil
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
	if debug {
		log.Printf("doCommand: write 0x%X\n", cmd)
	}
	if err := nano.WriteTo(cmd, commandDelay); err != nil {
		// typical error here: "write would block"
		return err
	}

	b, err := nano.ReadFor(responseDelay)
	if err != nil {
		return err
	}
	if b != sp.Ack(cmd) {
		return &UnexpectedResponseError {cmd, b}
	}
	return nil
}

func doCommandWithByte(nano *arduino.Arduino, cmd byte) (byte, error) {
    if err := doCommand(nano, cmd); err != nil {
        return 0, err
    }
    b, err := nano.ReadFor(responseDelay)
    if err != nil {
        return 0, fmt.Errorf("no response byte after command 0x%02X %v", cmd, err)
    }
	return b, nil
}
