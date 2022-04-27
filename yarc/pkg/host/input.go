// Copyright (c) Jeff Berkowitz 2021. All rights reserved.

package host

// Nonblocking handler for standard input, with specific concessions
// for interactive terminal use.
// About sleeps in this code: the standard input reader imposes a
// millisecond delay if there is nothing to read. As of March 2022,
// this is the only millisecond day in the program. The delay prevents
// hammering the Nano with poll commands (10 or 20 per second is fine).

import (
	"bufio"
	"fmt"
	"io"
	"log"
	"os"
	"time"

	"golang.org/x/term"
)

type Input struct {
	channel chan string
	interactive  bool
	promptNeeded bool
}

func NewInput() *Input {
	interactive := term.IsTerminal(int(os.Stdin.Fd()))
	result := &Input {make(chan string), interactive, interactive}
	go result.reader()
	return result
}

func (input *Input) promptIfTerminal() {
	if input.promptNeeded {
		fmt.Printf("nano> ")
		input.promptNeeded = false
	}
}

// Goroutine to consume standard input and send it to a channel
// we later select upon. Handles EOF by sending a marker in-band.
// The marker cannot be duplicated by any input line because the
// marker has no terminating newline (it wouldn't matter if it
// could be duplicated, it would just be equivalent to ^D).

func (input *Input) reader() {
	reader := bufio.NewReader(os.Stdin)
	for {
		s, err := reader.ReadString('\n')
		if err != nil {
			input.channel <- "EOF"
			close(input.channel)
			if err != io.EOF {
				// Report interesting errors
				log.Printf("reading input: %v\n", err)
			}
			return
		}
		input.channel <- s
	}
}

func (input *Input) get() string {
	input.promptIfTerminal()
	select {
	case stdin := <- input.channel:
		input.promptNeeded = true
		return stdin
	case <-time.After(50 * time.Millisecond):
		return ""
	}
	return ""
}

