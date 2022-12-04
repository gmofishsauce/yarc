// Copyright (c) Jeff Berkowitz 2021. All rights reserved.
//
// 456789012345678901234567890123456789012345678901234567890123456789012
//
// This file was split out of port_task.h because it got too large.
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
// Writing to an external register requires coordinating three ports. First,
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
  constexpr int BOTH_DECODERS = (PIN_SELECT_0_7 | PIN_SELECT_8_15);
        
  constexpr byte DECODER_ADDRESS_MASK = 7;
  constexpr byte DECODER_SELECT_MASK  = 8;

  typedef byte REGISTER_ID;

  // Addresses on low decoder
  constexpr byte DATA_INPUT = 0;
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
  constexpr REGISTER_ID BusInputRegister      = DATA_INPUT;
  constexpr REGISTER_ID DataRegisterHigh      = DATAHI;
  constexpr REGISTER_ID DataRegisterLow       = DATALO;
  constexpr REGISTER_ID AddrRegisterHigh      = ADDRHI;
  constexpr REGISTER_ID AddrRegisterLow       = ADDRLO;
  constexpr REGISTER_ID MachineControlRegisterInput = MCR_INPUT;

  // Register IDs on high decoder need bit 3 set
  constexpr REGISTER_ID WcsControlClock = (DECODER_SELECT_MASK|WCS_CLK);
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

  // Bits in the UCR (the U is an omicron; microcode control register)
  //
  // Sometimes called the WCS CR (writeable control store control register)
  // in the notes. The slice address is unfortunately duplicated and must
  // normally be written to both nybbles, i.e. 0x00, 0x11, 0x22, or 0x33.
  //
  // Bit 7 (0x80) sets the direction of per-slice transceivers; high (READ)
  // is "safe", low is not safe because the RAMs have bidirectional data
  // lines and bus conflicts must be avoided. To avoid this, we use the
  // "write block trick" described below.
  //
  // Bit 6 (0x40) clocks data into the K registers. Oddly, it makes sense
  // to clock the K registers with nothing on the bus, e.g. with Bit 7 in
  // the "read" state, because the internal bus is pulled high and the "safe"
  // state of the K registers is 0xFF. But note that it may take as much as
  // a microsecond (i.e. multiple clocks) for the bus to float up to a safe
  // high state.
  //
  // Bits 5:4 and 1:0 index the slice 0..3 as noted above.
  //
  // Bit 3 (0x08) is the write enable to the RAMs. When the clock is low, it
  // enables the write line to the RAMs; data is latched when it transistions
  // to high. It changes state one gate delay -after- the transceivers are
  // enabled, leading to a potential ~10ns bus conflict where the transceivers
  // are enabled but the RAM outputs have not yet been disabled. This was
  // eventually fixed by the "write block trick" described below.
  //
  // Bit 2 (0x04) enables the slice-addressed transceiver that passes data
  // from the sysaddr transceiver (enabled by WCS_EN_L, bit 0 in the MCR)
  // inbound to or outbound from the internal per-slice bus. The internal bus
  // will normally contain the content of the RAM location addressed by the
  // instruction register and the state counter. These must be placed in
  // defined states separately.
  
  constexpr byte UCR_SLICE_ADDR_MASK     = (0x01|0x02);
  constexpr byte UCR_SLICE_EN_L          = 0x04;
  constexpr byte UCR_RAM_WR_EN_L         = 0x08;
  constexpr unsigned int UCR_K_ADDR_SHFT = 4;
  constexpr byte UCR_KREG_ADDR_MASK      = (0x010|0x020);
  constexpr byte UCR_KREG_WR_EN_L        = 0x40;
  constexpr byte UCR_DIR_WR_L            = 0x80;
  constexpr byte UCR_SAFE                = 0xFF;

  // Write block trick: the low order 6 bits 0..5 of the state counter provide
  // the low order address bits to the microcode RAMs. This allows an instruction
  // to have 2^6 microcode words, 0..63. The physical state counter is 8 bits
  // wide, and for a long time bits :6 and :7 were not connected. Then I realized
  // that by connecting bit :6 (the 64-weight bit) to the output enable enable
  // lines of all four microcode RAMs, I could resolve the bus conflict issues
  // on writes. To write either the microcode RAMs or the K register, the Nano
  // must first write the upper bits of the address to the instruction register.
  // This clears the state counter. Then the Nano should generate 64 clocks. This
  // causes a rollover (carry) out of bit :5 of the state counter into bit :6,
  // which raises the OE# line on the RAMs and leaves bits 0..5 as 0s. The Nano
  // can now write up to 64 bytes to the RAM slice, or write the K register (but
  // not both). This trick is embedded in the functions below that write to the
  // microcode RAM and K register.

  // Bits in the MCR
  constexpr byte MCR_BIT_0_WCS_EN_L      = 0x01; // Enable transceiver to/from SYSDATA to/from microcode's internal bus
  constexpr byte MCR_BIT_1_IR_EN_L       = 0x02; // Clock enable for Nano writing to IR when SYSCLK
  constexpr byte MCR_BIT_2_UNUSED        = 0x04;
  constexpr byte MCR_BIT_POR_SENSE       = 0x08; // Read POR state (YARC in reset when low); MCR bit 3, onboard only
  constexpr byte MCR_BIT_FASTCLKEN_L     = 0x10; // Enable YARC fast clock when low;         MCR bit 4, MCR_EXT connector pin 1
  constexpr byte MCR_BIT_YARC_NANO_L     = 0x20; // Nano owns bus when low, YARC when high;  MCR bit 5, MCR_EXT connector pin 2
  constexpr byte MCR_BIT_SERVICE_STATUS  = 0x40; // Read YARC requests service when 1;       MCR bit 6, MCR_EXT connector pin 3
  constexpr byte MCR_BIT_7_UNUSED        = 0x80; // Unused;                                  MCR bit 7, MCR_EXT connector pin 4

  // MCR shadow register. Can do multiple update to this, then call
  // syncMCR() to update the hardware.
  byte mcrShadow = ~MCR_BIT_YARC_NANO_L;

  // === start of lowest level code for writing to ports ===
  
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
    // Bug fix (although no symptoms were ever seen): to prevent glitches
    // and overlap on busses, we must disable both decoders before enabling
    // either one.
    PORTC &= ~BOTH_DECODERS;
    
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
    nanoPutPort(portSelect, decoderAddress);
    
    nanoSetMode(portData, INPUT);
    
    byte result;
    byte decoderEnablePin = getDecoderSelectPinFromRegisterID(reg);
    PORTC |= decoderEnablePin;
    delayMicroseconds(2);
    result = nanoGetPort(portData);
    PORTC &= ~decoderEnablePin;
    
    nanoSetMode(portData, OUTPUT);
    return result;
  }
  
  void nanoSetRegister(REGISTER_ID reg, byte data) {
    nanoSetMode(portData, OUTPUT);
    nanoPutPort(portData, data);    
    nanoTogglePulse(reg);
  }

  // This is the second "layer" of code, including support for control
  // registers like the MCR (machine control register) and the UCR
  // (microcode = u control register).

  void singleClock() {
    nanoTogglePulse(RawNanoClock);
  }

  void syncMCR() {
    nanoSetMode(portData, OUTPUT);
    nanoPutPort(portData, mcrShadow);
    nanoTogglePulse(MachineControlRegister);
  }

  // Interface to the 4 write-only bus registers: setAH
  // (address high), AL, DH (data high), DL.
  
  inline void setAH(byte b) {
    nanoSetRegister(AddrRegisterHigh, b);
  }
  
  inline void setAL(byte b) {
    nanoSetRegister(AddrRegisterLow, b);  
  }
  
  inline void setDH(byte b) {
    nanoSetRegister(DataRegisterHigh, b);
  }
  
  inline void setDL(byte b) {
    nanoSetRegister(DataRegisterLow, b);
  }
  
  // Public interface to the read registers: Bus Input Register
  // and the readback value of the MCR.
  
  inline byte getBIR() {
    return nanoGetRegister(BusInputRegister);
  }
  
  inline byte getMCR() {
    return nanoGetRegister(MachineControlRegisterInput);
  }
  
  // MCR shadow register support. Something must call
  // syncMCR() after invoking any of these to update
  // the actual register.

  inline void mcrEnableWcs() {
    mcrShadow &= ~MCR_BIT_0_WCS_EN_L;
  }
  
  inline void mcrDisableWcs() {
    mcrShadow |= MCR_BIT_0_WCS_EN_L;
  }
  
  inline void mcrEnableIRwrite() {
    mcrShadow &= ~MCR_BIT_1_IR_EN_L;
  }
  
  inline void mcrDisableIRwrite() {
    mcrShadow |= MCR_BIT_1_IR_EN_L;
  }
  
  inline void mcrEnableFastclock() {
    mcrShadow &= ~MCR_BIT_FASTCLKEN_L;
  }
  
  inline void mcrDisableFastclock() {
    mcrShadow |= ~MCR_BIT_FASTCLKEN_L;
  }
  
  inline void mcrEnableYarc() {
    mcrShadow |= MCR_BIT_YARC_NANO_L;
  }
  
  inline void mcrDisableYarc() {
    mcrShadow &= ~MCR_BIT_YARC_NANO_L;
  }
  
  inline void mcrForceUnusedBitsHigh() {
    mcrShadow |= (MCR_BIT_2_UNUSED | MCR_BIT_7_UNUSED);
  }
  
  inline bool yarcIsPowerOnReset() {
    return (getMCR() & MCR_BIT_POR_SENSE) == 0;
  }
  
  inline bool yarcRequestsService() {
    return (getMCR() & MCR_BIT_SERVICE_STATUS) != 0;
  }

  // Make the MCR "safe" (from bus conflicts) and put
  // the Nano in control of the system D and A busses.
  void mcrMakeSafe() {
    mcrDisableWcs();
    mcrDisableIRwrite();
    mcrDisableFastclock();
    mcrDisableYarc();
    mcrForceUnusedBitsHigh();
    syncMCR();
  }

  // UCR (microcode control register) shadow support
  
  byte ucrShadow = UCR_SAFE;

  // Sync the microcode control register to its shadow.
  // This function updates the hardware.
  void syncUCR() {
    setAH(0x7F);
    setAL(0xFF);
    setDH(0x00);
    setDL(ucrShadow);
    mcrEnableWcs(); syncMCR();
    nanoTogglePulse(WcsControlClock);
    mcrDisableWcs(); syncMCR();
  }

  // The slice appears in the two low-order bits of both
  // nybbles of the UCR (WCS CR). But it's generally not
  // useful for the two sets of two-bit values to differ,
  // because one indexes the K registers while the other
  // indexes the slice transceivers (and signals the RAM).
  //
  // Does not update the hardware.
  void ucrSetSlice(byte slice) {
    slice &= UCR_SLICE_ADDR_MASK;
    ucrShadow &= ~(UCR_KREG_ADDR_MASK | UCR_SLICE_ADDR_MASK);
    ucrShadow |= (slice << UCR_K_ADDR_SHFT) | slice;
  }

  // Set the selected slice transceiver to the unsafe
  // write (inbound) direction. Note: use the "write block trick"
  // described above before pushing this setting to the hardware.
  //
  // Does not update the hardware.
  inline void ucrSetDirectionWrite() {
    ucrShadow &= ~UCR_DIR_WR_L;
  }

  // Set the slice transceiver into the safe (outbound) direction.
  // Does not update the hardware.
  inline void ucrSetDirectionRead() {
    ucrShadow |= UCR_DIR_WR_L;
  }

  inline void ucrSetKRegWrite() {
    ucrShadow &= ~UCR_KREG_WR_EN_L;
  }

  inline  void ucrUnsetKRegWrite() {
    ucrShadow |= UCR_KREG_WR_EN_L;
  }
  
  // Enable writes to the RAM. Writes occur on system clock.
  // Does not update the hardware.
  inline void ucrSetRAMWrite() {
    ucrShadow &= ~UCR_RAM_WR_EN_L;
  }

  // Disable writes to the WCS RAM.
  // Does not update the hardware.
  inline void ucrSetRAMRead() {
    ucrShadow |= UCR_RAM_WR_EN_L;
  }

  // Enable the per-slice bus transceiver for the slice
  // selected by bits 5:4 and 1:0.
  //
  // Does not update the hardware.
  inline void ucrEnableSliceTransceiver() {
    ucrShadow &= ~UCR_SLICE_EN_L;
  }

  // Disable the per-slice bus transceiver.
  // Does not update the hardware.
  inline void ucrDisableSliceTransceiver() {
    ucrShadow |= UCR_SLICE_EN_L;
  }

  // Make the WCS (microcode) RAM ready for runtime.
  // This function updates the hardware.
  void ucrMakeSafe() {
    ucrShadow = UCR_SAFE;
    syncUCR();
  }
}