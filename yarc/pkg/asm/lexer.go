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
	"io"
	"log"
)

const SP = byte(' ')
const TAB = byte('\t')
const NL = byte('\n')

const COMMA = byte(',')
const EQUALS = byte('=')
const COLON = byte(':')
const SEMI = byte(';')

const DOT = byte('.')
const UNDERSCORE = byte('_')

const COMMENT = byte('#')

// N.B. The below is my preferred solution to Go's appalling lack of support
// for type-checked enumerations. Note that if e.g. lexerStateType is changed
// to be an int instead of a struct containing an int, then assignments to the
// lexerState are no longer type checked - the RHS can be any int.

// Lexer states

type lexerStateType struct {
	s int
}

var stInError lexerStateType = lexerStateType{0}
var stBetween lexerStateType = lexerStateType{1}
var stInSymbol lexerStateType = lexerStateType{2}
var stInString lexerStateType = lexerStateType{3}
var stInNumber lexerStateType = lexerStateType{4}
var stInOperator lexerStateType = lexerStateType{5}
var stInComment lexerStateType = lexerStateType{6}
var stEnd lexerStateType = lexerStateType{7}

var lexerState lexerStateType = stBetween

// Token kinds

type tokenKindType struct {
	k int
}

var tkError tokenKindType = tokenKindType{0}
var tkNewline tokenKindType = tokenKindType{1}
var tkSymbol tokenKindType = tokenKindType{2}
var tkString tokenKindType = tokenKindType{3}
var tkNumber tokenKindType = tokenKindType{4}
var tkOperator tokenKindType = tokenKindType{5}
var tkEnd tokenKindType = tokenKindType{6}

var kindToString = []string{
	"tkError",
	"tkNewline",
	"tkSymbol",
	"tkString",
	"tkNumber",
	"tkOperator",
	"EOF",
}

type token struct {
	tokenText string
	tokenKind tokenKindType
}

func (t *token) String() string {
	s := t.tokenText
	if s == "\n" {
		s = "\\n"
	}
	return fmt.Sprintf("{%s %s}", kindToString[t.tokenKind.k], s)
}

func (t *token) text() string {
	return t.tokenText
}

func (t *token) kind() tokenKindType {
	return t.tokenKind
}

var eofToken = token{"EOF", tkEnd}
var nlToken = token{"\n", tkNewline}

// getToken returns the next lexer token (or an EOF or error token).
//
// The lexer's notion of "token" is very simpleminded and is enforced on the whole
// language: action functions of builtins can (and do) separately define the legality
// and meaning of any sequence of lexer tokens, but they cannot change the definition
// of "lexer token" from the one enforced here.
//
// The language is all ASCII - no exceptions, not even in quoted strings. White space
// includes only space, tab, and newline. Newline is returned as a separate token so
// that the language may be at least partially line-oriented. The handling of control
// characters other than the defined whitespace characters is undefined.
//
// Tokens are:
//
// 1. Symbols. These are unquoted restricted character strings. The first character
// must be one of the "initial symbol characters" and the remaining characters must
// be "symbol characters" (neither set is a subset of the other). Symbols terminate
// at a "white space character" or at a "single character token" (see next).
//
// 2. Single-character tokens. A few characters like equals, comma, semicolon (and
// maybe others over time) form tokens and can therefore act as delimiters for symbols.
// So foo=bar is accepted as is foo = bar and foo "bar". Newlines are also returned as
// a separate token which the caller may choose to treat as whitespace or a delimiter.
//
// 3. Quoted strings. These are surrounded by double quotes. Double quotes do not
// serve as single-character tokens for purposes of terminating a symbol, so a
// sequence like foo"bar" isn't legal. But an action function can choose to make
// a sequence like foo="bar" legal given that "=" is a valid single-character token.
// Newlines are never allowed in strings. Maybe add a multiline string deliminter in
// the future if needed.
//
// 4. Numbers. These can be decimal numbers or hex numbers starting with 0x or 0X and
// containing the letters a-f in either case.
//
// EOF is not equivalent to whitespace; a token won't be recognized if it's terminated
// by end of file without a newline (or tab or space). The language doesn't even have
// constant expressions, so the small set of "operator" characters are more like
// punctuation than arithment operators. Comments ("# ...") are terminated by newlines
// and must be preceded by whitespace, which is usually desirable for readability
// anyway. When the lexer encounters an error, it is returned as token; the lexer then
// enters an error state and throws away characters until it sees a newline (or EOF).
//
// And for the initial version of the lexer, that's it.

