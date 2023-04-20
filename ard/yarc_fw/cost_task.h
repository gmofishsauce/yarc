// Copyright (c) Jeff Berkowitz 2023. All rights reserved.
// Continuous Self Test (CoST) task for YARC.
// Symbol prefixes: co, CO, cost, etc.

// The self-test consists of multiple Tests, each of which may run for a
// long time (i.e. seconds, or thousands of calls to the costTask() body.)
// Of course no call to the costTask() body should run for "very long", e.g
// for more than 50uSec or so.
//
// Each test ("xyz") may define a distinct xyzTestData structure for its
// data. The contents are preserved across calls while the Test is running.
// The multiple per-test structs are contained in an anonymous union that
// is tagged with the running Test. Running tests have complete ownership
// of all YARC resources and may assume that nothing else will interfere.
//
// A single call to all the tests is a test cycle. The COST executive runs
// a test cycle every few seconds (currently one cycle ever 6 seconds).
// During a cycle, each test returns true to indicate it needs to be called
// again and returns false when it's done. Alternatively, tests may be
// terminated externally (e.g. by a protocol command).
//
//In both cases, the makeSafe() method is invoked to return the YARC to a
// ready state. Tests should run for no more than about 10 seconds (about
// 100,000 calls to the task body); longer tests should be broken up into
// multiple Tests.
//
// Tests which are "supposed to pass" (i.e. that test fully debugged and
// functional parts of YARC) may report failure by calling
// panic(PANIC_COST|TEST_ID, subcode)
// where the TEST_ID is their value between 0 and 15 and the subcode is
// test- specific data. Whether a test panics or not, it can log one line
// per cycle, typically when it completes or fails. Tests don't need to
// worry about overrunning the log if they follow this rule because the
// executive will not call them "too often".

#define COST 1

namespace CostPrivate {
	bool running = false;
  bool stopping = false;

#if COST

  static union {
    struct delayData {
      long int delay;      
    } delayData;
    struct ucodeBasicData {
      byte opcode;
      byte slice;
      byte data[64];
      byte failOffset;
    } ubData;
    struct memoryBasicData { // reused by memHammer test
      byte AH;
      byte AL;
      byte DH;
      byte DL;
      byte readValue;
    } mbData;
    struct memory16Data {
      byte AH;
      byte AL;
      byte DH;
      byte DL;
      byte readValue;
    } m16Data;
    struct registerBasicData {
      byte AH;
      byte AL;
      byte DH;
      byte DL;
      byte save_DH;
      byte save_DL;
      byte readValue;
      byte location;
    } regData;
    struct flagsData {
      byte flags;
      byte condition;
      byte location;
    } flagsData;
  };

  typedef void (*TestInit)();
  typedef bool (*Test)();

  void delayTaskInit(void);
  bool delayTaskBody(void);
  void m16TestInit(void);
  bool m16TestBody(void);
  void regTestInit(void);
  bool regTestBody(void);
  void ucodeTestInit(void);
  bool ucodeBasicTest(void);
  void memBasicTestInit(void);
  bool memBasicTest(void);
  void memHammerInit(void);
  bool memHammerTest(void);
  void flagsInit(void);
  bool flagsTest(void);

  typedef struct tr {
    TestInit init;
    Test test;
    const char* name;
  } TestRef;

  const PROGMEM TestRef Tests[] = {
    { delayTaskInit,    delayTaskBody,  "delay"     },
    { m16TestInit,      m16TestBody,    "mem16"     },
    { regTestInit,      regTestBody,    "reg"       },
    { ucodeTestInit,    ucodeBasicTest, "ucode"     },
    { memBasicTestInit, memBasicTest,   "membasic"  },
    { memHammerInit,    memHammerTest,  "memhammer" },
    { flagsInit,        flagsTest,      "flags"     }    
   };

  constexpr byte N_TESTS = (sizeof(Tests) / sizeof(TestRef));
  constexpr byte MAX_TESTS = 0x10;
  byte currentTestId = 0;
  byte lastTestId = 0;

