#include <stdlib.h>

#include <StringSplitter.h>

// These timings are all given in micro seconds
#define PROGRAMMING_MODE_RESET_DELAY 20000           // Treset
#define PROGRAMMING_MODE_VPP_LEAD_TIME 10            // Tppdp
#define PROGRAMMING_MODE_INPUT_LINE_SET_TIME 1       // Tset0
#define PROGRAMMING_MODE_HOLD_ACTIVATION_TIME 10     // Thld0
#define PROGRAMMING_MODE_EXIT_DELAY_TIME 2           // Texit
#define PROGRAMMING_MODE_PROGRAMMING_TIME 10000 * 10      // Tprog
#define PROGRAMMING_MODE_PROGRAMMING_END_TIME 1000 * 10   // Tdis
#define PROGRAMMING_MODE_ERASE_TIME 20000 * 10            // Tera

#define PROGRAMMING_MODE_CLOCK_CYCLE_DELAY 1         // Tset1, Thld1
#define PROGRAMMING_MODE_COMMAND_DELAY 5             // max(Tdly1, Tdly2, Tdly3)


void ___customDelay(int x) {
  x = 250;
  //if (x < 0 || x > 1000) {
  //  x = 1000;
  //}
  delay(x);
}

//#define micDelay(x) ___customDelay(x)
#define micDelay(x) delayMicroseconds(x)

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

    void pushOutBits(int value, int count) {
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

    void writeCommand(unsigned char cmd) {
      pushOutBits(cmd, 6);
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
        micDelay(PROGRAMMING_MODE_RESET_DELAY);
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
          micDelay(PROGRAMMING_MODE_INPUT_LINE_SET_TIME);
  
          digitalWrite(PROG_POWER_PIN, HIGH);
          micDelay(PROGRAMMING_MODE_VPP_LEAD_TIME);
          digitalWrite(POWER_PIN, HIGH);
          micDelay(PROGRAMMING_MODE_HOLD_ACTIVATION_TIME);

          location = -1;
          _inProgrammingMode = true;
          _hasPower = true;
        } else {
          digitalWrite(POWER_PIN, LOW);

          pinMode(PROGRAMMING_DATA_PIN, OUTPUT);
  
          digitalWrite(PROGRAMMING_CLOCK_PIN, LOW);
          digitalWrite(PROGRAMMING_DATA_PIN, LOW);
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

    int readWord(bool includeStartStopBits=false) {
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

    bool writeWord(int w) {
      w = (w & 0xFFF) << 1;
      
      if (_inProgrammingMode) {
        writeCommand(0x02);
        micDelay(PROGRAMMING_MODE_COMMAND_DELAY);

        pushOutBits(w, 16);
        
        digitalWrite(PROGRAMMING_DATA_PIN, LOW);
        return true;
      }
      return false;
    }

    bool incAddress() {
      if (_inProgrammingMode) {
        writeCommand(0x06);
        micDelay(PROGRAMMING_MODE_COMMAND_DELAY);
        location++;
        return true;
      }
      return false;
    }

    bool burnProgram() {
      if (_inProgrammingMode) {
        writeCommand(0x08);
        micDelay(PROGRAMMING_MODE_PROGRAMMING_TIME);
        writeCommand(0x0E);
        micDelay(PROGRAMMING_MODE_PROGRAMMING_END_TIME);
        return true;
      }
      return false;
    }
    
    bool erase() {
      if (_inProgrammingMode) {
        writeCommand(0x09);
        micDelay(PROGRAMMING_MODE_ERASE_TIME);
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
        Serial.println("Programming mode on");
      } else if (parameter == "off") {
        programmer->setProgrammingMode(false);
        Serial.println("Programming mode off");
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
      return name.startsWith("read");
    }
    
    virtual void execute(const String& commandString) {
      if (!programmer->inProgrammingMode()) {
        Serial.println("ERROR: Programmer not in programming mode");
      } else {
        StringSplitter splitter(commandString, ' ', 2);

        int count = 0;
        if (splitter.getItemCount() > 1) {
          String parameter = splitter.getItemAtIndex(1);
          parameter.toLowerCase();
          count = hexToInt(parameter);
        }

        do {
          int w = programmer->readWord(true);
          Serial.print("Word ");
          Serial.print(String(programmer->getLocation(), HEX));
          Serial.print(": ");
          for (int i = 0; i < 16; i++) {
            Serial.print(((w >> (15 - i)) & 1) == 1 ? "1 " : "0 ");
          }
          Serial.println();
          
          count--;
          if (count >= 0) {
            programmer->incAddress();
          }
        } while (count > 0);
      }
    }
    
    virtual void printHelpString() const {
      Serial.println("  read [count]          reads a word from the current memory location and displayes it as text");
    }
};

class WriteTextWordControl : public Command {
  public:
    virtual bool match(const String& name) const {
      return name.startsWith("write");
    }
    
    virtual void execute(const String& commandString) {
      if (!programmer->inProgrammingMode()) {
        Serial.println("ERROR: Programmer not in programming mode");
      } else {
        StringSplitter splitter(commandString, ' ', 2);
        if (splitter.getItemCount() < 2) {
          Serial.println("ERROR: Expected word to write as first argument");
        }
        String parameter = splitter.getItemAtIndex(1);
        parameter.toLowerCase();
        int data = hexToInt(parameter);

        if (data < 0 || data > 0xFFF) {
          Serial.println("ERROR: Word must be >= 0 and <= FFF");
        } else {
          programmer->writeWord(data);
        }
      }
    }
    
    virtual void printHelpString() const {
      Serial.println("  write hex_value       writes a word to the current memory location");
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
        StringSplitter splitter(commandString, ' ', 2);
        const bool numberGiven = splitter.getItemCount() > 1;
        
        int count = 1;
        if (numberGiven) {
          String parameter = splitter.getItemAtIndex(1);
          parameter.toLowerCase();
          count = hexToInt(parameter);
          if (count < 0) {
            Serial.println("ERROR: Invalid increment (must be positive!)");
          }
        }

        while (count > 0) {
          programmer->incAddress();
          count--;
        }

        if (numberGiven) {
          Serial.print("New address: ");
          Serial.println(String(programmer->getLocation(), HEX));
        }
      }
    }
    
    virtual void printHelpString() const {
      Serial.println("  inc [number]          increment write/read by number of addresses (default is 1)");
    }
};

class BurnProgramControl : public Command {
  public:
    virtual bool match(const String& name) const {
      return name.startsWith("burn");
    }
    
    virtual void execute(const String& commandString) {
      if (!programmer->inProgrammingMode()) {
        Serial.println("ERROR: Programmer not in programming mode");
      } else {
        programmer->burnProgram();
      }
    }
    
    virtual void printHelpString() const {
      Serial.println("  burn                  burn the program into program memory");
    }
};

class EraseControl : public Command {
  public:
    virtual bool match(const String& name) const {
      return name.startsWith("erase");
    }
    
    virtual void execute(const String& commandString) {
      if (!programmer->inProgrammingMode()) {
        Serial.println("ERROR: Programmer not in programming mode");
      } else {
        programmer->erase();
      }
    }
    
    virtual void printHelpString() const {
      Serial.println("  erase                 erases the program memory (mode depends on current PC location)");
    }
};



class RewriteChipCommand : public Command {
  public:
    virtual bool match(const String& name) const {
      return name.startsWith("rewrite");
    }
    
    virtual void execute(const String& commandString) {
      if (!programmer->inProgrammingMode()) {
        Serial.println("ERROR: Programmer not in programming mode");
      } else {
        StringSplitter splitter(commandString, ' ', 2);

        int count = 0;
        if (splitter.getItemCount() <= 1) {
          Serial.println("ERROR: Expected number of locations to rewrite");
        } else {
          String parameter = splitter.getItemAtIndex(1);
          parameter.toLowerCase();
          count = hexToInt(parameter);

          int oldProgress = 0;
          int newProgress = 0;
          int i = 0;
          while (i < count) {
            newProgress = 10 * i / (count - 1);
            if (newProgress != oldProgress) {
              oldProgress = newProgress;
              Serial.print(newProgress);
              Serial.print("0% ...");
            }
            
            int w = programmer->readWord();
            programmer->writeWord(w);
            programmer->burnProgram();
            
            programmer->incAddress();
            i++;
          }
          Serial.println("done");
        }
      }
    }
    
    virtual void printHelpString() const {
      Serial.println("  rewrite number        reads and rewrites the the given number of addresses");
    }
};


void setup() {
  memset(&commands, COMMANDS_SIZE * sizeof(commands[0]), 0);
  commands[0] = new HelpCommand();
  commands[1] = new PowerControl();
  commands[2] = new ProgrammingModeControl();
  commands[3] = new IncAddressControl();
  commands[4] = new ReadTextWordControl();
  commands[5] = new WriteTextWordControl();
  commands[6] = new BurnProgramControl();
  commands[7] = new EraseControl();
  commands[8] = new RewriteChipCommand();
  
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
