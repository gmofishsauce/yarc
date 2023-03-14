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

// The ALU chips for YARC are 8k x 8 static RAMs, the same component
// used for main memory and the microcode RAMs. A lookup table can
// simulate any boolean function. The RAMs have 13 address lines since
// 2^13 = 8k. This allows four address lines for the "A" operand, four
// lines for the "B" operand, 1 for the carry input, and four to specify
// an ALU operation. There are four bits of result with four bits of
// flags: C, Z, N, and V (overflow), for a total of 8 bits out (a byte).
//
// Of course, we need to load these RAMs with 8k of precomputed results
// for the different ALU operations. The ALU operations are defined by
// the microcode and repeated as the constants below. They form the high
// order four bits of the ALU RAM address.

package asm

// ALU operations (upper four bits of address)
const alu_add  = 0x00
const alu_sub  = 0x01
const alu_rsub = 0x02
const alu_adc  = 0x03
const alu_sbb  = 0x04
const alu_rsbb = 0x05
const alu_nand = 0x06
const alu_or   = 0x07
const alu_xor  = 0x08
const alu_not  = 0x09
const alu_neg  = 0x0A
const alu_rot  = 0x0B // four operations determined by "B" operand
const alu_0C   = 0x0C // unassigned
const alu_0D   = 0x0D // unassigned
const alu_0E   = 0x0E // unassigned
const alu_0F   = 0x0F // unassigned

type aluFunc func(gs *globalState)

var alu_ops = [16]aluFunc{do_add, do_sub,  do_rsub, do_adc,
                        do_sbb, do_rsbb, do_nand, do_or,
					    do_xor, do_not,  do_neg,  do_rot,
					    do_0C,  do_0D,   do_0E,   do_0F}

// Generate the entire 8k byte content of one RAM ALU chip and leave
// the result in the alu array in the global state.
func generateALUcontent(gs *globalState) {
	for _, f := range alu_ops {
		f(gs)
	}
}

func do_add(gs *globalState) {
	for a1 := 0; a1 < 16; a1++ {
		for a2 := 0; a2 < 16; a2++ {
			sum := a1 + a2
			result := sum & 0xF
			// Carry
			result |= sum & 0x10
			// Zero
			if z := sum & 0x0F; z == 0 {
				result |= 0x20
			}
			// Negative
			if n := sum & 0x08; n != 0 {
				result |= 0x40
			}
			// Overflow: both a1 and a2 are positive and the sum is negative,
			// or both a1 and a2  are negative and the sum is positive.
			// Or: the arguments have the same sign, and the sign of the
			// result is different. Remember, the arguments are four bits.
			overflow := (a1 & 0x08) == (a2 & 0x08) && (sum & 0x08) != (a1 & 0x08)
			if overflow {
				result |= 0x80
			}

			// Ignore carry in (see ADC) - set both possibilities the same
			offset := a1 | (a2 << 4) | (0 << 8) | (alu_add << 9)
			gs.alu[offset] = byte(result)

			offset = a1 | (a2 << 4) | (1 << 8) | (alu_add << 9)
			gs.alu[offset] = byte(result)
		}
	}
}

func do_sub(gs *globalState) {
}

func do_rsub(gs *globalState) {
}

func do_adc(gs *globalState) {
}

func do_sbb(gs *globalState) {
}

func do_rsbb(gs *globalState) {
}

func do_nand(gs *globalState) {
}

func do_or(gs *globalState) {
}

func do_xor(gs *globalState) {
}

func do_not(gs *globalState) {
}

func do_neg(gs *globalState) {
}

func do_rot(gs *globalState) {
}

func do_0C(gs *globalState) {
}

func do_0D(gs *globalState) {
}

func do_0E(gs *globalState) {
}

func do_0F(gs *globalState) {
}
