#include <stdlib.h>

#include <StringSplitter.h>

// These timings are all given in micro seconds
#define PROGRAMMING_MODE_RESET_DELAY 20000           // Treset
#define PROGRAMMING_MODE_VPP_LEAD_TIME 10            // Tppdp
#define PROGRAMMING_MODE_INPUT_LINE_SET_TIME 1       // Tset0
#define PROGRAMMING_MODE_HOLD_ACTIVATION_TIME 10     // Thld0
#define PROGRAMMING_MODE_EXIT_DELAY_TIME 2           // Texit

const int POWER_PIN = 40;
const int PROG_POWER_PIN = 41;
const int PROGRAMMING_CLOCK_PIN = 42;
const int PROGRAMMING_DATA_PIN = 43;

class PICProgrammer {
  private:
    bool hasPower, inProgrammingMode;
  public:
    PICProgrammer() {
      pinMode(POWER_PIN, OUTPUT);
      pinMode(PROG_POWER_PIN, OUTPUT);
      pinMode(PROGRAMMING_CLOCK_PIN, INPUT);
      pinMode(PROGRAMMING_DATA_PIN, INPUT);
    
      digitalWrite(POWER_PIN, LOW);
      digitalWrite(PROG_POWER_PIN, LOW);

      hasPower = false;
      inProgrammingMode = false;

      // Wait for a general reset
      delay(100);
    }

    void setPower(bool on) {
      if (inProgrammingMode) {
        setProgrammingMode(false);
      }
      if (on != hasPower) {
        digitalWrite(POWER_PIN, on ? HIGH : LOW);
        hasPower = on;
        delayMicroseconds(PROGRAMMING_MODE_RESET_DELAY);
      }
    }

    void setProgrammingMode(bool on) {
      if (on != inProgrammingMode) {
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

          inProgrammingMode = true;
          hasPower = true;
        } else {
          digitalWrite(POWER_PIN, LOW);

          pinMode(PROGRAMMING_CLOCK_PIN, OUTPUT);
          pinMode(PROGRAMMING_DATA_PIN, OUTPUT);
  
          digitalWrite(PROGRAMMING_CLOCK_PIN, LOW);
          digitalWrite(PROGRAMMING_DATA_PIN, LOW);
          delayMicroseconds(PROGRAMMING_MODE_EXIT_DELAY_TIME);
          
          digitalWrite(PROG_POWER_PIN, LOW);
          
          delayMicroseconds(PROGRAMMING_MODE_RESET_DELAY);
          
          inProgrammingMode = false;
          hasPower = false;
        }
      }
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

void setup() {
  memset(&commands, COMMANDS_SIZE * sizeof(commands[0]), 0);
  commands[0] = new HelpCommand();
  commands[1] = new PowerControl();
  commands[2] = new ProgrammingModeControl();
  
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
