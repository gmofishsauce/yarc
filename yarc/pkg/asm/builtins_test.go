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
	data := ".set foo bar\n"
	gs := newGlobalState(strings.NewReader(data), t.Name())
	process(gs)
}

func TestBuiltins2(t *testing.T) {
	data := ".set foo \"TROUBLE\"\nfoo\n"
	gs := newGlobalState(strings.NewReader(data), t.Name())
	process(gs)
}

func TestBuiltins3(t *testing.T) {
	data := `
	.set r0 0
	.set r1 1
	.set r2 2
	.set r3 3
	.bitfield src1 8 7:6
	.bitfield src2 8 5:3
	.bitfield dst  8 2:0
	.set r0_r1_r2 "src1 = r0, src2 = r1, dst = r2;"
	`
	gs := newGlobalState(strings.NewReader(data), t.Name())
	process(gs)
	dump(gs)
}

func TestBuiltins4(t *testing.T) {
	data := `
	# This is an invalid source file
	.get r0 0
	`
	gs := newGlobalState(strings.NewReader(data), t.Name())
	process(gs)
	dump(gs)
}

func TestBuiltins5(t *testing.T) {
	data := `
	# This is an invalid source file
	.bitfield foo 32 2?1
	`
	gs := newGlobalState(strings.NewReader(data), t.Name())
	process(gs)
	dump(gs)
}

func TestBuiltins6(t *testing.T) {
	data := `
	.set r0 0
	.set r1 1
	.set r2 2
	.set r3 3
	.bitfield src1 8 7:6
	.bitfield src2 8 5:3
	.bitfield dst  8 2:0
	.opcode ADD 0x80 3 src1 src2 dst
	ADD r0, r1, r2
	`
	gs := newGlobalState(strings.NewReader(data), t.Name())
	process(gs)
	dump(gs)
}
