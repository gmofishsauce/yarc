// Copyright (c) Jeff Berkowitz 2021. All rights reserved.
// Symbol prefix: port, PORT

// These are the declarations for port access by the rest
// of the firmware. The impl is in port_task.h.

#pragma once

// Public interface to the write-only 8-bit Display Register (DR)

void SetDisplay(byte b);

// Public functions exposed by the port task

void SetAH(byte b);
void SetAL(byte b);
void SetDH(byte b);
void SetDL(byte b);
void SetADHL(byte ah, byte al, byte dh, byte dl);
void SetMCR(byte b);
byte GetMCR(void);
byte GetBIR(void);
void MakeSafe(void);
void SingleClock(void);


