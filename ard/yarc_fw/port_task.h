// Copyright (c) Jeff Berkowitz 2021. All rights reserved.
//
// 456789012345678901234567890123456789012345678901234567890123456789012
//
// The lowest layer of the port support (roughly, the nanoXYZ() functions)
// has been moved into port_utils.h because this file got too large. There
// is an extensive comment in that file.

namespace PortPrivate {

  // Write a 16-bit value to the instruction register
  void writeIR(byte high, byte low) {
    setAH(0x7F); setAL(0xFF);
    setDH(high); setDL(low);
    mcrEnableIRwrite(); syncMCR();
    singleClock();
    mcrDisableIRwrite(); syncMCR();
  }

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

  // Write the value to the given K register. The RAM byte
  // addressed by the IR, the state counter, and the selected
  // slice is also overwritten (there is no alternative that
  // doesn't involve additional logic and wiring).
  //
  // This function updates the hardware and alters the WCS
  // RAM address. Starting conditions: Clock is high, UCR
  // is 0xFF (everything disabled), WCS/sysdata transceiver
  // disabled (MCR bit), and sysaddr:15 is high (read).
  void writeByteToK(byte kRegister, byte value) {

    // "write block trick"
    // BEFORE setting the UCR to write, load the IR and then
    // clock it 64 times to raise the OE# signal on the RAMs.
    // This sets the RAM location that will be overwritten;
	  // currently it's set to 0xFC, which is not used; we could
	  // carefully write the RAM "last" if necessary.
    writeIR(0xFC, 0x00);
    for (int i = 0; i < 64; ++i) {
      singleClock();
    }

    // The RAM output enable (OE#) line, which is connected
    // to the 64-weight bit of the microcode state counter,
    // should now be high, disabling the RAM outputs.
    
    // Set up the UCR for writes.
    ucrSetSlice(kRegister);
    ucrSetDirectionWrite();
    ucrEnableSliceTransceiver();
    ucrSetKRegWrite();
    syncUCR();

    // Enable the sysdata to microcode transceiver
    // and clock the data.
    mcrEnableWcs();
    syncMCR();
    
    // Set the data and address. The write line matters to
    // the WCS transceiver which we enable later, but doesn't
    // affect the inner, per-slice transceiver.
    setAH(0x7F); setAL(0xFF);
    setDH(0x00); setDL(reverse_byte(value));
    singleClock();

    mcrMakeSafe();
    ucrMakeSafe();
    writeIR(0xFC, 0x00); // reload state counter. This re-enables RAM output.
    setAH(0xFF); 
  }

  // Write up to 64 bytes to the slice for the given opcode, which must be
  // in the range 128 ... 255.
  void writeBytesToSlice(byte opcode, byte slice, byte *data, byte n) {
    // Write the opcode to the IR. This sets the upper address bits to the
    // opcode and resets the state counter (setting lower address bits to 0)
    writeIR(opcode, 0);

    // write block trick
    // BEFORE setting the UCR to write, generate 64 clock pulses
    // to raise the OE# signal on the RAMs.
    for (int i = 0; i < 64; ++i) {
      singleClock();
    }

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
      mcrEnableWcs(); syncMCR();
      singleClock();
      mcrDisableWcs(); syncMCR();
    }