  // General note about logging: there is a (necessary) issue throughout
  // the Nano firmware caused by the design of the logger. To conserve
  // memory, there is just a single line buffer, and the message isn't
  // formatted until it's about to sent to the host (Mac). There is no
  // dynamic heap so no easy way to "close" over the value of a variable
  // to be logged. And the tests here share memory through the anonymous
  // union above. So test "N+1" tends to change the value of the variables
  // that test "N" wanted to log before the old values get formatted into
  // the log buffer. Here in the COST tests only, we address this by
  // tracking the number of log messages *we* enqueue and not running the
  // "next" test in a test cycle until that number drops to 0. The max
  // value of queued messages is 2 because of the "test cycle starting"
  // message just below, but it doesn't rely on the value of any variable.
  //
  // N.B. - this means the tests will come to a halt unless the the host
  // program is runnning to soak up the log messages, because the serial
  // task doesn't invoke the log callbacks. 

  int queuedLogMessageCount = 0;

  // Callback for the executive's single log line per cycle
  int testCycleStarting(char* bp, int bmax) {
    int result = snprintf_P(bp, bmax, PSTR("cost: test cycle starting"));
    if (result > bmax) result = bmax;
    queuedLogMessageCount--;
    return result;
  }

  // Callback for test starting within a cycle. This function also doesn't
  // use any variable data.
  int testStarting(char* bp, int bmax) {
    int result = snprintf_P(bp, bmax, PSTR("  test %s starting"),
      pgm_read_ptr_near(&Tests[currentTestId].name));
    if (result > bmax) result = bmax;
    queuedLogMessageCount--;
    return result;
  }

  // Call back for tests stopped (e.g. due to a command from the Mac;
  // see serial_task.h)
  int costTestsStopped(char* bp, int bmax) {
    int result = snprintf_P(bp, bmax, PSTR("COST stopped"));
    if (result > bmax) result = bmax;
    queuedLogMessageCount--;
    return result;
  }

  // The test executive
  int internalCostTask() {
    constexpr int TIMEOUT_HOST_NOT_POLLING = 87; // check about 12 times a second
    constexpr int TIMEOUT_NOT_RUNNING = 513; // check about twice a second
    
    // Wait for all previously queued log messages to be formatted and sent
    // to the host. See the block comment above ("General note about...")
    if (queuedLogMessageCount > 0) {
      SetDisplay(0x38);
      return TIMEOUT_HOST_NOT_POLLING;
    }
    
    if (!running) {
      return TIMEOUT_NOT_RUNNING; // come back and check once or twice each second
    }

    // COST tests are running
    SetDisplay(0xC4);

    // Is a new test cycle starting? (Including first-time initialization)
    if (currentTestId >= N_TESTS) {
      currentTestId = 0;
      randomSeed(millis());
      queuedLogMessageCount++;
      logQueueCallback(testCycleStarting);
      return 0; // come back and check right away
    }

    // Is a new test starting within the current cycle?
    if (lastTestId != currentTestId) {
      if (stopping) {
        logQueueCallback(costTestsStopped);
        queuedLogMessageCount++;
        MakeSafe();
        running = false;
        stopping = false;
        return TIMEOUT_NOT_RUNNING;
      }
      queuedLogMessageCount++;
      logQueueCallback(testStarting);
      lastTestId = currentTestId;
      MakeSafe(); // clean up YARC state for the next test
      const TestInit testInit = pgm_read_ptr_near(&Tests[currentTestId].init);
      (*testInit)();
      return 0;
    }

    // Run the test function and move on to the next test if it returns false
    const Test test = pgm_read_ptr_near(&Tests[currentTestId].test);
    if (! (*test)()) {
      currentTestId++;
    }
    return 0;
  }

  // === delay task implements the startup and inter-cycle delay ===

  int delayTaskMessageCallback(char* bp, int bmax) {
    int result = snprintf_P(bp, bmax, PSTR("  delayTask: done"));
    if (result > bmax) result = bmax;
    queuedLogMessageCount--;
    return result;
  }

  void delayTaskInit() {
    constexpr long callsPerMillisecond = 12L; // estimate
    constexpr long delaySeconds = 5L;         // arbitrary
    constexpr long millisPerSecond = 1000L;    
    delayData.delay = callsPerMillisecond * delaySeconds * millisPerSecond; 
  }

