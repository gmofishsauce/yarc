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
	"math"
	"strconv"
	"strings"
)

// We must get a token that is a new symbol (not a redefinition).
func mustGetNewSymbol(gs *globalState) (*token, error) {
	name := getToken(gs)
	if name.kind() != tkSymbol {
		return nil, fmt.Errorf("expected symbol, found \"%s\"", name)
	}
	if _, ok := gs.symbols[name.text()]; ok {
		return nil, fmt.Errorf("symbol may not be redefined: \"%s\"", name)
	}
	return name, nil
}

func mustGetDefinedSymbol(gs *globalState) (*token, error) {
	var name *token
	for name = getToken(gs); name.kind() != tkSymbol; name = getToken(gs) {
		if name.kind() == tkString {
			stringValue := name.text()[1 : len(name.text())-1]
			gs.reader.push(newNameLineByteReader(name.text(), strings.NewReader(stringValue)))
		} else if name.kind() != tkSymbol {
			return nil, fmt.Errorf("expected defined symbol, found %s", name.text())
		}
	}
	if _, ok := gs.symbols[name.text()]; !ok {
		return nil, fmt.Errorf("symbol must be defined: \"%s\"", name)
	}
	return name, nil
}

func mustGetBitfield(gs *globalState) (*token, error) {
	var tk *token
	for tk = getToken(gs); ; tk = getToken(gs) {
		if tk.kind() == tkSymbol {
			var sym *symbol
			var defined bool
			if sym, defined = gs.symbols[tk.text()]; !defined {
				return nil, fmt.Errorf("symbol must be defined: %s", tk.text())
			}
			if _, ok := sym.symbolData.([]int64); ok {
				return tk, nil // found a bitfield
			}
			// Found some symbol that might evaluate to a bitfield
			val := fmt.Sprintf("%s", sym.data())
			gs.reader.push(newNameLineByteReader(tk.text(), strings.NewReader(val)))
		} else if tk.kind() == tkString {
			stringValue := tk.text()[1 : len(tk.text())-1]
			gs.reader.push(newNameLineByteReader(tk.text(), strings.NewReader(stringValue)))
		} else if tk.kind() == tkOperator && tk.text() == ";" {
			return tk, nil
		} else {
			return nil, fmt.Errorf("unexpected: %s", tk.String())
		}
	}
}

// We must get a "value" token - a number or string. (If the
// token is a symbol, it is expand now (no forwards) and must
// evaluate to a number or string.)
func mustGetValue(gs *globalState) (*token, error) {
	val, err := expandToken(gs)
	if err != nil {
		return nil, err
	}
	if val.kind() == tkError {
		return nil, fmt.Errorf(val.text())
	}
	if val.kind() != tkNumber && val.kind() != tkString {
		return nil, fmt.Errorf("expected value, found \"%s\"", val)
	}
	// The value of a symbol cannot contain a dot. This prevents redefining
	// the builtin symbols as the values of other symbols.
	if strings.Contains(val.text(), ".") {
		return nil, fmt.Errorf("symbol value may not contain a dot")
	}
	return val, nil
}

func mustGetNumber(gs *globalState) (int64, error) {
	val, err := expandToken(gs)
	if err != nil {
		return 0, err
	}
	if val.kind() == tkError {
		return 0, fmt.Errorf(val.text())
	}
	if val.kind() != tkNumber {
		return 0, fmt.Errorf("expected number, found \"%s\"", val)
	}
	n, e := strconv.ParseInt(val.text(), 0, 0)
	if e != nil {
		return 0, e
	}
	return n, nil
}

// Get a token. If it is a defined symbol, expand it, right now (no forwards)
// If we encounter an undefined symbol return an error.
func expandToken(gs *globalState) (*token, error) {
	var tk *token
	for tk = getToken(gs); ; tk = getToken(gs) {
		if tk.tokenKind != tkSymbol {
			return tk, nil
		}
		err := expand(gs, tk.text())
		if err != nil {
			return nil, err
		}
	}
}

// Push the value of the named symbol on the reader stack
func expand(gs *globalState, symToExpand string) error {
	val, ok := gs.symbols[symToExpand]
	if !ok {
		return fmt.Errorf("undefined: \"%s\"", symToExpand)
	}
	s := fmt.Sprintf("%s", val.symbolData)
	gs.reader.push(newNameLineByteReader(symToExpand, strings.NewReader(s)))
	return nil
}

// An opcode has been recognized as a key symbol by the main loop.
// The next token(s) should be the operands, which serve as actuals
// for the formals that were defined in the .opcode builtin. If we
// encounter a key symbol in the argument definitions, it specifies
// a fixup to be done after lexing; we create a fixup table entry.
func doOpcode(gs *globalState, opName string) error {
	opSym := gs.symbols[opName]
	op := *opSym.data().(*opcode)
	var lowbyte byte

	for i := 0; i < len(op.args); i++ {
		arg := op.args[i]
		sym, found := gs.symbols[arg.text()]
		if found && sym.symbolAction != nil {
			// It's a .abs, .immX, etc. Grab the next token and
			// stash it in a fixup struct for later.
			t := getToken(gs)
			// The matching token for a .abs, .imm, etc., must be
			// one of few kinds. The fixup function will sort out
			// the rest of the details later, possibly with error.
			if t.tokenKind != tkSymbol && t.tokenKind != tkLabel &&
				t.tokenKind != tkNumber && t.tokenKind != tkString {
				return fmt.Errorf("unexpected: %s", t)
			}
			fxm := sym.symbolData.(fixupMaker)
			gs.fixups = append(gs.fixups, fxm(gs.memNext, opSym, t))
		} else {
			n, err := mustGetNumber(gs)
			if err != nil {
				return err
			}
			pack(gs, n, &lowbyte, arg)
		}
	}
	gs.mem[gs.memNext] = lowbyte
	gs.memNext++
	gs.mem[gs.memNext] = op.code
	gs.memNext++
	return nil
}

// Pack the value of an operand into a field
func pack(gs *globalState, operandValue int64, target *byte, arg *token) error {
	sym := gs.symbols[arg.text()]
	var elements []int64
	var ok bool
	if elements, ok = sym.data().([]int64); !ok {
		return fmt.Errorf("%s: not a bitfield", arg.text())
	}
	if len(elements) != 3 {
		return fmt.Errorf("%s: not a bitfield", arg.text())
	}
	if elements[0] != 8 {
		return fmt.Errorf("%s: not in an 8-bit field", arg.text())
	}
	size := elements[1] - elements[2] + 1
	max := int64(math.Pow(2, float64(size))) - 1
	if operandValue < 0 || operandValue > max {
		return fmt.Errorf("%s: invalid value", arg.text())
	}
	*target |= byte((operandValue & max) << elements[2])
	return nil
}

// A label use has been found. Check that the same label isn't
// previously defined. Define it as ".set <label> getMemNext(gs)"
func makeLabel(gs *globalState, label string) error {
	if _, trouble := gs.symbols[label]; trouble {
		return fmt.Errorf("label already defined: %s", label)
	}
	set := fmt.Sprintf("%d", getMemNext(gs))
	gs.symbols[label] = newSymbol(label, set,
		func(gs *globalState) error { return expand(gs, label) })
	return nil
}
