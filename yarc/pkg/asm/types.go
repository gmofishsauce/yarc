/*
Copyright © 2022 Jeff Berkowitz (pdxjjb@gmail.com)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

package asm

import (
	"fmt"
	"io"
	"log"
	"strings"
)

// Interface that can allow treating the two enhanced ByteReader types together
type anyNameLineByteReader interface {
	io.ByteReader
	name() string
	line() int
}

// nameLineByteReader is an enhanced io.ByteReader
// The 0 value of this type is not useful. Create with newNameLineByteReader.
// Note: the line number returned by the readers is incremented immediately after
// a newline. This is misleading if the newline results in an error, but it keeps
// the readers simple, particularly because the outer reader (below) implements
// one byte of pushback.
type nameLineByteReader struct {
	sourceName   string        // source file name or symbol name
	sourceReader io.ByteReader // the place to get the next input byte
	sourceLine   int           // current line number within sourceName
}

func newNameLineByteReader(name string, reader io.ByteReader) *nameLineByteReader {
	return &nameLineByteReader{name, reader, 1}
}

func (br *nameLineByteReader) line() int {
	return br.sourceLine
}

func (br *nameLineByteReader) name() string {
	return br.sourceName
}

func (br *nameLineByteReader) ReadByte() (byte, error) {
	result, err := br.sourceReader.ReadByte()
	if err != nil {
		return 0, err
	}
	if result == NL {
		br.sourceLine++
	}
	return result, nil
}

func (br *nameLineByteReader) String() string {
	return fmt.Sprintf("%s:%d", br.name(), br.line())
}

// stackingNameLineByteReader wraps a stack of nameLineByteReaders.
// If the stack is empty, each call to ReadByte() will return io.EOF. Otherwise, the ReadByte()
// method of the nameLineByteReader at the top of the stack is called. If the result is io.EOF,
// that ByteReader() is popped and the process repeats. EOF is  stateful: once the stack goes
// empty and io.EOF is returned, stack pushes are not allowed. So the 0 value is ready for use,
// but the first operation must be a push.
type stackingNameLineByteReader struct {
	elements      []*nameLineByteReader
	previousName  string
	previousLine  int
	previousByte  byte
	wasPushedBack bool
	atEOF         bool
}

func (sbr *stackingNameLineByteReader) push(s *nameLineByteReader) {
	if sbr.atEOF {
		log.Println("push to byte reader stack ignored after EOF")
		return
	}
	sbr.elements = append(sbr.elements, s)
}

func (sbr *stackingNameLineByteReader) pop() *nameLineByteReader {
	n := len(sbr.elements)
	if n == 0 {
		log.Panicln("internal error: pop from an empty stack")
		return nil
	}
	result := sbr.elements[n-1]
	sbr.elements = sbr.elements[:n-1]
	return result
}

// peek returns the nameLineByteReader at the top of the stack, or
// nil if the stack is empty.
func (sbr *stackingNameLineByteReader) peek() *nameLineByteReader {
	n := len(sbr.elements)
	if n == 0 {
		return nil
	}
	return sbr.elements[n-1]
}

func (sbr *stackingNameLineByteReader) name() string {
	if sbr.wasPushedBack {
		return sbr.previousName
	}
	if br := sbr.peek(); br != nil {
		return br.name()
	}
	return "EOF"
}

func (sbr *stackingNameLineByteReader) line() int {
	if sbr.wasPushedBack {
		return sbr.previousLine
	}
	if br := sbr.peek(); br != nil {
		return br.line()
	}
	return 0 // at EOF
}

XXX FIXME

/*
The unreadByte() and its corresponding functionality in ReadByte() must
be implemented on the individual elements of the stacking reader, not on
the stacking reader itself. Suppose a newline terminates a symbol and the
newline is pushed back. Then the symbol's action function causes a push.
The next read will read the pushed-back character rather then the pushed
string. The pushed data needs to be stored on in the FileLineReader in the
stack, not on the global stack.
*/

func (sbr *stackingNameLineByteReader) ReadByte() (byte, error) {
	if sbr.atEOF {
		return 0, io.EOF
	}
	if sbr.wasPushedBack {
		sbr.wasPushedBack = false
		return sbr.previousByte, nil
	}
	for {
		br := sbr.peek()
		if br == nil {
			sbr.atEOF = true
			return 0, io.EOF
		}
		result, err := br.ReadByte()
		if err == io.EOF {
			sbr.pop()
			continue
		}
		if err != nil {
			return 0, err
		}
		sbr.previousName = br.name()
		sbr.previousLine = br.line()
		sbr.previousByte = result
		return result, nil
	}
}

// Unread (push back) the most recent byte returned by the reader. Only one byte of pushback
// is allowed, and the pushback byte must be the byte that as actually read. N.B. - yes, we
// implement this without the argument byte; but it would be counterintuitive and would expose
// details of the implementation.
func (sbr *stackingNameLineByteReader) unreadByte(b byte) error {
	if sbr.wasPushedBack {
		return fmt.Errorf("at %s:%d: too many unreads", sbr.previousName, sbr.previousLine)
	}
	if b != sbr.previousByte {
		return fmt.Errorf("at %s:%d: cannot unread a different value", sbr.previousName, sbr.previousLine)
	}
	sbr.wasPushedBack = true
	if b == NL {
		sbr.previousLine--
	}
	return nil
}

// String returns a stack trace-like dump of the reader. There are newlines between
// lines, but not before the first line nor ater the last line of the backtrace.
func (sbr *stackingNameLineByteReader) String() string {
	if sbr.atEOF || sbr.peek() == nil {
		return "(end of input)"
	}
	var result strings.Builder
	for i := len(sbr.elements) - 1; i >= 0; i-- {
		nlbr := sbr.elements[i]
		result.WriteString(nlbr.String())
		if i > 0 {
			result.WriteString("; ")
		}
	}
	return result.String()
}

// Key symbols have action functions that process the rest of statement after the key symbol
// is recognized, for example creating a new symbol definition or opening an include file.
// Symbols defined during the run have action functions defined by the symbol may use the
// symbol value and act on the global state, for example pushing a byte reader that results
// in the lexer consuming the symbol's value.
type actionFunc func(gs *globalState) error

// symbol has a name, arbitrary data e.g. a value, and an action function
type symbol struct {
	symbolName   string
	symbolData   interface{}
	symbolAction actionFunc
}

func newSymbol(symbolName string, symbolData interface{}, sAction actionFunc) *symbol {
	return &symbol{symbolName, symbolData, sAction}
}

func (s *symbol) name() string {
	return s.symbolName
}

func (s *symbol) data() interface{} {
	return s.symbolData
}

// Execute the symbol's action. The action will typically (but
// not necessarily) consume from the scanState and act on it
func (s *symbol) action(gs *globalState) error {
	return s.symbolAction(gs)
}

// symbolTable is the global symbol table for an execution of yasm.
type symbolTable map[string]*symbol

// globalState is the state of the assembler. Only the lexer
// has other private state.
type globalState struct {
	reader     *stackingNameLineByteReader
	lexerState lexerStateType
	symbols    symbolTable
}

func newGlobalState(reader io.ByteReader, mainSourceFile string) *globalState {
	gs := &globalState{}
	gs.reader = new(stackingNameLineByteReader)
	gs.symbols = make(symbolTable)
	gs.reader.push(newNameLineByteReader(mainSourceFile, reader))
	registerBuiltins(gs)
	return gs
}

func (gs *globalState) String() string {
	return fmt.Sprintf("%v\n%s", gs.symbols, gs.reader)
}
