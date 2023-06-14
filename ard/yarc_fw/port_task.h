// Copyright (c) Jeff Berkowitz 2021. All rights reserved.
//
// 456789012345678901234567890123456789012345678901234567890123456789012
//
// The lowest layer of the port support (roughly, the nanoXYZ() functions)
// has been moved into port_utils.h because this file got too large. There
// is an extensive comment in that file.

namespace PortPrivate {

  // Initialization: I've never been able to make up my mind about how it
  // should work. This much is clear: yarc_fw.ino::setup() calls InitTasks()
  // which calls the init functions of all the tasks, in the order specified
  // in the static task definition table in task_runner.h. At this point all
  // the system facilities are usable. Then InitTasks() calls postInit() which
  // just calls PortPrivate::internalPostInit() near line 270 in this file.
  // If postInit() returns false, InitTasks() calls panic(). internalPostInit()
  // has some built-in functionality and calls out to the following three
  // functions.

  void callWhenAnyReset(void);      // Called from the top of postInit() always
  void callWhenPowerOnReset(void);  // Called only when power-on reset occurring
  void callAfterPostInit(void);     // Called from the end of postInit() always
  
  // Ahem. The internal bus that connects the system data bus to the
  // four slice busses is wired backwards. So all the bits written to
  // the K register or to microcode memory are reversed, LSB for MSB,
  // etc. Rewiring is possible but messy. Instead, we reverse all bits
  // written to K or microcode RAM when we write them and reverse them
  // back when we read microcode RAM (K is not directly readable). The
  // read and write functions are messy, pretty much guaranteeing there
  // is no other way to get at the memory that would go "around" the
  // bit reversal function. The table is in program memory, which we
  // have plenty of, and performance isn't particularly critical because
  // writes only happen during system initialization. The code came
  // from StackOverflow and probably cannot be covered by copyright.
  
  byte reverse_byte(byte b) {
    static const PROGMEM byte table[] = {
        0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
        0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
        0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
        0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
        0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
        0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
        0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
        0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
        0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
        0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
        0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
        0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
        0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
        0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
        0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
        0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
        0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
        0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
        0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
        0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
        0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
        0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
        0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
        0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
        0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
        0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
        0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
        0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
        0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
        0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
        0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
        0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff,
      };
	  // Get the reversed by from program (flash) memory
      return pgm_read_ptr_near(&table[b]);
  }

  void disableMicrocodeRamOutputs() {
    nanoTogglePulse(DisableUCRamOut);
  }

  void enableMicrocodeRamOutputs() {
    nanoTogglePulse(EnableUCRamOut);
  }

  // Write the K register. The arguments follow the big-endian
  // convention we have for microcode.
  void internalWriteK(byte k3, byte k2, byte k1, byte k0) {
    disableMicrocodeRamOutputs();

    ucrSetDirectionWrite();
    ucrEnableSliceTransceiver();
    ucrSetKRegWrite();

    ucrSetSlice(3);
    syncUCR();
    SetMCR(McrEnableWcs(MCR_SAFE));
    setAH(0x7F); setAL(0xFF);
    setDH(0x00); setDL(reverse_byte(k3));
    singleClock();
    SetMCR(MCR_SAFE);

    ucrSetSlice(2);
    syncUCR();
    SetMCR(McrEnableWcs(MCR_SAFE));
    setAH(0x7F); setAL(0xFF);
    setDH(0x00); setDL(reverse_byte(k2));
    singleClock();
    SetMCR(MCR_SAFE);

    ucrSetSlice(1);
    syncUCR();
    SetMCR(McrEnableWcs(MCR_SAFE));
    setAH(0x7F); setAL(0xFF);
    setDH(0x00); setDL(reverse_byte(k1));
    singleClock();
    SetMCR(MCR_SAFE);

    ucrSetSlice(0);
    syncUCR();
    SetMCR(McrEnableWcs(MCR_SAFE));
    setAH(0x7F); setAL(0xFF);
    setDH(0x00); setDL(reverse_byte(k0));
    singleClock();
    SetMCR(MCR_SAFE);

    ucrMakeSafe();
    enableMicrocodeRamOutputs();
    setAH(0xFF); 
    McrMakeSafe();
  }

