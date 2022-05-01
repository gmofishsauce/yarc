// Copyright © 2022 Jeff Berkowitz (pdxjjb@gmail.com)

package asm

import (
	"io"
	"log"
)

type scanState struct {
	sourceName string			// source file name or symbol name
	sourceReader io.ByteReader	// the place to get the next input byte
	sourceLine int				// current line number within sourceName
}

func newScanState(name string, reader io.ByteReader, line int) *scanState {
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
	elements [] *scanState
}

func (ss scanStateStack) push(s *scanState) {
	ss.elements = append(ss.elements, s)
}

func (ss scanStateStack) pop() (*scanState) {
	n := len(ss.elements)
	if n == 0 {
		log.Fatal("internal error: pop from an empty stack")
	}
	result := ss.elements[n-1]
	ss.elements = ss.elements[:n-1]
	return result
}

func (ss scanStateStack) peek() (*scanState) {
	n := len(ss.elements)
	if n == 0 {
		log.Fatal("internal error: peek at an empty stack")
	}
	return ss.elements[n-1]
}

// -------
// Symbols
// -------

type actionFunc func(ss *scanState)

type symbol struct {
	sName	string
	sData	interface{}
	sAction	actionFunc
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

type symbolTable map[string] *symbol

func newSymbolTable() symbolTable {
	st := make(symbolTable)
	return st
}

// -------------------------------------------------------------
// State of the assembler. The lexer has its own internal state.
// -------------------------------------------------------------

type globalState struct {
	scanState	scanStateStack
	symbols		symbolTable
}

func newGlobalState(reader io.ByteReader, mainSourceFile string) (*globalState) {
	var result globalState
	result.scanState.push(newScanState(mainSourceFile, reader, 1))
	result.symbols = newSymbolTable()
	return &result
}

