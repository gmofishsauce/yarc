// Copyright (c) Jeff Berkowitz 2021. All rights reserved.
// Symbol prefixes: st, ST, serial

// Note: in general, naming is from perspective of the host (Mac).
// "Reading" means reading YARC memory and transmitting to host.
// "Writing" means writing YARC memory with data from host.

// TODO modify Protogen to use const, or at least to emit #pragma once.
// TODO The BadCommand implementation only gives one "spin", as little as
//      30uS or so, for the nak to go out. It should give at least 1 mS.

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

  // State of the connection. There are actually four states, as the
  // READY state is broken into "ready to accept a new command" and
  // "processing a command is in progress". But the latter state is
  // easier to represent by a function pointer that says what function
  // to call for continued processing of a long command or response.
  typedef byte State;
  
  constexpr State STATE_UNSYNC = 0;           // initial state; no session
  constexpr State STATE_DESYNCHRONIZING = 1;  // trouble; tearing down session
  constexpr State STATE_READY = 2;            // session in progress

  State state = STATE_UNSYNC;

  // Commands may require more than one call to the serial task to read
  // or push out all their data. When this occurs, we set an "in progress
  // handler", which is a contextless function. This is the second half
  // of the READY state - the substate where a command is in progress.
  typedef State (*InProgressHandler)(void);
  InProgressHandler inProgress = 0;

  // Enter the unsynchronized state immediately. This cancels any
  // pending output include NAKs sent, etc.
  void stProtoUnsync() {
    rcvBuf->head = 0;
    rcvBuf->tail = 0;
    xmtBuf->head = 0;
    xmtBuf->tail = 0;
    inProgress = 0;
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

  // Return true if it's possible to add n bytes to the transmit ring buffer
  bool canSend(byte n) {
    return n < avail(xmtBuf); // XXX should be <= ?
  }

  bool canReceive(byte n) {
    return n < len(rcvBuf);   // XXX should be <= ?
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

  // Command handlers interpret newly-arrived commands. They may
  // fully process the command or they may install an in-progress
  // command handler to process multiple calls.
  typedef State (*CommandHandler)(RING *const r, byte b);

  // The poll buffer (serial output buffer) is the single largest
  // user of RAM in the entire system. It allows us to hide the
  // nonblocking nature of the code from functions that want to
  // generate data for the host, allowing use of e.g. xnprintf().
  // It's 259 to allow for a command byte, a count byte, 255 data
  // bytes, an unneeded terminating nul should it be written by a
  // library function, and a guard byte at the end.
  //
  // XXX - In fact, everything must be sent and received in 64-byte
  // XXX - "chunkies" to prevent overrun errors on the serial line.
  // XXX - As a result, this buffer could be significantly reduced
  // XXX - in size. I haven't done this because the uniform 64-byte
  // XXX - chunk size also means we could get rid of count bytes
  // XXX - and it makes sense to change all that at the same time.
  // XXX - In addition to getting rid of the count field (the count
  // XXX - will always be 64), we can have just a single function
  // XXX - to collect or transmit 64 bytes and use it for all the
  // XXX - the six cases (reading and writing main, ALU, or WCS RAM).
  constexpr int POLL_BUF_SIZE = 259;
  constexpr int POLL_BUF_LAST = (POLL_BUF_SIZE - 1);
  constexpr int POLL_BUF_MAX_DATA = 255;
  constexpr byte GUARD_BYTE = 0xAA;
  constexpr byte MAX_CMD_SIZE = 8;

  typedef struct pollBuffer {
    int remaining;
    int next;
    bool inuse;
    byte cmd[MAX_CMD_SIZE];
    byte buf[POLL_BUF_SIZE];
  } PollBuffer;

  PollBuffer uniquePollBuffer;
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

  void internalSerialReset() {
    stProtoUnsync();
    pb->inuse = false;
    pb->remaining = 0;
    pb->next = 0;
    pb->buf[POLL_BUF_LAST] = GUARD_BYTE;
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
    inProgress = 0;
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
    consume(r, 1);
    sendAck(b);
    send(GetMCR());
    return state;
  }

  State stRunCost(RING* const r, byte b) {
    consume(r, 1);
    costRun();
    sendAck(b);
    return state;
  }

  State stStopCost(RING* const r, byte b) {
    consume(r, 1);
    costStop();
    sendAck(b);
    return state;
  }

  State stRun(RING* const r, byte b) {
    consume(r, 1);
    RunYARC();
    sendAck(b);
    return state;
  }

  State stStop(RING* const r, byte b) {
    consume(r, 1);
    StopYARC();
    sendAck(b);
    return state;
  }

  // This implementation is maximally decoupled - we just set
  // the clock control byte to a value, and the runtime task
  // eventually notices and does something about it. All values
  // are treated as meaningful.
  //
  // The spec says the MCR is returned. Unfortunately we have no
  // way to know when the command will take effect, so we return
  // the MCR from -before- we change any state. This might be
  // marginally useful.
  State stClockCtl(RING* const r, byte b) {
    byte cmd[2];
    copy(r, cmd, 2);
    consume(r, 2);
    byte result = GetMCR();
    SetClockControl(cmd[1]);
    sendAck(b);
    send(result);
    return state;
  }

  // Callback to log a bad debug command
  byte badDebugValue = 0;

  int badDebug(char* bp, int bmax) {
    int result = snprintf_P(bp, bmax, PSTR("serial: debug: bad command %d"), badDebugValue);
    if (result > bmax) result = bmax;
    return result;
  }

  // Handling for debug command 1. We call this function,
  // defined below, which needs a forward.
  State rdMemInProgress(void);

  // Debugging commands. These can be added without changing
  // the protocol definition.
  // cmd[0] == 1: stop the clock, take YARC out of run mode,
  //              dump the general registers r0..r3 to 0x7700,
  //              0x7702, 0x7704, 0x7706, dump the flags at 0x7708,
  //              and then return the 64 bytes at 0x7700.
  State stDebug(RING* const r, byte b) {
    allocPollBuffer();
    copy(r, pb->cmd, MAX_CMD_SIZE);
    consume(rcvBuf, MAX_CMD_SIZE);
    sendAck(b);

    if (pb->cmd[1] != 1) {
      // For now the only defined debug command is 1.
      // Unrecognized debug command. Don't report an error
      // which would end the session. Just send back nothing.
      badDebugValue = pb->cmd[1];
      logQueueCallback(badDebug);
      freePollBuffer();
      send(0);
      return state;
    }

    // Stop the clock. My ill-advised decision to decouple
    // clock processing from serial command processing means
    // I can't just SetClockControl(0) because the task that
    // interprets that won't run for some time. I have to
    // imperatively stop the clock and then tell the clock
    // control task to keep it stopped next time it runs.
    SetMCR(McrDisableFastclock(GetMCR()));
    SetClockControl(0);
    // Take the YARC out of run mode
    StopYARC();
    // Dump the registers at 0x7700 .. 0x7707
    ReadReg(0, 0x7700);
    ReadReg(1, 0x7702);
    ReadReg(2, 0x7704);
    ReadReg(3, 0x7706);
    // And the flags register
    unsigned short f = ReadFlags();
    WriteMem16(0x7708, &f, 1);
  
    // 0x770A .. 0x770F unassigned for now.
    // YARC tests update the memory 0x7710 .. 0x773F.

    // Send back the chunky at 0x7700. We allocated the
    // poll buffer and rdMemInProgress will eventually
    // free it.
    ReadMem16(0x7700, (unsigned short *)pb->buf, CHUNK_SIZE/2);
    pb->remaining = CHUNK_SIZE;
    pb->next = 0;
    inProgress = rdMemInProgress;
    send(pb->remaining);
    return rdMemInProgress();
  }

  // In-progress handler for transmitting buffered
  // messages from the poll buffer to the host. Transmit
  // as much of the poll buffer as possible. If finished,
  // free the buffer and clear the inProgress handler.
  State pollResponseInProgress() {
    while (canSend(1) && pb->remaining > 0) {
      send(pb->buf[pb->next]);
      pb->remaining--;
      pb->next++;
    }
    if (pb->remaining == 0) {
      freePollBuffer();
      inProgress = 0;              
    }
    return state;
  }

  // Respond to a poll request from the host.
  State stPoll(RING* const r, byte b) {    
    consume(r, 1);
    sendAck(b);
    if (logIsEmpty()) {
      // usual case
      send(0);
      return state;
    }

    allocPollBuffer();
    pb->remaining = logGetPending((char *)pb->buf, POLL_BUF_MAX_DATA);
    send(pb->remaining); // byte count follows ack back to host
    pb->next = 0;
    inProgress = pollResponseInProgress;
    return pollResponseInProgress();
  }

  // Process an inbound system call response from the host.
  State stResp(RING* const r, byte b) {
    return stBadCmd(r, b);
  }

  // GetVer command - when we can send, consume the command
  // byte and send the ack and version. Does not change state.
  State stGetVer(RING* const r, byte b) {
    consume(r, 1);
    sendAck(b);
    send(PROTOCOL_VERSION);
    return state;
  }

  // Sync command - just ack it and set the display register
  State stSync(RING* const r, byte b) {
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

  State stOneClk(RING* const r, byte b) {
    consume(r, 1);
    SingleClock();
    sendAck(b);
    send(GetBIR());
    return state;
  }

  State stGetBir(RING* const r, byte b) {
    consume(r, 1);
    sendAck(b);
    send(GetBIR());
    return state;
  }

  // Collect a buffer of words to write and then write them. Only one value
  // is allowed for the count, CHUNK_SIZE == 64 bytes.
  State wrMemInProgress() {
    while (canReceive(1) && pb->remaining > 0) {
      pb->buf[pb->next] = peek(rcvBuf);
      consume(rcvBuf, 1);
      pb->next++;
      pb->remaining--;
    }
    if (pb->remaining == 0) {
      WriteMem16(BtoS(pb->cmd[1], pb->cmd[2]), (unsigned short*) pb->buf, CHUNK_SIZE/2);
      freePollBuffer();
      inProgress = 0;              
    }
    return state;
  }

  // New write memory command. Write 64 bytes (exactly) at an even address.
  // cmd[0] is write mem, cmd[1] is address MSB, then address LSB and count.
  State stWrMem(RING* const r, byte b) {
    allocPollBuffer();
    copy(r, pb->cmd, 4);
    consume(rcvBuf, 4);
    if (pb->cmd[1] > 0x7F || pb->cmd[3] != CHUNK_SIZE) {
      freePollBuffer();
      return stBadCmd(r, b);
    }
    pb->cmd[2] &= ~(CHUNK_SIZE-1);
    pb->remaining = pb->cmd[3];
    pb->next = 0;
    inProgress = wrMemInProgress;
    sendAck(b);
    return wrMemInProgress();
  }
  
  // We have read a buffer of words from memory. Send them to the host.
  State rdMemInProgress() {
    while (canSend(1) && pb->remaining > 0) {
      send(pb->buf[pb->next]);
      pb->next++;
      pb->remaining--;
    }
    if (pb->remaining == 0) {
      freePollBuffer();
      inProgress = 0;              
    }
    return state;    
  }

  // New read memory command. Read up to 128 bytes at an even address.
  // cmd[0] is read mem, cmd[1] is address MSB, then address LSB and count.
  State stRdMem(RING* const r, byte b) {
    allocPollBuffer();
    copy(r, pb->cmd, 4);
    consume(rcvBuf, 4);
    if (pb->cmd[1] > 0x7F || pb->cmd[3] != CHUNK_SIZE) {
      freePollBuffer();
      return stBadCmd(r, b);
    }
    pb->cmd[2] &= ~(CHUNK_SIZE-1); // even address on 64-byte boundary
    ReadMem16(BtoS(pb->cmd[1], pb->cmd[2]), (unsigned short *)pb->buf, CHUNK_SIZE/2);

    pb->remaining = pb->cmd[3];
    pb->next = 0;
    inProgress = rdMemInProgress;
    sendAck(b);
    send(pb->remaining);
    return rdMemInProgress();
  }

  // Collect the bytes to write in the poll buffer to minimize the
  // number of calls to WriteSlice(), which is slow. WriteSlice()
  // panics if the write doesn't verify correctly.
  State writeSliceInProgress() {
    while (canReceive(1) && pb->remaining > 0) {
      pb->buf[pb->next] = peek(rcvBuf);
      consume(rcvBuf, 1);
      pb->next++;
      pb->remaining--;
    }
    if (pb->remaining == 0) {
      WriteSlice(pb->cmd[1], pb->cmd[2], pb->buf, pb->cmd[3], true);
      freePollBuffer();
      inProgress = 0;              
    }
    return state;
  }

  // Write and internally verify a slice of microcode RAM. The first
  // argument is the opcode to write. The second byte is the slice.
  // The third is the byte count.
  State stWrSlice(RING* const r, byte b) {
    allocPollBuffer();
    copy(r, pb->cmd, 4);
    consume(rcvBuf, 4);
    if (pb->cmd[1] < 0x80 || pb->cmd[2] > 0x03 || pb->cmd[3] > CHUNK_SIZE) {
      freePollBuffer();
      return stBadCmd(r, b);
    }
    pb->remaining = pb->cmd[3];
    pb->next = 0;
    inProgress = writeSliceInProgress;
    sendAck(b);
    return writeSliceInProgress();
  }

  // Send the bytes from the poll buffer
  State readSliceInProgress() {
    while (canSend(1) && pb->remaining > 0) {
      send(pb->buf[pb->next]);
      pb->next++;
      pb->remaining--;
    }
    if (pb->remaining == 0) {
      freePollBuffer();
      inProgress = 0;              
    }
    return state;
  }

  // Read a slice. Not used by the downloader because stWrSlice()
  // performs a write with verify.
  State stRdSlice(RING* const r, byte b) {
    allocPollBuffer();
    copy(r, pb->cmd, 4);
    consume(rcvBuf, 4);
    if (pb->cmd[1] < 0x80 || pb->cmd[2] > 0x03 || pb->cmd[3] > CHUNK_SIZE) {
      freePollBuffer();
      return stBadCmd(r, b);
    }
    ReadSlice(pb->cmd[1] | 0x80, pb->cmd[2], pb->buf, pb->cmd[3]);
    pb->remaining = pb->cmd[3];
    pb->next = 0;
    inProgress = readSliceInProgress;
    sendAck(b);
    send(pb->cmd[3]);
    return readSliceInProgress();
  }

  State writeAluInProgress() {
    while (canReceive(1) && pb->remaining > 0) {
      pb->buf[pb->next] = peek(rcvBuf);
      consume(rcvBuf, 1);
      pb->next++;
      pb->remaining--;
    }
    if (pb->remaining == 0) {
      // IMPORTANT: as of 5/19/2023, this calls the combined write/verify
      // function, and the downloader no longer needs to separately read
      // back each of the three RAMs. WriteCheckALU() panics on failure.
      WriteCheckALU(BtoS(pb->cmd[1], pb->cmd[2]), pb->buf, pb->cmd[3]);
      freePollBuffer();
      inProgress = 0;              
    }
    return state;
  }

  // Write the ALU RAMs. After the command byte, the bytes are
  // the write address high, write address low, and count. The
  // count "n" must be exactly 64 and the address must be aligned
  // on a 64-byte boundary so that a specific optimization can be
  // enabled in yarc_utils. 
  State stWrALU(RING* const r, byte b) {
    allocPollBuffer();
    copy(r, pb->cmd, 4);
    consume(rcvBuf, 4);
    unsigned int addr = BtoS(pb->cmd[1], pb->cmd[2]);
    unsigned int n = pb->cmd[3];
    if (addr > 0x1FFF || n != CHUNK_SIZE || (addr&0x3F) || addr + n > 0x2000) {
      freePollBuffer();
      return stBadCmd(r, b);
    } 
    pb->remaining = pb->cmd[3];
    pb->next = 0;
    inProgress = writeAluInProgress;
    sendAck(b);
    return writeAluInProgress();
  }

  State readAluInProgress() {
    while (canSend(1) && pb->remaining > 0) {
      send(pb->buf[pb->next]);
      pb->next++;
      pb->remaining--;
    }
    if (pb->remaining == 0) {
      freePollBuffer();
      inProgress = 0;              
    }
    return state;
  }

  // Read one of the three ALU RAMs. After the command byte,
  // the bytes are read address high, read address low, ALU
  // RAM ID in 0..2, and a count. The count "n" must be exactly
  // 64 and the address must be aligned on a 64-byte boundary so
  // that a specific optimization can be enabled in yarc_utils.
  State stRdALU(RING* const r, byte b) {
    allocPollBuffer();
    copy(r, pb->cmd, 5);
    consume(rcvBuf, 5);
    unsigned int addr = BtoS(pb->cmd[1], pb->cmd[2]);
    unsigned int ram = pb->cmd[3];
    unsigned int n = pb->cmd[4];
    if (addr > 0x1FFF || n != CHUNK_SIZE || (addr&0x3F) || addr + n > 0x2000 || ram > 2) {
      freePollBuffer();
      return stBadCmd(r, b);
    }
    ReadALU(addr, pb->buf, n, ram);
    pb->remaining = pb->cmd[4];
    pb->next = 0;
    inProgress = readAluInProgress;
    sendAck(b);
    send(pb->cmd[4]);
    return readAluInProgress();
  }

  // Set the K register to the four bytes that follow the command.
  // This command is used only for manual debugging; when the host
  // e.g. writes memory, code here takes care of setting K.
  State stSetK(RING* const r, byte b) {
    byte cmdBuf[4];
    copy(r, cmdBuf, 4);
    consume(r, 4);
    sendAck(b);
    WriteK(cmdBuf[0], cmdBuf[1], cmdBuf[2], cmdBuf[3]);
    return state;
  }
  
  // Set the MCR. This is a change in philosophy added December 2022.
  State stSetMCR(RING* const r, byte b) {
    consume(r, 1);
    byte mcr = peek(r);
    consume(r, 1);
    SetMCR(mcr);
    sendAck(b);
    return state;
  }

  typedef struct commandData {
    CommandHandler handler;     // handler function
    byte length;                // length of fixed part of command, 1 or more
  } CommandData;

  // Jump table for protocol command handlers. The table is stored in
  // PROGMEM (ROM) so requires special access, below.
  
  const PROGMEM CommandData handlers[] = {
    { stBadCmd,     1 },
    { stGetMcr,     1 },
    { stRunCost,    1 },
    { stStopCost,   1 },

    { stClockCtl,   2 },
    { stWrMem,      4 }, // cmd, addr hi, addr lo, count
    { stRdMem,      4 }, // cmd, addr hi, addr lo, count
    { stRun,        1 },

    { stStop,       1 },
    { stPoll,       1 },
    { stResp,       2 },
    { stDebug,      8 }, // cmd, 7 uncommitted

    { stUndef,      1 },
    { stUndef,      1 },
    { stGetVer,     1 },
    { stSync,       1 },
  
    { stSetAH,      2 },
    { stSetAL,      2 },
    { stSetDH,      2 },
    { stSetDL,      2 },

    { stOneClk,     1 },
    { stGetBir,     1 },
    { stWrSlice,    4 },
    { stRdSlice,    4 },

    { stUndef,      1 },
    { stUndef,      1 },
    { stUndef,      1 },
    { stSetK,       5 },
    
    { stSetMCR,     2 },
    { stWrALU,      4 },
    { stRdALU,      5 },
    { stBadCmd,     1 },
  };

  // The maximum fixed response currently specified by the protocol is 1
  // result byte in addition to the ack or nak. This result byte may be a
  // value or may be a byte count of variable bytes to follow. This is
  // checked by the top-level handler to ensure that called handler
  // subfunctions that transmit only the fixed response won't block
  // waiting for room in the transmit buffer. Functions that transmit
  // larger, variable-length responses return a count as the fixed result
  // and then must handle blocking while transmitting.
  constexpr byte MAX_FIXED_RESPONSE_BYTES = 2;

  // There is at least one command byte waiting to be processed in the
  // receive- side ring buffer at r. The command handler may or may not
  // consume the byte(s) on this call, but must always return the next
  // state.
  State process(RING *const r, byte b) {
    if (!isCommand(b)) {
      return stBadCmd(r, b);
    }

    CommandHandler handler;
    byte cmdLen = pgm_read_ptr_near(&handlers[b - STCMD_BASE].length);
    if (len(rcvBuf) < cmdLen || avail(xmtBuf) < MAX_FIXED_RESPONSE_BYTES) {
      // Come back later after more bytes arrive or go out. Checking this
      // here means individual handlers can assume their command is fully
      // available and there is space for the fixed part of the response.
      return state;
    }
    handler = pgm_read_ptr_near(&handlers[b - STCMD_BASE].handler);
    return (*handler)(r, b);
  }

  // The serial task. Called as often as possible (no delay).
  // Try to write everything in the write buffer. Then try to
  // read all the available bytes. If we have an in-progress
  // command, defer to it. Else finally, if the read ring
  // buffer is not empty, invoke process() to handle a new
  // command. Note that process() may do nothing, waiting
  // for more bytes to either come in or go out.
  
  int serialTask() {
    while (len(xmtBuf) > 0 && Serial.availableForWrite() != 0) {
      if (Serial.write(peek(xmtBuf)) != 1) {
        panic(PANIC_SERIAL_NUMBERED, 9);
      }
      consume(xmtBuf, 1);
    }

    while (!isFull(rcvBuf) && Serial.available()) {
      put(rcvBuf, Serial.read());
    }

    if (inProgress) {
      state = (*inProgress)();
      return 0;
    }
    
    if (len(rcvBuf) > 0) {
      byte b = peek(rcvBuf);
      if (state == STATE_READY) {
        state = process(rcvBuf, b);
      } else if (state == STATE_UNSYNC && b == STCMD_SYNC) {
        // By handling this case here, we make it unnecessary for
        // the individual command handlers to check the state.
        state = stSync(rcvBuf, b);
      } else {
        state = stBadCmd(rcvBuf, b);
      }
    }
    
    return 0;
  }
}

// Public interface

void serialShutdown() {
  SerialPrivate::stProtoUnsync();
}

void SerialReset() {
  SerialPrivate::internalSerialReset();
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