  // Write up to 64 bytes to the slice for the given opcode, which must be
  // in the range 128 ... 255.
  void writeBytesToSlice(byte opcode, byte slice, byte *data, byte n) {
    WriteIR(opcode, 0);
    disableMicrocodeRamOutputs();

    // Now RAM OE# is high and we can safely enable the slice transceiver
    // to the write state without a bus conflict.
    
    // Set up the UCR for writes.
    ucrSetSlice(slice);
    ucrSetDirectionWrite();
    ucrEnableSliceTransceiver();
    ucrSetRAMWrite();
    syncUCR();

    setAH(0x7F); setAL(0xFF);
    setDH(0x00);
    for (int i = 0; i < n; ++i, ++data) {
      setDL(reverse_byte(*data));
      SetMCR(McrEnableWcs(MCR_SAFE));
      singleClock();
      SetMCR(McrDisableWcs(MCR_SAFE));
    }

    ucrMakeSafe();
    enableMicrocodeRamOutputs();
  }

  // Read up to 64 bytes from the slice for the given opcode, which must be
  // in the range 128 ... 255.
  void readBytesFromSlice(byte opcode, byte slice, byte *data, byte n) {
    // Write the opcode to the IR. This sets the upper address bits to the
    // opcode and resets the state counter (setting lower address bits to 0)
    WriteIR(opcode, 0);
    
    // Set up the UCR for reads.
    ucrSetSlice(slice);
    ucrSetDirectionRead();
    ucrSetRAMRead();
    ucrEnableSliceTransceiver();
    syncUCR();

    setAH(0xFF); setAL(0xFF);
    for (int i = 0; i < n; ++i, ++data) {
      SetMCR(McrEnableWcs(MCR_SAFE));
      singleClock();
      *data = reverse_byte(getBIR());
      SetMCR(McrDisableWcs(MCR_SAFE));
    }

    ucrMakeSafe();
  }

  // Set the four K (microcode) registers to their "safe" value.
  void kRegMakeSafe() {
    internalWriteK(0xFF, 0xFF, 0xFF, 0xFF);
    ucrMakeSafe();
  }

  void internalMakeSafe() {
    kRegMakeSafe();
    ucrMakeSafe();
    AcrMakeSafe();
    setAH(0xFF); 
    setAL(0xFF);
    McrMakeSafe();
  }

  // Because of the order of initialization, this is basically
  // the very first code executed on either a hard or soft reset.
  // This (and all the init() functions) should be fast.
  
  void internalPortInit() {
    // Set the two decoder select pins to outputs. Delay after making
    // any change to this register.
    DDRC = DDRC | (_BV(DDC3) | _BV(DDC4));
    delayMicroseconds(2);
  
    // Turn off both of the decoder select lines so no decoder outputs
    // are active.
    PORTC &= ~(_BV(PORTC3) | _BV(PORTC4));
    
    nanoSetMode(portData,   OUTPUT);
    nanoSetMode(portSelect, OUTPUT);

    internalMakeSafe();
  }

  // Run the YARC. This should take a 2-byte argument which becomes the initial
  // PC in R3, but for now it's just a zero. This function loads 0x01 into the IR,
  // which is a jmp to address 0. The first clock will the microcode address 
  // 0b1_1111_1100_0000, the base of the last group of 64 slots, which contains
  // the microcode for JMP. This will load the IR (nanded with 0x0001) into R3
  // and fetch from there.
  void internalRunYARC() {
    WriteReg(0, 0);
    WriteReg(1, 0);
    WriteReg(2, 0);
    WriteReg(3, 0);
    internalMakeSafe();
    WriteIR(0, 1);
    SetMCR(McrEnableSysbus(McrEnableYarc(MCR_SAFE)));
  }

  void internalStopYARC() {
    SetMCR(MCR_SAFE);
    internalMakeSafe();
  }

  // PostInit() is called from setup after the init() functions are called for all the firmware tasks.
  // The name is a pun, because POST stands for Power On Self Test in addition to meaning "after". But
  // the "power on" part is a misleading pun, because postInit() runs on both power-on resets and "soft"
  // resets (of the Nano only) that occur when the host opens the serial port.
  //
  // The hardware allows the Nano to detect power-on reset by reading bit 0x08 of the MCR. A 0 value
  // means the YARC is in the reset state. This state lasts at least two seconds after power-on, much
  // longer than it takes the Nano to initialize. The Nano detects this and performs initialization
  // steps both before and after the YARC comes out of the POR state as can be seen in the code below.
    
  // Power on self test and initialization. Startup will hang if this function returns false.

