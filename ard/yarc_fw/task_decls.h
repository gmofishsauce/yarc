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
  PANIC_UCODE_VERIFY          = 0xED, // microcode write failure; subcode is opcode
  PANIC_ALIGNMENT             = 0xEC, // unaligned write request: subcode is code location
  PANIC_ARGUMENT              = 0xEB, // invalid argument: subcode is code location
  PANIC_MEM_VERIFY            = 0xEA, // memory write failure; subcode is value read back

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

// Convert two bytes to short, being careful about sign extension
#define BtoS(bh, bl) (((unsigned short)(bh) << 8) | (unsigned char)(bl))
// Convert the high byte of a short to byte, careful about sign extension
#define StoHB(s) ((unsigned char)(((s) >> 8)))
// Similarly for the low byte
#define StoLB(s) ((unsigned char)(s))

// I spent some time considering how to represent small chunks of microcode,
// especially individual K-register values which are single microcode words.
// I tried some "modern" ways, such as:
//
// constexpr byte WRMEM16_FROM_NANO[] = {0xFF, 0xFF, 0xFF, 0x3F};
// WriteK(WRMEM16_FROM_NANO);  // write memory, 16-bit access
//
// This is with an override of WriteK() that took a single pointer.
// I found that this generated 38 bytes of code before the call,
// taking about 18 clocks before the call instruction:
//
//  185c:   80 91 0b 01     lds r24, 0x010B ; 0x80010b <next+0x8>
//  1860:   90 91 0c 01     lds r25, 0x010C ; 0x80010c <next+0x9>
//  1864:   a0 91 0d 01     lds r26, 0x010D ; 0x80010d <next+0xa>
//  1868:   b0 91 0e 01     lds r27, 0x010E ; 0x80010e <next+0xb>
//  186c:   89 83           std Y+1, r24    ; 0x01
//  186e:   9a 83           std Y+2, r25    ; 0x02
//  1870:   ab 83           std Y+3, r26    ; 0x03
//  1872:   bc 83           std Y+4, r27    ; 0x04
//  1874:   ce 01           movw    r24, r28
//  1876:   01 96           adiw    r24, 0x01   ; 1
//  1878:   0e 94 29 0a     call    0x1452  ; 0x1452 <_Z6WriteKPh>
//
// And then I tried the "ugly", old-school C language #define solution:
// #define WRITE_REG_16_FROM_NANO(reg) (0xF8 | (reg & 0x03)), 0xFF, 0xFE, 0x3F
// WriteK(WRITE_REG_16_FROM_NANO(reg));    
//
// I found that this "older" solution generated 8 bytes of code before the
// function call, taking 4 clocks before the call:
//
// 19c6:   2f e3           ldi r18, 0x3F   ; 63
// 19c8:   4e ef           ldi r20, 0xFE   ; 254
// 19ca:   6f ef           ldi r22, 0xFF   ; 255
// 19cc:   8b ef           ldi r24, 0xFB   ; 251
// 19ce:   0e 94 85 09     call    0x130a  ; 0x130a <_ZN11PortPrivate14internalWriteKEhhhh>
//
// I went with the ugly old C language solution.

#define WRMEM16_FROM_NANO                     0xFF, 0xFF, 0xFF, 0x3F
#define RDMEM8_TO_NANO                        0xFF, 0xFF, 0x9F, 0xFF
#define WRMEM8_FROM_NANO                      0xFF, 0xFF, 0xFF, 0x7F
#define WRITE_REG_16_FROM_NANO(reg)          (0xF8 | (reg & 0x03)), 0xFF, 0xFE, 0x3F
#define STORE_REG_INDIRECT(aReg, dReg)       (0x07 | ((aReg&3) << 6) | ((dReg&3) << 3)), 0xFF, 0x1F, 0x3F
#define CONDITIONAL_MOVE_INDIRECT(a, d, c)   ((0x3C | (a&3) << 6) | (d&3)), (0x0F | (c&0xF) << 4), 0x9E, 0xBF
#define LOAD_FLAGS_INDIRECT_R3                0xFF, 0xFE, 0x9F, 0xFF
#define RD_FLAGS_TO_NANO                      0xFF, 0xFF, 0x7F, 0xFF
#define MICROCODE_IDLE                        0xFF, 0xFF, 0xFF, 0xFF

// For now, at least, the 12 unassigned opcodes from 0xF0 through 0xFB
// are reserved for use by the Nano in test and initialization sequences.
#define SCRATCH_OPCODE_F0 ((unsigned byte)0xF0) // write flags
#define SCRATCH_OPCODE_F1 ((unsigned byte)0xF1) // read value of register
#define SCRATCH_OPCODE_F2 ((unsigned byte)0xF2) // conditional move indirect memory to register

// For now, at least, the last 256 bytes of memory are reserved for scratch
// use by the Nano. This region may also be used for the eventual buffer
// holding YARC requests to the host (these are transmitted by the Nano).
#define SCRATCH_MEM ((unsigned short)0x7700)




