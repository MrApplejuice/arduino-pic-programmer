#include <stdlib.h>

#include <StringSplitter.h>

// These timings are all given in micro seconds
#define PROGRAMMING_MODE_RESET_DELAY 20000           // Treset
#define PROGRAMMING_MODE_VPP_LEAD_TIME 10            // Tppdp
#define PROGRAMMING_MODE_INPUT_LINE_SET_TIME 1       // Tset0
#define PROGRAMMING_MODE_HOLD_ACTIVATION_TIME 10     // Thld0
#define PROGRAMMING_MODE_EXIT_DELAY_TIME 2           // Texit

#define PROGRAMMING_MODE_CLOCK_CYCLE_DELAY 1         // Tset1, Thld1
#define PROGRAMMING_MODE_COMMAND_DELAY 2             // max(Tdly1, Tdly2, Tdly3)

const int POWER_PIN = 40;
const int PROG_POWER_PIN = 41;
const int PROGRAMMING_CLOCK_PIN = 42;
const int PROGRAMMING_DATA_PIN = 43;

int hexToInt(const String& s) {
  const static char HEX_CHARS[17] = "0123456789abcdef";
  int result = 0;

  for (int i = 0; i < s.length(); i++) {
    char c = s[i];
    for (int j = 0; j <= 16; j++) {
      if (j == 16) {
        return -1;
      }
      if (c == HEX_CHARS[j]) {
        result = (result << 4) | j;
        break;
      }
    }
  }

  return result;
}

class PICProgrammer {
  private:
    int location;
  
    bool _hasPower, _inProgrammingMode;

    void writeCommand(unsigned char cmd) {
      for (int i = 0; i < 6; i++) {
        digitalWrite(PROGRAMMING_CLOCK_PIN, HIGH);
        digitalWrite(PROGRAMMING_DATA_PIN, (cmd & 1) == 1 ? HIGH : LOW);
        
        delayMicroseconds(PROGRAMMING_MODE_CLOCK_CYCLE_DELAY);
        digitalWrite(PROGRAMMING_CLOCK_PIN, LOW);
        delayMicroseconds(PROGRAMMING_MODE_CLOCK_CYCLE_DELAY);

        cmd >>= 1;
      }
    }
  public:
    PICProgrammer() {
      pinMode(PROGRAMMING_CLOCK_PIN, INPUT);
      pinMode(PROGRAMMING_DATA_PIN, INPUT);
    
      pinMode(POWER_PIN, OUTPUT);
      pinMode(PROG_POWER_PIN, OUTPUT);
      digitalWrite(POWER_PIN, LOW);
      digitalWrite(PROG_POWER_PIN, LOW);

      _hasPower = false;
      _inProgrammingMode = false;

      // Wait for a general reset
      delay(100);
    }

    int getLocation() {
      return location;
    }

    bool hasPower() const {
      return _hasPower;
    }

    bool inProgrammingMode() {
      return _inProgrammingMode;
    }

    void setPower(bool on) {
      if (_inProgrammingMode) {
        setProgrammingMode(false);
      }
      if (on != _hasPower) {
        digitalWrite(POWER_PIN, on ? HIGH : LOW);
        _hasPower = on;
        delayMicroseconds(PROGRAMMING_MODE_RESET_DELAY);
      }
    }

    void setProgrammingMode(bool on) {
      if (on != _inProgrammingMode) {
        if (on) {
          setPower(false);
  
          pinMode(PROGRAMMING_CLOCK_PIN, OUTPUT);
          pinMode(PROGRAMMING_DATA_PIN, OUTPUT);
  
          digitalWrite(PROGRAMMING_CLOCK_PIN, LOW);
          digitalWrite(PROGRAMMING_DATA_PIN, LOW);
          delayMicroseconds(PROGRAMMING_MODE_INPUT_LINE_SET_TIME);
  
          digitalWrite(PROG_POWER_PIN, HIGH);
          delayMicroseconds(PROGRAMMING_MODE_VPP_LEAD_TIME);
          digitalWrite(POWER_PIN, HIGH);
          delayMicroseconds(PROGRAMMING_MODE_HOLD_ACTIVATION_TIME);

          location = 0;
          _inProgrammingMode = true;
          _hasPower = true;
        } else {
          digitalWrite(POWER_PIN, LOW);

          pinMode(PROGRAMMING_DATA_PIN, OUTPUT);
  
          digitalWrite(PROGRAMMING_CLOCK_PIN, LOW);
          digitalWrite(PROGRAMMING_DATA_PIN, LOW);
          delayMicroseconds(PROGRAMMING_MODE_EXIT_DELAY_TIME);
          
          digitalWrite(PROG_POWER_PIN, LOW);
          
          delayMicroseconds(PROGRAMMING_MODE_RESET_DELAY);
          
          pinMode(PROGRAMMING_CLOCK_PIN, INPUT);
          pinMode(PROGRAMMING_DATA_PIN, INPUT);
          
          _inProgrammingMode = false;
          _hasPower = false;
        }
      }
    }

