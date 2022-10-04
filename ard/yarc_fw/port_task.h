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

  // Lock the display to preserve the critical error code on panic()
  byte displayIsFrozen = 0;

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
  // lines and bus conflicts must be avoided.
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
  // are enabled but the RAM outputs have not yet been disabled. The solution
  // is a trixie programming pattern where the read/write line (bit 7, 0x80)
  // must not be set to low until write line has been set low a cycle before.
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
  //
  // This is equivalent to nanoSetPulseLow()/nanoSetPulseHigh(), but
  // is slightly faster.
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

  // Set a pulse output to low and return with it that way.
  void nanoSetPulseLow(REGISTER_ID reg) {
    byte decoderAddress = getAddressFromRegisterID(reg);
    nanoPutPort(portSelect, decoderAddress);    
    byte decoderEnablePin = getDecoderSelectPinFromRegisterID(reg);
    PORTC = PORTC | decoderEnablePin;
    delayMicroseconds(2);
  }

  // Set the low pulse output high (the noop state).
  void nanoSetPulseHigh(REGISTER_ID reg) {
    byte decoderEnablePin = getDecoderSelectPinFromRegisterID(reg);
    PORTC = PORTC & ~decoderEnablePin;
  }
  
  // Enable the specified register for input and call getPort() to read
  // the value. We cannot use nanoTogglePulse() here because we have to
  // read the value after setting the enable line low and before setting
  // it high again. As always, the delays are the result of careful
  // experimentation and are absolutely required.
  //
  // TODO: rewrite using nanoSetPulseLow() and nanoSetPulseHigh().
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

  /*
  constexpr byte UCR_SLICE_ADDR_MASK     = (0x01|0x02);
  constexpr byte UCR_SLICE_EN_L          = 0x04;
  constexpr byte UCR_RAM_WR_EN_L         = 0x08;
  constexpr unsigned int UCR_K_ADDR_SHFT = 4;
  constexpr byte UCR_KREG_ADDR_MASK      = (0x010|0x020);
  constexpr byte UCR_KREG_WR_EN_L        = 0x40;
  constexpr byte UCR_DIR_WR_L            = 0x80;
  constexpr byte UCR_SAFE                = 0xFF;
   */
   
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
  // write (inbound) direction.
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
    setDH(0x00); setDL(value);
    singleClock();

    mcrMakeSafe();
    ucrMakeSafe();
    setAH(0xFF); 
  }

  // Write up to 64 bytes to the slice for the given opcode, which must be
  // in the range 128 ... 255.
  void writeBytesToSlice(byte opcode, byte slice, byte *data, byte n) {
    // Write the opcode to the IR. This sets the upper address bits to the
    // opcode and resets the state counter (setting lower address bits to 0)
    setAH(0x7F); setAL(0xFF);
    setDH(opcode); setDL(0x00);
    mcrEnableIRwrite(); syncMCR();
    singleClock();
    mcrDisableIRwrite(); syncMCR();
    
    // Set up the UCR for writes.
    ucrSetSlice(slice);
    ucrSetDirectionWrite();
    ucrEnableSliceTransceiver();
    ucrSetRAMWrite();
    syncUCR();

    setAH(0x7F); setAL(0xFF);
    setDH(0x00);
    for (int i = 0; i < n; ++i, ++data) {
      setDL(*data);
      mcrEnableWcs(); syncMCR();
      singleClock();
      mcrDisableWcs(); syncMCR();
    }

    ucrMakeSafe();
  }

  // Read up to 64 bytes from the slice for the given opcode, which must be
  // in the range 128 ... 255.
  void readBytesFromSlice(byte opcode, byte slice, byte *data, byte n) {
    // Write the opcode to the IR. This sets the upper address bits to the
    // opcode and resets the state counter (setting lower address bits to 0)
    setAH(0x7F); setAL(0xFF);
    setDH(opcode); setDL(0x00);
    mcrEnableIRwrite(); syncMCR();
    singleClock();
    mcrDisableIRwrite(); syncMCR();
    
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
      *data = getBIR();
      mcrDisableWcs(); syncMCR();
    }

    ucrMakeSafe();
  }

  int bpState = -1;

  void stateBasedDebug() {
    //    // If we're in an even-number state, we're waiting for
    //    // a continue after a breakpoint, so just return.
    //    if ((bpState & 1) == 0) return;
    //
    //    // We're in an odd-number state. Increment to the next
    //    // even number state and take one action before requesting
    //    // a breakpoint.
    //    bpState++;
    //    
    //    switch (bpState) {
    //      case 0:
    //        // do nothing - make scope ready
    //        break;
    //      case 2:
    //        ucrSetSlice(0);
    //        ucrSetDirectionWrite();
    //        ucrEnableSliceTransceiver();
    //        syncUCR();
    //        break;
    //      case 4:
    //        break;
    //      case 6:
    //        setAH(0x7F); setAL(0xFF);
    //        setDH(0x00); setDL(0xAA);
    //        mcrEnableWcs();
    //        syncMCR();
    //        break;
    //      case 8:
    //        nanoSetPulseLow(RawNanoClock);
    //        break;
    //      case 10:
    //        nanoSetPulseHigh(RawNanoClock);
    //        break;
    //      case 12:
    //        mcrDisableWcs();
    //        ucrMakeSafe();
    //        setAH(0xFF); 
    //        break;
    //      default:
    //        return; // states all done - no bp request
    //    }
    //    breakpointRequest(&bpState);
  }

  // Set the four K (microcode) registers to their "safe" value.
  // This function clocks the floating bus into the K registers,
  // because it's easy and fast and the "safe" value of the K
  // registers is 0xFF (so we are relying on the pullup resistors
  // for the values clocked into the registers).
  //
  // We do this because the write sequence for arbitrary data into
  // the K register is more complex.
  void kRegMakeSafe() {
    for (byte kReg = 0; kReg < 4; ++kReg) {
      ucrShadow = UCR_SAFE & ~(UCR_KREG_ADDR_MASK|UCR_KREG_WR_EN_L);
      ucrShadow |= (kReg << UCR_K_ADDR_SHFT);

      syncUCR();
      
      singleClock();
    }
    
    ucrMakeSafe();
  }

  // Because of the order of initialization, this is basically
  // the very first code executed on either a hard or soft reset.
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
    ucrMakeSafe();
    mcrMakeSafe();
  }

  // Run the YARC.
  void internalRunYARC() {
    kRegMakeSafe();
    ucrMakeSafe();
    mcrMakeSafe();
    
    mcrEnableYarc();        // lock the Nano off the bus
    mcrEnableFastclock();   // enable the YARC to run at speed
    syncMCR();
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
  
  // Power on self test and initialization. Startup will hang if this function returns false.
  bool internalPostInit() {
    // Do unconditionally on any reset
    kRegMakeSafe();
    ucrMakeSafe();
    mcrMakeSafe();

    //    byte b = 0;
    //    for (;;) {
    //      writeByteToK(0, b);
    //      b++;
    //    }

    //    byte data[] = {0xAA};
    //    for (;;) {
    //      writeBytesToSlice(0x80, 0, data, 1);
    //    }

    //    static byte data[64];
    //    for (int i = 0; i < sizeof(data); ++i) {
    //      data[i] = (i << 1) + 1;
    //    }
    //    
    //    for (;;) {
    //      setDisplay(0xAA);
    //      for (byte opcode = 0x80; opcode != 0; ++opcode) {
    //        writeBytesToSlice(opcode, 0, data, sizeof(data));
    //      }
    //      setDisplay(0xAB);
    //      for (byte opcode = 0x80; opcode != 0; ++opcode) {
    //        readBytesFromSlice(opcode, 0, data, sizeof(data));
    //      }
    //    }
    
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
          postPanic(1);
        return false;
      }
      
      setAH(0x7F); // 0xFF would be a read
      setAL(0xF0); // 7FF0 or 7FF1 sets the flip-flop
      setDH(0x00); // The data doesn't matter
      setDL(0xFF);
      singleClock(); // set service
  
      if (!yarcRequestsService()) {
        postPanic(2);
        return false;
      }
    }
    
    // Now reset the "request service" flip-flop from the YARC so we
    // don't later see a false service request.
    nanoTogglePulse(ResetService);
  
    if (getMCR() != byte(~(MCR_BIT_POR_SENSE | MCR_BIT_SERVICE_STATUS | MCR_BIT_YARC_NANO_L))) {
      postPanic(3);
      return false;
    }
  
    // Write and read the first 4 bytes
    setAH(0x00);
    setAL(0x00); setDL('j' & 0x7F); singleClock();
    setAL(0x01); setDL('e' & 0x7F); singleClock();
    setAL(0x02); setDL('f' & 0x7F); singleClock();
    setAL(0x03); setDL('f' & 0x7F); singleClock();
  
    setAH(0x80); setAL(0x00); singleClock();
    if (getBIR() != ('j')) { postPanic(0x0A); }
    
    setAL(0x01); singleClock();
    if (getBIR() != ('e')) { postPanic(0x0B); }
    
    setAL(0x02); singleClock();
    if (getBIR() != ('f')) { postPanic(0x0C); }
    
    setAL(0x03); singleClock();
    if (getBIR() != ('f')) { postPanic(0x0D); }
  
    // Write and read the first 256 bytes
    setAH(0x00);
    for (int j = 0; j < 256; ++j) {
      setAL(j); setDL(256 - j); singleClock();
    }
    
    setAH(0x80);
    for (int j = 0; j < 256; ++j) {
      setAL(j); singleClock();
      if (getBIR() != byte(256 - j)) {
        postPanic(0x0E);
      }
    }
  
    // And now we'd better still in the /POR
    // state, or we're screwed.
  
     if (!yarcIsPowerOnReset()) {
      // Trouble, /POR should be low right now.
      postPanic(5);
      return false;
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
        postPanic(6);
      }
    }

    setDisplay(0xC0);
    return true;
  }

} // End of PortPrivate section

// Public interface to ports

void portInit() {
  PortPrivate::internalPortInit();
}

int portTask() {
  //PortPrivate::stateBasedDebug();

  static byte failed = false;
  static byte opcode = 0;
  static byte data[64];
  static byte result[64];

  if (failed) {
    return 100;
  }
  
  for (int i = 0; i < sizeof(data); ++i) {
    data[i] = opcode + i;
  }
  
  PortPrivate::writeBytesToSlice(opcode | 0x80, 0, data, sizeof(data));
  PortPrivate::readBytesFromSlice(opcode | 0x80, 0, result, sizeof(result));
  
  for (int i = 0; i < sizeof(data); ++i) {
    if (data[i] != result[i]) {
      failed = true;
      setDisplay(0xAA);
      return 100;
    }
  }
  
  setDisplay(opcode);
  opcode++;
  
  return 100;
}

bool postInit() {
  return PortPrivate::internalPostInit();
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