  bool delayTaskBody() {
    if (delayData.delay < 0L) {
      queuedLogMessageCount++;
      logQueueCallback(delayTaskMessageCallback);
      /* XXX */ if (millis() < 1) WriteFlags(0x01);
      return false; // done
    }
    delayData.delay = delayData.delay - 1L;
    return true; // not done
  }

  // === m16 (16 bit memory cycles) test ===
  int m16LowByteCallback(char* bp, int bmax) {
    int result = snprintf_P(bp, bmax,
      PSTR("  F m16 lo: A 0x%02X 0x%02X D 0x%02X 0x%02X got 0x%02X"),
      m16Data.AH, m16Data.AL, m16Data.DH, m16Data.DL, m16Data.readValue);
    if (result > bmax) result = bmax;
    queuedLogMessageCount--;
    return result;
  }

  int m16HighByteCallback(char* bp, int bmax) {
    int result = snprintf_P(bp, bmax,
      PSTR("  F m16 hi: A 0x%02X 0x%02X D 0x%02X 0x%02X got 0x%02X"),
      m16Data.AH, m16Data.AL, m16Data.DH, m16Data.DL, m16Data.readValue);
    if (result > bmax) result = bmax;
    queuedLogMessageCount--;
    return result;
  }

  void m16TestInit() {
    m16Data.AH = 0x00;
    m16Data.AL = 0x00;
    m16Data.DH = random(0, 255);
    m16Data.DL = random(0, 255);
  }

  // Write 256 bytes memory with 16-bit cycles. All
  // arguments are passed through the anonymous union.
  void writeStep16() {
    WriteK(0xFF, 0xFF, 0xFF, 0x3F);  // write memory, 16-bit access
    SetMCR(McrEnableSysbus(MCR_SAFE)); // YARC/NANO# low, SYSBUS_EN# low

    do {
      SetAH(m16Data.AH);
      SetAL(m16Data.AL);
      SetDH(m16Data.DH);
      SetDL(m16Data.DL);
      SingleClock();
      m16Data.AL += 2;       
    } while (m16Data.AL != 0);
  }

  // Verify 256 bytes memory using 16-bit cycles. For each cycle, check
  // just the low-order 8 bits of the value. The Nano only has an
  // 8-bit bus read register (a holdover of the previous 8-bit design)
  // so it cannot see the high byte of a 16-bit read. All arguments
  // are passed through the anonymous union.
  bool readStep16() {
    WriteK(0xFF, 0xFF, 0x9F, 0xBF); // read memory word (16-bit reads)      
    SetMCR(McrEnableSysbus(MCR_SAFE));

    do {
      SetAH(m16Data.AH | 0x80); // 0x80 => read
      SetAL(m16Data.AL);
      SingleClock();
      if ((m16Data.readValue = GetBIR()) != m16Data.DL) {
        return false; // just detect one failure
      }
      m16Data.AL += 2;       
    }  while (m16Data.AL != 0);
    return true;   
  }

  // Verify the high byte of 256 bytes memory using 8-bit cycles. For each
  // cycle, check just the high-order 8 bits of the value. The Nano only
  // has an 8-bit bus read register (a holdover of the previous 8-bit design)
  // so it can only see the high-order byte of 16-bit memory location by
  // triggering a byte transfer, which engagesthe "cross" transceiver to
  // return the high byte on the low-order bits of sysdata.
  bool readStep8() {
    WriteK(0xFF, 0xFF, 0x9F, 0xFF); // read memory word (byte read)      
    SetMCR(McrEnableSysbus(MCR_SAFE));

    do {
      SetAH(m16Data.AH | 0x80); // 0x80 => read
      SetAL(m16Data.AL | 0x01); // the high byte
      SingleClock();
      if ((m16Data.readValue = GetBIR()) != m16Data.DH) {
        return false; // just detect one failure
      }
      m16Data.AL += 2;       
    } while (m16Data.AL != 0);
    return true;
  }

