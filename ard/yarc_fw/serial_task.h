// Copyright (c) Jeff Berkowitz 2021. All rights reserved.
// Symbol prefixes: st, ST, serial

// Note: in general, naming is from perspective of the host (Mac).
// "Reading" means reading YARC memory and transmitting to host.
// "Writing" means writing YARC memory with data from host.

// TODO modify Protogen to use const, or at least to emit #pragma once.
// TODO All command implementations are supposed to check the state,
//      which is messy; and none of them currently check for an in-progress
//      command that isn't themselves, which is impossible, but would be
//      sensible to check.

namespace SerialPrivate {
  
  // The following is generated by a tool, protogen, so it stays in sync with
  // the Golang code at the other end of the serial line. This is the only
  // include file in the firmware that's not at the top level (in yarc_fw).
  #include "serial_protocol.h"

  // === the "lower layer": ring buffer implementation ===

  // Each ring buffer is a typical circular queue - since head == tail means "empty",
  // we can't use the last entry. So the queue can hold (RING_BUF_SIZE - 1) elements.
  // Since the head and tail are byte variables, the RING_BUF_SIZE must be <= 127. I'm
  // not sure that e.g. len() would work correctly for 128. The ring size does not have
  // to be a power of 2.

  constexpr int RING_BUF_SIZE = 16;
  constexpr int RING_MAX = (RING_BUF_SIZE - 1);
  
  typedef struct ring {
    byte head;  // Add at the head
    byte tail;  // Consume at the tail
    byte body[RING_BUF_SIZE];
  } RING;

  // Don't refer to these directly:
  RING receiveBuffer;
  RING transmitBuffer;

  // Instead, use these:
  RING* const rcvBuf = &receiveBuffer;
  RING* const xmtBuf = &transmitBuffer;

  // Return the number of data bytes in ring r.
  byte len(RING* const r) {
    int result = (r->head >= r->tail) ? r->head - r->tail : (r->head + RING_BUF_SIZE) - r->tail;
    if (result < 0 || result >= RING_BUF_SIZE) {
      panic(PANIC_SERIAL_NUMBERED, 1);
    }   
    return result;
  }

  // Return the available space in r
  byte avail(RING* const r) {
    byte result = (RING_BUF_SIZE - len(r)) - 1;
    if (result < 0 || result >= RING_BUF_SIZE) {
      panic(PANIC_SERIAL_NUMBERED, 2);
    }
    return result;
  }

  // Consume n bytes from the ring buffer r. In this
  // design, reading and consuming are separated.
  // panic: n < 0
  void consume(RING* const r, byte n) {
    if (n < 0) {
      panic(PANIC_SERIAL_NUMBERED, 3);
    }
    if (n == 0) {
      return;
    }
    if (n > len(r)) {
      panic(PANIC_SERIAL_NUMBERED, 4);
    }
    r->tail = (r->tail + n) % RING_BUF_SIZE;
  }

  // Return the next byte in the ring buffer. The state
  // of the ring is not changed.
  // panic: r is empty
  byte peek(RING* const r) {
    if (r->head == r->tail) {
      panic(PANIC_SERIAL_NUMBERED, 5);
    }
    return r->body[r->tail];
  }

  // Returns up to bMax bytes from the ring buffer, if any.
  // Does not change the state of the ring buffer.
  //
  // Unlike peek(), this function can be called when
  // when the buffer is empty. The value will not become
  // stale while the caller runs, assuming the caller
  // doesn't take any actions (put(), consume(), etc.) to
  // change the ring buffer state.
  //
  // returns the number of bytes placed at *bp, which may be
  // 0 and will not exceed bMax.
  byte copy(RING* const r, byte *bp, int bMax) {
    if (bMax < 0) {
      panic(PANIC_SERIAL_NUMBERED, 6);
    }
    
    int n = len(r);
    int i;
    for (i = 0; i < n && i < bMax; ++i) {
      *bp = r->body[(r->tail + i) % RING_BUF_SIZE];
      bp++;
    }
    return i;
  }
  