    ucrMakeSafe();
    writeIR(opcode, 0); // re-enable RAM outputs (no longer in the "write block")
  }

  // Read up to 64 bytes from the slice for the given opcode, which must be
  // in the range 128 ... 255.
  void readBytesFromSlice(byte opcode, byte slice, byte *data, byte n) {
    // Write the opcode to the IR. This sets the upper address bits to the
    // opcode and resets the state counter (setting lower address bits to 0)
    writeIR(opcode, 0);
    
    // Set up the UCR for reads.
    ucrSetSlice(slice);
    ucrSetDirectionRead();
    ucrSetRAMRead();
    ucrEnableSliceTransceiver();
    syncUCR();

    setAH(0xFF); setAL(0xFF);
    for (int i = 0; i < n; ++i, ++data) {
      mcrEnableWcs(); syncMCR();
      singleClock();
      *data = reverse_byte(getBIR());
      mcrDisableWcs(); syncMCR();
    }

    ucrMakeSafe();
  }

  // Set the four K (microcode) registers to their "safe" value.
  void kRegMakeSafe() {
    writeByteToK(0, 0xFF);
    writeByteToK(1, 0xFF);
    writeByteToK(2, 0xFF);
    writeByteToK(3, 0xFF);
    ucrMakeSafe();
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

    // Now put the MCR in a known state, use it to put the K registers
    // and UCR in a safe state, and then put the MCR back in a safe state.
    mcrMakeSafe();
    kRegMakeSafe();
    mcrMakeSafe();
  }

  // Run the YARC.
  void internalRunYARC() {
    kRegMakeSafe();
    ucrMakeSafe();
    mcrMakeSafe();
    
    mcrEnableSysbus();      // Allow other than Nano to drive the bus
    mcrEnableYarc();        // lock the Nano off the bus
    mcrEnableFastclock();   // enable the YARC to run at speed
    syncMCR();
  }

  void internalMakeSafe() {
    kRegMakeSafe();
    ucrMakeSafe();
    writeIR(0xFC, 0x00); // reload state counter. This re-enables RAM output.
    setAH(0xFF); 
    setAL(0xFF);
    mcrMakeSafe();
    setDisplay(0xC0);
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
    // Do unconditionally on any reset
    makeSafe();

#if 0
    WriteByteToK(3, 0xFF);
    WriteByteToK(2, 0xFF);
    WriteByteToK(1, 0xBF); // 0B1011_1111 (101 in the high order bits)

    SetMCR(0xDB); // YARC/NANO# low, SYSBUS_EN low, all other high
    setDisplay(0xC6);
    byte base = 0;
    byte dh = 0;
    byte dl = 0xAA;
    byte ah = 0x00;
    byte al = 0x00;

    for(;;) {
      WriteByteToK(0, 0xBF); // m16 bit low - enable 16 bit memory cycles
      SetMCR(0xDB); // YARC/NANO# low, SYSBUS_EN low, all other high

      SetAH(ah);  // mem write to 0
      SetAL(al);
      SetDL(dl);
      SetDH(dh);      
      SingleClock();
    
      SetAH(ah | 0x80); // mem read 0
      SingleClock();

      if (PortPrivate::getBIR() != dl) {
        setDisplay(0x7E);
      }

      WriteByteToK(0, 0xFF); // m16 bit high - byte cycle
      SetMCR(0xDB); // YARC/NANO# low, SYSBUS_EN low, all other high
      SetAH(ah | 0x80);
      SetAL(al | 0x01);   
      SingleClock(); // 8 bit mem read 1

      if (PortPrivate::getBIR() != dh) {
        setDisplay(PortPrivate::getBIR());
      }

      al++;
      if (al == 0) {
        ah++;
        if (ah == 0x78) {
          ah = 0;          
        }
      }
      
      dl++;
      if (dl == 0) {
        dh++;
        if (dh == 0) {
          base++;
          dh = base;
        }
      }
    }
  #endif
     
    if (!yarcIsPowerOnReset()) {
      // A soft reset from the host opening the serial port.
      // We only run the code below after a power cycle.
      return true;
    }
    
    // Looks like an actual power-on reset.

    // Set and reset the Service Request flip-flop a few times.
    for(int i = 0; i < 3; ++i) {
      nanoTogglePulse(ResetService);
      if (yarcRequestsService()) {
          panic(PANIC_POST, 1);
      }
      
      setAH(0x7F); // 0xFF would be a read
      setAL(0xF0); // 7FF0 or 7FF1 sets the flip-flop
      setDH(0x00); // The data doesn't matter
      setDL(0xFF);
      singleClock(); // set service
  
      if (!yarcRequestsService()) {
        panic(PANIC_POST, 2);
      }
    }
    
    // Now reset the "request service" flip-flop from the YARC so we
    // don't later see a false service request.
    nanoTogglePulse(ResetService);
  
    if (getMCR() != byte(~(MCR_BIT_POR_SENSE | MCR_BIT_SERVICE_STATUS | MCR_BIT_YARC_NANO_L))) {
      panic(PANIC_POST, 3);
    }
  
    setAH(0xFF); setAL(0xFF);

    // Now in order to read and write main memory, we need to set the
    // the sysdata_src field of the microcode control register K to the
    // value MEM. This is a value of 5 in the three MS bits of K byte 1.

    setAH(0xFF); setAL(0xFF);
    writeByteToK(1, 0xBF); // 0B1011_1111 (101 in the high order bits)
    mcrEnableSysbus();
    syncMCR();

    // Write and read the first 4 bytes
    setAH(0x00);
    setAL(0x00); setDL('j' & 0x7F); singleClock();
    setAL(0x01); setDL('e' & 0x7F); singleClock();
    setAL(0x02); setDL('f' & 0x7F); singleClock();
    setAL(0x03); setDL('f' & 0x7F); singleClock();
  
    setAH(0x80); setAL(0x00); singleClock();
    if (getBIR() != ('j')) { panic(PANIC_POST, 4); }
    
    setAL(0x01); singleClock();
    if (getBIR() != ('e')) { panic(PANIC_POST, 5); }
    
    setAL(0x02); singleClock();
    if (getBIR() != ('f')) { panic(PANIC_POST, 6); }
    
    setAL(0x03); singleClock();
    if (getBIR() != ('f')) { panic(PANIC_POST, 7); }
  
    // Write and read the first 256 bytes
    setAH(0x00);
    for (int j = 0; j < 256; ++j) {
      setAL(j); setDL(256 - j); singleClock();
    }
    
    setAH(0x80);
    for (int j = 0; j < 256; ++j) {
      setAL(j); singleClock();
      if (getBIR() != byte(256 - j)) {
        panic(PANIC_POST, 8);
      }
    }
  
    // And now we'd better still in the /POR
    // state, or we're screwed.
  
     if (!yarcIsPowerOnReset()) {
      // Trouble, /POR should be low right now.
      panic(PANIC_POST, 9);
    }
    
    // Wait for POR# to go high here, then test RAM:
    setDisplay(0xFF);
    while (!yarcIsPowerOnReset()) {
      // do nothing
    }
    
    // Write and read the entire 30k space
    for (int i = 0; i < 0x7800; i++) {
      setAH(i >> 8); setAL(i & 0xFF);
      setDL(i & 0xFF); singleClock();
    }
    for (int i = 0; i < 0x7800; i++) {
      setAH((i >> 8) | 0x80); setAL(i & 0xFF);
      singleClock();
      if (getBIR() != byte(i & 0xFF)) {
        panic(PANIC_POST, 10);
      }
    }

    kRegMakeSafe();
    ucrMakeSafe();
    mcrMakeSafe();
    setDisplay(0xC0);

    return true;
  } // End of internalPostInit()

} // End of PortPrivate section

