// Copyright (c) Jeff Berkowitz 2021. All rights reserved.
//
// 456789012345678901234567890123456789012345678901234567890123456789012
//
// There have been two version of this code. The first version used the
// Arduino library (digitalWrite, pinMode, etc.) while the second version,
// below, uses direct referencs to the ATmega328P's internal registers.
// Ths second version saved a couple of thousand bytes of program memory
// and runs things like a full scan of memory more than 10 times as fast.
//
// This change introduced ambiguity into the word "register". Originally,
// the word referred to the registers I've constructed outside the Nano.
// Now, it may refer to these or to the ATmega328P's internal registers
// (PORTB, PINB, DDRB, etc.) used to manipulate the Nano's external pins.
// This is unavoidable and must be resolved by context.
//
// There is also the standard Arduino confusion over the concept of "pins".
// There are 30 physical pins on the Arduino Nano, and the "data port" I've
// defined, for example, is on physical pins 8..15. But Arduino defines a
// sort of "logical pin" pin concept across all the many Arduino board
// types. So these pins of the "data port" are labelled D5, D6, ..., D12
// on the silkscreen and in most figures documenting the pins.
//
// The logical pin numbers 5, 6, ..., 12 are used with the Arduino library
// functions like pinMode() and digitalWrite(). But since this code has
// been rewritten to use the ATmega's internal registers, the logical pin
// numbers are no longer used at all. Instead, there is a mapping from the
// ATmega's internal registers (used for manipulating the pins) to the
// physical pins.
//
// I've defined two "ports" for communicating with external registers.
// The "data port" is physical pins 8..15 on the Nano and the "select
// port" is physical pins 19..21 and 22,23. The data port is used in
// both read and write mode. It drives (or receives from) the internal
// Nano I/O bus to all the external registers. The select port is used
// to clock and/or enable these registers and to directly control the
// YARC. The mapping from internal ATmega control registers to "port" bits:
//
// Internal register  Port Name     Physical pin on Nano (1..30)
// ~~~~~~~~~~~~~~~~~  ~~~~~~~~~     ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// PORTB 5:7          Data 0..2     5..7
// PORTD 0:4          Data 3..7     8..12
// PORTC 0:2          Select 0..2   19..21
//
// Physical pins 22 and 23 (PORTC:3 and PORTC:4) are used to "strobe" the
// decoder line select by the select port, which is bussed to two decoders.
//
// Writing nn external register requires coordinating three ports. First,
// the data port must be switched to output and a value set on its pins.
// Next, the select port must be set the index of 1 of 8 output strobes on
// the decoders (their three A-lines are bus-connected). Finally, one of the
// two decoders must be enabled, then disabled, producing a low-going pulse
// that ends with a rising edge to clock one of the output registers.
//
// Read is similar, except the port must be set to input and the actual read
// of the port must occur while the enable line is low, since the enable line
// is connected to output enable pin on register or transceiver that drives
// the internal bus (the Nano's I/O bus, not the YARC system bus).
//
// We don't manage the LED port here, because its port assignment (pin 13)
// is pretty standard across all Arduinos and clones. We leave that to a
// separate purpose-built LED task that can play various patterns.
//
// Again, ports are on the Nano and the "port" concept is completely
// internal to this task. Explicitly or implicitly, the public interface
// only describes actions on (external) registers, e.g. the Display register,
// the Machine Control Register (MCR), the bus interface registers, etc.

namespace PortPrivate {

  const byte NOT_PIN = 0;
  typedef const byte PinList[];

  // Identifiers for the data port (Arduino logical pins 5, 6, ... 12) and the
  // select port (Arduino logical pins 14, 15, 16, plus 17 and 18 as explained
  // below).
  //
  // In the original version of this code which used digitalWrite(), the pin
  // values were were essential and were stored in these two arrays:
  //
  // PinList portData   = {5, 6, 7, 8, 9, 10, 11, 12, NOT_PIN};
  // PinList portSelect = {14, 15, 16, NOT_PIN};
  //
  // In the new Nano-specific version of this code which writes directly to
  // the ATmega's PORTB, PORTC, and PORTD registers, the Nano pin numbers
  // no longer referenced; but there is still code that requires portData
  // and portSelect be defined as -something- for logical tests. So we use
  // empty arrays to save a little space.

  PinList portData = {};
  PinList portSelect = {};