  // Return true if the ring buffer r is full.
  bool isFull(RING* const r) {
    return avail(r) == 0;
  }
  
  // Put the byte b in the ring buffer r
  // panic: r is full
  void put(RING* const r, byte b) {
    if (isFull(r)) {
      panic(PANIC_SERIAL_NUMBERED, 7);
    }
    r->body[r->head] = b;
    r->head = (r->head + 1) % RING_BUF_SIZE;
  }

  // === end of the "lower layer" (ring buffer implementation) ===

  // === the "middle layer": connection state and send/receive ===

  typedef byte State;
  
  constexpr State STATE_UNSYNC = 0;
  constexpr State STATE_DESYNCHRONIZING = 1;
  constexpr State STATE_READY = 2;
  constexpr State STATE_PROCESSING = 3;
  
  State state = STATE_UNSYNC;

  // Enter the unsynchronized state immediately. This cancels any
  // pending output include NAKs sent, etc.
  void stProtoUnsync() {
    rcvBuf->head = 0;
    rcvBuf->tail = 0;
    xmtBuf->head = 0;
    xmtBuf->tail = 0;
    state = STATE_UNSYNC;
    SetDisplay(0xCF);
  }

  // Return true if the byte is a valid command byte.
  // The value STCMD_BASE itself is not permitted as a command
  // because its NAK is a transmissible ASCII character (space).
  bool isCommand(byte b) {
    return b > STCMD_BASE; // 0xE1 .. 0xFF
  }

  // Send the byte b without interpretation
  // panic: xmtBuf is full
  void send(byte b) {
    put(xmtBuf, b);
  }

  // Return true if it's possible to transmit (the transmit buffer is not full)
  bool canSend(byte n) {
    return n < avail(xmtBuf);
  }

  // Send an ack for the byte b, which must be
  // a valid command byte
  // panic: b is not a command byte
  // panic: xmtBuf is full
  void sendAck(byte b) {
    if (!isCommand(b)) {
      panic(PANIC_SERIAL_BAD_BYTE, b);
    }
    send(ACK(b));
  }

  // Nak the byte b, which was received in the context
  // of a command byte but may not in fact be a command.
  // The argument is not used.
  // panic: xmtBuf is full  
  void sendNak(byte b) {
    send(STERR_BADCMD);
  }

  // === Poll buffer support ===

  typedef State (*CommandHandler)(RING *const r, byte b);
  CommandHandler inProgress = 0;

  // The poll buffer (serial output buffer) is the single largest
  // user of RAM in the entire system. It allows us to hide the
  // nonblocking nature of the code from functions that want to
  // generate data for the host, allowing use of e.g. xnprintf().
  // It's 259 to allow for a command byte, a count byte, 255 data
  // bytes, an unneeded terminating nul should it be written by a
  // library function, and a guard byte at the end.
  constexpr int POLL_BUF_SIZE = 259;
  constexpr int POLL_BUF_LAST = (POLL_BUF_SIZE - 1);
  constexpr int POLL_BUF_MAX_DATA = 255;
  constexpr byte GUARD_BYTE = 0xAA;

  typedef struct pollBuffer {
    int remaining;
    int next;
    bool inuse;
    byte buf[POLL_BUF_SIZE];
  } PollBuffer;

  PollBuffer uniquePollBuffer; // there can be only one (in practice)
  PollBuffer* const pb = &uniquePollBuffer;

  // Allocate the poll buffer.
  // panic: poll buffer in use.
  void allocPollBuffer() {
    if (pb->inuse) {
      panic(PANIC_SERIAL_NUMBERED, 0xD);
    }
    pb->inuse = true;
    pb->remaining = 0;
    pb->next = 0;
    pb->buf[POLL_BUF_LAST] = GUARD_BYTE;
  }