// Public interface to ports

void portInit() {
  PortPrivate::internalPortInit();
}

int portTask() {
  return 171;
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

bool WriteByteToK(byte kReg, byte kVal) {
  if (kReg > 3) {
    return false;
  }
  PortPrivate::writeByteToK(kReg, kVal);
  return true;
}

bool postInit() {
  return PortPrivate::internalPostInit();
}

// Change of philosophy: direct set of MCR

void SetMCR(byte b) {
  PortPrivate::mcrShadow = b;
  PortPrivate::syncMCR();
}

void SingleClock() {
  PortPrivate::singleClock();
}

// Public interface to the write-only 8-bit Display Register (DR)

void setDisplay(byte b) {
  PortPrivate::nanoSetRegister(PortPrivate::DisplayRegister, b);
}

void makeSafe() {
  PortPrivate::internalMakeSafe();
}

// Write up to 64 bytes to the slice for the given opcode, which must be
// in the range 128 ... 255.
void WriteBytesToSlice(byte opcode, byte slice, byte *data, byte n) {
  PortPrivate::writeBytesToSlice(opcode, slice, data, n);
}

// Read up to 64 bytes from the slice for the given opcode, which must be
// in the range 128 ... 255.
void ReadBytesFromSlice(byte opcode, byte slice, byte *data, byte n) {
  PortPrivate::readBytesFromSlice(opcode, slice, data, n);
}  
