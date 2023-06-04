// Copyright (c) Jeff Berkowitz 2021. All rights reserved.
//
// This is the tasks implementation. It should be included
// after all the tasks themselves. The definitions required
// to create a task are in task_decls.h.

// TODO millisecond overflow after a couple of months.

namespace TaskPrivate {

  typedef struct ti {
    TaskInit initialize;
    TaskBody execute;
  } TaskInfo;
  
  // Although there are (theoretically) no ordering dependencies
  // for the include files in the main yarc_fw.ino, InitTasks()
  // does run the xxxInit() functions in order from first to last.
  // So  initialization imposes an ordering dependencies on this
  // array; ports must be initialied before use, etc. Also, init
  // tasks must be short, so that postInit() (below) can run
  // before the YARC comes out of its power-on reset state.

  // Note: PROGMEM - requires pgm_ functions to read.
  
  const PROGMEM TaskInfo Tasks[] = {
    {portInit,       portTask      },
    {ledInit,        ledTask       },
    {0,              heartbeatTask },
    {logInit,        0             },
    {serialTaskInit, serialTaskBody},
    {costTaskInit,   costTask      },
    {runtimeInit,    runtimeTask   },
  };

  const int N_TASKS = (sizeof(Tasks) / sizeof(TaskInfo));

  // Store the time of next run for each task in a parallel array
  // because the Tasks array is in ROM (PROGMEM).
  unsigned long nextRunMillis[N_TASKS];
}

// A few public utilities, in order to avoid further expanding
// the already large number of tabs in the IDE.

// Panic - don't rely on our own code except for writing to the display register.
// Change the display register between the panic code and an arbitrary subcode
// about every 5 seconds. Turn on the amber LED when the panic code is displayed,
// and blink it quickly while the subcode is displayed. And turn on the LED first
// of all, so in the very worst case,it stays on solid.

void panic(byte panicCode, byte subcode) {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  
  serialShutdown();
  SetDisplay(panicCode);
  int whichDisplay = 0;
  
  for (int n = 1; ; ++n) {
    if (whichDisplay == 0) {
      SetDisplay(panicCode);
      digitalWrite(LED_PIN, HIGH);
      delay(200);
    } else {
      SetDisplay(subcode);
      digitalWrite(LED_PIN, LOW);
      delay(100);
      digitalWrite(LED_PIN, HIGH);
      delay(100);
    }
    // 200mS per iteration, so 5 per second; 25 is five seconds.
    if (n % 25 == 0) {
      whichDisplay = 1 - whichDisplay; // old FORTRAN trick
    }
  }
}

// Task module public interface

void InitTasks() {
  // Ideally, would snapshot the time here, and log a message if
  // the time from here to postInit() is more than 0.1s or so.
  
  for (int i = 0; i < TaskPrivate::N_TASKS; ++i) {
    TaskPrivate::nextRunMillis[i] = 0;
    
    const TaskInit init = pgm_read_ptr_near(&TaskPrivate::Tasks[i].initialize);
    if (init != 0) {
      init();
    }
  }
  
  if (!postInit()) { // power on self test and initialization
    for(;;) {
      ; // POST failed - stuck until reset
    }
  }
}

void RunTasks() {
  unsigned long now = millis();
  hbIncIterationCount();
  for (int i = 0; i < TaskPrivate::N_TASKS; ++i) {

    const TaskBody body = pgm_read_ptr_near(&TaskPrivate::Tasks[i].execute);
    if (body != 0 && now >= TaskPrivate::nextRunMillis[i]) {
      TaskPrivate::nextRunMillis[i] = now + body();
    }
  }
}
