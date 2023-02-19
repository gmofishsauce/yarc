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
	bool running = true;

#if COST

  static union {
    struct delayData {
      long int delay;      
    } delayData;
    struct ucodeBasicData {
      byte opcode;
      byte slice;
      byte data[64];
      byte result[64];
    } ubData;
    struct memoryBasicData {
      byte AH;
      byte AL;
      byte data[64];
      byte result[64];
      byte DL;
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
    } regData;
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
  void memoryTestInit(void);
  bool memoryBasicTest(void);

  typedef struct tr {
    TestInit init;
    Test test;
    char* name;
  } TestRef;

  const PROGMEM TestRef Tests[] = {
    { delayTaskInit,    delayTaskBody,   "delay"  },
    { m16TestInit,      m16TestBody,     "mem16"  },
    { regTestInit,      regTestBody,     "reg"    },
    { ucodeTestInit,    ucodeBasicTest,  "ucode"  },
    { memoryTestInit,   memoryBasicTest, "memory" }  
  };

  constexpr byte N_TESTS = (sizeof(Tests) / sizeof(TestRef));
  constexpr byte MAX_TESTS = 0x10;
  byte currentTestId = N_TESTS;
  byte lastTestId = N_TESTS - 1;

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
  byte testCycleStarting(byte *bp, byte bmax) {
    int result = snprintf_P((char *)bp, bmax, PSTR("cost: test cycle starting"));
    if (result > bmax) result = bmax;
    queuedLogMessageCount--;
    return result;
  }

  // Callback for test starting within a cycle. This function also doesn't
  // use any variable data.
  byte testStarting(byte *bp, byte bmax) {
    int result = snprintf_P((char *)bp, bmax, PSTR("  test %s starting"),
      pgm_read_ptr_near(&Tests[currentTestId].name));
    if (result > bmax) result = bmax;
    queuedLogMessageCount--;
    return result;
  }

  // The test executive
  int internalCostTask() {
    if (!running) {
      return 257; // come back and check a few times per second
    }

    // Wait for all previously queued log messages to be formatted and sent
    // to the host. See the block comment above ("General note about...")
    if (queuedLogMessageCount > 0) {
      SetDisplay(0x38);
      return 87;
    }
    SetDisplay(0xC4);

    // Is a new test cycle starting? (Including first-time initialization)
    if (currentTestId >= N_TESTS) {
      currentTestId = 0;
      randomSeed(millis());
      queuedLogMessageCount++;
      logQueueCallback(testCycleStarting);
      return 0;
    }

    // Is a new test starting within the current cycle?
    if (lastTestId != currentTestId) {
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

  byte delayTaskMessageCallback(byte *bp, byte bmax) {
    int result = snprintf_P((char *)bp, bmax, PSTR("  delayTask: done"));
    if (result > bmax) result = bmax;
    queuedLogMessageCount--;
    return result;
  }

  void delayTaskInit() { 
    // about 30 calls per millisecond
    delayData.delay = 30L * 1000L * 11L;   
  }

  bool delayTaskBody() {
    if (delayData.delay < 0L) {
      queuedLogMessageCount++;
      logQueueCallback(delayTaskMessageCallback);
      return false; // done
    }
    delayData.delay = delayData.delay - 1L;
    return true; // not done
  }

  // === m16 (16 bit memory cycles) test ===

  byte m16LowByteCallback(byte *bp, byte bmax) {
    int result = snprintf_P((char *)bp, bmax, PSTR("  m16: low: got 0x%02X"), m16Data.readValue);
    if (result > bmax) result = bmax;
    queuedLogMessageCount--;
    return result;
  }

  byte m16HighByteCallback(byte *bp, byte bmax) {
    int result = snprintf_P((char *)bp, bmax, PSTR("  m16: high: got 0x%02X"), m16Data.readValue);
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
//#if 0
    if (!readStep8()) {
      queuedLogMessageCount++;
      logQueueCallback(m16HighByteCallback);
      return false; // only detect 1 failure
    }
//#endif
    m16Data.AH++;
    m16Data.DL += 7;
    m16Data.DH += 17;
    
    return true; // not done  
  }
  
  // === reg (general register) basic test ===

  void regTestInit() {
  }

  // Write 16 bits to main memory as a single write. Move the 16 bits to 
  // a register in a single operation. Read the register to a second memory
  // location with a single register read/memory write. Compare the two
  // memory locations (in the Nano, using byte reads and writes) and report
  // if the data transfers were all successful.
  bool regTestBody() {
    WriteByteToK(3, 0xF8);  // LS bits are dst (target) = unconditional R0
    WriteByteToK(2, 0xFF);  // no ALU activity
    WriteByteToK(1, 0xFE);  // LS bit enables write to register
    WriteByteToK(0, 0xFF);  // Don't clock IR; RSW from microcode
    SingleClock();
    return false;
  }
  
  // === ucode (Microcode RAM) basic test ===
  
  // Calllback for the executive's single log line per cycle. Here is an example
  // of a place where incorrect values would be logged if we didn't wait for the
  // log message queue to clear each time we queue a message: the test after ucode
  // would overwrite the opcode and slice, causing nonsense values to be logged.
  byte ucodeBasicMessageCallback(byte *bp, byte bmax) {
    int result = snprintf_P((char *)bp, bmax, PSTR("  ucodeBasic: fail op 0x%02X sl 0x%02X"),
      ubData.opcode, ubData.slice);
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
    
    WriteBytesToSlice(opcode | 0x80, slice, ubData.data, SIZE);
    ReadBytesFromSlice(opcode | 0x80, slice, ubData.result, SIZE);
    
    for (int i = 0; i < SIZE; ++i) {
      if (ubData.data[i] != ubData.result[i]) {
        return false;
      }
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

  // Solve the usual problem of closing over an address rather than a value:
  static byte savedResult;

  // Callback for the executive's single log line per cycle
  byte memoryBasicMessageCallback(byte *bp, byte bmax) {
    int result = snprintf_P((char *)bp, bmax,
      PSTR("  memoryBasic: test failed at 0x%02X 0x%02X exp 0x%02X got 0x%02X"),
      mbData.AH, mbData.AL, mbData.DL, savedResult);
    if (result > bmax) result = bmax;
    queuedLogMessageCount--;
    return result;
  }

  // In order to read and write main memory, we need to set the sysdata_src
  // field of the microcode control register K to the value MEM. This is a
  // value of 5 in the three MS bits of K byte 1. Then we need to enable
  // other things than the Nano to drive sysdata.
  void prepMemoryWrite() {    
    SetAH(0xFF);
    SetAL(0xFF);
    WriteK(0xFF, 0xFF, 0xBF, 0xFF); // K1 = 0B1011_1111 (sysdata_src = mem)
    SetMCR(McrEnableSysbus(MCR_SAFE));
    SetDH(0xFF);  // Not relevant since we only do 8-bit memory cycles
  }

  void doMemoryWrite(byte ah, byte al, byte n) {
      SetAL(mbData.AL);
      SetAH(mbData.AH & ~0x80); // write
      SetDL(n);
      SingleClock();
  }

  // Prepare for a single pass over memory
  void memoryTestInit() {
    prepMemoryWrite();

    mbData.AH = 0;
    mbData.AL = 0;
    mbData.DL = 0;
  }
  
  // Do a single pass over memory, 256 bytes per call.  
  bool memoryBasicTest() {
    if (mbData.AH >= 0x78) {
      return false; // done - 0x7800 = 30k RAM
    }
    
    byte n = random(0, 255);
    for (int i = 0; i < 256; ++i, ++n) {
      mbData.AL = (byte) i;
      doMemoryWrite(mbData.AH, mbData.AL, n); // given MCR and K set

      SetAH(mbData.AH | 0x80); // read
      SingleClock();
      savedResult = GetBIR();
      if (savedResult != n) {
        queuedLogMessageCount++;
        logQueueCallback(memoryBasicMessageCallback);
        return false;
      } 
    }

    mbData.AH++;
    return true;   
  }
#endif // COST
} // End of CostPrivate namespace

// === public functions of the CoST task ===

// This is called from the SerialTask or other executive to enable the
// tests to run. As always, no special synchronization is required since
// all activity runs in the foreground. For now, COST runs by default so
// there are no callers (TBD).
void costRun() {
  MakeSafe();
	CostPrivate::running = true;
}

// Called from the SerialTask or other executive to stop the tests from
// running. For now, COST runs continuously so there are not callers.
void costStop() {
	CostPrivate::running = false;
  MakeSafe(); // ???
}

void costTaskInit() {
  // The individual tests have their own init functions, called from task.
}

int costTask() {
#if COST
  // Do the next step in some test
  return CostPrivate::internalCostTask();
#else
  return 29023;
#endif  
}

