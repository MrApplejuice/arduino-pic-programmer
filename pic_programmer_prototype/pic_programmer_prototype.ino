#include <stdlib.h>

#include <StringSplitter.h>

const int POWER_PIN = 40;
const int PROG_POWER_PIN = 41;

class Command {
  public:
    typedef Command* Ptr;
  
    virtual bool match(const String& name) const;
    virtual void execute(const String& commandString);
    virtual void printHelpString() const;
};

const size_t COMMAND_COUNT = 1;
Command::Ptr commands[1] = {nullptr};

class HelpCommand : public Command {
  public:
    virtual bool match(const String& name) const {
      return name == "help";
    }
    
    virtual void execute(const String& commandString) {
      Serial.println("Available commands:");
      for (size_t i = 0; i < COMMAND_COUNT; i++) {
        commands[i]->printHelpString();
      }
    }
    
    virtual void printHelpString() const {
      Serial.println("  help             prints this help screen");
    }
};

void setup() {
  commands[0] = new HelpCommand();
  
  Serial.begin(57600);
}

void loop() {
  String cmd = Serial.readString();
  cmd.trim();
  if (cmd.length() > 0) {
    bool cmatched = false;
    for (int i = 0; i < COMMAND_COUNT; i++) {
      if (commands[i]->match(cmd)) {
        commands[i]->execute(cmd);
        cmatched = true;
        break;
      }
    }
    if (!cmatched) {
      Serial.println("invalid command, use 'help' to show all available commands");
    }
  }
}
