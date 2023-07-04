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
	"bufio"
	"fmt"
	"os"
	"path"
)

const END_MEM = 30*1024

// action func for the .set builtin. Create a new symbol that is not a key symbol.
func actionSet(gs *globalState) error {
	name, err := mustGetNewSymbol(gs)
	if err != nil {
		return err
	}
	val, err := mustGetValue(gs)
	if err != nil {
		return err
	}
	gs.symbols[name.text()] = newSymbol(name.text(), val.tokenText,
		func(gs *globalState) error { return expand(gs, name.text()) })
	return nil
}

// action func for the "." (location) builtin, ".=value". The value can be
// a symbol, but the symbol must be defined before use. We don't even allow
// white space around the equals sign. Get past it.
func actionDot(gs *globalState) error {
	mustBeEquals := getToken(gs)
	if mustBeEquals.text() != "=" {
		return fmt.Errorf("usage: .=value")
	}
	val, err := evaluateToken(gs, getToken(gs), 0, END_MEM-2)
	if err != nil {
		return err
	}
	if val & 1 != 0 {
		return fmt.Errorf("absolute address: odd addresses not allowed: %d", val)
	}
	gs.memNext = val
	return nil
}

// action func for the .include builtin. Push a reader for the file.
func actionInclude(gs *globalState) error {
	tk := getToken(gs)
	if tk.kind() != tkString {
		return fmt.Errorf(".include: expected string, found \"%s\"", tk)
	}
	includeFile := tk.text()[1 : len(tk.text())-1]
	f, err := os.Open(path.Join(gs.includeDir, includeFile))
	if err != nil {
		return fmt.Errorf(".include: %s: %s", includeFile, err)
	}
	gs.reader.push(newNameLineByteReader(tk.tokenText, bufio.NewReader(f)))
	return nil
}

// action func for the .bitfield builtin. Define a bitfield.
func actionBitfield(gs *globalState) error {
	name, err := mustGetNewSymbol(gs)
	if err != nil {
		return err
	}
	// Trying the following for now: we require that symbols used
	// here not be "forward", i.e. we expand any symbols we find
	// right now, so symbols must have already been defined.
	wordsize, err := mustGetNumber(gs)
	if err != nil {
		return fmt.Errorf(".bitfield: numeric word size expected: %s", err)
	}
	upperLimit, err := mustGetNumber(gs)
	if err != nil {
		return fmt.Errorf(".bitfield: numeric upper bound expected: %s", err)
	}
	colon := getToken(gs)
	if colon.kind() != tkOperator || colon.tokenText != ":" {
		return fmt.Errorf(".bitfield: colon expected")
	}
	lowerLimit, err := mustGetNumber(gs)
	if err != nil {
		return fmt.Errorf(".bitfield: numeric lower bound expected: %s", err)
	}

	if upperLimit < lowerLimit || upperLimit-lowerLimit+1 > wordsize {
		return fmt.Errorf(".bitfield: illegal bounds")
	}
	gs.symbols[name.text()] = newSymbol(name.text(), []int64{wordsize, upperLimit, lowerLimit}, nil)
	return nil
}

type opcode struct {
	code byte
	args []*token
	argValues []*token
}

func opcodeToWcsOffset(opVal byte) int {
	return int(opVal&^0x80) * WCS_SLOTS_PER_OPCODE
}

