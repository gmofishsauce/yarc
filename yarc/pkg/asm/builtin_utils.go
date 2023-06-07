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
	"math"
	"strconv"
	"strings"
)

// We must get a token that is a new symbol (not a redefinition).
func mustGetNewSymbol(gs *globalState) (*token, error) {
	name := getToken(gs)
	if name.kind() != tkSymbol {
		return nil, fmt.Errorf("expected symbol, found \"%s\"", name)
	}
	if _, ok := gs.symbols[name.text()]; ok {
		return nil, fmt.Errorf("symbol may not be redefined: \"%s\"", name)
	}
	return name, nil
}

func mustGetDefinedSymbol(gs *globalState) (*token, error) {
	var name *token
	for name = getToken(gs); name.kind() != tkSymbol; name = getToken(gs) {
		if name.kind() == tkString {
			stringValue := name.text()[1 : len(name.text())-1]
			gs.reader.push(newNameLineByteReader(name.text(), strings.NewReader(stringValue)))
		} else if name.kind() != tkSymbol {
			return nil, fmt.Errorf("expected defined symbol, found %s", name.text())
		}
	}
	if _, ok := gs.symbols[name.text()]; !ok {
		return nil, fmt.Errorf("symbol must be defined: \"%s\"", name)
	}
	return name, nil
}

func mustGetBitfield(gs *globalState) (*token, error) {
	var tk *token
	for tk = getToken(gs); ; tk = getToken(gs) {
		if tk.kind() == tkSymbol {
			var sym *symbol
			var defined bool
			if sym, defined = gs.symbols[tk.text()]; !defined {
				return nil, fmt.Errorf("symbol must be defined: %s", tk.text())
			}
			if _, ok := sym.symbolData.([]int64); ok {
				return tk, nil // found a bitfield
			}
			// Found some symbol that might evaluate to a bitfield
			val := fmt.Sprintf("%s", sym.data())
			gs.reader.push(newNameLineByteReader(tk.text(), strings.NewReader(val)))
		} else if tk.kind() == tkString {
			stringValue := tk.text()[1 : len(tk.text())-1]
			gs.reader.push(newNameLineByteReader(tk.text(), strings.NewReader(stringValue)))
		} else if tk.kind() == tkOperator && tk.text() == ";" {
			return tk, nil
		} else {
			return nil, fmt.Errorf("unexpected: %s", tk.String())
		}
	}
}

// We must get a "value" token - a number or string. (If the
// token is a symbol, it is expand now (no forwards) and must
// evaluate to a number or string.)
func mustGetValue(gs *globalState) (*token, error) {
	val, err := expandToken(gs)
	if err != nil {
		return nil, err
	}
	if val.kind() == tkError {
		return nil, fmt.Errorf(val.text())
	}
	if val.kind() != tkNumber && val.kind() != tkString {
		return nil, fmt.Errorf("expected value, found \"%s\"", val)
	}
	// The value of a symbol cannot contain a dot. This prevents redefining
	// the builtin symbols as the values of other symbols.
	if strings.Contains(val.text(), ".") {
		return nil, fmt.Errorf("symbol value may not contain a dot")
	}
	return val, nil
}

func mustGetNumber(gs *globalState) (int64, error) {
	val, err := expandToken(gs)
	if err != nil {
		return 0, err
	}
	if val.kind() == tkError {
		return 0, fmt.Errorf(val.text())
	}
	if val.kind() != tkNumber {
		return 0, fmt.Errorf("expected number, found \"%s\"", val)
	}
	n, e := strconv.ParseInt(val.text(), 0, 0)
	if e != nil {
		return 0, e
	}
	return n, nil
}

// Get a token. If it is a defined symbol, expand it, right now (no forwards)
// If we encounter an undefined symbol return an error.
func expandToken(gs *globalState) (*token, error) {
	var tk *token
	for tk = getToken(gs); ; tk = getToken(gs) {
		if tk.tokenKind != tkSymbol {
			return tk, nil
		}
		err := expand(gs, tk.text())
		if err != nil {
			return nil, err
		}
	}
}

