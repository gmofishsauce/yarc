// Copyright (c) Jeff Berkowitz 2021. All rights reserved.


namespace DisplayPrivate { 
  // byte displayValue = 0;  See comment below.
}

int displayTask() {
  // The commented code no longer compiles
  // because setDisplay() now takes an enum value. Still,
  // they may be useful for debugging.
  // setDisplay(DisplayPrivate::displayValue);
  // DisplayPrivate::displayValue++;
  
  // setCCRH(DisplayPrivate::displayValue);
  // setCCRL(DisplayPrivate::displayValue);
  // setOCRH(DisplayPrivate::displayValue);
  // setOCRL(DisplayPrivate::displayValue);
  // setMCR(DisplayPrivate::displayValue);

  // setDisplay(getBIR());
  // setDisplay(getMCR());
  return 171;
}