// actionFunc for the .opcode builtin. Define an opcode symbol
// .opcode symbol opcode nargs arg0 ... argNargs - 1
func actionOpcode(gs *globalState) error {
	if gs.inOpcode || gs.opcodeValue != 0 {
		return fmt.Errorf(".opcode: not allowed in an opcode definition")
	}
	name, err := mustGetNewSymbol(gs)
	if err != nil {
		return fmt.Errorf(".opcode: name expected: %s", err)
	}
	code, err := mustGetNumber(gs)
	if err != nil {
		return fmt.Errorf(".opcode: numeric opcode expected: %s", err)
	}
	if code < 0x80 || code > 0xFF { // YARC specific
		return fmt.Errorf(".opcode: opcodes must be in 0x80..0xFF")
	}
	nargs, err := mustGetNumber(gs)
	if err != nil {
		return fmt.Errorf(".opcode: number of arguments expected")
	}
	if nargs < 0 || nargs > 8 {
		return fmt.Errorf(".opcode: only 0 to 8 arguments allowed")
	}

	// Process the opcode's metasymbol arguments (.dst, .immb, etc.)
	var i int64
	args := make([]*token, nargs, nargs)
	argValues := make([]*token, nargs, nargs)

	for i = 0; i < nargs; i++ {
		arg, err := mustGetDefinedSymbol(gs)
		if err != nil {
			return fmt.Errorf("symbol expected: %s", err)
		}
		args[i] = arg

		// Check for the metasymbol having a fixed value attached.
		// syntax: .meta=numeric_value
		maybeAssignment := getToken(gs)
		if maybeAssignment.text() != string(EQUALS) {
			// Nope, just push it back and continue
			ungetToken(gs, maybeAssignment)
		} else {
			// Yes, the metasymbol has a fixed value. It will
			// not correspond to an actual argument to the
			// assembler mnemonic we're defining; the fixed
			// value will be inserted into this field every
			// time this opcode is used.
			argValue := getToken(gs)
			if argValue.kind() != tkSymbol && argValue.kind() != tkNumber {
				return fmt.Errorf("invalid fixed value: %s", err)
			}
			argValues[i] = argValue
		}
	}

	data := &opcode{byte(code), args, argValues}

	gs.symbols[name.text()] = newSymbol(name.text(), data,
		func(gs *globalState) error { return doOpcode(gs, name.text()) })
	gs.inOpcode = true
	gs.opcodeValue = byte(code)
	gs.wcsNext = opcodeToWcsOffset(gs.opcodeValue)
	return nil
}

// actionFunc for the .endOpcode builtin.
func actionEndOpcode(gs *globalState) error {
	if !gs.inOpcode {
		return fmt.Errorf(".endopcode: not in opcode")
	}
	gs.inOpcode = false
	gs.opcodeValue = 0
	gs.wcsNext = -1
	return nil
}

// Action fund for a .slot builtin defining one slot in the writeable
// control store. There may be any number of bitfield = value clauses.
// If the same bitfield is mentioned more than once, the last one wins.
// The bitfield clauses are separate by spaces and terminated by a
// semicolon or an error (end of line is not relevant). All bitfields
// must have a width of 32. All strings and symbols are resolved now;
// forwards are not allowed.
func actionSlot(gs *globalState) error {
	if !gs.inOpcode || gs.opcodeValue == 0 {
		return fmt.Errorf(".slot: not in an opcode definition")
	}
	for {
		tk, err := mustGetBitfield(gs)
		if err != nil {
			return err
		}
		if tk.kind() == tkOperator && tk.text() == ";" {
			gs.wcsNext++
			return nil
		}
		field, ok := gs.symbols[tk.text()].symbolData.([]int64)
		if !ok {
			return fmt.Errorf("%s: bitfield expected", tk.text())
		}
		if field[0] != 32 {
			return fmt.Errorf("%s: .slot bitfield width must be 32", tk.text())
		}
		equals := getToken(gs)
		if equals.kind() != tkOperator || equals.text() != "=" {
			return fmt.Errorf(".bitfield: equals expected: found %s", equals.String())
		}
		num, err := mustGetNumber(gs)
		if err != nil {
			return fmt.Errorf(".bifield: number expected: %s", err.Error())
		}
		size := field[1] - field[2] + 1
		var max int64 = (1 << size) - 1
		if num < 0 || num > max {
			return fmt.Errorf(".bitfield: %d out of range for %s", num, tk)
		}
		gs.wcs[gs.wcsNext] &^= uint32(max << field[2])
		gs.wcs[gs.wcsNext] |= uint32((num & max) << field[2])
		//fmt.Printf("set opcode 0x%02x slot %d bit offset %d to 0x%02X value 0x%08x\n",
		//	gs.opcodeValue, gs.wcsNext, field[2], uint32(num & max), gs.wcs[gs.wcsNext])
	}
}