  // Outside the Nano there are two 3-to-8 decoder chips, providing a total
  // of 16 pulse outputs. The pulse outputs are used to clock output registers,
  // enable input registers to the Nano's I/O bus, and as direct controls to
  // the YARC.
  //
  // The three bit address on the decoders is bused from the Nano to both
  // decoders. But there are two distinct select pins, one for each decoder,
  // allowing for a 17th state where none of the 16 pulse outputs are active
  // (and in theory additional states where two outputs are active, etc.)
  //
  // As a result there are two ways of representing the "address" of one of
  // the pulse outputs. In both representations, bits 2:0 go to the address
  // input lines (A-lines) of both decoders. But in the REGISTER_ID, bit 3
  // is 0 for the "low" (0-7) decoder and 1 for the "high" (8-15) decoder.
  // Later, which doing the actual I/O operation, we need to either toggle
  // PORTC:3 for the low decoder, or PORTC:4 for the high decoder.
  //
  // Finally, note that the two toggles run to the active HIGH enable inputs
  // of the decoder chips. This was done because the Nano initializes output
  // pins to LOW by default. But the active HIGH enables cause negative-going
  // pulses on the decoder outputs, because that's how 74XX138s work, always.

  constexpr int PIN_SELECT_0_7 = _BV(PORTC3);
  constexpr int PIN_SELECT_8_15 = _BV(PORTC4);
        
  constexpr byte DECODER_ADDRESS_MASK = 7;
  constexpr byte DECODER_SELECT_MASK  = 8;

  typedef byte REGISTER_ID;

  // Addresses on low decoder
  constexpr byte IR_INPUT = 0;
  constexpr byte DATAHI = 1;
  constexpr byte DATALO = 2;
  constexpr byte ADDRHI = 3;
  constexpr byte ADDRLO = 4;
  constexpr byte MCR_INPUT = 5;
  constexpr byte LOW_UNUSED_6 = 6;
  constexpr byte LOW_UNUSED_7 = 7;

  // Addresses on high decoder
  constexpr byte WCS_CLK = 0;                 // Clock the microcode control register; bussed as below
  constexpr byte O1_UNCOMMITTED = 1;          // Bussed north of RAM to 6-pin connector; unused 
  constexpr byte HIGH_UNUSED_2 = 2;
  constexpr byte HIGH_UNUSED_OFFBOARD_3 = 3; // Unused;                     PULSE_EXT connector pin 1
  constexpr byte RESET_SERVICE = 4;          // Reset service request bit;  PULSE_EXT connector pin 2
  constexpr byte RAW_NANO_CLK = 5;           // Generate one YARC clock;    PULSE_EXT connector pin 3
  constexpr byte DISP_CLK = 6;               // Clock the display register; PULSE_EXT connector pin 4
  constexpr byte MCR_OUTPUT = 7;

  // Register IDs on low decoder are just their address
  constexpr REGISTER_ID BusInputRegister      = IR_INPUT;
  constexpr REGISTER_ID DataRegisterHigh      = DATAHI;
  constexpr REGISTER_ID DataRegisterLow       = DATALO;
  constexpr REGISTER_ID AddrRegisterHigh      = ADDRHI;
  constexpr REGISTER_ID AddrRegisterLow       = ADDRLO;
  constexpr REGISTER_ID MachineControlRegisterInput = MCR_INPUT;

  // Register IDs on high decoder need bit 3 set
  constexpr REGISTER_ID ScopeSync = (DECODER_SELECT_MASK|HIGH_UNUSED_2);
  constexpr REGISTER_ID ResetService = (DECODER_SELECT_MASK|RESET_SERVICE);
  constexpr REGISTER_ID RawNanoClock = (DECODER_SELECT_MASK|RAW_NANO_CLK);
  constexpr REGISTER_ID DisplayRegister = (DECODER_SELECT_MASK|DISP_CLK);
  constexpr REGISTER_ID MachineControlRegister = (DECODER_SELECT_MASK|MCR_OUTPUT);

  constexpr byte getAddressFromRegisterID(REGISTER_ID reg) {
    return reg & DECODER_ADDRESS_MASK;
  }

  // This function returns the select pin for a register ID as above.
  constexpr int getDecoderSelectPinFromRegisterID(REGISTER_ID reg) {
    return (reg & DECODER_SELECT_MASK) ? PIN_SELECT_8_15 : PIN_SELECT_0_7;
  }

