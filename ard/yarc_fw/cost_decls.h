// Copyright (c) Jeff Berkowitz 2023. All rights reserved.
// Continuous Self Test (CoST) task for YARC.
// Symbol prefixes: co, CO, cost, etc.

void costRun();
void costStop();

namespace CostPrivate {
  void flagsInit(void);
  bool flagsTest(void);
}
