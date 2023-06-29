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
const alu_add = 0x00
const alu_sub = 0x01
const alu_rsub = 0x02
const alu_adc = 0x03
const alu_sbb = 0x04
const alu_rsbb = 0x05
const alu_nand = 0x06
const alu_or = 0x07
const alu_xor = 0x08
const alu_not = 0x09
const alu_neg = 0x0A
const alu_rot = 0x0B // four operations determined by "B" operand
const alu_0C = 0x0C  // unassigned
const alu_0D = 0x0D  // unassigned
const alu_0E = 0x0E  // unassigned
const alu_pass = 0x0F

const CarryFlag = 0x10
const ZeroFlag = 0x20
const NegativeFlag = 0x40
const OverflowFlag = 0x80

type aluFunc func(gs *globalState)

var alu_ops = [16]aluFunc{do_add, do_sub, do_rsub, do_adc,
	do_sbb, do_rsbb, do_nand, do_or,
	do_xor, do_not, do_neg, do_rot,
	do_0C, do_0D, do_0E, do_pass}

func sanity(result int) {
	if result & 0xFF != result {
		panic("result not a byte")
	}
}

// Generate the entire 8k byte content of one RAM ALU chip and leave
// the result in the alu array in the global state.
func generateALUcontent(gs *globalState) {
	for _, f := range alu_ops {
		f(gs)
	}
}

func do_add(gs *globalState) {
	for carry_in := 0; carry_in < 2; carry_in++ {
		for a_bus := 0; a_bus < 16; a_bus++ {
			for b_bus := 0; b_bus < 16; b_bus++ {
				result := a_bus + b_bus + carry_in

				// Carry - the carry flag is positioned as the next bit
				// after a four bit add, so it sets itself in result.

				// Zero
				if z := result & 0x0F; z == 0 {
					result |= ZeroFlag
				}
				// Negative
				if n := result & 0x08; n != 0 {
					result |= NegativeFlag
				}
				// Overflow: both a_bus and b_bus are positive and the sum is negative,
				// or both a_bus and b_bus  are negative and the sum is positive.
				// Or: the arguments have the same sign, and the sign of the
				// result is different. Remember, the arguments are four bits.
				if ov := (a_bus&0x08) == (b_bus&0x08) && (result&0x08) != (a_bus&0x08); ov {
					result |= OverflowFlag
				}

				sanity(result)
				offset := (alu_add << 9) | (carry_in << 8) | (b_bus << 4) | (a_bus)
				gs.alu[offset] = byte(result)
			}
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

// Generate the PASS function f(a_bus, b_bus) = b_bus
// An immediate byte has to be sign-extended which requires moving it
// through ALU port 2 rather than just moving it from the IR to register.
func do_pass(gs *globalState) {
	for a_bus := 0; a_bus < 16; a_bus++ {
		for b_bus := 0; b_bus < 16; b_bus++ {
			// Set the result to be b_bus - somewhat weirdly, it's the
			// inner index; maybe I should change this.
			// Set the zero flag for zero bits so that the downloader
			// won't see a bunch of 0 bytes that it might ignore; the
			// flags aren't set by the microcode for alu_pass.
			// Ignore carry in - set both possibilities the same
			result := b_bus
			if result == 0 {
				result |= ZeroFlag
			}
			
			sanity(result)
			offset := (alu_pass << 9) | (0 << 8) | (b_bus << 4) | a_bus
			gs.alu[offset] = byte(result)
			gs.alu[offset | (1 << 8)] = byte(result)
		}
	}
}