// Push the value of the named symbol on the reader stack
func expand(gs *globalState, symToExpand string) error {
	val, ok := gs.symbols[symToExpand]
	if !ok {
		return fmt.Errorf("undefined: \"%s\"", symToExpand)
	}
	s := fmt.Sprintf("%s", val.symbolData)
	gs.reader.push(newNameLineByteReader(symToExpand, strings.NewReader(s)))
	return nil
}

// An opcode has been recognized as a key symbol by the main loop.
// The next token(s) should be the operands, which serve as actuals
// for the formals that were defined in the .opcode builtin. If we
// encounter a key symbol in the argument definitions, it specifies
// a fixup to be done after lexing; we create a fixup table entry.
func doOpcode(gs *globalState, opName string) error {
	opSym := gs.symbols[opName]
	op := *opSym.data().(*opcode)
	var allocateWord bool = false

	for i := 0; i < len(op.args); i++ {
		arg := op.args[i]
		sym, found := gs.symbols[arg.text()]
		if !found || sym.symbolAction == nil {
			return fmt.Errorf("unrecognized: %s", arg.text())
		}

		// It's a .abs, .immX, .src1, etc. Grab the next token
		// and stash it in a fixup struct for later.
		t := getToken(gs)

		// The matching token for a .abs, .imm, etc., must be
		// one of few kinds. The fixup function will sort out
		// the rest of the details later, possibly with error.
		if t.tokenKind != tkSymbol && t.tokenKind != tkLabel &&
			t.tokenKind != tkNumber && t.tokenKind != tkString {
			return fmt.Errorf("unexpected: %s", t)
		}

		// The code fixups happen after lexing is complete, but
		// here in the lex pass we need to allocated space for
		// the code. Normally that's one word for each instruction,
		// but if there is an immediate word (".immw") metasymbol
		// then we need to allocate space for it.
		if arg.text() == ".immw" {
			allocateWord = true
		}

		fxmaker := sym.symbolData.(fixupMaker)
		gs.fixups = append(gs.fixups, fxmaker(gs.memNext, opSym, t))
	}
	gs.mem[gs.memNext] = 0
	gs.memNext++
	gs.mem[gs.memNext] = op.code
	gs.memNext++
	if allocateWord {
		gs.memNext += 2
	}
	return nil
}

// Pack the value of an operand into a field
//func pack(gs *globalState, operandValue int64, target *byte, arg *token) error {
//	fmt.Printf("pack(gs, operandValue %d, target 0x%02X, token %s\n", operandValue, *target, arg)
//	sym := gs.symbols[arg.text()]
//	var elements []int64
//	var ok bool
//	if elements, ok = sym.data().([]int64); !ok {
//		return fmt.Errorf("%s: not a bitfield", arg.text())
//	}
//	if len(elements) != 3 {
//		return fmt.Errorf("%s: not a bitfield", arg.text())
//	}
//	if elements[0] != 8 {
//		return fmt.Errorf("%s: not in an 8-bit field", arg.text())
//	}
//	size := elements[1] - elements[2] + 1
//	max := int64(math.Pow(2, float64(size))) - 1
//	if operandValue < 0 || operandValue > max {
//		return fmt.Errorf("%s: invalid value", arg.text())
//	}
//	fmt.Printf("pack(): field size %d max %d\n", size, max)
//	*target |= byte((operandValue & max) << elements[2])
//	return nil
//}

// A label use has been found. Check that the same label isn't
// previously defined. Define it as the string value of the location.
// Labels are not key symbols.
func makeLabel(gs *globalState, label string) error {
	if _, trouble := gs.symbols[label]; trouble {
		return fmt.Errorf("label already defined: %s", label)
	}
	location := fmt.Sprintf("%d", getMemNext(gs))
	gs.symbols[label] = newSymbol(label, location, nil)
	return nil
}

