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
	"path"
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
	tk := getToken(gs)
	if tk.kind() != tkString {
		return fmt.Errorf(".include: expected string, found \"%s\"", tk)
	}
	includeFile := tk.text()[1 : len(tk.text())-1]
	f, err := os.Open(path.Join(gs.includeDir, includeFile))
	if err != nil {
		return fmt.Errorf(".include: %s: %s", includeFile, err)
	}
	gs.reader.push(newNameLineByteReader(tk.tokenText, bufio.NewReader(f)))
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
	if err != nil {
		return fmt.Errorf(".opcode: name expected: %s", err)
	}
	code, err := mustGetNumber(gs)
	if err != nil {
		return fmt.Errorf(".opcode: numeric opcode expected: %s", err)
	}
	if code < 0x80 || code > 0xFF { // YARC specific
		return fmt.Errorf(".opcode: opcodes must be in 0x80..0xFF")
	}
	nargs, err := mustGetNumber(gs)
	if err != nil {
		return fmt.Errorf(".opcode: number of arguments expected")
	}
	if nargs < 0 || nargs > 8 {
		return fmt.Errorf(".opcode: only 0 to 8 arguments allowed")
	}
	var i int64
	args := make([]*token, nargs, nargs)
	for i = 0; i < nargs; i++ {
		argN, err := mustGetDefinedSymbol(gs)
		if err != nil {
			return fmt.Errorf("symbol expected: %s", err)
		}
		args[i] = argN
	}
	data := &opcode{byte(code), args}

	gs.symbols[name.text()] = newSymbol(name.text(), data,
		func(gs *globalState) error { return doOpcode(gs, name.text()) })
	gs.inOpcode = true
	gs.opcodeValue = byte(code)
	gs.wcsNext = int(gs.opcodeValue &^ 0x80) * WCS_SLOTS_PER_OPCODE
	return nil
}

// actionFunc for the .endOpcode builtin.
func actionEndOpcode(gs *globalState) error {
	if !gs.inOpcode {
		return fmt.Errorf(".endopcode: not in opcode")
	}
	gs.inOpcode = false
	gs.opcodeValue = 0
	gs.wcsNext = -1;
	return nil
}

// Action fund for a .slot builtin defining one slot in the writeable
// control store. There may be any number of bitfield = value clauses.
// If the same bitfield is mentioned more than once, the last one wins.
// The bitfield clauses are separate by spaces and terminated by a
// semicolon or an error (end of line is not relevant). All bitfields
// must have a width of 32. All strings and symbols are resolved now;
// forwards are not allowed.
func actionSlot(gs *globalState) error {
	if !gs.inOpcode || gs.opcodeValue == 0 {
		return fmt.Errorf(".slot: not in an opcode definition")
	}
	for {
		tk, err := mustGetBitfield(gs)
		if err != nil {
			return err
		}
		if tk.kind() == tkOperator && tk.text() == ";" {
			gs.wcsNext++;
			return nil
		}
		field, ok := gs.symbols[tk.text()].symbolData.([]int64)
		if !ok {
			return fmt.Errorf("%s: bitfield expected", tk.text())
		}
		if field[0] != 32 {
			return fmt.Errorf("%s: .slot bitfield width must be 32", tk.text())
		}
		equals := getToken(gs)
		if equals.kind() != tkOperator || equals.text() != "=" {
			return fmt.Errorf(".bitfield: equals expected: found %s", equals.String())
		}
		num, err := mustGetNumber(gs)
		if err != nil {
			return fmt.Errorf(".bifield: number expected: %s", err.Error())
		}
		size := field[1] - field[2] + 1
		var max int64 = (1 << size) - 1
		if num < 0 || num > max {
			return fmt.Errorf(".bitfield: %d out of range for %s", num, tk)
		}
		gs.wcs[gs.wcsNext] &^= uint32(max << field[2]);
		gs.wcs[gs.wcsNext] |= uint32((num & max) << field[2])
		//fmt.Printf("set opcode 0x%02x slot %d bit offset %d to 0x%02X value 0x%08x\n",
		//	gs.opcodeValue, gs.wcsNext, field[2], uint32(num & max), gs.wcs[gs.wcsNext])
	}
}

// Process the use of a label. Definition of a label
// is handled by the lexer.
func actionLabel(gs *globalState) error {
	fmt.Printf("actionLabel called\n")
	return nil
}

// Key symbols. Most, but not all, appear at the start of a line
var builtinSet *symbol = newSymbol(".set", nil, actionSet)
var builtinInclude *symbol = newSymbol(".include", nil, actionInclude)
var builtinBitfield *symbol = newSymbol(".bitfield", nil, actionBitfield)
var builtinOpcode *symbol = newSymbol(".opcode", nil, actionOpcode)
var builtinEndOpcode *symbol = newSymbol(".endopcode", nil, actionEndOpcode)
var builtinSlot *symbol = newSymbol(".slot", nil, actionSlot)
var builtinLabel *symbol = newSymbol(".label", nil, actionLabel)

func registerBuiltins(gs *globalState) {
	gs.symbols[builtinSet.name()] = builtinSet
	gs.symbols[builtinInclude.name()] = builtinInclude
	gs.symbols[builtinBitfield.name()] = builtinBitfield
	gs.symbols[builtinOpcode.name()] = builtinOpcode
	gs.symbols[builtinEndOpcode.name()] = builtinEndOpcode
	gs.symbols[builtinSlot.name()] = builtinSlot
	gs.symbols[builtinLabel.name()] = builtinLabel
}
