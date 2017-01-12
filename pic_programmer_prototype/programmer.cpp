#include "programmer.h"

#include <Arduino.h>

// These timings are all given in micro seconds
#define PROGRAMMING_MODE_RESET_DELAY 20000           // Treset
#define PROGRAMMING_MODE_VPP_LEAD_TIME 10            // Tppdp
#define PROGRAMMING_MODE_INPUT_LINE_SET_TIME 1       // Tset0
#define PROGRAMMING_MODE_HOLD_ACTIVATION_TIME 10     // Thld0
#define PROGRAMMING_MODE_EXIT_DELAY_TIME 2           // Texit
#define PROGRAMMING_MODE_PROGRAMMING_TIME 10000      // Tprog
#define PROGRAMMING_MODE_PROGRAMMING_END_TIME 1000   // Tdis
#define PROGRAMMING_MODE_ERASE_TIME 20000            // Tera

#define PROGRAMMING_MODE_CLOCK_CYCLE_DELAY 1         // Tset1, Thld1
#define PROGRAMMING_MODE_COMMAND_DELAY 5             // max(Tdly1, Tdly2, Tdly3)


/* Wrapper to allow custom increases of the delay intervals for debugging purposes
void ___customDelay(int x) {
  x = 250;
  //if (x < 0 || x > 1000) {
  //  x = 1000;
  //}
  delay(x);
}
 */
//#define micDelay(x) ___customDelay(x)

#define micDelay(x) delayMicroseconds(x)


const int POWER_PIN = 40;
const int PROG_POWER_PIN = 41;
const int PROGRAMMING_CLOCK_PIN = 42;
const int PROGRAMMING_DATA_PIN = 43;

void PICProgrammer :: pushOutBits(int value, int count) {
  for (int i = 0; i < count; i++) {
    digitalWrite(PROGRAMMING_CLOCK_PIN, HIGH);
    micDelay(PROGRAMMING_MODE_CLOCK_CYCLE_DELAY);
    digitalWrite(PROGRAMMING_DATA_PIN, (value & 1) == 1 ? HIGH : LOW);
    micDelay(PROGRAMMING_MODE_CLOCK_CYCLE_DELAY);
    digitalWrite(PROGRAMMING_CLOCK_PIN, LOW);
    micDelay(PROGRAMMING_MODE_CLOCK_CYCLE_DELAY);
    value = (value >> 1) & 0x7FFF;
  }
}

void PICProgrammer :: writeCommand(unsigned char cmd) {
  pushOutBits(cmd, 6);
}

PICProgrammer :: PICProgrammer(int memory_length) {
  pinMode(PROGRAMMING_CLOCK_PIN, INPUT);
  pinMode(PROGRAMMING_DATA_PIN, INPUT);

  pinMode(POWER_PIN, OUTPUT);
  pinMode(PROG_POWER_PIN, OUTPUT);
  digitalWrite(POWER_PIN, LOW);
  digitalWrite(PROG_POWER_PIN, LOW);

  _hasPower = false;
  _inProgrammingMode = false;

  this->memory_length = memory_length;

  // Wait for a general reset
  delay(100);
}

int PICProgrammer :: getLocation() const {
  return location;
}

bool PICProgrammer :: hasPower() const {
  return _hasPower;
}

bool PICProgrammer :: inProgrammingMode() const {
  return _inProgrammingMode;
}

void PICProgrammer :: setPower(bool on) {
  if (_inProgrammingMode) {
    setProgrammingMode(false);
  }
  if (on != _hasPower) {
    digitalWrite(POWER_PIN, on ? HIGH : LOW);
    _hasPower = on;
    micDelay(PROGRAMMING_MODE_RESET_DELAY);
  }
}

void PICProgrammer :: setProgrammingMode(bool on) {
  if (on != _inProgrammingMode) {
    if (on) {
      setPower(false);

      pinMode(PROGRAMMING_CLOCK_PIN, OUTPUT);
      pinMode(PROGRAMMING_DATA_PIN, OUTPUT);

      digitalWrite(PROGRAMMING_CLOCK_PIN, LOW);
      digitalWrite(PROGRAMMING_DATA_PIN, LOW);
      micDelay(PROGRAMMING_MODE_INPUT_LINE_SET_TIME);

      digitalWrite(PROG_POWER_PIN, HIGH);
      micDelay(PROGRAMMING_MODE_VPP_LEAD_TIME);
      digitalWrite(POWER_PIN, HIGH);
      micDelay(PROGRAMMING_MODE_HOLD_ACTIVATION_TIME);

      location = -1;
      _inProgrammingMode = true;
      _hasPower = true;
    } else {
      pinMode(PROGRAMMING_DATA_PIN, OUTPUT);

      digitalWrite(PROGRAMMING_DATA_PIN, LOW);
      digitalWrite(PROGRAMMING_CLOCK_PIN, LOW);
      digitalWrite(POWER_PIN, LOW);
      micDelay(PROGRAMMING_MODE_EXIT_DELAY_TIME);
      
      digitalWrite(PROG_POWER_PIN, LOW);
      
      micDelay(PROGRAMMING_MODE_RESET_DELAY);
      
      pinMode(PROGRAMMING_CLOCK_PIN, INPUT);
      pinMode(PROGRAMMING_DATA_PIN, INPUT);
      
      _inProgrammingMode = false;
      _hasPower = false;
    }
  }
}

int PICProgrammer :: readWord(bool includeStartStopBits) {
  if (_inProgrammingMode) {
    writeCommand(0x04);

    pinMode(PROGRAMMING_DATA_PIN, INPUT);
    micDelay(PROGRAMMING_MODE_COMMAND_DELAY);

    int result = 0;
    for (int i = 0; i < 16; i++) {
      digitalWrite(PROGRAMMING_CLOCK_PIN, HIGH);
      micDelay(PROGRAMMING_MODE_COMMAND_DELAY);
      result = (result >> 1) & 0x7FFF | (((bool) digitalRead(PROGRAMMING_DATA_PIN)) ? 0x8000 : 0);
      digitalWrite(PROGRAMMING_CLOCK_PIN, LOW);
      micDelay(PROGRAMMING_MODE_COMMAND_DELAY);
    }

    pinMode(PROGRAMMING_DATA_PIN, OUTPUT);

    if (includeStartStopBits) {
      return result;
    } else {
      return (result >> 1) & 0x3FFF;
    }
  }
  return -1;
}

bool PICProgrammer :: writeWord(int w) {
  w = (w & 0x3FFF) << 1;
  
  if (_inProgrammingMode) {
    writeCommand(0x02);
    micDelay(PROGRAMMING_MODE_COMMAND_DELAY);

    pushOutBits(w, 16);
    
    digitalWrite(PROGRAMMING_DATA_PIN, LOW);
    return true;
  }
  return false;
}

bool PICProgrammer :: incAddress() {
  if (_inProgrammingMode) {
    writeCommand(0x06);
    micDelay(PROGRAMMING_MODE_COMMAND_DELAY);
    location = (location + 1) % memory_length;
    return true;
  }
  return false;
}

bool PICProgrammer :: burn() {
  if (_inProgrammingMode) {
    writeCommand(0x08);
    micDelay(PROGRAMMING_MODE_PROGRAMMING_TIME);
    writeCommand(0x0E);
    micDelay(PROGRAMMING_MODE_PROGRAMMING_END_TIME);
    return true;
  }
  return false;
}

bool PICProgrammer :: erase() {
  if (_inProgrammingMode) {
    writeCommand(0x09);
    micDelay(PROGRAMMING_MODE_ERASE_TIME);
    return true;
  }
  return false;
}

