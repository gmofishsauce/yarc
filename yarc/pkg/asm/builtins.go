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
	"log"
	"strings"
)

// action func for the .set builtin
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
	log.Printf("create symbol %s value [%s]\n", name.text(), val.text())
	gs.symbols[name.text()] = newSymbol(name.text(), val.tokenText,
		func(gs *globalState) error { return expand(gs, name.text()) })
	log.Println("done with actionSet")
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

var builtinSet *symbol = newSymbol(".set", nil, actionSet)

func registerBuiltins(gs *globalState) {
	gs.symbols[builtinSet.name()] = builtinSet
}