  bool m16TestBody() {
    if (m16Data.AH == 0x78) {
      return false; // done
    }

    writeStep16();
    if (!readStep16()) {
      queuedLogMessageCount++;
      logQueueCallback(m16LowByteCallback);
      return false; // only detect 1 failure
    }

    if (!readStep8()) {
      queuedLogMessageCount++;
      logQueueCallback(m16HighByteCallback);
      return false; // only detect 1 failure
    }

    m16Data.AH++;
    m16Data.DL += 7;
    m16Data.DH += 17;
    
    return true; // not done  
  }
  
  // === reg (general register) basic test ===

  void regTestInit() {
  }

  // Write 16 bits of data at addr. Changes AH, AL, DH, DL.
  //  This function acts only on its arguments and the hardware,
  // so it's portable to other parts of the code.
  void Write16(unsigned int addr, unsigned int data) {
    WriteK(0xFF, 0xFF, 0xFF, 0x3F);  // write memory, 16-bit access
    SetADHL(StoHB(addr & 0x7F00), StoLB(addr), StoHB(data), StoLB(data));
    SingleClock();
  }
  
  // Read 8 bits of data at addr with the noise pattern in DH/DL.
  // Changes AH, AL, DH, DL. This function acts only on its arguments
  // and the hardware, so it's portable to other parts of the code.
  byte Read8(unsigned int addr, unsigned int noise) {
    WriteK(0xFF, 0xFF, 0x9F, 0xFF); // read memory byte     
    SetADHL(StoHB(addr | 0x8000), StoLB(addr), StoHB(noise), StoLB(noise));
    SetMCR(McrEnableSysbus(MCR_SAFE));
    SingleClock();
    SetMCR(MCR_SAFE);
    return GetBIR();
  }
  
  int regCallback(char* bp, int bmax) {
    int result = snprintf_P(bp, bmax,
      PSTR("  F reg: (%d): A 0x%02X 0x%02X D 0x%02X 0x%02X got 0x%02X save 0x%02X 0x%02X"),
      regData.location, regData.AH, regData.AL, regData.DH, regData.DL, regData.readValue, regData.save_DH, regData.save_DL);
    if (result > bmax) result = bmax;
    queuedLogMessageCount--;
    return result;
  }

  // Read 8 bits of data at addr with the noise pattern in DH/DL.
  // Check it against the expected value and issue a log message
  // labeled with the location of the test if it's mismatched.
  // Return true if OK and false if read doesn't match expected.
  // This function uses the regData union so it's not portable.
  bool check8(unsigned int addr, unsigned int noise, byte expected, byte loc) {
    regData.readValue = Read8(addr, noise);
    if (regData.readValue != expected) {
      regData.location = loc;
      queuedLogMessageCount++;
      logQueueCallback(regCallback);
      return false;
    }
    return true;
  }

