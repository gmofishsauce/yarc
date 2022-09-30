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
    {0,              displayTask   },
    {serialTaskInit, serialTaskBody},
  };

  const int N_TASKS = (sizeof(Tasks) / sizeof(TaskInfo));

  // Store the time of next run for each task in a parallel array
  // because the Tasks array is in ROM (PROGMEM).
  unsigned long nextRunMillis[N_TASKS];
}

// A few public utilities, in order to avoid further expanding
// the already large number of tabs in the IDE.

void panic(byte errorRegisterValue) {
  serialShutdown();
  freezeDisplay(errorRegisterValue);
  ledPlaySos();
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