  bool internalPostInit() {

    // All the internalInit functions have been called, so all
    // the Nano's system facilities are supposed to be available.

    callWhenAnyReset();

    if (YarcIsPowerOnReset()) {
      callWhenPowerOnReset();
    }
                
    // Now reset the "request service" flip-flop from the YARC
    // so we don't later see a false service request.
    nanoTogglePulse(ResetService);
    if (YarcRequestsService()) {
      panic(PANIC_POST, 3);      
    }
  
    // Read and write the first byte of YARC RAM as the quickest
    // possible check for basic functionality. This is supposed
    // to work with the YARC still in the power on reset state.
    unsigned short toWrite = 0xAA;
    unsigned short toRead = 0x55;
    WriteMem16(0, &toWrite, 1);
    ReadMem16(0, &toRead, 1);
    if (toWrite != toRead) {
      panic(PANIC_POST, 5);
    }

    // Wait for the power on reset signal to clear
    for (long now = millis(), end = millis() + 5000; now < end; now = millis()) {
      if (!YarcIsPowerOnReset()) break;
    }
    if (YarcIsPowerOnReset()) {
      panic(PANIC_POST, 6); // POR# never went high
    }
    
    // Now do some other tests, which can panic.
    callAfterPostInit();
    internalMakeSafe();
    SetDisplay(0xC0);
    return true;
  }
} // End of PortPrivate section

// Public interface to ports

void portInit() {
  PortPrivate::internalPortInit();
}

int portTask() {
  return 171;
}

bool postInit() {
  return PortPrivate::internalPostInit();
}

// Interface to the 4 write-only bus registers: setAH
// (address high), AL, DH (data high), DL.
  
void SetAH(byte b) {
  PortPrivate::setAH(b);
}

void SetAL(byte b) {
  PortPrivate::setAL(b);
}
  
void SetDH(byte b) {
  PortPrivate::setDH(b);
}

void SetDL(byte b) {
  PortPrivate::setDL(b);
}

// Public interface to the read registers: Bus Input Register
// and the readback value of the MCR.

byte GetBIR() {
  return PortPrivate::getBIR();
}

byte GetMCR() {
  return PortPrivate::getMCR();
}

// Change of philosophy: direct set of MCR

void SetMCR(byte b) {
  PortPrivate::setMCR(b);
}

void SingleClock() {
  PortPrivate::singleClock();
}

// Public interface to the write-only 8-bit Display Register (DR)

void SetDisplay(byte b) {
  PortPrivate::nanoSetRegister(PortPrivate::DisplayRegister, b);
}

void MakeSafe() {
  PortPrivate::internalMakeSafe();
}

// Set the bus registers (AH, AL, DH, DL)
void SetADHL(byte ah, byte al, byte dh, byte dl) {
  SetAH(ah);
  SetAL(al);
  SetDH(dh);
  SetDL(dl);  
}

// Set the YARC to RUN mode. Do not alter the clock settings, i.e.
// don't start the clock running.
void RunYARC() {
  PortPrivate::internalRunYARC();
}

void StopYARC() {
  PortPrivate::internalStopYARC();
}

// These are convenience functions. Making them functions allows me to stash them
// at the very bottom of the file.
namespace PortPrivate {

  void callWhenPowerOnReset() {
  }

  void callWhenAnyReset() {
    SerialReset();
  }

  void callAfterPostInit() {
    union {
      byte bytes[64];
      unsigned short words[32];
    };

    constexpr short BCOUNT = sizeof(bytes);
    constexpr short WCOUNT = sizeof(words) >> 1;

    for (byte b = 0; b < BCOUNT; ++b) {
      bytes[b] = 0xFF;      
    }

    for (byte n = 0, b = 0x80; b != 0; b++, n++) {
      SetDisplay(n);
      WriteSlice(b, 0, bytes, BCOUNT, true); 
      WriteSlice(b, 1, bytes, BCOUNT, true); 
      WriteSlice(b, 2, bytes, BCOUNT, true); 
      WriteSlice(b, 3, bytes, BCOUNT, true); 
    }

    for (byte w = 0; w < WCOUNT; ++w) {
      words[w] = 0x00;
    }

    for (unsigned short addr = 0; addr < END_MEM; addr += BCOUNT) {
      SetDisplay(addr >> 8);
      WriteMem16(addr, words, WCOUNT);
    }

    SetDisplay(0xCC);
    enableMicrocodeRamOutputs();
    MakeSafe();
  }
}


