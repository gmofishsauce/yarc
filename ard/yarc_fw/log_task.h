// Copyright (c) Jeff Berkowitz 2021. All rights reserved.

// 
// The logger should not be used for chit-chat.

namespace LogPrivate {

  // Typical circular queue - since head == tail means "empty", we can't
  // use the last entry and leave head == tail when the queue is full. So
  // the queue actually holds LOG_QUEUE_SIZE - 1 elements.
  #define LOG_QUEUE_SIZE 8
  logCallback logCallbacks[LOG_QUEUE_SIZE];
  byte logHeadIndex = 0;      // Insertion point
  byte logTailIndex = 0;      // Consumption point
  byte messagesWereLost = 0;  // Queue overrun occurred

  // Return is a boolean value that is nonzero if the callback queue
  // was full (i.e. the callback was not queued).
  byte internalLogQueueCallback(logCallback callback) {
    byte n = (logHeadIndex + 1) % LOG_QUEUE_SIZE;
    if (n == logTailIndex) {
      messagesWereLost = 1;
      return 0;
    }
  
    logCallbacks[logHeadIndex] = callback;
    logHeadIndex = n;
    return 1;
  }
  
  byte internalLogGetPending(byte *next, byte maxCount) {
    if (logHeadIndex == logTailIndex) return 0;
    logCallback callback = logCallbacks[logTailIndex];
    logTailIndex = (logTailIndex + 1) % LOG_QUEUE_SIZE;
    if (messagesWereLost) {
      *next++ = '*'; maxCount--;
      *next++ = ' '; maxCount--;
      messagesWereLost = 0;
    }
    return callback(next, maxCount);
  }
}

// public interface

byte logQueueCallback(logCallback callback) {
  return LogPrivate::internalLogQueueCallback(callback);
}

byte logGetPending(byte *next, byte maxCount) {
  return LogPrivate::internalLogGetPending(next, maxCount);
}