  // Free the poll buffer
  // panic: poll buffer is not in use
  // panic: the guard byte was overwritten.
  void freePollBuffer() {
    if (!pb->inuse) {
      panic(PANIC_SERIAL_NUMBERED, 0xE);
    }
    if (pb->buf[POLL_BUF_LAST] != GUARD_BYTE) {
      panic(PANIC_SERIAL_NUMBERED, 0xA);
    }
    pb->next = 0;
    pb->remaining = 0;
    pb->inuse = false;
  }

  // === end of the "middle layer" ===

  // === Protocol command handlers ===
  // Command handlers must return the "next" state
  // When an error occurs, the host responds by ending the session.
  // This greatly simplifies error handling here - there's no need
  // to consume the rest of the supposed command after an error.

  // A bad command byte was processed (either not a command byte
  // value or an unimplemented command). We cannot directly enter
  // the UNSYNC state because clearing the ring buffer would mean
  // the NAK would not be sent. So we send the NAK, enter the
  // "desynchronizing" state, and we don't consume the command
  // byte. This ensures we'll come back here again soon but after
  // process() has a chance to at least try and push out the NAK.
  // Then we reset everything and wait for the host to start a
  // new session.
  State stBadCmd(RING* const r, byte b) {
    if (state != STATE_DESYNCHRONIZING) {
      if (!canSend(1)) {
        // We have an error -and- the transmit buffer is full.
        // Give up with a panic that is distinct to the condition.
        panic(PANIC_SERIAL_NUMBERED, 0xC);
      }
      sendNak(b);
      return STATE_DESYNCHRONIZING;
    } else {
      // There's no need to consume() here either because this
      // will reset the ring buffer:
      stProtoUnsync();
      return STATE_UNSYNC;
    }
  }

  State stUndef(RING* const r, byte b) {
    return stBadCmd(r, b); // for now    
  }

  State stGetMcr(RING* const r, byte b) {
    if (!canSend(2)) {
      return state;
    }
    consume(r, 1);

    if (state != STATE_READY) {
      sendNak(b);
    } else {   
      sendAck(b);
      send(GetMCR());
    }
    return state;
  }

  State stRunCost(RING* const r, byte b) {
    consume(r, 1);
    if (state != STATE_READY) {
      sendNak(b);
    } else {   
      costRun();
      sendAck(b);
    }
    return state;
  }

  State stStopCost(RING* const r, byte b) {
    consume(r, 1);
    if (state != STATE_READY) {
      sendNak(b);
    } else {   
      costStop();
      sendAck(b);
    }
    return state;
  }

  State stRun(RING* const r, byte b) {
    return stBadCmd(r, b);
  }

  State stStop(RING* const r, byte b) {
    return stBadCmd(r, b);
  }

  // Respond to a poll request from the host. This is a state machine
  // that maintains its own state, separate from the State enum.
  State stPoll(RING* const r, byte b) {
    if (inProgress) {
      while (canSend(1) && pb->remaining > 0) {
        send(pb->buf[pb->next]);
        pb->remaining--;
        pb->next++;
      }
      if (pb->remaining == 0) {
        consume(r, 1);
        freePollBuffer();
        inProgress = 0;              
      }
    } else {
      if (pb->inuse) {
        // We need the buffer but it's in use.
        panic(PANIC_SERIAL_NUMBERED, 0xB);
      }
      
      inProgress = stPoll;
      allocPollBuffer();
      byte n = logGetPending((char *)&pb->buf[2], POLL_BUF_MAX_DATA);
      pb->buf[0] = ACK(b);
      pb->buf[1] = n; // may be zero (it usually is)
      pb->next = 0;
      pb->remaining = 2 + n;
    }
    return state;
  }

  State stResp(RING* const r, byte b) {
    return stBadCmd(r, b);
  }

  // GetVer command - when we can send, consume the command
  // byte and send the ack and version. Does not change state.
  State stGetVer(RING* const r, byte b) {
    if (!canSend(2)) {
      return state;
    }
    consume(r, 1);

    if (state != STATE_READY) {
      sendNak(b);
    } else {   
      sendAck(b);
      send(PROTOCOL_VERSION);
    }
    return state;
  }

