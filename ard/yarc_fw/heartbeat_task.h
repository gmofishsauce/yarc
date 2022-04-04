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