    int readWord() {
      if (_inProgrammingMode) {
        writeCommand(0x04);

        pinMode(PROGRAMMING_DATA_PIN, INPUT);
        delayMicroseconds(PROGRAMMING_MODE_COMMAND_DELAY);

        int result = 0;
        for (int i = 0; i < 16; i++) {
          digitalWrite(PROGRAMMING_CLOCK_PIN, HIGH);
          delayMicroseconds(PROGRAMMING_MODE_COMMAND_DELAY);
          result = (result << 1) | ((bool) digitalRead(PROGRAMMING_DATA_PIN)) ? 1 : 0;
          digitalWrite(PROGRAMMING_CLOCK_PIN, LOW);
          delayMicroseconds(PROGRAMMING_MODE_COMMAND_DELAY);
        }

        pinMode(PROGRAMMING_DATA_PIN, OUTPUT);
        
        return result;
      }
      return -1;
    }

    bool incAddress() {
      if (_inProgrammingMode) {
        writeCommand(0x06);
        delayMicroseconds(PROGRAMMING_MODE_COMMAND_DELAY);
        location++;
        return true;
      }
      return false;
    }
};

PICProgrammer* programmer;


class Command {
  public:
    typedef Command* Ptr;
  
    virtual bool match(const String& name) const;
    virtual void execute(const String& commandString);
    virtual void printHelpString() const;
};

const size_t COMMANDS_SIZE = 10;
Command::Ptr commands[COMMANDS_SIZE];

class HelpCommand : public Command {
  public:
    virtual bool match(const String& cmd) const {
      return cmd == "help";
    }
    
    virtual void execute(const String& commandString) {
      Serial.println("Available commands:");
      for (size_t i = 0; i < COMMANDS_SIZE; i++) {
        if (commands[i] == nullptr) {
          break;
        }
        commands[i]->printHelpString();
      }
    }
    
    virtual void printHelpString() const {
      Serial.println("  help                  prints this help screen");
    }
};

class PowerControl : public Command {
  public:
    virtual bool match(const String& name) const {
      return name.startsWith("power");
    }
    
    virtual void execute(const String& commandString) {
      StringSplitter splitter(commandString, ' ', 2);
      String parameter = splitter.getItemAtIndex(1);
      parameter.toLowerCase();

      if (parameter == "on") {
        programmer->setPower(true);
      } else if (parameter == "off") {
        programmer->setPower(false);
      } else {
        Serial.println("ERROR: on/off required");
      }
    }
    
    virtual void printHelpString() const {
      Serial.println("  power on/off          turns power to the PIC on or off");
    }
};

class ProgrammingModeControl : public Command {
  public:
    virtual bool match(const String& name) const {
      return name.startsWith("prog_mode");
    }
    
    virtual void execute(const String& commandString) {
      StringSplitter splitter(commandString, ' ', 2);
      String parameter = splitter.getItemAtIndex(1);
      parameter.toLowerCase();

      if (parameter == "on") {
        programmer->setProgrammingMode(true);
      } else if (parameter == "off") {
        programmer->setProgrammingMode(false);
      } else {
        Serial.println("ERROR: on/off required");
      }
    }
    
    virtual void printHelpString() const {
      Serial.println("  prog_mode on/off      turns programming mode of the PIC on or off");
    }
};

class ReadTextWordControl : public Command {
  public:
    virtual bool match(const String& name) const {
      return name.startsWith("text_read");
    }
    
    virtual void execute(const String& commandString) {
      if (!programmer->inProgrammingMode()) {
        Serial.println("ERROR: Programmer not in programming mode");
      } else {
        StringSplitter splitter(commandString, ' ', 2);

        int count = 1;
        if (splitter.getItemCount() > 1) {
          String parameter = splitter.getItemAtIndex(1);
          parameter.toLowerCase();
          count = hexToInt(parameter);
        }

        while (count > 0) {
          int w = programmer->readWord();
          Serial.print("Word ");
          Serial.print(String(programmer->getLocation(), HEX));
          Serial.print(": ");
          for (int i = 0; i < 16; i++) {
            Serial.print((w & 1) == 1 ? "1 " : "0 ");
            w >>= 1;
          }
          Serial.println();
          
          count--;
          if (count > 0) {
            programmer->incAddress();
          }
        }
      }
    }
    
    virtual void printHelpString() const {
      Serial.println("  text_read [count]     reads a word from the current memory location and displayes it as text");
    }
};

class IncAddressControl : public Command {
  public:
    virtual bool match(const String& name) const {
      return name.startsWith("inc");
    }
    
    virtual void execute(const String& commandString) {
      if (!programmer->inProgrammingMode()) {
        Serial.println("ERROR: Programmer not in programming mode");
      } else {
        programmer->incAddress();
      }
    }
    
    virtual void printHelpString() const {
      Serial.println("  inc                   increment write/read address");
    }
};

void setup() {
  memset(&commands, COMMANDS_SIZE * sizeof(commands[0]), 0);
  commands[0] = new HelpCommand();
  commands[1] = new PowerControl();
  commands[2] = new ProgrammingModeControl();
  commands[3] = new IncAddressControl();
  commands[4] = new ReadTextWordControl();
  
  Serial.begin(57600);
  Serial.setTimeout(10);

  programmer = new PICProgrammer();
}

void loop() {
  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  if (cmd.length() > 0) {
    bool cmatched = false;
    for (int i = 0; i < COMMANDS_SIZE; i++) {
      if (commands[i] == nullptr) {
        break;
      }
      if (commands[i]->match(cmd)) {
        commands[i]->execute(cmd);
        cmatched = true;
        break;
      }
    }
    if (!cmatched) {
      Serial.println("ERROR: invalid command, use 'help' to show all available commands");
    }
  }
}