// Evaluate a token for an integer value. This function is used after
// lexing is complete (i.e. during the fixup "pass"). The lex-time
// functions like mustGetValue(), etc., can't be used at fixup time
// because they work by pushing expansion values on the input reader
// stack; the input reader has reached EOF at this point. So the token
// t must have a value that can be obtained without expansion, which
// makes sense since all the expansions were done during the lex pass.
func evaluateToken(gs *globalState, t *token, min int, max int) (int, error) {
	switch t.kind() {
		case tkString: // TODO implement inline byte strings in memory
			return 0, fmt.Errorf("%s: not implemented as token value", t)
		case tkError, tkNewline, tkOperator, tkEnd:
			return 0, fmt.Errorf("%s: unexpected token type", t)
		case tkNumber:
			n64, e := strconv.ParseInt(t.text(), 0, 0)
			if e != nil {
				return 0, e
			}
			n := int(n64)
			if n < min || n > max {
				return 0, fmt.Errorf("%s: value (%d) of range (%d, %d)",
					t.text(), n, min, max)
			}
			return n, nil
		case tkSymbol, tkLabel:
			// XXX need a loop here for .set sym (.set sym ...)
			sym, ok := gs.symbols[t.text()]
			if !ok {
				return 0, fmt.Errorf("%s: undefined symbol", t.text())
			}
			s, ok := sym.data().(string)
			if !ok {
				return 0, fmt.Errorf("%s: invalid value (\"%v\"", sym.name(), sym.data())
			}
			n64, e := strconv.ParseInt(s, 0, 0)
			if e != nil {
				return 0, e
			}
			n := int(n64)
			if n < min || n > max {
				return 0, fmt.Errorf("%s: value (%d) of range (%d, %d)",
					t.text(), n, min, max)
			}
			return n, nil
	}
	return 0, fmt.Errorf("internal error: evaluateToken(): missing case in type switch")
}

// Lexing is complete. Fix up an absolute value symbol (".abs")
// found in an opcode definition
func fixupAbs(gs *globalState, fx *fixup) error {
	// absolute jump or call fixup: the value of token t is the absolute
	// address of the target. In YARC, target addresses are their own
	// opcodes, so the value replaces the entire 16-bit opcode.
	val, err := evaluateToken(gs, fx.t, 0, 30*1024-1) // END_MEM
	if err != nil {
		return err
	}
	if val & 1 != 0 {
		return fmt.Errorf("absolute address: odd addresses not allowed: %d", val)
	}
	// This is a total hack. The assembler is all supposed to be driven by these
	// metasymbols like ".abs" that specify generic behaviors like placing the
	// absolute value of a symbol in the code stream. Unfortunately, there is
	// one specific feature of the YARC's partially-not-microcoded instruction
	// set that defeats this: jump instructions are distinguished from calls
	// by the LSB of the 16-bit word being set. No other instruction puts what
	// is essentially a fixed opcode bit in the low-order bit; the low-order
	// byte usually contains operands. I could have e.g. ".abseven" and ".absodd"
	// but that's just as much of a hack. So I just special case it here.
	opsym := fx.ref
	if opsym == nil {
		return fmt.Errorf("internal error: .abs: no opcode symbols found")
	}
	op, ok := opsym.data().(*opcode)
	if !ok {
		return fmt.Errorf("internal error: .abs: no opcode in opcode symbol")
	}
	if op.code == 0xFF {
		// It's a jump instruction
		// Set the LS bit to distinguish it from a call instruction
		val |= 1
	}
	//fmt.Printf("fixupAbs(%v): write value 0x%04X at location %d\n", fx, uint16(val), fx.location)
	gs.mem[fx.location] = byte(val&0xFF)
	gs.mem[fx.location+1] = byte((val&0xFF00) >> 8)
	return nil
}

