// Copyright (c) Jeff Berkowitz 2021, 2022. All rights reserved.
// Generate symbols for inclusion in by host and Arduino protocol engines

package protogen

import (
	"fmt"
	"os"
	"strings"
	"unicode"
)

var names = []struct {
	name string
	val int
}{
	{ "STCMD_BASE", 0xE0 },
	{ "STCMD_GET_MCR", 0xE1 },
	{ "STCMD_EN_FAST", 0xE2 },
	{ "STCMD_DIS_FAST", 0xE3 },
	{ "STCMD_EN_SLOW", 0xE4 },
	{ "STCMD_DIS_SLOW", 0xE5 },
	{ "STCMD_RUN_YARC", 0xE7 },
	{ "STCMD_STOP_YARC", 0xE8 },
	{ "STCMD_POLL", 0xE9 },
	{ "STCMD_SVC_RESPONSE", 0xEA },
	{ "STCMD_GET_VER", 0xEE },
	{ "STCMD_SYNC", 0xEF },
	{ "STCMD_SET_ARH", 0xF0 },
	{ "STCMD_SET_ARL", 0xF1 },
	{ "STCMD_SET_DRH", 0xF2 },
	{ "STCMD_SET_DRL", 0xF3 },
	{ "STCMD_DO_CYCLE", 0xF4 },
	{ "STCMD_GET_RESULT", 0xF5 },
	{ "STCMD_WR_SLICE", 0xF6 },
	{ "STCMD_RD_SLICE", 0xF7 },
	{ "STCMD_XFER_SINGLE", 0xF8 },
	{ "STCMD_WRITE_PAGE", 0xF9 },
	{ "STCMD_READ_PAGE", 0xFA },
	{ "STCMD_SET_K", 0xFB },
}

var errors = []struct {
	name string
	val int
	msg string
}{
	{ "STERR_NOSYNC", 0x80, "not synchonized" },
	{ "STERR_PASSIVE", 0x81, "not allowed when passive" },
	{ "STERR_ONECLOCK", 0x82, "cannot enable multiple clocks" },
	{ "STERR_CANT_SS", 0x83, "cannot single step with clock enable" },
	{ "STERR_CANT_PG", 0x84, "invalid state for page transfer" },
	{ "STERR_INTERNAL", 0x85, "Arduino reported an internal error" },
	{ "STERR_BADCMD", 0x86, "invalid command byte" },
}

// Protocol version 5. Added the read and write microcode slice commands.

const protocolVersion = 5

func Generate() {
	generateCSymbols("serial_protocol.h")
	generateGoSymbols("serial_protocol.go")
}

func generateCSymbols(filename string) {
	f := openFile(filename)
	defer f.Close()

	fmt.Fprintf(f, "#define PROTOCOL_VERSION %d\n", protocolVersion)
	fmt.Fprintf(f, "#define ACK(CMD) ((byte)~CMD)\n")
	fmt.Fprintf(f, "\n")

	for _, ns := range(names) {
		fmt.Fprintf(f, "#define %-20s 0x%02X\n", ns.name, ns.val)
	}
	fmt.Fprintf(f, "\n")

	for _, es := range(errors) {
		fmt.Fprintf(f, "#define %-20s 0x%02X\n", es.name, es.val)
	}
}

func generateGoSymbols(filename string) {
	f := openFile(filename)
	defer f.Close()

	fmt.Fprintf(f, "package serial_protocol\n")
	fmt.Fprintf(f, "\n")

	fmt.Fprintf(f, "const ProtocolVersion = %d\n", protocolVersion)
	fmt.Fprintf(f, "\n")

	fmt.Fprintf(f, "func Ack(b byte) byte {\n\treturn ^b\n}\n")
	fmt.Fprintf(f, "\n")

	for _, ns := range(names) {
		fmt.Fprintf(f, "const %-20s = 0x%02X\n", mkGoSym(ns.name), ns.val)
	}
	fmt.Fprintf(f, "\n")

	for _, es := range(errors) {
		fmt.Fprintf(f, "const %-20s = 0x%02X\n", mkGoSym(es.name), es.val)
	}
	fmt.Fprintf(f, "\n")

	fmt.Fprintf(f, "var ErrorMessages = []string {\n")
	for _, es := range(errors) {
		fmt.Fprintf(f, "\t\"%s\",\n", es.msg)
	}
	fmt.Fprintf(f, "}\n")
	fmt.Fprintf(f, "\n")

}

// Convert SNAKE_UPPER_CASE to UpperCamelCase. The code knows that all symbols
// start with "ST" not followed by an underscore, hence the 2:

func mkGoSym(sym string) string {
	var result strings.Builder
	uppercaseNext := true
	for _, c := range strings.ToLower(sym[2:]) {
		if (uppercaseNext) {
			result.WriteRune(unicode.ToUpper(c))
			uppercaseNext = false
		} else if c == '_' {
			uppercaseNext = true
		} else {
			result.WriteRune(c)
		}
	}
	return result.String()
}

func openFile(filename string) (*os.File) {
	f, err := os.Create(filename)
	if err != nil {
		panic(err)
	}
	emitHeader(f)
	return f
}

// Syntax happens to be the same in both languages so we can share this
func emitHeader(f *os.File) {
	fmt.Fprintf(f, "// Copyright (c) Jeff Berkowitz 2021, 2022. All Rights Reserved\n");
	fmt.Fprintf(f, "// Automatically generated by Protogen - do not edit\n\n");
}