  // Bits in the MCR
  constexpr byte MCR_BIT_0_WCS_EN_L      = 0x00; // Enable transceiver to/from SYSDATA to/from microcode's internal bus
  constexpr byte MCR_BIT_1_IR_EN_L       = 0x01; // Clock enable for Nano writing to IR when SYSCLK
  constexpr byte MCR_BIT_2_UNUSED        = 0x02;
  constexpr byte MCR_BIT_POR_SENSE       = 0x08; // Read POR state (YARC in reset when low); MCR bit 3, onboard only
  constexpr byte MCR_BIT_FASTCLKEN_L     = 0x10; // Enable YARC fast clock when low;         MCR bit 4, MCR_EXT connector pin 1
  constexpr byte MCR_BIT_YARC_NANO_L     = 0x20; // Nano owns bus when low, YARC when high;  MCR bit 5, MCR_EXT connector pin 2
  constexpr byte MCR_BIT_SERVICE_STATUS  = 0x40; // Read YARC requests service when 1;       MCR bit 6, MCR_EXT connector pin 3
  constexpr byte MCR_BIT_7_UNUSED        = 0x80; // Unused;                                  MCR bit 7, MCR_EXT connector pin 4
  
  // Set the data port to the byte b. The data port is made from pieces of
  // the Nano's internal PORTB and PORTD.
  void nanoPutDataPort(byte b) {
    // The "data port" is made of Nano physical pins 8 through 15. The
    // three low order bits are in Nano PORTD. The five higher order are
    // in the low-order bits of PORTB.
    // First set PD5, PD6, and PD7 to the three low order bits of b.
    int portDlowOrderBits = PORTD & 0x1F; // note: may sign extend
    PORTD = byte(portDlowOrderBits | ((b & 0x07) << 5));

    // Now set the low order 5 bits of PORTB to the high 5 bits of b.
    // These PORTB outputs appear on pins 11 through 15 inclusive.
    int portBhighOrderBits = PORTB & 0xE0; // note: may sign extend
    PORTB = byte(portBhighOrderBits | ((b & 0xF8) >> 3));
  }

  // Set PORTC bits 0, 1, and 2 to the three-bit address of one of eight
  // outputs on a 74HC138 decoder. Do not change the 5 high order bits of
  // PORTC. The choice of which decoder is made seprately in nanoTogglePort().
  void nanoPutSelectPort(byte b) {
      int portChighOrder5bits = PORTC & 0xF8; // note: may sign extend
      PORTC = byte(portChighOrder5bits | (b & 0x07));
  }
  
  void nanoPutPort(PinList port, int value) {
    if (port == portData) {
      nanoPutDataPort(value);
    } else {
      nanoPutSelectPort(value);
    }
  }
  
  // We take advantage of the fact that we only ever call get()
  // on the data port.
  byte nanoGetPort(PinList port) {
    // The "data port" is made of Nano physical pins 8..15. The three
    // low order bits are in ATmega PORTD. The five higher order, PORTB.
    // First get PD7:5 and put them in the low order bits of the result.
    byte portDbits = (PIND >> 5) & 0x07;
    byte portBbits = (PINB & 0x1F) << 3;
    return byte(portDbits | portBbits);
  }

  // Set the data port to be output or input. Delays in this file are
  // critical and must not be altered; some of them handle documented
  // issues with the ATmega, and some handle registrictions imposed by
  // the design of the external registers. This one is the first kind.
  void nanoSetDataPortMode(int mode) {
      if (mode == OUTPUT) {
        DDRD = DDRD | 0xE0;
        DDRB = DDRB | 0x1F;
      } else {
        DDRD = DDRD & ~0xE0;
        DDRB = DDRB & ~0x1F;
      }
      delayMicroseconds(2);
  }

  // Set the select port to be output (it's always output). Again,
  // delays in this file are critical and must not be altered.
  void nanoSetSelectPortMode(int mode) {
    DDRC |= DDRC | 0x07;
    delayMicroseconds(2);
  }
  
  void nanoSetMode(PinList port, int mode) {
    if (port == portData) {
      nanoSetDataPortMode(mode);
    } else {
      nanoSetSelectPortMode(mode);
    }
  }

  // This is a critical function that serves to pulse one of the 16
  // decoder outputs. To do this, it has to put the 3-bit address of
  // one of 8 data ports on to the 3-bit select port which is bussed
  // to the address (A) lines of the decoders. Then it has to enable
  // the correct decoder by togging either PORTC:3 or PORTC:4. One
  // of these values is returned by getDecoderSelectPinFromRegisterID().
  void nanoTogglePulse(REGISTER_ID reg) {
    byte decoderAddress = getAddressFromRegisterID(reg);
    nanoPutPort(portSelect, decoderAddress);
    
    byte decoderEnablePin = getDecoderSelectPinFromRegisterID(reg);
    PORTC = PORTC | decoderEnablePin;
    PORTC = PORTC & ~decoderEnablePin;
  }

