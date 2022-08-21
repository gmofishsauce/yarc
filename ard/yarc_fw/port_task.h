// Copyright (c) Jeff Berkowitz 2021. All rights reserved.
//
// 456789012345678901234567890123456789012345678901234567890123456789012
//
// The downloader design uses ports (groups of pins on the Nano) to
// communicate with registers (outside the Nano).
//
// A "port" is an ordered list of Nano pins. Ports need not have 8 pins,
// and the pins need not be physically adjacent or contiguous, although
// for sanity we try to arrange wiring so they are.
//
// When a value is sent to a port, the LS bit of the data is sent to
// pin [0] of the port, etc., and similarly for read. The pin list is
// terminated by some invalid pin number.
//
// Writing a register requires coordinating three ports. First, the data
// port must be switched to output and a value set on its pins. Next, the
// select port must be set the index of 1 of 8 output strobes on decoders
// (note: their three A-lines are bus-connected). Finally, one of the two
// decoders must be enabled, then disabled, producing a low-going puls that
// ends with a rising edge to clock one of the output registers.
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
// Again, ports are on the Nano, registers are outside. The "port" concept
// is completely internal to this task. Explicitly or implicitly, the public
// interface only describes actions on registers, e.g. the Display register,
// the Machine Control Register (MCR), the bus interface registers, etc.

namespace PortPrivate {

  // I'm starting the process of optimizing this code by writing directly
  // to the ATmega328's port registers today (August 20). Each ATmega port
  // has three registers associated with it: an input register, an output
  // register, and a mode register. If the mode is output, the state of the
  // corresponding pin is set by the output register. If the state is input,
  // setting the output port is repurposed to enable the input pull-up. If
  // the state is input, the value may be read at the input register.
  //
  // The Sparkfun video I watched cheated on a critical detail: they picked
  // pins where the pin number on the Arduino board was the same as the pin
  // number on the part. I think I need to use the pin numbers on the part
  // here in the code.
  //
  // For the byte I/O port on Nano pins 9 through 16 inclusive, the pins
  // on the part are PD5, PD6, PD7, PB0, PB1, PB2, PB3, and PB4. For the
  // select port on Nano pins 20 through 24 inclusive, PC0, PC1, PC2, PC3,
  // and PC4. And although it's totally confusing, the pin numbers passed
  // to digitalRead() and digitalWrite() in the Arduino library have the
  // following completely arbitrary values: the byte I/O port is described
  // as "D5, D6, ..., D12"; the select port as "D14, D15, ..., D17, D18".
  // These values can be seen in the non-optimized PinLists below.
  //
  // Functions name "nanoXYZ()" are optimized in this way, for the Nano
  // specifically, and refer directly to the internal port registers.
  // Functions named "arduinoXYZ()", or having neither prefix, use the
  // the standard Arduino library. These may be removed once I trust the
  // Nano-specific code.

#define OPTIMIZE_FOR_NANO 0

#if OPTIMIZE_FOR_NANO
#define singleClock nanoSingleClock
#define setMCR      nanoSetMCR
#define putPort     nanoPutPort
#define togglePulse nanoTogglePulse
#define getPort     nanoGetPort
#define setMode     nanoSetMode
#define getRegister nanoGetRegister
#define setRegister nanoSetRegister
#define singleClock nanoSingleClock
#else
#define singleClock arduinoSingleClock
#define setMCR      arduinoSetMCR
#define putPort     arduinoPutPort
#define togglePulse arduinoTogglePulse
#define getPort     arduinoGetPort
#define setMode     arduinoSetMode
#define getRegister arduinoGetRegister
#define setRegister arduinoSetRegister
#define singleClock arduinoSingleClock
#endif

  const byte NOT_PIN = 0;
  
  typedef const byte PinList[];
  
  PinList portData   = {5, 6, 7, 8, 9, 10, 11, 12, NOT_PIN};
  PinList portSelect = {14, 15, 16, NOT_PIN};
  
