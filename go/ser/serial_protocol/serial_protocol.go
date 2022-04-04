// Copyright (c) Jeff Berkowitz 2021, 2022. All Rights Reserved
// Automatically generated by Protogen - do not edit

package serial_protocol

const ProtocolVersion = 3

func Ack(b byte) byte {
	return ^b
}

const CmdBase              = 0xE0
const CmdGetMcr            = 0xE1
const CmdEnFast            = 0xE2
const CmdDisFast           = 0xE3
const CmdEnSlow            = 0xE4
const CmdDisSlow           = 0xE5
const CmdSingle            = 0xE6
const CmdRunYarc           = 0xE7
const CmdStopYarc          = 0xE8
const CmdPoll              = 0xE9
const CmdSvcResponse       = 0xEA
const CmdGetVer            = 0xEE
const CmdSync              = 0xEF
const CmdSetArh            = 0xF0
const CmdSetArl            = 0xF1
const CmdSetDrh            = 0xF2
const CmdSetDrl            = 0xF3
const CmdDoCycle           = 0xF4
const CmdGetResult         = 0xF5
const CmdXferSingle        = 0xF8
const CmdWritePage         = 0xF9
const CmdReadPage          = 0xFA

const ErrNosync            = 0x80
const ErrPassive           = 0x81
const ErrOneclock          = 0x82
const ErrCantSs            = 0x83
const ErrCantPg            = 0x84
const ErrInternal          = 0x85
const ErrBadcmd            = 0x86

var ErrorMessages = []string {
	"not synchonized",
	"not allowed when passive",
	"cannot enable multiple clocks",
	"cannot single step with clock enable",
	"invalid state for page transfer",
	"Arduino reported an internal error",
	"invalid command byte",
}