  // Write 16 bits to main memory as a single write. Move the 16 bits to 
  // a register in a single operation. Read the register to a second memory
  // location with a single register read/memory write. Compare the two
  // memory locations (in the Nano, using byte reads and writes) and report
  // if the data transfers were all successful.
  //
  // NOTE: this was the most complicated test ever when I first wrote it,
  // but now there are much easier ways to do this.
  bool regTestBody() {

    // This is a newer register test that just writes and reads a value to
    // all the registers using the YARC utility functions. It was written
    // much later than the rest of the code below it.

    WriteReg(2, 0x2332);
    WriteReg(3, 0x3443);
    WriteReg(1, 0x1221);
    WriteReg(0, 0x0110);

    bool fail = false;
    byte location;
    if (ReadReg(0, 0x7700) != 0x0110) {
      fail = true;
      location = 10;
    }
    if (!fail && ReadReg(1, 0x7700) != 0x1221) {
      fail = true;
      location = 11;
    }
    if (ReadReg(2, 0x7700) != 0x2332) {
      fail = true;
      location = 12;
    }
    if (!fail && ReadReg(3, 0x7700) != 0x3443) {
      fail = true;
      location = 13;
    }
    if (fail) {
      regData.location = location;
      regData.AH = regData.AL = regData.DH = regData.DL = regData.readValue = regData.save_DH = regData.save_DL = 0;
      queuedLogMessageCount++;
      logQueueCallback(regCallback);
      return false;
    }

    // The rest of this test was the first effort I made to read and write
    // the registers. But it only used register R3 (0b11) so it failed to
    // to catch the swapped write address lines that eventually cost me a
    // week of troubleshooting.
    
    regData.save_DH = random(0, 256);
    regData.save_DL = random(0, 256);
    regData.AH = 0;
    regData.AL = 0x10;
    regData.DH = regData.save_DH;
    regData.DL = regData.save_DL;

    // (1) write the random values at 0x10 and 0x11
    Write16(BtoS(regData.AH, regData.AL), BtoS(regData.DH, regData.DL));

    // (2) Check the both bytes. Set the data registers to
    // some arbitrary value different than what we wrote
    // (here AA, 55) make sure we're not reading from them.
    regData.AH = 0x80;
    regData.AL = 0x10; 
    regData.DH = 0xAA;
    regData.DL = 0x55;
    if (!check8(BtoS(regData.AH, regData.AL), BtoS(regData.DH, regData.DL), regData.save_DL, 1)) {
      return false;
    }

    regData.AL = 0x11;
    if (!check8(BtoS(regData.AH, regData.AL), BtoS(regData.DH, regData.DL), regData.save_DH, 2)) {
      return false;
    }

    // (3) preset 0xF00D at 0x20 and 0x21
    WriteK(0xFF, 0xFF, 0xFF, 0x3F); // write memory, 16-bit access
    regData.AH = 0x00;
    regData.AL = 0x20; 
    regData.DH = 0xF0; 
    regData.DL = 0x0D;
    Write16(BtoS(regData.AH, regData.AL), BtoS(regData.DH, regData.DL));

    // (4) check it, low byte first, byte at a time
    regData.DH = 0x77;
    regData.DL = 0xEE;    
    regData.AH = 0x80;
    regData.AL = 0x20; 
    if (!check8(BtoS(regData.AH, regData.AL), BtoS(regData.DH, regData.DL), 0x0D, 3)) {
      return false;
    }  
    SetMCR(MCR_SAFE);

    regData.AL = 0x21;
    if (!check8(BtoS(regData.AH, regData.AL), BtoS(regData.DH, regData.DL), 0xF0, 4)) {
      return false;
    }  
    SetMCR(MCR_SAFE);
    
    // Step (5) 16-bit move 0x10 and 0x11 to register 3.
    WriteK(0xFB, 0xFF, 0x9E, 0xBF);
    
    // Now we need to set AH to 0x80 (SYSADDR:15 high) because this
    // will cause the Nano's data bus drivers to believe the bus cycle
    // is a read so it won't try to drive the bus; otherwise, it will.
    // Again, set the data registers to something "different".
    regData.AH = 0x80; regData.AL = 0x10; regData.DH = 0xFF; regData.DL = 0xFF;
    SetADHL(regData.AH, regData.AL, regData.DH, regData.DL);
    SetMCR(McrEnableRegisterWrite(McrEnableSysbus(MCR_SAFE)));
    SingleClock();
    SetMCR(MCR_SAFE); // freeze the registers and the bus

    // (6) Clock register R3 into memory location 0x20/0x21. Note that the Nano
    // provides the address, always, when it's in control - the Nano's address
    // bus drivers are enabled by YARC/NANO# low. But again, we'll set the high
    // order address bit to disable the Nano's data bus drivers.
    WriteK(0xDF, 0xFF, 0x1F, 0x3F);
    regData.AH = 0x80; regData.AL = 0x20; regData.DH = 0x33; regData.DL = 0x44;
    SetADHL(regData.AH, regData.AL, regData.DH, regData.DL);
    SetMCR(McrEnableSysbus(MCR_SAFE));
    SingleClock();

    // (7) check it, low byte first, byte at a time
    if (!check8(BtoS(regData.AH, regData.AL), BtoS(regData.DH, regData.DL), regData.save_DL, 5)) {
      return false;
    }  
    SetMCR(MCR_SAFE);

    regData.AL = 0x21;
    if (!check8(BtoS(regData.AH, regData.AL), BtoS(regData.DH, regData.DL), regData.save_DH, 6)) {
      return false;
    }  
    SetMCR(MCR_SAFE);

    return false; // done
  }
  
  // === ucode (Microcode RAM) basic test ===
  
