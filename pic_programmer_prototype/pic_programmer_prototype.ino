#include <stdlib.h>

#include <StringSplitter.h>

#include "programmer.h"

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

        if (data < 0 || data > 0x3FFF) {
          Serial.println("ERROR: Word must be >= 0 and <= 3FFF");
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
        programmer->burn();
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
            programmer->burn();
            
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
