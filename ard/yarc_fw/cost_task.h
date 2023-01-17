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

namespace CostPrivate {
	bool running = true;

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
  };

  typedef void (*TestInit)();
  typedef bool (*Test)();

  void delayTaskInit(void);
  bool delayTaskBody(void);
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

  int queuedLogMessageCount = 0;

  // Callback for the executive's single log line per cycle
  byte testCycleStarting(byte *bp, byte bmax) {
    randomSeed(millis());
    int result = snprintf_P((char *)bp, bmax, PSTR("cost: test cycle starting"));
    if (result > bmax) result = bmax;
    queuedLogMessageCount--;
    return result;
  }

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
      return 87;
    }

    // Is a new test cycle starting? (Including first-time initialization)
    if (currentTestId >= N_TESTS) {
      currentTestId = 0;
      queuedLogMessageCount++;
      logQueueCallback(testCycleStarting);
      return 0;
    }

    // Is a new test starting within the current cycle?
    if (lastTestId != currentTestId) {
      queuedLogMessageCount++;
      logQueueCallback(testStarting);
      lastTestId = currentTestId;
      makeSafe(); // clean up YARC state for the next test
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

  // === ucode (Microcode RAM) basic test ===
  
  // The usual problem of saving data to be logged so it won't change underneath
  // us before the log callback is invoked and the log messages is formatted.
  static byte savedFailedOpcode = 0;
  static byte savedFailedSlice = 0;

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

  void memoryTestInit() {
    // Now in order to read and write main memory, we need to set the
    // the sysdata_src field of the microcode control register K to the
    // value MEM. This is a value of 5 in the three MS bits of K byte 1.
    // Then we need to enable other things than the Nano to drive sysdata.
    SetAH(0xFF);
    SetAL(0xFF);
    WriteByteToK(1, 0xBF); // 0B1011_1111 (101 in the high order bits)
    SetMCR(0xDB); // YARC/NANO# low, SYSBUS_EN low, all other high
    SetDH(0xFF);  // Not relevant since we only do 8-bit memory cycles

    mbData.AH = 0;
    mbData.AL = 0;
    mbData.DL = 0;
  }
  
  bool memoryBasicTest() {
    if (mbData.AH >= 0x78) {
      return false; // done - 0x7800 = 30k RAM
    }
    
    byte n = random(0, 255);
    for (int i = 0; i < 256; ++i) {
      mbData.AL = (byte) i;
      SetAL(mbData.AL);
      SetAH(mbData.AH & ~0x80); // write
      SetDL(n);
      SingleClock();

      SetAH(mbData.AH | 0x80); // read
      SingleClock();
      savedResult = GetBIR();
      // if (mbData.AL == 0x31 && n < 2) {
      //   // Make a "failure" happen every so often
      //   savedResult = 1 + n;
      // }
      if (savedResult != n) {
        queuedLogMessageCount++;
        logQueueCallback(memoryBasicMessageCallback);
        return false;
      } 

      n++;     
    }

    mbData.AH++;
    return true;   
  }
}

// === public functions of the CoST task ===

// This is called from the SerialTask or other executive to enable the
// tests to run. As always, no special synchronization is required since
// all activity runs in the foreground. For now, COST runs by default so
// there are no callers (TBD).
void costRun() {
  makeSafe();
	CostPrivate::running = true;
}

// Called from the SerialTask or other executive to stop the tests from
// running. For now, COST runs continuously so there are not callers.
void costStop() {
	CostPrivate::running = false;
  makeSafe();
}

void costTaskInit() {
  // The individual tests have their own init functions, called from task.
}

int costTask() {
  // Do the next step in some test
  return CostPrivate::internalCostTask();
}