  // Calllback for the executive's single log line per cycle. Here is an example
  // of a place where incorrect values would be logged if we didn't wait for the
  // log message queue to clear each time we queue a message: the test after ucode
  // would overwrite the opcode and slice, causing nonsense values to be logged.
  int ucodeBasicMessageCallback(char* bp, int bmax) {
    int result = snprintf_P(bp, bmax,
      PSTR("  F ucodeBasic: fail op 0x%02X sl 0x%02X offset %d data 0x%02X"),
      ubData.opcode, ubData.slice, ubData.failOffset, ubData.data[ubData.failOffset]);
    if (result > bmax) result = bmax;
    queuedLogMessageCount--;
    return result;
  }

  // Write the entire 64-byte slice of data for the given opcode with
  // values derived from the opcode. Read the data back from the slice
  // and check it.
  bool validateOpcodeForSlice(byte opcode, byte slice) {
    constexpr int SIZE = sizeof(ubData.data);

    for (int i = 0; i < SIZE; ++i) {
      ubData.data[i] = opcode + i;
    }
    
    ubData.failOffset = WriteSlice(opcode, slice, ubData.data, SIZE, false);
    if (ubData.failOffset != SIZE) {
      return false;      
    }

    return true;
  }

  void ucodeTestInit() {
    ubData.opcode = 0x80;
    ubData.slice = 0;
  }
  
  bool ucodeBasicTest() {
    if (!validateOpcodeForSlice(ubData.opcode, ubData.slice)) {
      queuedLogMessageCount++;
      logQueueCallback(ucodeBasicMessageCallback);
      return false;
    }

    if (++ubData.slice > 3) {
      ubData.slice = 0;
      ubData.opcode++;
    }
    if ((ubData.opcode & 0x80) == 0) {
      return false; // done with one pass over all opcodes and slices
    }
    return true; // not done
  }

  // === memory (main system memory) basic test ===

  int memBasicMessageCallback(char* bp, int bmax) {
    int result = snprintf_P(bp, bmax,
      PSTR("  F memBasic: at 0x%02X 0x%02X data 0x%02X 0x%02X read 0x%02X"),
      mbData.AH, mbData.AL, mbData.DH, mbData.DL, mbData.readValue);
    if (result > bmax) result = bmax;
    queuedLogMessageCount--;
    return result;
  }

  void memBasicTestInit() {
    mbData.AH = random(0, 0x78);
    mbData.AL = random(0, 256);
    mbData.DH = random(0, 256);
    mbData.DL = random(0, 256);
  }

  bool memBasicTest() {
    WriteK(0xFF, 0xFF, 0xFF, 0x7F);  // write memory, 8-bit access
    SetADHL(mbData.AH, mbData.AL, mbData.DH, mbData.DL);
    SetMCR(MCR_SAFE);
    SingleClock();

    // Now write some nearby locations with different data
    // We don't worry about carries out of AL
    SetDL(~mbData.DL);
    for (byte i = 1; i < 64; i = i << 1) {
      SetAL(mbData.AL + i);
      SingleClock();
      SetAL(mbData.AL - i);
      SingleClock();
    }

    // Check the original location. Set the data registers
    // to some arbitrary value.
    WriteK(0xFF, 0xFF, 0x9F, 0xFF);  // read memory, 8-bit access
    SetADHL(mbData.AH | 0x80, mbData.AL, 0x55, 0x55); // 0x80 = nano read
    SetMCR(McrEnableSysbus(MCR_SAFE));
    SingleClock();
    if ((mbData.readValue = GetBIR()) != mbData.DL) {
      queuedLogMessageCount++;
      logQueueCallback(memBasicMessageCallback);
      return false;
    }

    mbData.AL++;
    if (mbData.AL == 0) {
      return false; // done
    }
    return true;
  }

  // === memhammer test using the more recent functions in yarc_utils.h
  // This test reuses the memory basic chunk of the union (mbData)

  int memHammerCallback(char* bp, int bmax) {
    int result = snprintf_P(bp, bmax,
      PSTR("  F memHammer: at 0x%02X 0x%02X data 0x%02X 0x%02X read 0x%02X"),
      mbData.AH, mbData.AL, mbData.DH, mbData.DL, mbData.readValue);
    if (result > bmax) result = bmax;
    queuedLogMessageCount--;
    return result;
  }

