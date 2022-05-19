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
const SEMI = byte(';')

const DOT = byte('.')
const UNDERSCORE = byte('_')

// N.B. - the language does not even have constant expressions,
// so the (small) set of "operators" are more like punctuation..

var whitespaceChars []byte
var operatorChars []byte
var initialSymbolChars []byte
var symbolChars []byte
var decimalChars []byte
var hexChars []byte

func init() {
	whitespaceChars = []byte{SP, NL, TAB}
	operatorChars = []byte{COMMA, EQUALS, SEMI}

	initialSymbolChars = append(initialSymbolChars, DOT)
	initialSymbolChars = append(initialSymbolChars, UNDERSCORE)
	symbolChars = append(symbolChars, UNDERSCORE)

	for i := 'A'; i <= 'Z'; i++ {
		initialSymbolChars = append(initialSymbolChars, byte(i))
		symbolChars = append(symbolChars, byte(i))
		if i < 'G' {
			hexChars = append(hexChars, byte(i))
		}
	}
	for i := 'a'; i <= 'z'; i++ {
		initialSymbolChars = append(initialSymbolChars, byte(i))
		symbolChars = append(symbolChars, byte(i))
		if i < 'g' {
			hexChars = append(hexChars, byte(i))
		}
	}
	for i := '0'; i <= '9'; i++ {
		symbolChars = append(symbolChars, byte(i))
		decimalChars = append(decimalChars, byte(i))
		hexChars = append(hexChars, byte(i))
	}
}

// Token kinds
const (
	tkError = iota
	tkNewline
	tkSymbol
	tkString
	tkNumber
	tkOperator
	tkEnd
)

var kindToString = []string{
	"error",
	"newline",
	"symbol",
	"string",
	"number",
	"operator",
	"EOF",
}

// Lexer states
const (
	stInError = iota
	stInWhite
	stInSymbol
	stInString
	stInNumber
	stInOperator
	stEnd
)

var lexerState = stInWhite

type token struct {
	tokenText string
	tokenKind int
}

func (t *token) String() string {
	return fmt.Sprintf("{%s %s}", kindToString[t.tokenKind], t.tokenText)
}

func (t *token) text() string {
	return t.tokenText
}

func (t *token) kind() int {
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
// a separate token which the caller may choose to treat as whitesspace or a delimiter.
//
// 3. Quoted strings. These are surrounded by double quotes. Double quotes do not
// serve as single-character tokens for purposes of terminating a symbol, so a
// sequence like foo"bar" isn't legal. But an action function can choose to make
// a sequence like foo="bar" legal given that "=" is a valid single-character token.
// Newlines are never allowed in strings. Maybe add a multiline string deliminter in
// the future if needed.
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

		switch lexerState {
		case stInWhite:
			if len(accumulator) != 0 {
				log.Fatalf("token accumulator not empty between tokens: %s\n", accumulator)
			}
			if b == NL {
				// Still inWhite, but returned as a distinct token so that
				// caller may implement a line-oriented higher level syntax
				return &nlToken
			}
			if isWhiteSpaceChar(b) {
				// move along, nothing to see here
			} else if isDigitChar(b) {
				accumulator = append(accumulator, b)
				lexerState = stInNumber
			} else if isInitialSymbolChar(b) {
				accumulator = append(accumulator, b)
				lexerState = stInSymbol
			} else if isQuoteChar(b) {
				//accumulator = append(accumulator, b)
				lexerState = stInString
			} else if isOperatorChar(b) {
				accumulator = append(accumulator, b)
				lexerState = stInOperator
			}
		case stInSymbol:
			if len(accumulator) == 0 {
				log.Fatalln("token accumulator empty in symbol")
			}
			if isWhiteSpaceChar(b) {
				lexerState = stInWhite
				result := &token{string(accumulator), tkSymbol}
				accumulator = nil
				return result
			} else {
				accumulator = append(accumulator, b)
			}
		case stInString:
		case stInNumber:

		}
	}
}

func isWhiteSpaceChar(b byte) bool {
	return b == SP || b == TAB || b == NL
}

func isDigitChar(b byte) bool {
	return b >= '0' && b <= '9'
}

func isQuoteChar(b byte) bool {
	return b == '"' // || b == '`' future multiline string
}

func isOperatorChar(b byte) bool {
	return b == COMMA || b == EQUALS || b == SEMI
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