  // Write the bits of value to the port pins.
  void arduinoPutPort(PinList port, int value) {
    for (int i = 0; port[i] != NOT_PIN; i++) {
      digitalWrite(port[i], value&1);
      value = value >> 1;
    }
  }

  // Set the data port to the byte b. The data port is made from pieces of
  // the Nano's internal PORTB and PORTD.
  void nanoPutDataPort(byte b) {
    // The "data port" is made of Nano pins 9 through 16. The three
    // low order bits are in Nano PORTD. The five higher order, PORTB.
    // First set PD5, PD6, and PD7 to the three low order bits of b.
    int portDlowOrderBits = PORTD & 0x1F; // note: may sign extend
    PORTD = byte(portDlowOrderBits | ((b & 0x07) << 5));

    // Now set the low order 5 bits of PORTB to the high 5 bits of b.
    // These PORTB outputs appear on pins 12 through 16 inclusive.
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
  
  // Read the port pins and return them. We need to work
  // backwards through the port pins or we'll reverse the bits.
  int arduinoGetPort(PinList port) {
    int last;
    for (last = 0; port[last] != NOT_PIN; ++last) {
      ; // nothing
    }
  
    int result = 0;
    for (int i = last - 1; i >= 0; --i) {
      result = result << 1;
      result = result | (digitalRead(port[i]));
    }
    return result;
  }

  int nanoGetPort(PinList port) {
    return arduinoGetPort(port); // for now
  }
  
  // Set the port pins as outputs. The data port implements
  // a pseudo-3 state bus. In output mode, it can be used to
  // write to the bus control registers and MCR. But after
  // triggering a read cycle on the YARC's bus, we change it
  // to input mode, enable the output of the bus result register,
  // and read the port to get the value pulled from YARC memory.
  void arduinoSetMode(PinList port, int mode) {
    for (int i = 0; port[i] != NOT_PIN; i++) {
      pinMode(port[i], mode);
    }
  }

  void nanoSetMode(PinList port, int mode) {
    arduinoSetMode(port, mode); // for now
  }

  // Outside the Nano there are two 3-to-8 decoder chips, providing a total
  // of 16 pulse outputs. The pulse outputs are used to clock output registers,
  // enable input registers to the Nano's I/O bus, and as direct controls to
  // the YARC.
  //
  // The three bit address on the decoders is bused from the Nano to both
  // decoders. But there are two distinct select pins, one for each decoder,
  // allowing for a 17th state where none of the 16 pulse outputs are active.
  //
  // As a result there are two ways of representing the "address" of one of
  // the pulse outputs. In both representations, bits 2:0 go to the address
  // input lines (A-lines) of both decoders. But in the REGISTER_ID, bit 3
  // is 0 for the "low" (0-7) decoder and 1 for the "high" (8-15) decoder.
  // Later, which doing the actual I/O operation, we need to either toggle
  // Nano pin 17 for the low decoder, or Nano pin 18 for high decoder.
  //
  // Finally, note that the two toggles run to the active HIGH enable inputs
  // of the decoder chips. This was done because the Nano initializes output
  // pins to LOW by default. But the active HIGH enables cause negative-going
  // pulses on the decoder outputs, because that's how 74XX138s work, always.

  constexpr int PIN_SELECT_0_7 = 17;
  constexpr int PIN_SELECT_8_15 = 18;
        
  constexpr byte DECODER_ADDRESS_MASK = 7;
  constexpr byte DECODER_SELECT_MASK  = 8;

  typedef byte REGISTER_ID;
  
  constexpr byte getAddressFromRegisterID(REGISTER_ID reg) {
    return reg & DECODER_ADDRESS_MASK;
  }

  // This function returns the select pin for a register ID as above.
  constexpr int getDecoderSelectPinFromRegisterID(REGISTER_ID reg) {
    return (reg & DECODER_SELECT_MASK) ? PIN_SELECT_8_15 : PIN_SELECT_0_7;
  }

  void arduinoTogglePulse(REGISTER_ID reg) {
    byte decoderAddress = getAddressFromRegisterID(reg);
    putPort(portSelect, decoderAddress);
    byte decoderEnablePin = getDecoderSelectPinFromRegisterID(reg);

    // Again, this causes a LOW-going output pulse on the selected decoder
    digitalWrite(decoderEnablePin, HIGH);
    digitalWrite(decoderEnablePin, LOW);  
  }

  void nanoTogglePulse(REGISTER_ID reg) {
    byte decoderAddress = getAddressFromRegisterID(reg);
    putPort(portSelect, decoderAddress);
    byte decoderEnablePin = getDecoderSelectPinFromRegisterID(reg);
    if (decoderEnablePin == PIN_SELECT_0_7) {
      // Nano pin 17 is PORTC PORTC3
      PORTC |= _BV(PORTC3);
      PORTC &= ~(_BV(PORTC3));
    } else {
      // Nano pin 18 is PORTC PORTC4
      PORTC |= _BV(PORTC4);
      PORTC &= ~(_BV(PORTC4));
    }
  }
  
  byte arduinoGetRegister(REGISTER_ID reg) {
    PortPrivate::setMode(portData, INPUT);
    
    byte decoderAddress = getAddressFromRegisterID(reg);
    PortPrivate::putPort(portSelect, decoderAddress);
    byte decoderEnablePin = getDecoderSelectPinFromRegisterID(reg);

    digitalWrite(decoderEnablePin, HIGH); // enable input register to bus
    byte result = getPort(portData);
    digitalWrite(decoderEnablePin, LOW);  // disconnect input register from bus

    return result;
  }

  byte nanoGetRegister(REGISTER_ID reg) {
    arduinoGetRegister(reg); // for now
  }
  
  void arduinoSetRegister(REGISTER_ID reg, byte data) {
    PortPrivate::setMode(portData, OUTPUT);
    PortPrivate::putPort(portData, data);    
    togglePulse(reg);
  }

  void nanoSetRegister(REGISTER_ID reg, byte data) {
    arduinoSetRegister(reg, data); // fow now
  }

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
  constexpr byte HIGH_UNUSED_0 = 0;
  constexpr byte HIGH_UNUSED_1 = 1;
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
  constexpr REGISTER_ID ScopeSync = (DECODER_SELECT_MASK|HIGH_UNUSED_0);
  constexpr REGISTER_ID ResetService = (DECODER_SELECT_MASK|RESET_SERVICE);
  constexpr REGISTER_ID RawNanoClock = (DECODER_SELECT_MASK|RAW_NANO_CLK);
  constexpr REGISTER_ID DisplayRegister = (DECODER_SELECT_MASK|DISP_CLK);
  constexpr REGISTER_ID MachineControlRegister = (DECODER_SELECT_MASK|MCR_OUTPUT);

  // Bits in the MCR
  constexpr byte MCR_BIT_0_UNUSED        = 0x00;
  constexpr byte MCR_BIT_1_UNUSED        = 0x01;
  constexpr byte MCR_BIT_2_UNUSED        = 0x02;
  constexpr byte MCR_BIT_POR_SENSE       = 0x08; // Read POR state (YARC in reset when low); MCR bit 3, onboard only
  constexpr byte MCR_BIT_FASTCLKEN_L     = 0x10; // Enable YARC fast clock when low;         MCR bit 4, MCR_EXT connector pin 1
  constexpr byte MCR_BIT_YARC_NANO_L     = 0x20; // Nano owns bus when low, YARC when high;  MCR bit 5, MCR_EXT connector pin 2
  constexpr byte MCR_BIT_SERVICE_STATUS  = 0x40; // Read YARC requests service when 1;       MCR bit 6, MCR_EXT connector pin 3
  constexpr byte MCR_BIT_7_UNUSED        = 0x80; // Unused;                                  MCR bit 7, MCR_EXT connector pin 4

  // This one was already slightly optimized from the obvious call to
  // toggglePulse(), and that optimization did help a little.
  void arduinoInternalSingleClock() {
    putPort(portSelect, RAW_NANO_CLK);
    digitalWrite(PIN_SELECT_8_15, HIGH);
    digitalWrite(PIN_SELECT_8_15, LOW);
  }

  // But for the Nano-specific code, we can just call togglePulse().
  void nanoInternalSingleClock() {
    nanoTogglePulse(RawNanoClock);
  }

  // Finally, just for freezePort(), which locks the display register
  // until the Nano is reset. This preserves the critical error code.
  byte displayIsFrozen = 0;
}

// Port module public interface. Because of the ordering of tasks in
// the tasks array, this is more or less the very first init code.

void portInit() {  
  pinMode(PortPrivate::PIN_SELECT_0_7, OUTPUT);
  digitalWrite(PortPrivate::PIN_SELECT_0_7, LOW);
  
  pinMode(PortPrivate::PIN_SELECT_8_15, OUTPUT);
  digitalWrite(PortPrivate::PIN_SELECT_8_15, LOW);
  
  PortPrivate::setMode(PortPrivate::portData,   OUTPUT);
  PortPrivate::setMode(PortPrivate::portSelect, OUTPUT);
}

// Public interface to the write-only 8-bit Display Register (DR)
// After freezeDisplay, the DR will not change until the Nano is reset.

void setDisplay(byte b) {
  if (!PortPrivate::displayIsFrozen) {
    PortPrivate::setRegister(PortPrivate::DisplayRegister, b);
  }
}

void freezeDisplay(byte b) {
  setDisplay(b);
  PortPrivate::displayIsFrozen = 1;
}

// Public interface to the 4 write-only bus registers: setAH
// (address high), AL, DH (data high), DL.

void setAH(byte b) {
  PortPrivate::setRegister(PortPrivate::AddrRegisterHigh, b);
}

void setAL(byte b) {
  PortPrivate::setRegister(PortPrivate::AddrRegisterLow, b);  
}

void setDH(byte b) {
  PortPrivate::setRegister(PortPrivate::DataRegisterHigh, b);
}

void setDL(byte b) {
  PortPrivate::setRegister(PortPrivate::DataRegisterLow, b);
}

// Public interface to the read registers: Bus Input Register
// and the readback value of the MCR.

byte getBIR() {
  return PortPrivate::getRegister(PortPrivate::BusInputRegister);
}

byte getMCR() {
  return PortPrivate::getRegister(PortPrivate::MachineControlRegisterInput);
}

void arduinoSetMCR(byte b) {
  PortPrivate::setRegister(PortPrivate::MachineControlRegister, b);
}

void nanoSetMCR(byte b) {
    PortPrivate::setMode(PortPrivate::portData, OUTPUT); // nanoSetDataPort(OUTPUT);
    PortPrivate::putPort(PortPrivate::portData, b);      // nanoPutDataPort(b);
    PortPrivate::togglePulse(PortPrivate::MachineControlRegister); // nanoTogglePulse(PortPrivate::MachineControlRegister)
}

void arduinoSingleClock() {
  PortPrivate::arduinoInternalSingleClock();
}

void nanoSingleClock() {
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
    PortPrivate::togglePulse(PortPrivate::ResetService);
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
  PortPrivate::togglePulse(PortPrivate::ResetService);

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

  // // Write and read the entire 30k space
  for (int i = 0; i < 0x7800; i++) {
    setAH(BYTE(i >> 8)); setAL(BYTE(i & 0xFF));
    setDL(BYTE(i & 0xFF)); singleClock();
  }
  for (int i = 0; i < 0x7800; i++) {
    setAH(BYTE((i >> 8) | 0x80)); setAL(BYTE(i & 0xFF));
    singleClock();
    if (getBIR() != BYTE(i & 0xFF)) {
      panic(BYTE((i >> 8) | 0x80));
    }
  }
  
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
