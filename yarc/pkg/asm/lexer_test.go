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
	"strings"
	"testing"

	"github.com/stretchr/testify/assert"
)

func TestLexer1(t *testing.T) {
	data := ".symbol\n"
	gs := newGlobalState(strings.NewReader(data), t.Name())
	tk := getToken(gs)
	assert.Equal(t, tkSymbol, tk.tokenKind)
	assert.Equal(t, data[:len(data)-1], tk.tokenText)
}

func TestLexer2(t *testing.T) {
	data := ".sym\"bol\n"
	gs := newGlobalState(strings.NewReader(data), t.Name())
	tk := getToken(gs)
	assert.Equal(t, tkError, tk.tokenKind)
	assert.Equal(t, "character 0x22 (\") unexpected", tk.tokenText)
}

func TestLexer3(t *testing.T) {
	data := ".aSymbol \"and a string\"\n"
	gs := newGlobalState(strings.NewReader(data), t.Name())
	tk := getToken(gs)
	assert.Equal(t, tkSymbol, tk.tokenKind)
	assert.Equal(t, ".aSymbol", tk.tokenText)
	tk = getToken(gs)
	assert.Equal(t, tkString, tk.tokenKind)
	assert.Equal(t, "and a string", tk.tokenText)
}

func TestLexer4(t *testing.T) {
	data := "# .symbol\n"
	gs := newGlobalState(strings.NewReader(data), t.Name())
	tk := getToken(gs)
	assert.Equal(t, tkNewline, tk.tokenKind)
}

func TestLexer5(t *testing.T) {
	data := "10\n0x10\n0X3F\n"
	gs := newGlobalState(strings.NewReader(data), t.Name())

	tk := getToken(gs)
	assert.Equal(t, tkNumber, tk.tokenKind)
	assert.Equal(t, "10", tk.text())
	tk = getToken(gs)
	assert.Equal(t, tkNewline, tk.tokenKind)

	tk = getToken(gs)
	assert.Equal(t, tkNumber, tk.tokenKind)
	assert.Equal(t, "0x10", tk.text())
	tk = getToken(gs)
	assert.Equal(t, tkNewline, tk.tokenKind)

	tk = getToken(gs)
	assert.Equal(t, tkNumber, tk.tokenKind)
	assert.Equal(t, "0X3F", tk.text())
	tk = getToken(gs)
	assert.Equal(t, tkNewline, tk.tokenKind)
}

func TestLexer6(t *testing.T) {
	data := "1x0\n0xxxx10\n3F\n"
	gs := newGlobalState(strings.NewReader(data), t.Name())

	tk := getToken(gs)
	assert.Equal(t, tkError, tk.tokenKind)
	tk = getToken(gs)
	assert.Equal(t, tkNewline, tk.tokenKind)

	tk = getToken(gs)
	assert.Equal(t, tkError, tk.tokenKind)
	tk = getToken(gs)
	assert.Equal(t, tkNewline, tk.tokenKind)

	tk = getToken(gs)
	assert.Equal(t, tkError, tk.tokenKind)
	tk = getToken(gs)
	assert.Equal(t, tkNewline, tk.tokenKind)
}
