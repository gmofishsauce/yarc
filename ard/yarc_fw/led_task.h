// Copyright (c) Jeff Berkowitz 2021. All rights reserved.
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
