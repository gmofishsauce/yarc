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
