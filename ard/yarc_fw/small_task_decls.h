// Copyright (c) Jeff Berkowitz 2021. All rights reserved.
// Public functions for heartbeat.

// Increment the number of iterations of the task loop.
// Useful only to the task runner.
void hbIncIterationCount();

// Management task for Nano's on-board LED

void ledPlayStandardHeartbeat();
void ledPlaySos();

/*
 * Log users have to implement a callback function that places a
 * messages in the buffer "bp" passed as an argument, staying within
 * the "count" passed as an argument. Where possible, the string
 * should be stored in ROM (PROGMEM). The following sample shows
 * the usual way of doing this.

  unsigned long i, j, k; // for example
  set i, j, k to something;
  int n = snprintf_P(bp, count, PSTR("message: %ld %ld %ld"), i, j, k);

 * snprintf_P returns "...the number of characters that would have been written to s if there were enough space."
 * http://www.nongnu.org/avr-libc/user-manual/group__avr__stdio.html#ga53ff61856759709eeceae10aaa10a0a3, so ...
 *
 * return (n > bcount) ? bcount : n;
 */

typedef int (*logCallback)(char *bp, int count);

// Return true if there is nothing to send up to the host.
bool logIsEmpty(void);

// Queue a callback. There is a status return, but it's not
// very useful because there's not much the caller can do if
// the log queue fills up.
byte logQueueCallback(logCallback callback);

// Pull the next message from the queue. Normally called from
// the serial task when the host polls it for messages. The
// next queued callback is called from this function.
int logGetPending(char *next, int maxCount);

// Clock control API (consumed by runtime task)

void SetClockControl(byte b);