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
	data := ".set bar 7\n.set foo bar\n"
	gs := newGlobalState(strings.NewReader(data), t.Name())
	if process(gs) > 0 {
		t.Fail()
	}
}

func TestBuiltins1Fail(t *testing.T) {
	data := ".set foo bar\n" // fail because bar doesn't expand
	gs := newGlobalState(strings.NewReader(data), t.Name())
	if process(gs) != 1 {
		t.Fail()
	}
}

func TestBuiltins2(t *testing.T) {
	data := ".set bar \"hello\"\n"
	gs := newGlobalState(strings.NewReader(data), t.Name())
	if process(gs) > 0 {
		t.Fail()
	}
}

func TestBuiltins2Fail(t *testing.T) {
	data := ".set foo bar\nbar\n"
	gs := newGlobalState(strings.NewReader(data), t.Name())
	if process(gs) != 2 {
		// line 1 bar undefined, line 2 key symbol expected
		t.Fail()
	}
}

func TestBuiltins3(t *testing.T) {
	data := ".set bar \"TROUBLE\"\n.set foo bar\n"
	gs := newGlobalState(strings.NewReader(data), t.Name())
	if process(gs) > 0 {
		t.Fail()
	}
}

func TestBuiltins3Fail(t *testing.T) {
	data := ".set foo \"TROUBLE\"\nfoo\n"
	gs := newGlobalState(strings.NewReader(data), t.Name())
	if process(gs) != 1 {
		t.Fail()
	}
}

func TestBuiltins4(t *testing.T) {
	data := ".set bar 8\n.set foo bar\n.bitfield fake bar 7:0\n"
	gs := newGlobalState(strings.NewReader(data), t.Name())
	if process(gs) > 0 {
		t.Fail()
	}
}

func TestBuiltins4Fail(t *testing.T) {
	data := ".set foo bar\n.set bar 8\n.bitfield fake foo 7:0\n"
	gs := newGlobalState(strings.NewReader(data), t.Name())
	if process(gs) != 2 {
		t.Fail()
	}
}

func TestBuiltins4Extra(t *testing.T) {
	data := ".set bar 8\n.set foo bar\n.bitfield fake foo 7:0\n"
	gs := newGlobalState(strings.NewReader(data), t.Name())
	if process(gs) > 0 {
		t.Fail()
	}
}

func TestBuiltins5(t *testing.T) {
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
	if process(gs) > 0 {
		t.Fail()
	}
}

func TestBuiltins6Fail(t *testing.T) {
	data := `
	# This is an invalid source file
	.get r0 0
	`
	gs := newGlobalState(strings.NewReader(data), t.Name())
	if process(gs) != 1 {
		t.Fail()
	}
}

func TestBuiltins7Fail(t *testing.T) {
	data := `
	# This is an invalid source file
	.bitfield foo 32 2?1
	`
	gs := newGlobalState(strings.NewReader(data), t.Name())
	if process(gs) != 1 {
		t.Fail()
	}
}

func TestBuiltins8(t *testing.T) {
	data := `
	.set r0 0
	.set r1 1
	.set r2 2
	.set r3 3
	.bitfield src1 8 7:6
	.bitfield src2 8 5:3
	.bitfield dst  8 2:0
	.opcode ADD 0x80 3 src1 src2 dst
	ADD r0 r1 r2
	`
	gs := newGlobalState(strings.NewReader(data), t.Name())
	if process(gs) > 0 {
		t.Fail()
	}
	//dump(gs)
}

func TestBuiltins8Fail(t *testing.T) {
	data := `
	.set r0 0
	.set r1 1
	.set r2 2
	.set r3 3
	.bitfield src1 8 7:6
	.bitfield src2 8 5:3
	.bitfield dst  8 2:0
	.opcode ADD 0x80 3 src1 src2 dst
	ADD "r0 r1 r2"
	`
	gs := newGlobalState(strings.NewReader(data), t.Name())
	if process(gs) != 1 {
		t.Fail()
	}
	//dump(gs)
}

func TestBuiltins9(t *testing.T) {
	data := `
	.set r0 0
	.set r1 1
	.set r2 2
	.set r3 3
	.bitfield src1 8 7:6
	.bitfield src2 8 5:3
	.bitfield dst  8 2:0
	.bitfield rsw 32 7:0
	.opcode ADD 0x80 3 src1 src2 dst
	.slot rsw = 0x77;
	.endopcode
	ADD r0 r1 r2
	`
	gs := newGlobalState(strings.NewReader(data), t.Name())
	if process(gs) != 0 {
		t.Fail()
	}
	//dump(gs)
}

func TestBuiltins10(t *testing.T) {
	data := `
	.bitfield src1  32  7:6
	.bitfield src2  32  5:3
	.bitfield dst   32  2:0
	.opcode ADD 0x80 3 src1 src2 dst
	`
	gs := newGlobalState(strings.NewReader(data), t.Name())
	if process(gs) != 0 {
		t.Fail()
	}
}

func TestBuiltins10Fail(t *testing.T) {
	data := `
	.bitfield src1  32  7:6
	.bitfield src2  32  5:3
	.bitfield dst   32  2:0
	.opcode ADD 0x80 src1 src2 dst # nargs is missing
	`
	gs := newGlobalState(strings.NewReader(data), t.Name())
	if process(gs) != 1 {
		t.Fail()
	}
}

func TestBuiltins11(t *testing.T) {
	data := `
	.set r0 0
	.set r1 1
	.set r2 2
	.set r3 3
	.bitfield src1 8 7:6
	.bitfield src2 8 5:3
	.bitfield dst  8 2:0
	.bitfield rsw 32 7:0
	.set NOOP "rsw = 0x77"
	.opcode ADD 0x80 3 src1 src2 dst
	.slot NOOP;
	.endopcode
	ADD r0 r1 r2
	`
	gs := newGlobalState(strings.NewReader(data), t.Name())
	if process(gs) != 0 {
		t.Fail()
	}
}