  // Enable the specified register for input and call getPort() to read
  // the value. We cannot use nanoTogglePulse() here because we have to
  // read the value after setting the enable line low and before setting
  // it high again. As always, the delays are the result of careful
  // experimentation and are absolutely required.
  byte nanoGetRegister(REGISTER_ID reg) {    
    byte decoderAddress = getAddressFromRegisterID(reg);
    PortPrivate::nanoPutPort(portSelect, decoderAddress);
    
    PortPrivate::nanoSetMode(portData, INPUT);
    
    byte result;
    byte decoderEnablePin = getDecoderSelectPinFromRegisterID(reg);
    PORTC |= decoderEnablePin;
    delayMicroseconds(2);
    result = nanoGetPort(portData);
    PORTC &= ~decoderEnablePin;
    
    PortPrivate::nanoSetMode(portData, OUTPUT);
    return result;
  }
  
  void nanoSetRegister(REGISTER_ID reg, byte data) {
    PortPrivate::nanoSetMode(portData, OUTPUT);
    PortPrivate::nanoPutPort(portData, data);    
    nanoTogglePulse(reg);
  }

  void nanoInternalSingleClock() {
    nanoTogglePulse(RawNanoClock);
  }

  // Finally, just for freezePort(), which locks the display register
  // until the Nano is reset. This preserves the critical error code.
  byte displayIsFrozen = 0;
}

// Port module public interface. Because of the ordering of tasks in
// the tasks array, this is more or less the very first init code.
// Public functions (from here down) do not have the "nano.." prefix.

void portInit() {
  // Set the two decoder select pins to outputs. Delay after making
  // any change to this register.
  DDRC = DDRC | (_BV(DDC3) | _BV(DDC4));
  delayMicroseconds(2);

  // Turn off both of the decoder select lines so no decoder outputs
  // are active.
  PORTC &= ~(_BV(PORTC3) | _BV(PORTC4));
  
  PortPrivate::nanoSetMode(PortPrivate::portData,   OUTPUT);
  PortPrivate::nanoSetMode(PortPrivate::portSelect, OUTPUT);
}

// Public interface to the write-only 8-bit Display Register (DR)
// After freezeDisplay, the DR will not change until the Nano is reset.

void setDisplay(byte b) {
  if (!PortPrivate::displayIsFrozen) {
    PortPrivate::nanoSetRegister(PortPrivate::DisplayRegister, b);
  }
}

void freezeDisplay(byte b) {
  setDisplay(b);
  PortPrivate::displayIsFrozen = 1;
}

// Public interface to the 4 write-only bus registers: setAH
// (address high), AL, DH (data high), DL.

void setAH(byte b) {
  PortPrivate::nanoSetRegister(PortPrivate::AddrRegisterHigh, b);
}

void setAL(byte b) {
  PortPrivate::nanoSetRegister(PortPrivate::AddrRegisterLow, b);  
}

void setDH(byte b) {
  PortPrivate::nanoSetRegister(PortPrivate::DataRegisterHigh, b);
}

void setDL(byte b) {
  PortPrivate::nanoSetRegister(PortPrivate::DataRegisterLow, b);
}

// Public interface to the read registers: Bus Input Register
// and the readback value of the MCR.

byte getBIR() {
  return PortPrivate::nanoGetRegister(PortPrivate::BusInputRegister);
}

byte getMCR() {
  return PortPrivate::nanoGetRegister(PortPrivate::MachineControlRegisterInput);
}

void setMCR(byte b) {
    PortPrivate::nanoSetMode(PortPrivate::portData, OUTPUT);
    PortPrivate::nanoPutPort(PortPrivate::portData, b);
    PortPrivate::nanoTogglePulse(PortPrivate::MachineControlRegister);
}

void singleClock() {
  PortPrivate::nanoInternalSingleClock();
}

// PostInit() is called from setup after the init() functions are called for all the firmware tasks.
// The name is a pun, because POST stands for Power On Self Test in addition to meaning "after". But
// it's a misleading pun, because postInit() runs on both power-on resets and "soft" resets (of the
// Nano only) that occur when the host opens the serial port.
//
// The hardware allows the Nano to detect power-on reset by reading bit 0x08 of the MCR. A 0 value
// means the YARC is in the reset state. This state lasts at least two seconds after power-on, much
// longer than it takes the Nano to initialize. The Nano detects this and performs initialization
// steps both before and after the YARC comes out of the POR state as can be seen in the code below.

inline void postPanic(byte n) { panic(PANIC_POST|n); }
inline byte BYTE(int n) { return n&0xFF; } // XXX probably not the right way to solve this problem?