  // Sync command - just ack it and set the display register
  State stSync(RING* const r, byte b) {
    if (!canSend(1)) {
      return state;
    }
    consume(r, 1);
    sendAck(b);
    SetDisplay(0xC2);
    return STATE_READY;
  }

  State stSetAH(RING* const r, byte b) {
    return stBadCmd(r, b);
  }

  State stSetAL(RING* const r, byte b) {
    return stBadCmd(r, b);
  }

  State stSetDH(RING* const r, byte b) {
    return stBadCmd(r, b);
  }

  State stSetDL(RING* const r, byte b) {
    return stBadCmd(r, b);
  }

  // TODO - if ever implemented, this function must return
  // the Bus Interface Register (BIR) - spec change, 5/2023.
  State stOneClk(RING* const r, byte b) {
    return stBadCmd(r, b);
  }

  State stGetBir(RING* const r, byte b) {
    return stBadCmd(r, b);
  }

  State stWrSlice(RING* const r, byte b) {
    return stBadCmd(r, b);
  }

  State stRdSlice(RING* const r, byte b) {
    return stBadCmd(r, b);
  }

  // This variable is set by stOneXfr() in preparation for a page
  // read or write. It should be checked against END_MEM before
  // every use and set back to END_MEM when a page write ends for
  // any reason.
  static unsigned short stShadowAHAL = END_MEM;

  // Do a bus transfer with the given AH, AL, DH, and DL. Save
  // the values of AH, AL so that a following WrPage or RdPage
  // command can use the value. The transfers are always
  // byte transfers, and the host has no control over the K register
  // so can only transfer bytes to main memory using these commands.
  // Microcode and ALU memory are written with distinct commands,
  // and arbitrary registers may (at least in theory) be written
  // from the host with combinations of SetK, SetMCR, the four set
  // bus register commands, and single clock cycles. N.B. - there's
  // no count byte with fixed arguments. Return the BIR to the host
  // (spec change 5/2023).
  State stOneXfr(RING* const r, byte b) {
    byte cmdBuf[5];
    if (!canSend(1)) {
      return state;
    }
    if (state != STATE_READY) {
      sendNak(b);
      return state;
    }
    if (copy(rcvBuf, cmdBuf, 5) < 5) {
      // Entire commmand not available yet
      return state;
    }
    consume(rcvBuf, 5);
    stShadowAHAL = BtoS(cmdBuf[1], cmdBuf[2]);
    if (stShadowAHAL >= END_MEM) {
      stShadowAHAL = END_MEM;
      sendNak(b);
      return state;
    }

    WriteK(0xFF, 0xFF, 0xFF, 0xFF);
    SetAH(StoHB(stShadowAHAL));
    SetAL(StoLB(stShadowAHAL));
    SetDH(cmdBuf[3]);
    SetDL(cmdBuf[4]);
    SingleClock();
    byte bir = GetBIR();
    stShadowAHAL++;
    sendAck(b);
    send(bir); // Protocol v8 - always return BIR
    return state;
  }