// Lexing is complete. Fix up a relative offset (".rel") found
// in an opcode definition
func fixupRel(gs *globalState, fx *fixup) error {
	// relative branch fixup: the difference between the value of the
	// token t and (memNext + 2) at the point of the instruction must
	// fit in a signed byte. The value replaces the low byte of the
	// instruction.
	val, err := evaluateToken(gs, fx.t, 0, 30*1024-1)
	if err != nil {
		return err
	}

	// As in most computers, the program counter has already been incremented
	// when the branch offset is added to the PC. YARC uses byte addressing,
	// but all instructions and immediate values are 16 bits each. So the reach
	// of a branch is effectively halved and the value -2 is "branch to self".
	if val & 1 != 0 {
		return fmt.Errorf("relative address: odd values not allowed: %d\n", val)
	}
	offset := val - fx.location - 2
	if offset > math.MaxInt8 || offset < math.MinInt8 {
		return fmt.Errorf("relative address: value out of range: %d\n", val)
	}
	//fmt.Printf("fixupRel(%v): write value 0x%02X at location %d\n", fx, uint8(offset), fx.location)
	gs.mem[fx.location] = byte(offset)
	return nil
}

// Lexing is complete. Fix up a byte immediate (".immb") found
// in an opcode definition
func fixupImmb(gs *globalState, fx *fixup) error {
	// immediate byte fixup: the value of the token t must fit in a
	// byte. The value replaces the low byte of the instruction.
	val, err := evaluateToken(gs, fx.t, math.MinInt8, math.MaxUint8)
	if err != nil {
		return err
	}
	//fmt.Printf("fixupImmb(%v): write value 0x%02X at location %d\n", fx, uint8(val), fx.location)
	gs.mem[fx.location] = byte(val)
	return nil
}

// Lexing is complete. Fix up a word immediate (".immw") found in
// an opcode definition
func fixupImmw(gs *globalState, fx *fixup) error {
	// Immediate word fixup: the value of token t replaces the word at
	// (memnext + 2) from the point of the instruction.
	val, err := evaluateToken(gs, fx.t, math.MinInt16, math.MaxUint16)
	if err != nil {
		return err
	}
	//fmt.Printf("fixupAbs(%v): write value 0x%04X at location %d\n", fx, uint16(val), fx.location+2)
	gs.mem[fx.location+2] = byte(val&0xFF)
	gs.mem[fx.location+3] = byte((val&0xFF00) >> 8)
	return nil
}

// Fix up an rt (register target) by placing a general register
// (in 0..3) in the low order 2 bits of the high byte of the opcode.
// This is used for mvi reg, value (move immediate to register)
func fixupRt(gs *globalState, fx *fixup) error {
	val, err := evaluateToken(gs, fx.t, 0, 3)
	if err != nil {
		return err
	}
	lsByteLocation := 1 + fx.location // high order byte
	fmt.Printf("fixupRt: write value 0x%1X in low order 2 bits at location %d\n", val, lsByteLocation)
	gs.mem[lsByteLocation] &^= 0x03
	gs.mem[lsByteLocation] |= byte(val)
	return nil
}

// Lexing is complete. Fix up an acn (alu condition nybble), the
// value 0..15 in the low order 4 bits of the instruction opcode.
//func fixupAcn(gs *globalState, fx *fixup) error {
//	val, err := evaluateToken(gs, fx.t, 0, 0xF)
//	if err != nil {
//		return err
//	}
//	msByteLocation := fx.location+1
//	//fmt.Printf("fixupAcn: write value 0x%1X in low order bits at location %d\n", val, msByteLocation)
//	gs.mem[msByteLocation] &^= 0x0F
//	gs.mem[msByteLocation] |= byte(val)
//	return nil
//}

func fixupSrc1(gs *globalState, fx *fixup) error {
	val, err := evaluateToken(gs, fx.t, 0, 3)
	if err != nil {
		return err
	}
	gs.mem[fx.location] &^= byte(0xC0)
	gs.mem[fx.location] |= byte(val << 6)
	return nil
}

func fixupSrc2(gs *globalState, fx *fixup) error {
	val, err := evaluateToken(gs, fx.t, 0, 7)
	if err != nil {
		return err
	}
	gs.mem[fx.location] &^= byte(0x38)
	gs.mem[fx.location] |= byte(val << 3)
	return nil
}

func fixupDst(gs *globalState, fx *fixup) error {
	val, err := evaluateToken(gs, fx.t, 0, 7)
	if err != nil {
		return err
	}
	gs.mem[fx.location] &^= byte(0x07)
	gs.mem[fx.location] |= byte(val)
	return nil
}
