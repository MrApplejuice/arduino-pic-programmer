#!/usr/bin/python3

import time
import sys
import argparse
import serial

class PICModelCollector(type):
    MODELS = {}

    @classmethod
    def validate_pic_model(cls, class_to_check):
        def raise_if_not_type(name, _type):
            if not isinstance(getattr(class_to_check, name), _type):
                raise ValueError("PICModel.{} must be of type {}".format(name, _type))
        def raise_if_None(name):
            if getattr(class_to_check, name) is None:
                raise ValueError("PICModel.{} must not be None".format(name))
        def raise_if_not_in_set(name, _set):
            if getattr(class_to_check, name) not in _set:
                raise ValueError("PICModel.{} must have one of the values: {}".format(name, tuple(_set)))
                
        raise_if_not_in_set("voltage", [5])
        raise_if_not_in_set("config_word_at_address", ["last"])

        raise_if_not_type("memory_size", int)
        if class_to_check.memory_size <= 0:
            raise ValueError("PICModel.memory_size must be > 0")
    
        raise_if_not_type("memory_unwritable_regions", tuple)
        for uwr in class_to_check.memory_unwritable_regions:
            if not isinstance(uwr, tuple):
                raise ValueError("PICModel.memory_unwritable_regions must only contain tuples")
            if (len(uwr) != 2) or (not all(isinstance(x, int) for x in uwr)):
                raise ValueError("PICModel.memory_unwritable_regions must contain pairs of integers")
            if not (0 <= uwr[0] <= uwr[1] < class_to_check.memory_size):
                raise ValueError("PICModel.memory_unwritable_regions pairs values must be ordered and fall into PIC memory range")
        

    def __init__(cls, name, bases, nmspc):
        super(PICModelCollector, cls).__init__(name, bases, nmspc)
        if name != "PICModel":
            PICModelCollector.validate_pic_model(cls)
            PICModelCollector.MODELS[name] = cls


class PICModel(metaclass=PICModelCollector):
    voltage = None
    memory_size = None
    memory_unwritable_regions = ()
    config_word_at_address = None
    
class PIC10F206(PICModel):
    voltage = 5
    memory_size = 0x400
    memory_unwritable_regions = ((0x240, 0x3FE),)
    config_word_at_address = "last"


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description='Invokes the arduino programmer to program a PIC controller.'
                    'with the data from a given HEX file.')
    
    parser.add_argument('serial_url',
                        type=str, 
                        help='the pyserial-url for the serial port that should'
                             'be used to program the PIC')
    parser.add_argument('pic_model',
                        type=str, 
                        choices=PICModelCollector.MODELS,
                        help='the PIC model number to use for programming '
                             '(CAUTION, selecting the wrong model number can'
                             'damage your PIC)')
    parser.add_argument('hex_file',
                        type=argparse.FileType('r'), 
                        nargs=1, 
                        help='the hex-file to use to program the PIC')

    parsed_args = parser.parse_args(sys.argv[1:])
    
    
    serial_port = serial.serial_for_url(parsed_args.serial_url, do_not_open=True)
    serial_port.baudrate = 57600
    serial_port.timeout = .25
    
    pic_model = PICModelCollector.MODELS[parsed_args.pic_model]
    
    serial_port.open()
    time.sleep(1)
    try:
        serial_port.write("help\n".encode("UTF-8"))
        serial_port.flush()
        help_text = serial_port.read(100).decode("UTF-8")
        
        if "available commands" in help_text.lower():
            print("Connected to PIC programmer...")
            
            while len(serial_port.read(100)) > 0:
                pass # eat up all remaining help text
            
            def issue_command(text):
                serial_port.write(text.encode("UTF-8"))
                short_answer = serial_port.read(3)
                if short_answer == "OK\n".encode("UTF-8"):
                    return "OK\n"
                if short_answer == "ERR".encode("UTF-8"):
                    error_msg = short_answer
                    last_read = 1
                    while last_read > 0:
                        new_data = serial_port.read(1024)
                        error_msg = error_msg + new_data
                        last_read = len(new_data)
                    raise ValueError("PIC programmer rported error:\n{}".format(error_msg.decode("UTF-8")))
                else:
                    answer = short_answer
                    last_read = 1
                    while last_read > 0:
                        new_data = serial_port.read(1024)
                        answer = answer + new_data
                        last_read = len(new_data)
                    return answer.decode("UTF-8")
                    
            issue_command("prog_mode on")
            try:
                issue_command("inc")
                issue_command("burn 3FFF")
                print(issue_command("read_hex 400"))
            finally:
                issue_command("prog_mode off")
        else:
            print("Could not get a valid answer from PIC programmer")
    finally:
        serial_port.close()