  // Write up to 255 bytes at the address set by a previous single
  // transfer (stOneXfr()) command. We demand through the entire
  // process that there is space for a response, although we could
  // check that only at the end. We allocate the poll buffer because
  // we can use its header fields for tracking the number of bytes
  // left to read from the connection and write to memory. (We also
  // stash the command byte there so we can defer the decision about
  // ack versus nak.)
  State stWrPage(RING* const r, byte b) {
    byte cmdBuf[2];

    if (!canSend(1)) {
      return state;
    }
    if (state != STATE_READY) {
      stShadowAHAL = END_MEM;
      sendNak(b);
      return state;      
    }

    // Check for initialization of a page write. If
    // the length byte isn't here yet, come back later.
    if (!inProgress) {
      if (copy(rcvBuf, cmdBuf, 2) < 2) {
        return state;
      }
      consume(rcvBuf, 2);

      if (pb->inuse) {
        // We need the buffer but it's in use.
        panic(PANIC_SERIAL_NUMBERED, 0xF);
      }
      allocPollBuffer();

      inProgress = stWrPage;
      pb->buf[0] = b; // for later ack or nak
      pb->next = 1;   // keep sanity, but not used
      pb->remaining = cmdBuf[1];
      // fall into...
    }

    // In progress consuming bytes from the host link and
    // writing them to main memory. Loop while we're not done,
    // there are bytes available in the receive buffer, and
    // we're still in the same millisecond. When the millisecond
    // clicks over, we remain in progress, but we return and give
    // the rest of the tasks a change to execute.
  
    unsigned long start = millis();
    while (pb->remaining > 0 && len(r) > 0 && millis() == start) {
      if (stShadowAHAL >= END_MEM) {
        sendNak(pb->buf[0]);
        stShadowAHAL = END_MEM;
        freePollBuffer();
        inProgress = 0;              
        return state;
      }
      SetAH(StoHB(stShadowAHAL));
      SetAL(StoLB(stShadowAHAL));
      SetDH(0); // doesn't matter - it's a byte write
      SetDL(peek(r));
      SingleClock();

      consume(r, 1);
      pb->remaining--;
      stShadowAHAL++;
    }

    // Now we may be done, or we might have caught up with the buffer,
    // or our millisecond may have run out. We only care if we're done.
    if (pb->remaining == 0) {
      // We're done. Ack the command and clean up.
      sendAck(pb->buf[0]);
      stShadowAHAL = END_MEM;
      freePollBuffer();
      inProgress = 0;              
    }
    return state;
  }

  // Read a page from the address established by a previous single transfer.
  // Like stWrPage, this function can only address main memory, and only as
  // a set of bytes.
  //
  // XXX - stOneXfr() only allows writes, so at the moment there's no way to
  // initialize the address (stShadowAHAL).
  State stRdPage(RING* const r, byte b) {
    byte cmdBuf[2];

    if (!canSend(1)) {
      return state;
    }
    if (state != STATE_READY) {
      stShadowAHAL = END_MEM;
      sendNak(b);
      return state;      
    }
    if (inProgress && inProgress != stRdPage) {
      panic(PANIC_SERIAL_NUMBERED, 0x10);
    }

    // Check for initialization of a page write. If
    // the length byte isn't here yet, come back later.
    if (!inProgress) {
      if (copy(rcvBuf, cmdBuf, 2) < 2) {
        return state;
      }
      consume(rcvBuf, 2);
      if (pb->inuse) {
        // We need the buffer but it's in use.
        panic(PANIC_SERIAL_NUMBERED, 0x11);
      }
      allocPollBuffer();

      inProgress = stRdPage;
      pb->buf[0] = b; // for later ack or nak
      pb->next = 1;   // keep sanity, but not used
      pb->remaining = cmdBuf[1];
      // fall into...
    }

    // In progress sending bytes to the host link and reading
    // them from main memory. Loop while we're not done, there
    // is space available in the transmit buffer, and we're still
    // in the same millisecond. When the millisecond clicks over,
    // we remain in progress, but we return and give the rest of
    // the tasks a change to execute.
  
    unsigned long start = millis();
    while (pb->remaining > 0 && avail(xmtBuf) > 0 && millis() == start) {
      if (stShadowAHAL >= END_MEM) {
        sendNak(pb->buf[0]);
        stShadowAHAL = END_MEM;
        freePollBuffer();
        inProgress = 0;              
        return state;
      }
      SetAH(StoHB(stShadowAHAL) | 0x80); // 0x80 = Nano's read bit
      SetAL(StoLB(stShadowAHAL));
      SetDH(0); // doesn't matter - it's a byte write
      SetDL(0); // Also doesn't matter - it's a read
      SingleClock();
      send(GetBIR());
      pb->remaining--;
      stShadowAHAL++;
    }
    return state;
  }

