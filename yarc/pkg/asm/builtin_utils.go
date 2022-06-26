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
// The symbol is not expanded, so forwards are (accidentally?) allowed.
func mustGetNewSymbol(gs *globalState) (*token, error) {
	name := getToken(gs)
	if name.kind() != tkSymbol {
		return nil, fmt.Errorf(".set: expected symbol, found \"%s\"", name)
	}
	if _, ok := gs.symbols[name.text()]; ok {
		return nil, fmt.Errorf(".set: symbol may not be redefined: \"%s\"", name)
	}
	return name, nil
}

// We must get a "value" token - symbol, number, or string. If
// the token is a symbol, it is expand now (no forwards) and must
// evaluate to a number or string.
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
