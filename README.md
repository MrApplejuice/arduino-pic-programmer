# Arduino PIC Programmer

My take on using an Arduino for programming a PIC microcontroller.

Currently, the code is specific to the PIC10F206 which is my guinea pig for
testing the programmer implementation.

## Repository overview

- `pic_programmer_prototype` contains the code for the Arduino programmer
    - `pic_programmer_prototype/pic_programmer_prototype.ino`
       
        Implements a serial protocol to send programming commands to the PIC.
       
    - `pic_programmer_prototype/programmer.*`
    
        Implements the communication protocol for the PIC