// Power on self test and initialization. Startup will hang if this function returns false.
bool postInit() {

  // The panic below normally displays 0xF7 after power on. It could be 0xB7 if the service request
  // flip-flop came up cleared at power on, but it normally seems to come up set. The hardware doesn't
  // guarantee an initialization value for it, because it doesn't matter - we just clear it, below.
  // panic(getMCR());
  // return false;
    
  byte postMcrInitialValue = getMCR();
  if (postMcrInitialValue & PortPrivate::MCR_BIT_POR_SENSE) {
    // A soft reset from the host opening the serial port.
    // We only run this code after a hard init (power cycle).
    return true;
  }

  // Looks like an actual power on reset.

  setMCR(0xFF & ~PortPrivate::MCR_BIT_YARC_NANO_L);

  // Set and reset the Service Request flip-flop a few times.
  for(int i = 0; i < 10; ++i) {
    PortPrivate::nanoTogglePulse(PortPrivate::ResetService);
    if ((getMCR() & PortPrivate::MCR_BIT_SERVICE_STATUS) != 0) {
      postPanic(1);
      return false;
    }
    
    setAH(0x7F); // 0xFF would be a read
    setAL(0xF0); // 7FF0 or 7FF1 sets the flip-flop
    setDH(0x00); // The data doesn't matter
    setDL(0xFF);
    singleClock(); // set service

    if ((getMCR() & PortPrivate::MCR_BIT_SERVICE_STATUS) == 0) {
      postPanic(2);
      return false;
    }
  }
  
  // Now reset the "request service" flip-flop from the YARC so we
  // don't later see a false service request.
  PortPrivate::nanoTogglePulse(PortPrivate::ResetService);

  // The RHS expression amounts to 0x97
  if (getMCR() != BYTE(~(PortPrivate::MCR_BIT_POR_SENSE | PortPrivate::MCR_BIT_SERVICE_STATUS | PortPrivate::MCR_BIT_YARC_NANO_L))) {
    postPanic(3);
    return false;
  }

  // Write and read the first 4 bytes
  setAH(0x00);
  setAL(0x00); setDL('j' & 0x7F); singleClock();
  setAL(0x01); setDL('e' & 0x7F); singleClock();
  setAL(0x02); setDL('f' & 0x7F); singleClock();
  setAL(0x03); setDL('f' & 0x7F); singleClock();

  setAH(0x80);
  setAL(0x00); singleClock();
  if (getBIR() != ('j' & 0x7F)) { postPanic(0x0A); }
  
  setAL(0x01); singleClock();
  if (getBIR() != ('e' & 0x7F)) { postPanic(0x0B); }
  
  setAL(0x02); singleClock();
  if (getBIR() != ('f' & 0x7F)) { postPanic(0x0C); }
  
  setAL(0x03); singleClock();
  if (getBIR() != ('f' & 0x7F)) { postPanic(0x0D); }

  // Write and read the first 256 bytes
  setAH(0x00);
  for (int j = 0; j < 256; ++j) {
    setAL(BYTE(j)); setDL(BYTE(256 - j)); singleClock();
  }
  
  setAH(0x80);
  for (int j = 0; j < 256; ++j) {
    setAL(BYTE(j)); singleClock();
    if (getBIR() != BYTE(256 - j)) { postPanic(0x0E); }
  }

  // And now we'd better still in the /POR
  // state, or we're screwed.

   if (getMCR() & PortPrivate::MCR_BIT_POR_SENSE) {
    // Trouble, /POR should be low right now.
    // We are locked out from the bus, give up.
    postPanic(5);
    return false;
  }

  // Wait for POR# to go high here, then test RAM:
  setDisplay(0xFF);
  while ((getMCR() & PortPrivate::MCR_BIT_POR_SENSE) == 0) {
    // do nothing
  }
  
  // // Write and read the entire 30k space
  for (int i = 0; i < 0x7800; i++) {
    setAH(BYTE(i >> 8)); setAL(BYTE(i & 0xFF));
    setDL(BYTE(i & 0xFF)); singleClock();
  }
  for (int i = 0; i < 0x7800; i++) {
    setAH(BYTE((i >> 8) | 0x80)); setAL(BYTE(i & 0xFF));
    singleClock();
    if (getBIR() != BYTE(i & 0xFF)) {
      postPanic(6);
    }
  }

  setDisplay(0xC0);
  return true;
}

// Run the YARC.
void runYARC() {
  /*
  byte mcr = getMCR();
  mcr |= PortPrivate::MCR_BIT_YARC_NANO_L;
  mcr &= ~PortPrivate::MCR_BIT_FASTCLKEN_L;
  setMCR(mcr);
  */
  setMCR(0x20);
}
