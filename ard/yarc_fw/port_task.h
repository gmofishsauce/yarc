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

  // === end of lowest layer of port and decoder control ===
  
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

  // Write a 16-bit value to the instruction register
  void writeIR(byte high, byte low) {
    setAH(0x7F); setAL(0xFF);
    setDH(high); setDL(low);
    mcrEnableIRwrite(); syncMCR();
    singleClock();
    mcrDisableIRwrite(); syncMCR();
  }
  
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
    // This sets the RAM location that will be overwritten.
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
    writeIR(0xFC, 0x00); // re-enable RAM output
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
    kRegMakeSafe();
    ucrMakeSafe();
    mcrMakeSafe();

#if 1
    setDisplay(0xCC);
    for (;;) {
      byte b = 1;
      writeByteToK(3, 0x01);
      writeBytesToSlice(0x80, 3, &b, 1);
      writeIR(0x83, 0x83);

      setAH(0x00); setDH(0x00);
      setAL(0x00); setDL(b); singleClock();
      setAL(0x01); setDL(b); singleClock();
      setAL(0x02); setDL(b); singleClock();
      setAL(0x03); setDL(b); singleClock();
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

    setDisplay(0xC0);

    return true;
  }

} // End of PortPrivate section

// Public interface to ports

void portInit() {
  PortPrivate::internalPortInit();
}

#if 0
// Write the entire 64-byte slice of data for the given opcode with
// values derived from the opcode. Read the data back from the slice
// and check it. This function uses 128 bytes of static storage.
bool validateOpcodeForSlice(byte opcode, byte slice) {
  static byte data[64];
  static byte result[64];

  for (int i = 0; i < sizeof(data); ++i) {
    data[i] = opcode + i;
  }
  
  PortPrivate::writeBytesToSlice(opcode | 0x80, slice, data, sizeof(data));
  PortPrivate::readBytesFromSlice(opcode | 0x80, slice, result, sizeof(result));
  
  for (int i = 0; i < sizeof(data); ++i) {
    if (data[i] != result[i]) {
      return false;
    }
  }

  return true;
}
#endif

int portTask() {
#if 0
  static byte failed = false;
  static byte done = true;
  static byte opcode;
  static byte slice;

  if (failed) {
    return 103;
  }

  if (done) {
    opcode = 0x80;
    slice = 0;
    done = false;
  }

  if (!validateOpcodeForSlice(opcode, slice)) {
      setDisplay(0xAA);
      failed = true;
      return 103;
  }

  if (++slice > 3) {
    slice = 0;
    opcode++;
  }
  if ((opcode & 0x80) == 0) {
    done = true;
  }

  setDisplay(opcode);
  return 11;
#endif
  // Some bits of code from power on testing for hardware bring up
  //
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

  //    for (;;) {
  //      writeByteToK(3, 0x02);
  //      writeIR(0x80,   0x02);
  //      writeByteToK(3, 0x02);
  //      writeIR(0x80,   0x02);
  //    }

}

bool publicWriteByteToK(byte kReg, byte kVal) {
  if (kReg > 3) {
    return false;
  }
  PortPrivate::writeByteToK(kReg, kVal);
  return true;
}

bool postInit() {
  return PortPrivate::internalPostInit();
}

// Public interface to the write-only 8-bit Display Register (DR)

void setDisplay(byte b) {
  PortPrivate::nanoSetRegister(PortPrivate::DisplayRegister, b);
}
