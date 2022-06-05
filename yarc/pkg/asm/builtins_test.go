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
)

func TestBuiltins1(t *testing.T) {
	reinitLexer()
	data := ".set foo bar\n"
	gs := newGlobalState(strings.NewReader(data), t.Name())
	registerBuiltins(gs)
	process(gs)
}

func TestBuiltins2(t *testing.T) {
	reinitLexer()
	data := ".set foo \"bar in the big blue house\"\nfoo\n"
	gs := newGlobalState(strings.NewReader(data), t.Name())
	registerBuiltins(gs)
	process(gs)
}
