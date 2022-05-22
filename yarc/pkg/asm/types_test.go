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
	"strings"
	"testing"

	"github.com/stretchr/testify/assert"
)

func TestBaseReader1(t *testing.T) {
	sr := strings.NewReader("hello world")
	br := newNameLineByteReader(t.Name(), sr)

	assert.Equal(t, br.line(), 1)
	assert.Equal(t, br.name(), t.Name())
	b, err := br.ReadByte()
	assert.Equal(t, err, nil)
	assert.Equal(t, b, byte('h'))
}

func TestBaseReader2(t *testing.T) {
	testWrappedStringReader(t, "hello\nworld\n")
}

func TestBaseReader3(t *testing.T) {
	lines := testWrappedStringReader(t, "hello\n \n \nworld\n \n \n")
	assert.Equal(t, lines, 7)
}

func testWrappedStringReader(t *testing.T, data string) int {
	sr := strings.NewReader(data)
	br := newNameLineByteReader(t.Name(), sr)
	return testAnyNameLineByteReader(t, br, data)
}

func testAnyNameLineByteReader(t *testing.T, br anyNameLineByteReader, data string) int {
	assert.Equal(t, br.line(), 1)
	assert.Equal(t, br.name(), t.Name())
	i := 0
	line := 1
	for b, err := br.ReadByte(); i < len(data); b, err = br.ReadByte() {
		assert.Equal(t, err, nil)
		assert.Equal(t, b, byte(data[i]))
		if b == byte('\n') {
			line++
		}
		assert.Equal(t, line, br.line())
		i++
	}
	last, shouldBeEof := br.ReadByte()
	assert.Equal(t, io.EOF, shouldBeEof)
	assert.Equal(t, last, byte(0))
	// And do it again to be sure it's persistent
	last, shouldBeEof = br.ReadByte()
	assert.Equal(t, io.EOF, shouldBeEof)
	assert.Equal(t, last, byte(0))
	return line
}

func makeSimpleStackingReader(name string, data string) *stackingNameLineByteReader {
	sr := strings.NewReader(data)
	br := newNameLineByteReader(name, sr)
	var sbr stackingNameLineByteReader
	sbr.push(br)
	return &sbr
}

func TestStackingReader1(t *testing.T) {
	var sbr stackingNameLineByteReader
	b, err := sbr.ReadByte()
	assert.Equal(t, b, byte(0))
	assert.Equal(t, err, io.EOF)
}

func TestStackingReader2(t *testing.T) {
	sr := strings.NewReader("hello world")
	br := newNameLineByteReader(t.Name(), sr)

	var sbr stackingNameLineByteReader
	b, err := sbr.ReadByte()
	assert.Equal(t, b, byte(0))
	assert.Equal(t, err, io.EOF)

	sbr.push(br) // ignored at EOF
	b, err = sbr.ReadByte()
	assert.Equal(t, b, byte(0))
	assert.Equal(t, err, io.EOF)
}

func TestStackingReader3(t *testing.T) {
	data := "hello world"
	sbr := makeSimpleStackingReader(t.Name(), data)
	testAnyNameLineByteReader(t, sbr, data)
}

func TestStackingReader4(t *testing.T) {
	data := "first\nsecond\nthird\nfourth\nempty follows\n\nlast\n"
	// Push every byte in the data on the stack as separate 1-byte readers
	// Push last byte first so we don't reverse the string
	var sbr stackingNameLineByteReader
	dammit := make([]byte, 1, 1)
	for i := len(data) - 1; i >= 0; i-- {
		dammit[0] = byte(data[i])
		sr := strings.NewReader(string(dammit))
		sbr.push(newNameLineByteReader(t.Name()+fmt.Sprintf("-%d", i), sr))
	}

	for i := range data {
		r, err := sbr.ReadByte()
		assert.Equal(t, nil, err)
		assert.Equal(t, r, data[i])
		if r == NL {
			assert.Equal(t, 2, sbr.line())
		} else {
			assert.Equal(t, 1, sbr.line())
		}
	}

	r, err := sbr.ReadByte()
	assert.Equal(t, io.EOF, err)
	assert.Equal(t, byte(0), r)
}

func TestToString1(t *testing.T) {
	sbr := new(stackingNameLineByteReader)
	sr := strings.NewReader("world")
	sbr.push(newNameLineByteReader("world-reader", sr))
	sr = strings.NewReader("hello")
	sbr.push(newNameLineByteReader("hello-reader", sr))
	expected := "hello-reader:1; world-reader:1"
	assert.Equal(t, expected, sbr.String())
}

func TestUnread1(t *testing.T) {
	sbr := new(stackingNameLineByteReader)
	sr := strings.NewReader("world")
	sbr.push(newNameLineByteReader("world-reader", sr))
	got, err := sbr.ReadByte()
	if err != nil {
		t.Fail()
	}
	assert.Equal(t, byte('w'), got)
	sbr.unreadByte(got)
	got, err = sbr.ReadByte()
	if err != nil {
		t.Fail()
	}
	assert.Equal(t, byte('w'), got)
}

func TestUnread2(t *testing.T) {
	sbr := new(stackingNameLineByteReader)
	sr := strings.NewReader("the\nworld\n")
	sbr.push(newNameLineByteReader("world-reader", sr))
	sr = strings.NewReader("hello\nto\n")
	sbr.push(newNameLineByteReader("hello-reader", sr))

	for i := 0; i < len("hello"); i++ {
		got, err := sbr.ReadByte()
		if err != nil {
			t.Fail()
		}
		assert.Equal(t, byte("hello"[i]), got)
	}
	assert.Equal(t, sbr.name(), "hello-reader")
	assert.Equal(t, sbr.line(), 1)
	got, err := sbr.ReadByte()
	if err != nil {
		t.Fail()
	}

	assert.Equal(t, NL, got)
	assert.Equal(t, "hello-reader", sbr.name())
	assert.Equal(t, 2, sbr.line())

	err = sbr.unreadByte(got)
	assert.Equal(t, "hello-reader", sbr.name())
	assert.Equal(t, 1, sbr.line())

	got, err = sbr.ReadByte()
	assert.Equal(t, NL, got)
	assert.Equal(t, "hello-reader", sbr.name())
	assert.Equal(t, 2, sbr.line())
}
func TestGlobalState(t *testing.T) {
	gs := newGlobalState(strings.NewReader("main source"), "main.yasm")
	gs.symbols[".foo"] = newSymbol(".foo", nil, nil)
}
