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
)

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
	tkEND
)

var kindToString = []string{
	"error",
	"symbol",
	"string",
	"int",
	"comma",
	"equals",
	"semicolon",
	"END",
}

// Lexer states
const (
	stBetween = iota
	stInSeq
	stError
	stEnd
)

var lexerState = stBetween

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
	for b, err := gs.reader.ReadByte(); ; b, err = gs.reader.ReadByte() {
		if err == io.EOF {
			lexerState = stEnd
			return &token{"EOF", tkEND}
		}
		if err != nil {
			lexerState = stError
			return &token{err.Error(), tkErr}
		}
		switch lexerState {
		default:
			log.Fatalf("internal error: lexerState %d input 0x%2X\n", lexerState, b)
			return &token{"internal error", tkErr}
		case stBetween:
			if isWhiteSpace(b) {
				continue
			}
			if isInitialSymbol(b) {
				lexerState = stInSeq

			}
			return &token{".set", tkSym}
		case stInSeq:
			return &token{"value", tkStr}
		}
	}
}

func isWhiteSpace(b byte) bool {
	return false // STUB
}

func isInitialSymbol(b byte) bool {
	return false // STUB
}

// const (
// 	chLetter = 1 << iota
// 	chDigit
// 	chSymbol
// 	chInitialSymbol
// 	stWhitespace
// )

// var characterizer = [128]int{
// 	1, 2, 3, 4, 5, 6, 7, 8,
// 	1, 2, 3, 4, 5, 6, 7, 8,
// 	1, 2, 3, 4, 5, 6, 7, 8,
// 	1, 2, 3, 4, 5, 6, 7, 8,
// 	1, 2, 3, 4, 5, 6, 7, 8,
// 	1, 2, 3, 4, 5, 6, 7, 8,
// 	1, 2, 3, 4, 5, 6, 7, 8,
// 	1, 2, 3, 4, 5, 6, 7, 8,
// 	1, 2, 3, 4, 5, 6, 7, 8,
// 	1, 2, 3, 4, 5, 6, 7, 8,
// 	1, 2, 3, 4, 5, 6, 7, 8,
// 	1, 2, 3, 4, 5, 6, 7, 8,
// 	1, 2, 3, 4, 5, 6, 7, 8,
// 	1, 2, 3, 4, 5, 6, 7, 8,
// 	1, 2, 3, 4, 5, 6, 7, 8,
// 	1, 2, 3, 4, 5, 6, 7, 8,
// }
