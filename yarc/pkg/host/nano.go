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
	if (debug) {
		log.Println("protocol version OK")
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
	var nanoCmd []byte = []byte{sp.CmdGetVer}
	b, err := doFixedCommand(nano, nanoCmd, 1)
	if err != nil {
		return err
	}
	if b[0] != sp.ProtocolVersion {
		return fmt.Errorf("protocol version mismatch: host 0x%02X, Arduino 0x%02X\n",
			sp.ProtocolVersion, b[0])
	}
	return nil
}

func getNanoRequest(nano *arduino.Arduino) (string, error) {
	bytes, err := doCountedReceive(nano, []byte{sp.CmdPoll});
	if err != nil {
		return nostr, err
	}
	return string(bytes), nil
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

// ============ XXX DELETE =============

//func getNanoRequest(nano *arduino.Arduino) (string, error) {
//	var nanoCmd []byte = []byte{sp.CmdPoll}
//	b, err := doFixedCommand(nano, nanoCmd, 1)
//	if err != nil {
//		return nostr, err
//	}
//	if b[0] == 0 {
//		return nostr, nil
//	}
//	if debug {
//		log.Printf("reading %d byte request from Nano\n", b)
//	}
//
//	var msg string
//	msg, err = getCountedStringFromNano(nano, b[0]);
//	if err != nil {
//		return nostr, fmt.Errorf("getNanoRequest(): reading %d byte request string: %v", b, err)
//	}
//	if len(msg) == 0 {
//		return nostr, fmt.Errorf("getNanoRequest(): read 0 of %d bytes", b)
//	}
//	return msg, nil
//}

// Get a counted number of bytes from the Nano and return them as a string.
// The protocol only allows the Nano to send the characters 0x20 through 0x7F
// in the body of a string (no control characters or 8-bit characters). We
// check this and declare a serious error if we see an offending character.
//func getCountedStringFromNano(nano *arduino.Arduino, count byte) (string, error) {
//	bytes, err := getCountedBytesFromNano(nano, count)
//	if err != nil {
//		return "", fmt.Errorf("while getting string from Nano: %v", err)
//	}
//	badByte := false
//	// XXX - spec says none-ASCII bytes should be replaced with a tilde.
//	for i := range(bytes) {
//		if bytes[i] < 0x20 || bytes[i] > 0x7F {
//			log.Printf("while getting string from Nano: bad byte 0x%02X\n", bytes[i])
//			badByte = true
//		}
//	}
//
//	if badByte {
//		return "", fmt.Errorf("invalid string from Nano");
//	}
//
//	return string(bytes[:]), nil
//}

//func getCountedBytesFromNano(nano *arduino.Arduino, count byte) ([]byte, error) {
//	result := make([]byte, count, count)
//	n := int(count)
//	for i := 0; i < n; i++ {
//		b, err := nano.ReadFor(responseDelay)
//		if err != nil {
//			return nil, err
//		}
//		result[i] = b
//	}
//	return result, nil
//}

//func doCommandWithFixedArgs(nano *arduino.Arduino, cmd []byte) error {
//	if len(cmd) < 1 || len(cmd) > 8 {
//		return fmt.Errorf("invalid command length")
//	}
//	if debug {
//		log.Printf("doCommand: write 0x%X with %d argument bytes\n", cmd[0], len(cmd) - 1)
//	}
//	if err := nano.Write(cmd); err != nil {
//		// typical error here: "write would block"
//		return err
//	}
//	return getAck(nano, cmd[0])
//}

//func doCommandReturningByte(nano *arduino.Arduino, cmd byte) (byte, error) {
//    if err := doCommand(nano, cmd); err != nil {
//        return 0, err
//    }
//    b, err := nano.ReadFor(responseDelay)
//    if err != nil {
//        return 0, fmt.Errorf("no response byte after command 0x%02X %v", cmd, err)
//    }
//	return b, nil
//}

// ============ XXX DELETE =============
// Issue a command with a counted string argument
//func doCommandWithString(nano *arduino.Arduino, cmd byte, body string) error {
//	return doCommandWithCountedBytes(nano, cmd, []byte(body))
//}

// ============ XXX DELETE =============
// Issue a command with an array of byte arguments.
// Do not include the byte count in the bytes argument.
//func doCommandWithCountedBytes(nano *arduino.Arduino, cmd byte, bytes []byte) error {
//	if len(bytes) == 0 || len(bytes) > 255 {
//		return fmt.Errorf("doCommandWithCountedBytes(): invalid body length %d", len(bytes))
//	}
//	msg := make([]byte, 2 + len(bytes))
//	msg[0] = cmd
//	msg[1] = byte(len(bytes))
//	for i := range(bytes) {
//		msg[2 + i] = bytes[i]
//	}
//	nano.Write(msg)
//  return getAck(nano, cmd)
//}

// ============== END OF OBSOLETE CODE. START OF REPLACEMENT CODE =============

type UnexpectedResponseError struct {
	Command byte
	Response byte
}

func (u *UnexpectedResponseError) Error() string {
	return fmt.Sprintf("command 0x%X: unexpected response 0x%X", u.Command, u.Response)
}

// Do a command, returning error if no ack or bad ack.
// Only for commands with no arguments and no return.
func doCommand(nano *arduino.Arduino, cmd byte) error {
	_, err := doFixedCommand(nano, []byte{cmd}, 0)
	return err
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

// Do the fixed part of a command, which may be the entire command, and optionally
// return the fixed response if any.
//
// Every protocol command has a command byte and 0 or more fixed bytes specified
// by the protocol spec, the last of which may be a count. After receipt of the
// fixed bytes, the Nano is supposed to ack or nak. If the protocol specifies a
// fixed response, it immediately follows the ack. The fixed response is not sent
// if the Nano nak's the command. If an ack is seen and the command specified a
// counted number of bytes, they follow. The counted bytes are neither ack'd nor
// nak'd.
//
// This function handles the fixed part of the command, the ack or nak, and the
// fixed response if one is specified. If the Nano nak's, the returned byte slice
// is empty and the error is non-nil. If the Nano ack's and the expected argument
// is zero, the error is nil and the returned byte slice is empty. If the Nano
// ack's and the fixed response does not arrive, an error is returned and the
// state of the returned byte slice is undefined. Otherwise, the error is nil
// and the returned byte slice contains the fixed response. If the command includes
// counted bytes, the caller must then read or write them if the error return from
// this function is nil but must not if the error return is non-nil.
func doFixedCommand(nano *arduino.Arduino, fixed []byte, expected int) ([]byte, error) {
	var response []byte

    if len(fixed) < 1 || len(fixed) > 8 {
        return response, fmt.Errorf("invalid command length")
    }
	if expected < 0 || expected > 8 {
		return response, fmt.Errorf("invalid fixed response expected")
	}
    if debug {
        log.Printf("doFixedCommand: write 0x%X with %d argument bytes\n", fixed[0], len(fixed) - 1)
    }
    if err := nano.Write(fixed); err != nil {
        return response, err
    }
    if err := getAck(nano, fixed[0]); err != nil {
		return response, err
	}

	if expected > 0 {
		response = make([]byte, expected, expected)
		for i := 0; i < expected; i++ {
			b, err := nano.ReadFor(responseDelay)
			if err != nil {
				// If we encounter an error here,
				// the state of the response slice
				// is undefined as described above
				return response, err
			}
			response[i] = b
		}
	}
	return response, nil
}

// Send a counted set of bytes to the Nano. The count must be in the last
// byte of the fixed slice. The counted slice must be at least as long as
// the count. If the fixed command is ack'd, the counted bytes are just
// stuffed down the pipe. If a nak or other error occurs, it is returned.
// No counted command has fixed response so only the error is returned.
func doCountedSend(nano *arduino.Arduino, fixed []byte, counted []byte) error {
	n := len(fixed)
	if n < 2 || n > 8 {
        return fmt.Errorf("invalid command length")
	}
	// The count is in the last fixed byte
	count := fixed[n-1];
	if len(counted) < int(count) {
		return fmt.Errorf("not enough data")
	}
	if _, err := doFixedCommand(nano, fixed, 0); err != nil {
		return err
	}
    if err := nano.Write(counted[0:count]); err != nil {
        return err
    }
	return nil
}

// Issue a command to the Nano and receive a counted response. As of protocol
// v8, all commands that return a counted set of bytes have a count as the
// sole fixed response byte (even though it's often just an echo of the count
// the request asked for).
func doCountedReceive(nano *arduino.Arduino, fixed []byte) ([]byte, error) {
	expected, err := doFixedCommand(nano, fixed, 1)
	if err != nil {
		return expected, err
	}
	count := expected[0]
	if debug && count > 0 {
		log.Printf("Receive %d\n", count)
	}
	response := make([]byte, count, count)
	for i := 0; i < int(count); i++ {
		b, err := nano.ReadFor(responseDelay)
		if err != nil {
			return response, err
		}
		response[i] = b
	}
	return response, nil
}