func getToken(gs *globalState) *token {
	if lexerState == stEnd {
		return &eofToken
	}

	var accumulator []byte

	for b, err := gs.reader.ReadByte(); ; b, err = gs.reader.ReadByte() {
		// Preliminaries
		if err == io.EOF {
			lexerState = stEnd
			return &eofToken
		}
		if err != nil {
			lexerState = stInError
			return &token{err.Error(), tkError}
		}
		if b >= 0x80 {
			lexerState = stInError
			return &token{fmt.Sprintf("non-ASCII character 0x%02x", b), tkError}
		}

		// Switch on lexer state. Within each case, handle all character types. The
		// "stBetween" state is the start state. It's similar to an "in white space"
		// state except for some subtleties: currently all operators (punctuation)
		// are single characters, so we can just return a token when we see one and
		// remain in the "stBetween" state for sequences like 7:4 that contain no
		// actual whitespace around the colon operator.

		switch lexerState {
		case stInError, stInComment:
			if b == NL {
				lexerState = stBetween
				return &nlToken
			}
		case stBetween:
			if len(accumulator) != 0 {
				log.Fatalf("token accumulator not empty between tokens: %s\n", accumulator)
			}
			if b == NL {
				// Still between, but returned as a distinct token so that
				// caller may implement a line-oriented higher level syntax
				return &nlToken
			}
			if b == COMMENT {
				lexerState = stInComment
			} else if isWhiteSpaceChar(b) {
				// move along, nothing to see here
			} else if isDigitChar(b) {
				accumulator = append(accumulator, b)
				lexerState = stInNumber
			} else if isInitialSymbolChar(b) {
				accumulator = append(accumulator, b)
				lexerState = stInSymbol
			} else if isQuoteChar(b) {
				// we do not capture the quotes in the result
				lexerState = stInString
			} else if isOperatorChar(b) {
				lexerState = stBetween
				return &token{string(b), tkOperator}
			} else {
				msg := fmt.Sprintf("character 0x%02x (%c) unexpected", b, rune(b))
				lexerState = stInError
				return &token{msg, tkError}
			}
		case stInSymbol:
			if len(accumulator) == 0 {
				log.Fatalln("token accumulator empty in symbol")
			}
			if isWhiteSpaceChar(b) || isOperatorChar(b) {
				lexerState = stBetween
				result := &token{string(accumulator), tkSymbol}
				accumulator = nil
				// Even for whitespace, we need to push it back
				// and process it next time we're called because
				// it might be a newline, which gets returned as
				// a separate token while still being white space.
				gs.reader.unreadByte(b)
				return result
			} else if isSymbolChar(b) {
				accumulator = append(accumulator, b)
			} else {
				msg := fmt.Sprintf("character 0x%02x (%c) unexpected", b, rune(b))
				lexerState = stInError
				return &token{msg, tkError}
			}
		case stInString:
			if isQuoteChar(b) {
				// Changing directly to "between" here means a symbol or something
				// can come after a quoted string without any intervening white space.
				// Wrong/ugly, but not worth fixing. Also, the caller may separately
				// demand that e.g. builtin symbols be preceded by a newline and optional
				// whitespace, etc., so this may be reported as an error there.
				lexerState = stBetween
				result := &token{string(accumulator), tkString}
				accumulator = nil
				return result
			} else if b == NL {
				// There is no escape convention
				lexerState = stInError
				return &token{"newline in string", tkError}
			} else {
				accumulator = append(accumulator, b)
			}
		case stInNumber:
			// We get into the number state when we see a digit 0-9. When in the number state,
			// we accumulate any digit, a-f, A-F, x, or X, i.e. we allow garbage sequences with
			// multiple x's, hex letters without a leading 0x, etc. Then at the end we apply the
			// validity tests and return error if the numeric string is garbage.
			if isDigitChar(b) || isHexLetter(b) || isX(b) {
				accumulator = append(accumulator, b)
			} else if isWhiteSpaceChar(b) || isOperatorChar(b) {
				var result *token
				if !validNumber(accumulator) {
					result = &token{fmt.Sprintf("invalid number %s", string(accumulator)), tkError}
					lexerState = stInError
				} else {
					result = &token{string(accumulator), tkNumber}
					lexerState = stBetween
				}
				accumulator = nil
				gs.reader.unreadByte(b)
				return result
			} else {
				msg := fmt.Sprintf("character 0x%02x (%c) unexpected in number", b, rune(b))
				lexerState = stInError
				return &token{msg, tkError}
			}
			// That's it - no state called stInOperator since they are all single characters
		}
	}
}

func validNumber(num []byte) bool {
	isHex := false
	digitOffset := 0
	if len(num) > 2 && num[0] == byte('0') && isX(num[1]) {
		isHex = true
		digitOffset = 2
	}
	for i := digitOffset; i < len(num); i++ {
		switch { // no fallthrough in Go
		case isDigitChar(num[i]): // OK
		case isHex && isHexLetter(num[i]): // OK
		default:
			return false
		}
	}
	return true
}

func isWhiteSpaceChar(b byte) bool {
	return b == SP || b == TAB || b == NL
}

func isDigitChar(b byte) bool {
	return b >= '0' && b <= '9'
}

func isHexLetter(b byte) bool {
	switch {
	case b >= 'A' && b <= 'F':
		return true
	case b >= 'a' && b <= 'f':
		return true
	}
	return false
}

func isX(b byte) bool {
	return b == 'x' || b == 'X'
}

func isQuoteChar(b byte) bool {
	return b == '"' // || b == '`' future multiline string
}

func isOperatorChar(b byte) bool {
	return b == COMMA || b == EQUALS || b == SEMI || b == COLON
}

// Dot is allowed only as the initial character
// of a symbol, where it means "builtin"
func isInitialSymbolChar(b byte) bool {
	switch {
	case b >= 'a' && b <= 'z':
		return true
	case b == '.' || b == '_':
		return true
	case b >= 'A' && b <= 'Z':
		return true
	}
	return false
}

func isSymbolChar(b byte) bool {
	switch {
	case b >= 'a' && b <= 'z':
		return true
	case b >= '0' && b <= '9':
		return true
	case b == '_':
		return true
	case b >= 'A' && b <= 'Z':
		return true
	}
	return false
}

// A single run of yasm involves a single lexer reading
// a single logical stream, so there's no mileage in the
// lexer being an instance. But the tests need to create
// a bunch of lexers in a single process, so need a way
// to reset the state.

func reinitLexer() {
	lexerState = stBetween
}
