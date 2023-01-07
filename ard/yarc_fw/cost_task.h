// Copyright (c) Jeff Berkowitz 2023. All rights reserved.
// Continuous Self Test (CoST) task for YARC.
// Symbol prefixes: co, CO, cost, etc.

namespace CostPrivate {
	bool running = false;
}

void costRun() {
	CostPrivate::running = true;
}

void costStop() {
	CostPrivate::running = false;
}

void costTaskInit() {
}

int costTask() {
	if (!CostPrivate::running) {
		return 101;
	}

	// Test code here

	return 0;
}


/*

  // Write the entire 64-byte slice of data for the given opcode with
  // values derived from the opcode. Read the data back from the slice
  // and check it. This function uses 128 bytes of static storage, so
  // it's inside an #if that can be disabled. The #if also disables
  // calls to this function from the portTask() function, below.
  bool validateOpcodeForSlice(byte opcode, byte slice) {
    static byte data[64];
    static byte result[64];
    const byte N = 2; // XXX - for troubleshooting can be less than 64 

    for (int i = 0; i < N; ++i) {
      data[i] = opcode + i;
    }
    
    writeBytesToSlice(opcode | 0x80, slice, data, N);
    readBytesFromSlice(opcode | 0x80, slice, result, N);
    
    for (int i = 0; i < N; ++i) {
      if (data[i] != result[i]) {
        return false;
      }
    }

    return true;
  }

*/

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
