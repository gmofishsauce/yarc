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

// Read n bytes from the given slice of the given opcode.
void ReadSlice(byte opcode, byte slice, byte *data, byte n) {
  PortPrivate::readBytesFromSlice(opcode | 0x80, slice, data, n);
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

// This function is used by both ReadALU() and WriteALU(). We only need
// to set the low order byte of the 13-bit ALU RAM address, as the high
// order five bits come from other places. And we want the same address
// for both the low RAM and the dual high RAMS.
//
// The operands will be written through to the ALU from R0 (the Port1
// operand) and R1 (the Port2 operand). The address lines of the RAMs
// are "swizzled" so that the low-order nybbles of both operands address
// the low RAM and the high-order nybbles address the high RAMs. Example:
// during phase 1 of an ALU operation (phi1), Port1 operand XXBA and Port2
// operand XXDC will address CA on the low RAM and DB on the high RAMs.
//
// Here, we have just one address. We want only the low byte for all three
// RAMs. Call it XXFE. If we simply store 0x00FE in both R1 and R0, the
// addresses will be EE on the low RAM and FF on the high RAMs. So instead
// we need to duplicate the low nybble in the Port1 (R0) operand and
// duplicate the high nybble in the Port2 (R1) operand, which will cause
// both sets of address lines to be 0xXXFE.
void swizzleAddressToR1R0(unsigned short addr) {
    unsigned short bits;
    // Construct the ALU Port1 address and store it in R0
    bits = addr & 0x000F;
    bits = bits | (bits << 4);
    WriteReg(0, bits);
    // Now the eventual Port2 operand
    bits = (addr & 0x00F0) >> 4;
    bits = bits | (bits << 4);
    WriteReg(1, bits);
}

// Write and then validate up to n bytes of data to the ALU RAM at the given
// address offset, where offset is a multiple of 64, n is exactly 64, and
// offset + n <= END_ALU_MEM. There are three ALU RAMs that are written in
// parallel but read separately, so we do four operations per byte. Doing
// them together is faster than making a call to WriteALU() and then calling
// ReadALU() three times, because here we only have to set up the addressing
// once for each byte that is written and read - thanks to the alignment
// restriction, none of the four high order address bits nor the carry bit
// in the ACR will change during a single call.
void WriteCheckALU(unsigned short offset, byte *data, unsigned short n) {
  if ((offset&0x1FFC) != offset || n != 64) {
    panic(PANIC_ARGUMENT, 10);
  }

  for (unsigned short addr = offset; addr < offset + n; ++addr, ++data) {
    // Set the low order bits of the RAM address in R1 and R0
    swizzleAddressToR1R0(addr);

    // Now the four high order address bits. These bits come from the
    // microcode although they could probably come from the IR if I 
    // changed the WR_ALU_FROM_NANO microcode word to select it. We
    // only have to set this once because the write is aligned.
    byte aluBits = (addr >> 9) & 0x000F;
    WriteK(WR_ALU_RAM_FROM_NANO(aluBits));

    // Now set the ACR, including address bit :8 (the carry bit),
    // to do a write. And then do the write.
    byte acrBits = AcrSetOp(ACR_SAFE, ACR_WRITE);
    acrBits = AcrSetA8(acrBits, (addr & 0x100) ? 1 : 0);
    SetACR(AcrEnable(acrBits));

    SetMCR(McrEnableWcs(MCR_SAFE));
    SetADHL(0x7F, 0xFF, 0xBB, *data);
    SingleClock();
    SetACR(ACR_SAFE);
    SetMCR(MCR_SAFE);

    // Now read back and check all 3 RAMs. Since the high order
    // address bits won't change within a call, we only need to
    // set them once, along with setting the control lines to read.
    WriteK(RD_ALU_RAM_FROM_NANO(aluBits));

    // The key optimization over calling WriteALU() and then three
    // ReadALU() calls is here: we don't have to WriteK in the loop.
    for (byte ram = 0; ram < 3; ++ram) {
      byte acrBits = AcrSetOp(ACR_SAFE, ram);
      acrBits = AcrSetA8(acrBits, (addr & 0x100) ? 1 : 0);
      SetACR(AcrEnable(acrBits));
      SetMCR(McrEnableWcs(MCR_SAFE));
      SetADHL(0xFF, 0xFF, 0xCC, 0xBB);
      SingleClock();
      bool ok = (GetBIR() == *data);
      SetACR(ACR_SAFE);
      SetMCR(MCR_SAFE);
      if (!ok) {
        // This panic can be confused with statically-allocated
        // panic codes, but it's worth it to get some information.
        panic(n, ram);
      }
    }
  }
  WriteK(MICROCODE_IDLE);
}

// Write up to "n" bytes of data to ALU RAM at the given offset. The values
// are not read back for validation. The ALU contains three physical RAMs,
// but the hardware writes them in parallel with the same data. They can be
// be read back separately for validation.
void WriteALU(unsigned short offset, byte *data, unsigned short n) {
  if (offset >= END_ALU_MEM || n >= 256 || offset + n > END_ALU_MEM) {
    panic(PANIC_ARGUMENT, 10);
  }

  for (unsigned short addr = offset; addr < offset + n; ++addr, ++data) {
    // Set the low order bits of the RAM address in R1 and R0
    swizzleAddressToR1R0(addr);

    // For debugging, write the K register with a harmless high order two bits of
    // 00 so that R0 appears on the S1BUS (input to the operand A holding register).
    // This allows me to check it with a scope. Since -something- always appears
    // there, R0 (00) is as good as any of the other three choices. We remove this
    // for production because writing K is very slow.
    // WriteK(0x3F, 0xFF, 0xFF, 0xFF);

    // Now the five high order address bits. Address bit 8 is the carry input A8 to
    // all three RAMs when the Nano has control. It comes from the ALU Control
    // Register (ACR) bit 3. The other bits come from the microcode.
    byte acrBits = AcrSetOp(ACR_SAFE, ACR_WRITE);
    acrBits = AcrSetA8(acrBits, (addr & 0x100) ? 1 : 0);
    SetACR(acrBits);
    byte aluBits = (addr >> 9) & 0x000F;
    WriteK(WR_ALU_RAM_FROM_NANO(aluBits));

    // Finally do the write - the low order bit of ACR is the enable. We share
    // the "KX" back bus to the ALU RAM with the microcode and K register; the
    // MCR code calls this EnableWCS() (writeable control store).
    SetACR(AcrEnable(acrBits));
    SetMCR(McrEnableWcs(MCR_SAFE));
    SetADHL(0x7F, 0xFF, 0xBB, *data);
    SingleClock();
    SetACR(ACR_SAFE);
    SetMCR(MCR_SAFE);
  }
  WriteK(MICROCODE_IDLE);
}

// Read "n" bytes of data from the address offset of the ALU RAM identified
// by the ramID argument into *data. The ramID is 0 for the low nybble RAM,
// 1 for the high nybble carry = 0 RAM, and 2 for the high nybble carry = 1
// RAM. 
void ReadALU(unsigned short offset, byte *data, unsigned short n, byte ramID) {
  if (offset >= END_ALU_MEM || n > 256 || offset + n > END_ALU_MEM) {
    panic(PANIC_ARGUMENT, 11);
  }
  if (ramID > 2) {
    panic(PANIC_ARGUMENT, 12);
  }

  for (unsigned short addr = offset; addr < offset + n; ++addr, ++data) {
    // Set the low order bits of the RAM address in R1 and R0
    swizzleAddressToR1R0(addr);

    // For debugging, write the K register with a harmless high order two bits of
    // 00 so that R0 appears on the S1BUS (input to the operand A holding register).
    // This allows me to check it with a scope. Since -something- always appears
    // there, R0 (00) is as good as any of the other three choices. We remove this
    // for production because writing K is very slow.
    // WriteK(0x3F, 0xFF, 0xFF, 0xFF);

    // Now the five high order address bits. Address bit 8 is the carry input A8 to
    // all three RAMs when the Nano has control. It comes from the ALU Control
    // Register (ACR) bit 3.    
    byte acrBits = AcrSetOp(ACR_SAFE, ramID);
    acrBits = AcrSetA8(acrBits, (addr & 0x100) ? 1 : 0);
    SetACR(acrBits);
    byte aluBits = (addr >> 9) & 0x000F;
    WriteK(RD_ALU_RAM_FROM_NANO(aluBits));

    // Finally do the read. We need to set the Nano's direction line (A15) to 1
    // to reverse the KX bus transceiver. We set the address in I/O space so that
    // main memory doesn't respond and try to drive the data bus. The DH and DL
    // registers don't matter.
    SetACR(AcrEnable(acrBits));
    SetMCR(McrEnableWcs(MCR_SAFE));
    SetADHL(0xFF, 0xFF, 0xCC, 0xBB);
    SingleClock();
    *data = GetBIR();
    SetACR(ACR_SAFE);
    SetMCR(MCR_SAFE);
  }
  WriteK(MICROCODE_IDLE);
}

// Write the bytes at *data to microcode memory. The length of the data array
// must be 4 * nWords bytes, so this function can write as much as 256 bytes
// of data.
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
  WriteK(WRMEM16_FROM_NANO);
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
  WriteK(RDMEM8_TO_NANO);
  SetMCR(McrEnableSysbus(MCR_SAFE));

  // The Nano can only read bytes (not words) from the data bus
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
  WriteK(WRMEM8_FROM_NANO);
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

// This dangerous function assumes the caller has set the K register,
// which is slow. Returns the bus value. Used by the downloader.
// byte WrMemFast(unsigned short addr, byte data) {
//   SetMCR(MCR_SAFE);
//   SetADHL(StoHB(addr & 0x7F00), StoLB(addr), 0x99, StoLB(data));
//   SingleClock();
//   byte result = GetBIR();
//   SetMCR(MCR_SAFE);
//   return result;
// }

// This dangerous function matches WrMemFast()
// byte RdMemFast(unsigned short addr) {
//   SetMCR(McrEnableSysbus(MCR_SAFE));
//   SetAH(StoHB(addr | 0x8000));
//   SetAL(StoLB(addr));
//   SingleClock();
//   byte result = GetBIR();
//   SetMCR(MCR_SAFE);
//   return result;
// }

// Read nBytes into *data from contiguous addresses starting at addr.
// All Nano machine state and the K register are altered.
void ReadMem8(unsigned short addr, unsigned char *data, short nBytes) {
  if (nBytes < 0) {
    panic(PANIC_ARGUMENT, 4);
  }
  WriteK(RDMEM8_TO_NANO); // read memory byte
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
// This does not require running the YARC; the Nano can do it.
void WriteReg(unsigned char reg, unsigned short value) {
  // Set the microcode for a write from sysdata to reg
  WriteK(LOAD_REG_16_FROM_NANO(reg));    
  // Enable writing to a general register
  SetMCR(McrEnableRegisterWrite(MCR_SAFE));
  SetADHL(0x7F, 0xFE, StoHB(value), StoLB(value));
  SingleClock();
  SetMCR(MCR_SAFE);

  // This is absolutely required. I'm not completely sure why,
  // but it makes sense - the microcode word in K still says
  // to write the same register on the next clock. This will
  // not have any effect so long as the Nano has control of
  // the buses because of the register write enable/disable
  // bit in the MCR. But as soon as we enable YARC, the next
  // clock will write garbage into the target register unless
  // we put the current microcode word back to inactivity.
  WriteK(0xFF, 0xFF, 0xFF, 0xFF);
}

// Read the value of given general register. This function moves the register contents
// to the given location in memory (typically in the scratch space). The Nano never
// disables its address bus drivers as long as it has control, so the write is rather
// odd - the Nano supplies the address, but thinks it is doing a read cycle; the YARC
// general registers supply the data, and the YARC thinks it's doing a write cycle;
// the memory controller "listens" to the YARC's signals and does the write.
unsigned short ReadReg(unsigned char reg, unsigned short memAddr) {
  WriteK(STORE_REG_16_TO_MEMORY(reg));
  SetMCR(McrEnableSysbus(MCR_SAFE));
  SetADHL(0x80 | StoHB(memAddr), StoLB(memAddr), 0xAA, 0x55);
  SingleClock();
  SetMCR(MCR_SAFE);

  // Now since the bus input register is clocked on every clock, it holds the low
  // byte of whatever was transferred to memory, i.e. the low byte of the register.
  byte low = GetBIR();
  // Similarly to WriteReg, we should put the microcode word back to inactivity now.
  WriteK(0xFF, 0xFF, 0xFF, 0xFF);
  byte high;
  ReadMem8(memAddr + 1, &high, 1);
  return BtoS(high, low);
}

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
  WriteMem8(SCRATCH_MEM, &flags, 1);
  byte validFlags;
  ReadMem8(SCRATCH_MEM, &validFlags, 1);
  if (flags != validFlags) {
    panic(PANIC_MEM_VERIFY, validFlags); // I guess they're "invalid flags" now ;-)    
  }
  WriteReg(3, SCRATCH_MEM);

  byte microcode[] = {
    LOAD_FLAGS_INDIRECT_R3,
    MICROCODE_IDLE
  };
  WriteMicrocode(SCRATCH_OPCODE_F0, microcode, 2);

  // Finally run the YARC to clock the value into F
  WriteIR(SCRATCH_OPCODE_F0, 0x00);
  SetMCR(McrEnableYarc(McrEnableSysbus((MCR_SAFE))));
  SingleClock();
  SingleClock();
  SetMCR(MCR_SAFE);
}

// Read the flags register
byte ReadFlags() {
  WriteK(RD_FLAGS_TO_NANO); // read flags byte
  SetMCR(McrEnableSysbus(MCR_SAFE));
  SingleClock();
  SetMCR(MCR_SAFE);
  return GetBIR();
}