// The dup builtin duplicates the microcode for a previously-defined opcode
// under a new opcode symbol name. Because of the partial hardware decoding
// of instructions, many instructions can share the same microcode. Syntax
// is .dup newOpcodeName opcodeValue previouslyDefinedOpcodeName
func actionDup(gs *globalState) error {
	if gs.inOpcode || gs.opcodeValue != 0 {
		return fmt.Errorf(".dup: not allowed in an opcode definition")
	}
	newName, err := mustGetNewSymbol(gs)
	if err != nil {
		return fmt.Errorf(".dup: new name expected: %s", err)
	}

	n, err := mustGetNumber(gs)
	if err != nil {
		return fmt.Errorf(".dup: numeric new opcode expected: %s", err)
	}
	if n < 0 || n > 0xFF {
		return fmt.Errorf(".dup: byte value expected: got 0x%X", n)
	}
	newOpcodeValue := byte(n)

	// Extract the opcode structure for the old opcode. Reuse the
	// arguments (the list of metasymbols, not the actuals which are
	// specified when the opcode is used), but with the new opcode.

	existingOpcodeSymbol, err := mustGetDefinedSymbol(gs)
	if err != nil {
		return fmt.Errorf(".dup: existing opcode name expected: %s", err)
	}
	existingOpcode, ok := gs.symbols[existingOpcodeSymbol.text()].data().(*opcode)
	if !ok {
		return fmt.Errorf(".dup: not a defined opcode: %s", existingOpcodeSymbol.text())
	}
	newOpcode := &opcode{newOpcodeValue, existingOpcode.args, existingOpcode.argValues}

	// Create the new opcode and copy the microcode

	gs.symbols[newName.text()] = newSymbol(newName.text(), newOpcode,
		func(gs *globalState) error { return doOpcode(gs, newName.text()) })
	wcsSource := opcodeToWcsOffset(existingOpcode.code)
	wcsDest := opcodeToWcsOffset(newOpcode.code)
	wcsEnd := wcsSource + WCS_SLOTS_PER_OPCODE;
	for ; wcsSource < wcsEnd ; {
		gs.wcs[wcsDest] = gs.wcs[wcsSource]
		wcsSource++
		wcsDest++
	}

	return nil
}

// Symbol actions were originally conceived as a way to implement the lexer loop
// in asm.go: it largely grabs a symbol and then delegates to the action function.
// As a result the action functions take only the global state and return only an
// error. When I implemented fixups for labels (e.g. jump forward to as-yet unseen
// label, relative branches, etc.) I wanted to add key symbols that were processed
// not in the main loop, but in the course of handling an opcode. And the processing
// for these symbols didn't occur when they were encountered. Rather I wanted to
// stash away some state in a fixup structure, add the fixup to a list, and process
// the entire list after lexing was complete. Finally, key symbols are identified
// by the presence of an action function. I could change that, but I don't want to
// crap up the relatively clean structure of the key symbols that trigger processing
// during the lex pass.
//
// So these new fixup-style key symbols (.abs, .rel, .imm, and maybe others in the
// future) are implemented as key symbols with action functions that just report an
// error. Their static symbol data is a function that takes arguments for all the state
// any fixup might need, creates the fixup entry, adds the correct deferred processing
// function to the entry, and then adds the fixup to the fixup list. The fixup list is
// processed if and after lexing completes successfully. The fixup functions themselves
// are found in builtin_utils.go.

func actionFixup(gs *globalState) error {
	return fmt.Errorf("internal error: fixup action called")
}

// Return a fixup entry for a .abs (absolute label reference)
func createAbsFixupAction(loc int, ref *symbol, t *token) *fixup {
	return newFixup(".abs", loc, fixupAbs, ref, t)
}

// Return a fixup entry for a .rel (relative label reference)
func createRelFixupAction(loc int, ref *symbol, t *token) *fixup {
	return newFixup(".rel", loc, fixupRel, ref, t)
}

// Return a fixup entry for a .immb (immediate byte)
func createImmbFixupAction(loc int, ref *symbol, t *token) *fixup {
	return newFixup(".immb", loc, fixupImmb, ref, t)
}

// Return a fixup entry for a .immw (immediate word)
func createImmwFixupAction(loc int, ref *symbol, t *token) *fixup {
	return newFixup(".immw", loc, fixupImmw, ref, t)
}

// Return a fixup entry for a .rs (r0 or r1 in low bit of high opcode byte)
func createRsFixupAction(loc int, ref *symbol, t *token) *fixup {
	return newFixup(".rs", loc, fixupRs, ref, t)
}

