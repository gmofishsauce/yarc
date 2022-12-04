// Copyright (c) Jeff Berkowitz 2021. All rights reserved.
// This is the heartbeat task. Its produces a periodic log message.

namespace HeartbeatPrivate {
  
  #define HB_DELAY_MILLIS 7993
  
  #define HB_MS_PER_SEC 1000L
  #define HB_MS_PER_MIN (60L * HB_MS_PER_SEC)
  #define HB_MS_PER_HOUR (60L * HB_MS_PER_MIN)
  #define HB_MS_PER_DAY (24L * HB_MS_PER_HOUR)
  
  unsigned long hbLastHeartbeatMillis = 0;
  unsigned long hbTaskIterations = 0;
  
  // Queued for callback by the heartbeat task, called back from the
  // serial task next time the host requests a message.
  byte heartbeatMessageCallback(byte *bp, byte bmax) {
    int days = 0, hours = 0, minutes = 0, seconds = 0, ms = 0;
    
    unsigned long now = millis();
    unsigned long elapsed = now - hbLastHeartbeatMillis;
    hbLastHeartbeatMillis = now;
  
    ms = now % HB_MS_PER_SEC;
    now -= ms;
    if (now > 0) {
      long n = now % HB_MS_PER_MIN;
      now -= n;
      seconds = n / HB_MS_PER_SEC;
    }
    if (now > 0) {
      long n = now % HB_MS_PER_HOUR;
      now -= n;
      minutes = n / HB_MS_PER_MIN;
    }
    if (now > 0) {
      long n = now % HB_MS_PER_DAY;
      now -= n;
      hours = n / HB_MS_PER_HOUR;
    }
    if (now > 0) {
      long n = now % HB_MS_PER_DAY;
      days = n / HB_MS_PER_DAY;
    }
  
    // snprintf_P returns "...the number of characters that would have been written to s if there were enough space."
    // http://www.nongnu.org/avr-libc/user-manual/group__avr__stdio.html#ga53ff61856759709eeceae10aaa10a0a3
    int result = snprintf_P((char *)bp, bmax, PSTR("Running %02d:%02d:%02d:%02d.%03d, about %ld task loops/ms"),
               days, hours, minutes, seconds, ms, hbTaskIterations / elapsed);
    hbTaskIterations = 0;
    if (result > bmax) result = bmax;
    return result;
  }
}

// Public interface to heartbeat task

// The HB task tracks iterations so it can log the number of task
// executions in the recent past.
void hbIncIterationCount() {
  HeartbeatPrivate::hbTaskIterations++;
}

int heartbeatTask() {
  logQueueCallback(HeartbeatPrivate::heartbeatMessageCallback);
  return HB_DELAY_MILLIS;  
}

// Management task for Nano's on-board LED
// Symbol prefixes: led, LED_

// This task plays one of a few prespecified on-off patterns on the
// onboard LED. The current pattern is always played to completion
// before starting the next. The next pattern played will be the same
// as the current pattern unless a call to set a different pattern is
// made by some other part of the code. The queue of pending patterns
// is one-deep; if multiple calls to set the next pattern occur, the
// last call wins.

namespace LedPrivate {
  
  const byte *ledCurrentPattern;
  const byte *ledNextPattern;
  int ledPatternIndex;
  
  #define LED_PIN 13 // all Arduinos
  
  // These macros need to be used with care. The duration is encoded in
  // the high-order 7 bits of the byte and the state of the LED in the
  // LSB. So the maximum duration is (0xFF >> 1) == 127 ticks or 1270ms
  // so long as the tick frequency is 100Hz (period 10ms).
  
  #define LED_TICK_INTERVAL_MILLIS 10
  #define LED_MILLIS_TO_TICKS(n) ((n) / LED_TICK_INTERVAL_MILLIS)
  #define LED_TICKS_TO_MILLIS(n) ((n) * LED_TICK_INTERVAL_MILLIS)
  #define LED_ON(ms) ((LED_MILLIS_TO_TICKS(ms) << 1) | 1)
  #define LED_OFF(ms) ((LED_MILLIS_TO_TICKS(ms) << 1) & 0xFE)
  #define LED_END_PATTERN 0
  
  const PROGMEM byte ledStandardHeartbeat[] = {LED_ON(700), LED_OFF(700), LED_END_PATTERN};
  
  #define LED_DIT LED_OFF(150),LED_ON(150)
  #define LED_DAH LED_OFF(250),LED_ON(500)
  #define LED_SPACE LED_OFF(250)
  #define LED_PAUSE LED_OFF(750)
  
  const PROGMEM byte ledSos[] = { LED_DIT, LED_DIT, LED_DIT, LED_SPACE, LED_DAH, LED_DAH, LED_DAH,
                          LED_SPACE, LED_DIT, LED_DIT, LED_DIT, LED_PAUSE, LED_END_PATTERN };
}

// public funcions for LED

void ledPlayStandardHeartbeat() {
  LedPrivate::ledNextPattern = LedPrivate::ledStandardHeartbeat;
}

void ledPlaySos() {
  LedPrivate::ledNextPattern = LedPrivate::ledSos;
}

int ledTask() {
  byte todo = pgm_read_byte_near(&LedPrivate::ledCurrentPattern[LedPrivate::ledPatternIndex]);
  if (todo == LED_END_PATTERN) {
    LedPrivate::ledPatternIndex = 0;
    LedPrivate::ledCurrentPattern = LedPrivate::ledNextPattern;
    todo = pgm_read_byte_near(&LedPrivate::ledCurrentPattern[LedPrivate::ledPatternIndex]);
  }
  LedPrivate::ledPatternIndex++;
  digitalWrite(LED_PIN, todo & 1);
  return LED_TICKS_TO_MILLIS(todo >> 1);
}

void ledInit() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  LedPrivate::ledCurrentPattern = LedPrivate::ledStandardHeartbeat;
  LedPrivate::ledNextPattern = LedPrivate::ledStandardHeartbeat;
  LedPrivate::ledPatternIndex = 0;
}

// This is the logging task.
// 
// The logger should not be used for chit-chat.
//
// Lack of foresight: this is the infrastructure for making general
// requests to the Mac. The default request is to log a message; this
// is indicated because the request starts with a letter or number,
// specifically any character except these: '!', '#', or '$'.
//
// When the first byte of the messages is one of the characters in
// that set, it indicates some non-logging request to the Mac. Currently
// only '$' is used for requests and '!' for responses.
//
// The prefix on this identifiers in this file will eventually be
// generalized from "log" to "req" for request...lack of foresight.

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
