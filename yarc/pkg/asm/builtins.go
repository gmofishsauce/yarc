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
	"bufio"
	"fmt"
	"os"
	"strconv"
	"strings"
)

// action func for the .set builtin. Create a new symbol that is not a key symbol.
func actionSet(gs *globalState) error {
	name := getToken(gs)
	if name.kind() != tkSymbol {
		return fmt.Errorf(".set: expected symbol, found \"%s\"", name)
	}
	val := getToken(gs)
	if val.kind() != tkNumber && val.kind() != tkString && val.kind() != tkSymbol {
		return fmt.Errorf(".set: expected value, found \"%s\"", val)
	}
	// The value of a symbol cannot contain a dot. This prevents redefining
	// the builtin symbols as the values of other symbols. Also, symbols cannot
	// be redefined.
	if strings.Contains(val.text(), ".") {
		return fmt.Errorf(".set: value may not contain a dot")
	}
	if _, ok := gs.symbols[name.text()]; ok {
		return fmt.Errorf(".set: symbol may not be redefined: \"%s\"", name)
	}
	gs.symbols[name.text()] = newSymbol(name.text(), val.tokenText,
		func(gs *globalState) error { return expand(gs, name.text()) })
	return nil
}

// action func for the .include builtin. Push a reader for the file.
func actionInclude(gs *globalState) error {
	val := getToken(gs)
	if val.kind() != tkString {
		return fmt.Errorf(".set: expected string, found \"%s\"", val)
	}
	f, err := os.Open(val.text())
	if err != nil {
		return fmt.Errorf(".include: %s: %s", val.text(), err)
	}
	gs.reader.push(newNameLineByteReader(val.tokenText, bufio.NewReader(f)))
	return nil
}

// action func for the .bitfield builtin. Define a bitfield.
func actionBitfield(gs *globalState) error {
	name := getToken(gs)
	if name.kind() != tkSymbol {
		return fmt.Errorf(".bitfield: name expected")
	}
	if _, ok := gs.symbols[name.text()]; ok {
		return fmt.Errorf(".bitfield: symbol may not be redefined: \"%s\"", name)
	}
	wordsize := getToken(gs)
	if wordsize.kind() != tkNumber {
		return fmt.Errorf(".bitfield: numeric word size expected")
	}
	upperLimit := getToken(gs)
	if upperLimit.kind() != tkNumber {
		return fmt.Errorf(".bitfield: numeric upper bound expected")
	}
	colon := getToken(gs)
	if colon.kind() != tkOperator || colon.tokenText != ":" {
		return fmt.Errorf(".bitfield: colon expected")
	}
	lowerLimit := getToken(gs)
	if lowerLimit.kind() != tkNumber {
		return fmt.Errorf(".bitfield: numeric lower bound expected")
	}

	w, e := strconv.ParseInt(wordsize.text(), 0, 0)
	if e != nil {
		return e
	}
	u, e := strconv.ParseInt(upperLimit.text(), 0, 0)
	if e != nil {
		return e
	}
	l, e := strconv.ParseInt(lowerLimit.text(), 0, 0)
	if e != nil {
		return e
	}
	if u < l || u-l+1 > w {
		return fmt.Errorf(".bitfield: illegal bounds")
	}
	gs.symbols[name.text()] = newSymbol(name.text(), []int64{w, u, l}, nil)
	return nil
}

type opcode struct {
	code byte
	args []*token
}

// actionFunc for the .opcode builtin. Define an opcode symbol
// .opcode symbol opcode nargs arg0 ... argNargs - 1
func actionOpcode(gs *globalState) error {
	name := getToken(gs)
	if name.kind() != tkSymbol {
		return fmt.Errorf(".opcode: name expected")
	}
	code := getToken(gs)
	if code.kind() != tkNumber {
		return fmt.Errorf(".opcode: numeric opcode expected")
	}
	c, e := strconv.ParseInt(code.text(), 0, 0)
	if e != nil {
		return e
	}
	if c < 0x80 || c > 0xFF { // YARC specific
		return fmt.Errorf(".opcode: opcodes must be in 0x80..0xFF")
	}
	nargs := getToken(gs)
	if nargs.kind() != tkNumber {
		return fmt.Errorf(".opcode: number of arguments expected")
	}
	n, e := strconv.ParseInt(nargs.text(), 0, 0)
	if e != nil {
		return e
	}
	if n < 0 || n > 8 {
		return fmt.Errorf(".opcode: only 0 to 8 arguments allowed")
	}
	var i int64
	args := make([]*token, n, n)
	for i = 0; i < n; i++ {
		argN := getToken(gs)
		if argN.kind() != tkSymbol {
			return fmt.Errorf("argument symbol expected")
		}
		args[i] = argN
	}
	data := &opcode{byte(c), args}

	gs.symbols[name.text()] = newSymbol(name.text(), data,
		func(gs *globalState) error { return doOpcode(gs, name.text()) })
	return nil
}

// Push the value of the named symbol on the reader stack
func expand(gs *globalState, symToExpand string) error {
	val, ok := gs.symbols[symToExpand]
	if !ok {
		return fmt.Errorf("internal error: unable to expand symbol \"%s\"", symToExpand)
	}
	s := fmt.Sprintf("%s", val.symbolData)
	gs.reader.push(newNameLineByteReader(symToExpand, strings.NewReader(s)))
	return nil
}

func doOpcode(gs *globalState, symOpcode string) error {
	// XXX FIXME
	return nil
}

var builtinSet *symbol = newSymbol(".set", nil, actionSet)
var builtinInclude *symbol = newSymbol(".include", nil, actionInclude)
var builtinBitfield *symbol = newSymbol(".bitfield", nil, actionBitfield)
var builtinOpcode *symbol = newSymbol(".opcode", nil, actionOpcode)

func registerBuiltins(gs *globalState) {
	gs.symbols[builtinSet.name()] = builtinSet
	gs.symbols[builtinInclude.name()] = builtinInclude
	gs.symbols[builtinBitfield.name()] = builtinBitfield
	gs.symbols[builtinOpcode.name()] = builtinOpcode
}
