// Copyright (c) Jeff Berkowitz 2021. All rights reserved.
//
// 456789012345678901234567890123456789012345678901234567890123456789012
//
// YARC-specific utility routines to read and write all YARC resources -
// instruction register IR, flags register F, microcode registers K0 through
// K3, general registers 0 through 3, microcode memory, ALU input and holding
// registers, and ALU memory.
//
void WriteIR(byte high, byte low);
void WriteK(byte k3, byte k2, byte k1, byte k0);
int WriteMicrocode(byte opcode, byte slice, byte *data, byte n, bool panicOnFail);
void WriteMem16(unsigned short addr, unsigned short *data, short nWords);
void ReadMem16(unsigned short addr, unsigned short *data, short nWords);
void WriteMem8(unsigned short addr, unsigned char *data, short nBytes);
void ReadMem8(unsigned short addr, unsigned char *data, short nBytes);
