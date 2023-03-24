// Copyright (c) Jeff Berkowitz 2021. All rights reserved.
//
// 456789012345678901234567890123456789012345678901234567890123456789012
//
// YARC-specific utility routines to read and write all YARC resources -
// instruction register IR, flags register F, microcode registers K0 through
// K3, general registers 0 through 3, microcode memory, ALU input and holding
// registers, and ALU memory.
//
// Write a 16-bit value to the instruction register
void WriteIR(byte high, byte low) {
  SetAH(0x7F); SetAL(0xFF);
  SetDH(high); SetDL(low);
  SetMCR(McrEnableIRwrite(MCR_SAFE));
  SingleClock();
  SetMCR(McrDisableIRwrite(MCR_SAFE));
}

// Write all bytes of the K (microcode pipeline, "control") register.
//
// This could be made far more efficient by implementing it in
// the PortPrivate section. Each call to PortPrivate:writeByteToK()
// currently does 64 clocks in order to disable the microcode RAM
// outputs. It could then do four writes, but each call only does
// one. This isn't (currently?) important enough to worry about.
//
// This function alters essentially all external registers under
// the Nano's control and does not restore them. The K register is
// not accessible for reading, so there is no verify.
void WriteK(byte k3, byte k2, byte k1, byte k0) {
  PortPrivate::writeByteToK(3, k3);
  PortPrivate::writeByteToK(2, k2);
  PortPrivate::writeByteToK(1, k1);
  PortPrivate::writeByteToK(0, k0);
}

// Write the microcode RAM for slice s [0..3] of opcode n [0x80..0xFF]
// with up to n [usefully, 1 to 64] bytes at *data and verify them.
// panic: UCODE_VERIFY with subcode = the failed opcode
void WriteMicrocode(byte opcode, byte slice, byte *data, byte n) {
  PortPrivate::writeBytesToSlice(opcode, slice, data, n);
  byte written[64];
  PortPrivate::readBytesFromSlice(opcode, slice, written, n);
  for (int i = 0; i < n; ++i) {
    if (data[i] != written[i]) {
      panic(PANIC_UCODE_VERIFY, opcode);
    }
  }
}


