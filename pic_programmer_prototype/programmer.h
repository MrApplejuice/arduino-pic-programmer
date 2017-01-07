class PICProgrammer {
  private:
    int location;
  
    bool _hasPower, _inProgrammingMode;

    void pushOutBits(int value, int count);

    void writeCommand(unsigned char cmd);
  public:
    PICProgrammer();

    /**
     * Checks if the PIC is powered up (programming mode or otherwise)
     */
    bool hasPower() const;
    
    /**
     * Checks if the PIC is in programming mode.
     */
    bool inProgrammingMode() const;
    
    /**
     * If the PIC is in programming mode, this stores the current address
     * the PIC is at. Invalid if not in programming mode. Returns -1 if
     * the PIC is not in programming mode OR if the PIC is on the 
     * configuration word.
     */
    int getLocation() const;

    /**
     * Enables or disables power to the PIC. If the PIC is in programming
     * mode, this resets it and puts it into "normal" powered-on mode.
     */
    void setPower(bool on);

    /**
     * Puts the PIC in programming mode, or exits from programming mode. If
     * the PIC was powered on before, this will reset the power-on state and
     */
    void setProgrammingMode(bool on);

    /**
     * Reads a word from the PIC and returns it. If the function fails (not
     * in programming mode) it returns -1.
     * 
     * @param includeStartStopBits:
     *     Set to true to include the stop bits in the result (useful for debugging).
     *     False by default.
     */
    int readWord(bool includeStartStopBits=false);

    /**
     * Writes a word to programming memory but does yet write it into
     * flash memory. To commit the written word to flash memory use
     * the burn() function.
     * 
     * @param w:
     *   The word to write only the lower 14 bits are used. Stop
     *   bits are added as-needed.
     */
    bool writeWord(int w);

    /**
     * Increments the position of the program counter by one. Returns
     * true if successful.
     */
    bool incAddress();

    /**
     * Issues the programming command pair BEGIN_PROGRAMMING and END_PROGRAMMING
     * to "burn" the programming memory to flash memory.
     * 
     * Returns true if successfully executed.
     */
    bool burn();

    /**
     * Issues the erase command at the current location of the flash memory. Returns
     * true if the command executed successfully.
     */
    bool erase();
};

