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
	"io"
	"log"
)

type scanState struct {
	sourceName   string        // source file name or symbol name
	sourceReader io.ByteReader // the place to get the next input byte
	sourceLine   int           // current line number within sourceName
}

func newScanState(name string, reader io.ByteReader, line int) *scanState {
	log.Printf("in newScanState(%v, %v, %v)\n", name, "reader", line)
	return &scanState{name, reader, line}
}

func (ss *scanState) line() int {
	return ss.sourceLine
}

func (ss *scanState) name() string {
	return ss.sourceName
}

// Maybe just expose a byte getter, not the raw reader,
// and the byte getter would keep line and name in sync,
// return an EOF token when the last reader at the base
// of the stack goes empty, etc. TODO FIXME

func (ss *scanState) reader() io.ByteReader {
	return ss.sourceReader
}

// ------------------
// Stack of scanState
// ------------------

type scanStateStack struct {
	elements []*scanState
}

func (ss scanStateStack) push(s *scanState) {
	log.Printf("in push(%v)\n", s)
	ss.elements = append(ss.elements, s)
	log.Printf("after push [0] %v, cap %d, len %d\n", ss.elements[0], cap(ss.elements), len(ss.elements))
}

func (ss scanStateStack) pop() *scanState {
	n := len(ss.elements)
	if n == 0 {
		log.Println("internal error: pop from an empty stack")
		return nil
	}
	result := ss.elements[n-1]
	ss.elements = ss.elements[:n-1]
	return result
}

func (ss scanStateStack) peek() *scanState {
	n := len(ss.elements)
	if n == 0 {
		log.Println("internal error: peek at an empty stack")
		return nil
	}
	return ss.elements[n-1]
}

// -------
// Symbols
// -------

type actionFunc func(gs *globalState)

type symbol struct {
	sName   string
	sData   interface{}
	sAction actionFunc
}

func newSymbol(sName string, sData interface{}, sAction actionFunc) *symbol {
	return &symbol{sName, sData, sAction}
}

func (s *symbol) name() string {
	return s.sName
}

func (s *symbol) data() interface{} {
	return s.sData
}

func (s *symbol) action() actionFunc {
	return s.sAction
}

// ------------
// Symbol table
// ------------

type symbolTable map[string]*symbol

func newSymbolTable() symbolTable {
	st := make(symbolTable)
	return st
}

// -----------------------
// State of the assembler.
// -----------------------

type globalState struct {
	scanState scanStateStack
	symbols   symbolTable
}

func newGlobalState(reader io.ByteReader, mainSourceFile string) *globalState {
	gs := &globalState{}
	gs.scanState.push(newScanState(mainSourceFile, reader, 1))
	log.Printf("Back in newGlobalState %v %d %d\n", gs.scanState.elements[0], cap(gs.scanState.elements), len(gs.scanState.elements))
	gs.symbols = newSymbolTable()
	registerBuiltins(gs)
	return gs
}