  void memHammerInit() {
  }

  bool memHammerTest() {
    constexpr short N = 16;
    unsigned short writeData[N];
    unsigned short readData[N];

    mbData.AH = random(0x10, 0x78 - 0x11);
    mbData.AL = 2 * random(0, 0x70);
    mbData.DH = 0;
    mbData.DL = 0;
    const unsigned short addr = BtoS(mbData.AH, mbData.AL);
        
    // We use the misnamed "read data" for all the noise writes
    for (short i = 0, s = random(0, 0x8000); i < N; ++i, s += 137) {
      writeData[i] = s;
      readData[i] = 37 + s;
    }    
    WriteMem16(addr, writeData, N);
    for (short i = 1; i < 0x07; ++i) {
      unsigned short a = addr + (i << 5);
      WriteMem16(a, readData, N);
      a = addr - (i << 5);
      WriteMem16(a, readData, N);
    }

    ReadMem16(addr, readData, N);
    for (short i = 0; i < N; ++i) {
      if (writeData[i] != readData[i]) {
        mbData.readValue = readData[i]; // truncates
        queuedLogMessageCount++;
        logQueueCallback(memHammerCallback);
        return false;
      }
    }
    return false;
  }

  // === flagTest verifies the condition code logic.

  int flagsCallback(char* bp, int bmax) {
    unsigned short memvalues[2];
    ReadMem16(SCRATCH_MEM, memvalues, 2);
    int result = snprintf_P(bp, bmax,
      PSTR("  F flagTest: (%d) flags 0x%02X cond 0x%02X SCRATCH 0x%04X 0x%04X"),
      flagsData.location, flagsData.flags, flagsData.condition, memvalues[0], memvalues[1]);
    if (result > bmax) result = bmax;
    queuedLogMessageCount--;
    return result;
  }

  void flagsInit() {
    flagsData.location = 0;
    flagsData.flags = 0;
    flagsData.condition = 0;
    unsigned short memval = 0x3C3C;
    WriteMem16(SCRATCH_MEM, &memval, 1);
    WriteMem16(SCRATCH_MEM + 2, &memval, 1);
  }

  // This function does two unrelated tests that use the same callback.
  // They are distinguished by the location value.
  bool flagsTest() {
    // First test: just read and write the flags register
    for (flagsData.flags = 0; flagsData.flags <= 0x0F; ++flagsData.flags) {
      WriteFlags(flagsData.flags);
      flagsData.condition = ReadFlags() & 0x0F;
      if (flagsData.flags != flagsData.condition) {
        flagsData.location = 1;
        queuedLogMessageCount++;
        logQueueCallback(flagsCallback);
        return false;
      }
    }
    return false; // for now:

    // Second test: do a conditional move on carry
    // TODO
  }
#endif // COST
} // End of CostPrivate namespace

// === public functions of the CoST task ===

// This is called from the SerialTask or other executive to enable the
// tests to run. As always, no special synchronization is required since
// all activity runs in the foreground. 
void costRun() {
  MakeSafe();
  CostPrivate::currentTestId = CostPrivate::N_TESTS;
  CostPrivate::lastTestId = CostPrivate::N_TESTS - 1;  
	CostPrivate::running = true;
}

int costTestsStopping(char* bp, int bmax) {
  int result = snprintf_P(bp, bmax, PSTR("COST stopping"));
  if (result > bmax) result = bmax;
  return result;
}

// Called from the SerialTask or other executive to stop the tests from
// running. We stop the tests synchronously at the conclusion of the current
// tests, make all safe, and log a message. This requires a separate state
// in the state machine, implemented with a second boolean.
void costStop() {
  if (CostPrivate::running) {
    logQueueCallback(costTestsStopping);
    CostPrivate::stopping = true;
  }
}

void costTaskInit() {
  // The individual tests have their own init functions, called from task.
}

int costTask() {
#if COST
  // Do the next step in some test
  return CostPrivate::internalCostTask();
#else
  return 29023; // milliseconds
#endif  
}

