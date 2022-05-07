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

import "fmt"

//"log"

// Token kinds
const (
	tkErr = iota
	tkSym
	tkStr
	tkInt
	tkComma
	tkEqu
	tkSemi
)

var kindToString = []string{
	"error",
	"symbol",
	"string",
	"int",
	"comma",
	"equals",
	"semicolon",
}

// Lexer states
const (
	stStart = iota
)

type token struct {
	tokenText string
	tokenKind int
}

func (t *token) String() string {
	return fmt.Sprintf("{%s %s}", kindToString[t.tokenKind], t.tokenText)
}

func (t *token) text() string {
	return t.tokenText
}

func (t *token) kind() int {
	return t.tokenKind
}

func getToken(gs *globalState) *token {
	r := gs.scanState.peek().reader()
	for b, err := r.ReadByte(); err == nil; b, err = r.ReadByte() {
		if err != nil {
			return &token{err.Error(), tkErr}
		}
		if b >= 0x80 {
			return &token{"illegal character", tkErr}
		}
	}
	return &token{".set", tkSym}
}