// Return a fixup entry for a .rt (register in low 2 bits of high opcode byte)
func createRtFixupAction(loc int, ref *symbol, t *token) *fixup {
	return newFixup(".rt", loc, fixupRt, ref, t)
}

// Return a fixup entry for a .acn (alu condition nybble)
func createAcnFixupAction(loc int, ref *symbol, t *token) *fixup {
	return newFixup(".acn", loc, fixupAcn, ref, t)
}

// Return a fixup entry for the src1 field of the RCW
func createSrc1FixupAction(loc int, ref *symbol, t *token) *fixup {
	return newFixup(".src1", loc, fixupSrc1, ref, t)
}

// Return a fixup entry for the src2 field of the RCW
func createSrc2FixupAction(loc int, ref *symbol, t *token) *fixup {
	return newFixup(".src2", loc, fixupSrc2, ref, t)
}

// Return a fixup entry for the dest field of the RCW
func createDstFixupAction(loc int, ref *symbol, t *token) *fixup {
	return newFixup(".dst", loc, fixupDst, ref, t)
}

// Key symbols. Most, but not all, appear at the start of a line
var builtinSet *symbol = newSymbol(".set", nil, actionSet)
var builtinDot *symbol = newSymbol(".", nil, actionDot)
var builtinInclude *symbol = newSymbol(".include", nil, actionInclude)
var builtinBitfield *symbol = newSymbol(".bitfield", nil, actionBitfield)
var builtinOpcode *symbol = newSymbol(".opcode", nil, actionOpcode)
var builtinEndOpcode *symbol = newSymbol(".endopcode", nil, actionEndOpcode)
var builtinSlot *symbol = newSymbol(".slot", nil, actionSlot)
var builtinDup *symbol = newSymbol(".dup", nil, actionDup)

// Embedded key symbols that specify fixups when used in opcode definitions
// These are recognized and processed in doOpcode(), not the main loop.
var builtinAbs *symbol = newSymbol(".abs", createAbsFixupAction, actionFixup)
var builtinRel *symbol = newSymbol(".rel", createRelFixupAction, actionFixup)
var builtinImmb *symbol = newSymbol(".immb", createImmbFixupAction, actionFixup)
var builtinImmw *symbol = newSymbol(".immw", createImmwFixupAction, actionFixup)
var builtinRs *symbol = newSymbol(".rs", createRsFixupAction, actionFixup)
var builtinRt *symbol = newSymbol(".rt", createRtFixupAction, actionFixup)
var builtinAcn *symbol = newSymbol(".acn", createAcnFixupAction, actionFixup)
var builtinSrc1 *symbol = newSymbol(".src1", createSrc1FixupAction, actionFixup)
var builtinSrc2 *symbol = newSymbol(".src2", createSrc2FixupAction, actionFixup)
var builtinDst *symbol = newSymbol(".dst", createDstFixupAction, actionFixup)

func registerBuiltins(gs *globalState) {
	gs.symbols[builtinSet.name()] = builtinSet
	gs.symbols[builtinDot.name()] = builtinDot
	gs.symbols[builtinInclude.name()] = builtinInclude
	gs.symbols[builtinBitfield.name()] = builtinBitfield
	gs.symbols[builtinOpcode.name()] = builtinOpcode
	gs.symbols[builtinEndOpcode.name()] = builtinEndOpcode
	gs.symbols[builtinSlot.name()] = builtinSlot
	gs.symbols[builtinDup.name()] = builtinDup

	gs.symbols[builtinAbs.name()] = builtinAbs
	gs.symbols[builtinRel.name()] = builtinRel
	gs.symbols[builtinImmb.name()] = builtinImmb
	gs.symbols[builtinImmw.name()] = builtinImmw
	gs.symbols[builtinRs.name()] = builtinRs
	gs.symbols[builtinRt.name()] = builtinRt
 	gs.symbols[builtinAcn.name()] = builtinAcn
	gs.symbols[builtinSrc1.name()] = builtinSrc1
	gs.symbols[builtinSrc2.name()] = builtinSrc2
	gs.symbols[builtinDst.name()] = builtinDst
}
