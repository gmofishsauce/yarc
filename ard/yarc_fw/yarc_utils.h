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
// with up to n [usefully, 1 to 64] bytes at *data and verify them. If
// the panic argument is true and verification fails, panic with UCODE_VERIFY
// and subcode = the failed opcode. If the panic argument is false and
// verification fails, return the offset of the failed byte 0..n-1 within
// the data array. Return n for success.
int WriteMicrocode(byte opcode, byte slice, byte *data, byte n, bool panicOnFail) {
  PortPrivate::writeBytesToSlice(opcode | 0x80, slice, data, n);
  byte written[64];
  PortPrivate::readBytesFromSlice(opcode | 0x80, slice, written, n);
  for (int i = 0; i < n; ++i) {
    if (data[i] != written[i]) {
      if (panicOnFail) {
        panic(PANIC_UCODE_VERIFY, opcode);
      }
      return i;
    }
  }
  return n;
}

// Write nWords 16-bit words at *data into contiguous addresses starting
// at addr. Addr must be aligned (even). All Nano machine state and the
// K register are altered. The write is not verified.
void WriteMem16(unsigned short addr, unsigned short *data, short nWords) {
  if (addr & 1) {
    panic(PANIC_ALIGNMENT, 1);
  }
  if (nWords < 0) {
    panic(PANIC_ARGUMENT, 1);
  }
  WriteK(0xFF, 0xFF, 0xFF, 0x3F);  // write memory, 16-bit access
  SetMCR(MCR_SAFE);
  for (short i = 0; i < nWords; ++i) {
    SetADHL(StoHB(addr & 0x7F00), StoLB(addr), StoHB(*data), StoLB(*data));
    SingleClock();
    addr += 2;
    data++;      
  }
  SetMCR(MCR_SAFE);
}

// Read nWords 16-bit words into *data from contiguous addresses starting
// at addr. All Nano machine state and the K register are altered.
void ReadMem16(unsigned short addr, unsigned short *data, short nWords) {
  if (addr & 1) {
    panic(PANIC_ALIGNMENT, 2);
  }
  if (nWords < 0) {
    panic(PANIC_ARGUMENT, 2);
  }

  WriteK(0xFF, 0xFF, 0x9F, 0xFF); // read memory byte
  SetMCR(McrEnableSysbus(MCR_SAFE));

  // The Nano can only read bytes from the data bus
  unsigned char *bytes = (unsigned char *)data;
  for (int i = 0; i < 2 * nWords; ++i) {
    // The data values are noise that's not supposed to matter
    SetADHL(StoHB(addr | 0x8000), StoLB(addr), 0xAA, 0x55);
    SingleClock();
    *bytes++ = GetBIR();
    addr++;
  }
  SetMCR(MCR_SAFE);
}

// Write nBytes from *data at contiguous addresses starting at addr. All
// Nano machine state and the K register are altered.
void WriteMem8(unsigned short addr, unsigned char *data, short nBytes) {
  if (nBytes < 0) {
    panic(PANIC_ARGUMENT, 3);
  }
  WriteK(0xFF, 0xFF, 0xFF, 0x7F);  // write memory, 8-bit access
  SetMCR(MCR_SAFE);
  for (int i = 0; i < nBytes; ++i) {
    // The high data byte is a noise value that is not supposed to matter.
    SetADHL(StoHB(addr & 0x7F00), StoLB(addr), 0x99, StoLB(*data));
    SingleClock();
    addr++;
    data++;
  }  
  SetMCR(MCR_SAFE);
}

// Read nBytes into *data from contiguous addresses starting at addr.
// All Nano machine state and the K register are altered.
void ReadMem8(unsigned short addr, unsigned char *data, short nBytes) {
  if (nBytes < 0) {
    panic(PANIC_ARGUMENT, 4);
  }
  WriteK(0xFF, 0xFF, 0x9F, 0xFF); // read memory byte
  SetMCR(McrEnableSysbus(MCR_SAFE));

  for (int i = 0; i < nBytes; ++i) {
    // The data values are noise that's not supposed to matter
    SetADHL(StoHB(addr | 0x8000), StoLB(addr), 0xAA, 0x55);
    SingleClock();
    *data++ = GetBIR();
    addr++;
  }
  SetMCR(MCR_SAFE);
}