  // SetK has a count byte which may be 2 or 4 followed by either
  // 2 or 4 argument bytes. If 4, the bytes are all four K register
  // values in order 3..0. The command was designed this way partly
  // to see what the implementation code would look like. The 2-arg
  // form is no longer supported because it was never used.
  State stSetK(RING* const r, byte b) {
    byte cmdBuf[6];
    if (!canSend(1)) {
      return state;
    }

    if (copy(rcvBuf, cmdBuf, 2) == 2) {
      // We have the command and the count
      if (cmdBuf[1] == 2 && copy(rcvBuf, cmdBuf, 4) == 4) { // short form
        // No longer supported
        consume(rcvBuf, 4);
        sendNak(b);
      } else if (cmdBuf[1] == 4 && copy(rcvBuf, cmdBuf, 6) == 6) { // long form
        consume(rcvBuf, 6);
        WriteK(cmdBuf[2], cmdBuf[3], cmdBuf[4], cmdBuf[5]);
        sendAck(b);
      } // else don't consume and don't reply; just wait
    }
    return state;
  }
  
  // Set the MCR. This is a change in philosophy added December 2022.
  State stSetMCR(RING* const r, byte b) {
    if (!canSend(1)) {
      return state;
    }
    byte cmdbuf[2];
    if (copy(r, cmdbuf, 2) != 2) {
      return state;
    }
    consume(r, 2);

    if (state != STATE_READY) {
      sendNak(b);
    } else {
      SetMCR(cmdbuf[1]);
      sendAck(b);
    }
    return state;
  }

  // Jump table for protocol command handlers. The table is stored in
  // PROGMEM (ROM) so requires special access, below.
  
  const PROGMEM CommandHandler handlers[] = {
    stBadCmd,   stGetMcr,  stRunCost,  stStopCost,   // 0xE0 ...
    stUndef,    stUndef,   stUndef,    stRun,        // 0xE4 ...
    stStop,     stPoll,    stResp,     stUndef,      // 0xE8 ...
    stUndef,    stUndef,   stGetVer,   stSync,       // 0xEC ...
  
    stSetAH,    stSetAL,   stSetDH,    stSetDL,      // 0xF0 ...
    stOneClk,   stGetBir,  stWrSlice,  stRdSlice,    // 0xF4 ...
    stOneXfr,   stWrPage,  stRdPage,   stSetK,       // 0xF8 ...
    stSetMCR,   stBadCmd,  stBadCmd,   stBadCmd,     // 0xFC ...
  };

  // There is at least one byte waiting to be processed in the receive-
  // side ring buffer at r. The command handler may or may not consume
  // the byte(s) on this call, but must always return the next state.
  
  void process(RING *const r, byte b) {
    CommandHandler handler;
    if (inProgress) {
      handler = inProgress;
    } else if (isCommand(b)) {
      handler = pgm_read_ptr_near(&handlers[b - STCMD_BASE]);
    } else {
      handler = stBadCmd;
    }
    state = (*handler)(r, b);
  }

  // The serial task. Called as often as possible (no delay).
  // Try to write everything in the write buffer. Then try to
  // read all the available bytes. Finally, if the read ring
  // buffer is not empty, invoke process() to handle it. Note
  // that process() may do nothing, waiting for more bytes.
  
  int serialTask() {
    
    while (len(xmtBuf) > 0 && Serial.availableForWrite() != 0) {
      if (Serial.write(peek(xmtBuf)) != 1) {
        panic(PANIC_SERIAL_NUMBERED, 9); // TODO
      }
      consume(xmtBuf, 1);
    }

    while (!isFull(rcvBuf) && Serial.available()) {
      put(rcvBuf, Serial.read());
    }

    if (len(rcvBuf) > 0) {
      process(rcvBuf, peek(rcvBuf));
    }
    
    return 0;
  }
}

// Public interface

void serialShutdown() {
  SerialPrivate::stProtoUnsync();
}

void serialTaskInit() {
  SetDisplay(TRACE_BEFORE_SERIAL_INIT);
  SerialPrivate::stProtoUnsync();

  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect.
  }
}

int serialTaskBody() {
  return SerialPrivate::serialTask();
}
