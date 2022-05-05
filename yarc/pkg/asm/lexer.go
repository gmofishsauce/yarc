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
	//"log"
)

type tokenKind int

// Token kinds
const (
	ttErr = iota
	ttSym
	ttStr
	ttInt
)


// Lexer states
const (
	stStart = iota
)

type token struct {
	t string
	k tokenKind
}

func (t *token) text() string {
	return t.t
}

func (t *token) kind() tokenKind {
	return t.k
}

func getToken(gs *globalState) *token {
	r := gs.scanState.peek().reader()
	for b, err := r.ReadByte(); err == nil; b, err = r.ReadByte() {
		if err != nil {
			return &token{err.Error(), ttErr}
		}
		if b >= 0x80 {
			return &token{"illegal character", ttErr}
		}
	}
	return &token{".set", ttSym}
}


