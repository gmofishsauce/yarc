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
  PortPrivate::internalWriteK(k3, k2, k1, k0);
}

// Write the microcode RAM for slice s [0..3] of opcode n [0x80..0xFF]
// with up to n [usefully, 1 to 64] bytes at *data and verify them. If
// the panic argument is true and verification fails, panic with UCODE_VERIFY
// and subcode = the failed opcode. If the panic argument is false and
// verification fails, return the offset of the failed byte 0..n-1 within
// the data array. Return n for success.
int WriteSlice(byte opcode, byte slice, byte *data, byte n, bool panicOnFail) {
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

// Write the bytes at *data to microcode memory. The length of the data array
// must be 4 * nWords bytes, so this function can write as much as 256 bytes
// of data (in practice, the only place this data could be passed from is the
// poll buffer in the serial task, which can only hold 255 data bytes; thus,
// the largest value of nWords in practice is 63 and the last microcode slot
// of every instruction is unavailable.)
//
// IMPORTANT: the YARC is 16-bit big-endian, meaning the byte with the most
// significant bits of a 16-bit value comes first in memory followed by the
// byte with the less significant bits. The only place we ever have to deal
// with 32-bit quantities is the microcode, that is here. So we maintain the
// big-endian convention. Each microcode word takes 4 bytes in the data array,
// and the bytes are ordered 3, 2, 1, 0. This puts bits 31:24 of the microcode
// word in byte 0, bits 23:16 in byte 1, etc., and so on for each 4-byte
// microcode word. The implementation of this is commented below.
void WriteMicrocode(byte opcode, byte *data, byte nWords) {
  constexpr byte SIZE = 64;
  constexpr byte nSlices = 4;
  byte sliceBuffer[SIZE];

  if (nWords > SIZE) {
    panic(PANIC_ARGUMENT, 5);
  }

  unsigned short nBytes = nSlices * nWords;
  byte bytesPerSlice = nWords;
  byte packed;

  for (byte slice = 0; slice < nSlices; ++slice) {
    packed = 0;
    for (unsigned short i = 0; i < nBytes; i += nSlices) {
      sliceBuffer[packed] = data[slice + i];
      packed++;
    }

    // Here is the implementation of the big-endian microcode: the slice
    // number we pass goes 3, 2, 1, 0 as the variable slice goes 0, 1, 2, 3.
    WriteSlice(opcode, (nSlices - 1) - slice, sliceBuffer, bytesPerSlice, true);
  }
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

// Write the argument value into general register reg, 0..3
void WriteReg(unsigned char reg, unsigned short value) {
    // Set the microcode for a write from sysdata to reg
    // Write this out the long way to keep the autogenerated comments
    WriteK(0xF8 | (reg & 0x03), 0xFF, 0xFE, 0x3F);
    
    // Enable writing to a general register
    SetMCR(McrEnableRegisterWrite(McrEnableSysbus(MCR_SAFE)));

    // Set a write to an arbitrary address not in system RAM in the Nano's address register
    // Set the value 0x7700 (= end of memory - 256) in the Nano's data register
    SetADHL(0x7F, 0xFE, StoHB(value), StoLB(value));

    // Do the write and make all shipshape again
    SingleClock();
    SetMCR(MCR_SAFE); // freeze the registers and the bus

    MakeSafe(); // and turn off everything
}

constexpr unsigned short SCRATCH = 0x7700; // start of scratch space, end of memory - 256 bytes

// The Nano doesn't have a write enable bit for the flags register the way it does
// for the instruction register and the general registers, so it can't write directly
// to the flags. (If we turn on the enable bit in the microcode and have the Nano try
// to write, the value will be corrupted when we try to write to the microcode register
// in order to turn the enable bit back off!) So instead, we have the YARC do it - we
// store the flags value at the scratch location in main memory, set R3 to point at it,
// write some microcode into microcode memory, set the instruction register to refer to
// that, and then enable the YARC and give it a couple of clocks. (This function was the
// first time the YARC ever "did anything" under microcode control.)
void WriteFlags(byte flags) {
  WriteMem8(SCRATCH, &flags, 1);
  byte validFlags;
  ReadMem8(SCRATCH, &validFlags, 1);
  if (flags != validFlags) {
    panic(PANIC_MEM_VERIFY, validFlags); // I guess they're "invalid flags" now ;-)    
  }
  WriteReg(3, SCRATCH);

  constexpr byte SCRATCH_OPCODE = 0x80;
  byte microcode[] = {
    0xFF, 0xFE, 0x9F, 0xFF, // word 0: src1 = R3, load_flags = yes, 8-bit cycle, flags from sysdata
    0xFF, 0xFF, 0xFF, 0xFF  // word 1: ...and then disable everything
  };
  WriteMicrocode(SCRATCH_OPCODE, microcode, 2);

  // Finally run the YARC to clock the value into F
  WriteIR(SCRATCH_OPCODE, 0x00);
  SetMCR(McrEnableYarc(MCR_SAFE));
  SingleClock();
  SingleClock();
  SetMCR(MCR_SAFE);
}


