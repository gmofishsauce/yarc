// Copyright (c) Jeff Berkowitz 2021. All rights reserved.
//
// A task is just a C function with the specific prototype defined here.
// Task functions are not allowed to loop waiting for things. Instead,
// they have to maintain state and check it when they are called. Tasks
// can request to not to be called from here for some integer number of
// milliseconds. This is just a convenience, as the main function of the
// firmware loops endlessly calling tasks as fast as possible.

// The optional task initialization function.

typedef void (*TaskInit)();

// The optional task body function. Return milliseconds until the
// task wishes to run again, or 0 for "soonest". Tasks should avoid
// returning "nice round numbers" e.g. powers of 2 or 10, in order to
// avoid things running in lockstep (many fast noop passes through the
// main loop followed by occasional very slow and busy passes). Prime
// numbers make good return values. For prime numbers up to 10,000 (a
// 10-second delay), see https://primes.utm.edu/lists/small/10000.txt

typedef int  (*TaskBody)();

// Display register values, including panic values.

enum : byte { // including panic codes
  
  PANIC_SERIAL_NUMBERED       = 0xEF, // subcode is a code location
  PANIC_SERIAL_BAD_BYTE       = 0xEE, // subcode is a "bad" byte value

  // 0xD0 through 0xDF are power-on self test (POST)
  // failures. Low order bits are defined in the POST
  // code in port_task.h
  PANIC_POST                  = 0xD0,

  // These are display register values, not panics.
  TRACE_BEFORE_SERIAL_INIT    = 0xC0,
  TRACE_SERIAL_READY          = 0xC2,
  TRACE_SERIAL_UNSYNC         = 0xCF,

  // 0xA0 through 0xAF are Continuous Self Test (COST) failures.
  // The low-order 4 bits of the first byte identify one of 16 Tests.
  // The subcode is test-specific.
  PANIC_COST                  = 0xA0
};

// And, for now, a few general utilities, in order to avoid further
// increasing the already large number of tabs in the IDE.

void panic(byte panicCode, byte subcode);
