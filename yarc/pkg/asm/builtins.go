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
)

// action func for the .set builtin. Create a new symbol that is not a key symbol.
func actionSet(gs *globalState) error {
	name, err := mustGetNewSymbol(gs)
	if err != nil {
		return err
	}
	val, err := mustGetValue(gs)
	if err != nil {
		return err
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
	name, err := mustGetNewSymbol(gs)
	if err != nil {
		return err
	}
	// Trying the following for now: we require that symbols used
	// here not be "forward", i.e. we expand any symbols we find
	// right now, so symbols must have already been defined.
	wordsize, err := mustGetNumber(gs)
	if err != nil {
		return fmt.Errorf(".bitfield: numeric word size expected: %s", err)
	}
	upperLimit, err := mustGetNumber(gs)
	if err != nil {
		return fmt.Errorf(".bitfield: numeric upper bound expected: %s", err)
	}
	colon := getToken(gs)
	if colon.kind() != tkOperator || colon.tokenText != ":" {
		return fmt.Errorf(".bitfield: colon expected")
	}
	lowerLimit, err := mustGetNumber(gs)
	if err != nil {
		return fmt.Errorf(".bitfield: numeric lower bound expected: %s", err)
	}

	if upperLimit < lowerLimit || upperLimit-lowerLimit+1 > wordsize {
		return fmt.Errorf(".bitfield: illegal bounds")
	}
	gs.symbols[name.text()] = newSymbol(name.text(), []int64{wordsize, upperLimit, lowerLimit}, nil)
	return nil
}

type opcode struct {
	code byte
	args []*token
}

// actionFunc for the .opcode builtin. Define an opcode symbol
// .opcode symbol opcode nargs arg0 ... argNargs - 1
func actionOpcode(gs *globalState) error {
	name, err := mustGetNewSymbol(gs)
	if err != nil || name.kind() != tkSymbol {
		return fmt.Errorf(".opcode: name expected")
	}
	code, err := mustGetValue(gs)
	if err != nil || code.kind() != tkNumber {
		return fmt.Errorf(".opcode: numeric opcode expected")
	}
	c, e := strconv.ParseInt(code.text(), 0, 0)
	if e != nil {
		return e
	}
	if c < 0x80 || c > 0xFF { // YARC specific
		return fmt.Errorf(".opcode: opcodes must be in 0x80..0xFF")
	}
	nargs, err := mustGetValue(gs)
	if err != nil || nargs.kind() != tkNumber {
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

// An opcode has been recognized as a key symbol by the main loop.
// The next token(s) should be the operands, which serve as actuals
// for the formals that were defined in the .opcode builtin.
func doOpcode(gs *globalState, symOpcode string) error {
	op := *gs.symbols[symOpcode].data().(*opcode)
	var lowbyte byte
	for i := 0; i < len(op.args); i++ {
		tk := getToken(gs)
		if tk.kind() == tkSymbol || tk.kind() == tkString {
			expand(gs, gs.symbols[tk.text()].name())
			tk = getToken(gs)
		}
		if tk.kind() != tkNumber {
			return fmt.Errorf("operand must evaluate to number")
		}
		n, e := strconv.ParseInt(tk.text(), 0, 0)
		if e != nil {
			return e
		}
		pack(gs, n, &lowbyte, op.args[i])
	}
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
