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
      int x;      
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
  } TestRef;

  const PROGMEM TestRef Tests[] = {
    { delayTaskInit,    delayTaskBody },
    { ucodeTestInit,    ucodeBasicTest },
    { memoryTestInit,   memoryBasicTest }  
  };

  constexpr byte N_TESTS = (sizeof(Tests) / sizeof(TestRef));
  constexpr byte MAX_TESTS = 0x10;
  byte currentTestId = N_TESTS;
  byte lastTestId = N_TESTS - 1;

  // Calllback for the executive's single log line per cycle
  byte costMessageCallback(byte *bp, byte bmax) {
    int result = snprintf_P((char *)bp, bmax, PSTR("cost: test cycle starting"));
    if (result > bmax) result = bmax;
    return result;
  }

  byte costTestStarting(byte *bp, byte bmax) {
    int result = snprintf_P((char *)bp, bmax, PSTR("  new test starting"));
    if (result > bmax) result = bmax;
    return result;
  }

  // The test executive
  int internalCostTask() {
    if (!running) {
      return 257; // come back and check a few times per second
    }

    // Is a new test cycle starting? (Including first-time initialization)
    if (currentTestId >= N_TESTS) {
      currentTestId = 0;
      logQueueCallback(costMessageCallback);
    }

    // Is a new test starting within the current cycle?
    if (lastTestId != currentTestId) {
      logQueueCallback(costTestStarting);
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
    return result;
  }

  void delayTaskInit() { 
    // about 40 calls per millisecond
    delayData.delay = 40L * 1000L * 11L;   
  }

  bool delayTaskBody() {
    if (delayData.delay < 0L) {
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

  // Calllback for the executive's single log line per cycle
  byte ucodeBasicMessageCallback(byte *bp, byte bmax) {
    int result = snprintf_P((char *)bp, bmax, PSTR("  ucodeBasic: fail op 0x%02X sl 0x%02X"),
      savedFailedOpcode, savedFailedSlice);
    if (result > bmax) result = bmax;
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
      savedFailedOpcode = ubData.opcode;
      savedFailedSlice = ubData.slice;
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
  // Calllback for the executive's single log line per cycle
  byte memoryBasicMessageCallback(byte *bp, byte bmax) {
    int result = snprintf_P((char *)bp, bmax, PSTR("  memoryBasic: nothing done"));
    if (result > bmax) result = bmax;
    return result;
  }

  void memoryTestInit() {
  }
  
  bool memoryBasicTest() { 
    logQueueCallback(memoryBasicMessageCallback);
    return false;   
  }
}

// === public functions of the CoST task ===

// This is called from the SerialTask or other executive to enable the
// tests to run. As always, no special synchronization is required since
// all activity runs in the foreground.
void costRun() {
  makeSafe();
	CostPrivate::running = true;
}

// Called from the SerialTask or other executive to stop the tests from
// running.
void costStop() {
	CostPrivate::running = false;
  makeSafe();
}

void costTaskInit() {
}

int costTask() {
  return CostPrivate::internalCostTask();
}



/*

int portTask() {
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

  if (!PortPrivate::validateOpcodeForSlice(opcode, slice)) {
      panic(opcode, slice);
      //setDisplay(0xAA);
      //failed = true;
      //return 103;
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
}

*/
