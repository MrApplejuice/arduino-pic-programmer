#!/usr/bin/python3

import time
import sys
import re
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
        raise_if_not_type("config_word_at_address", int)
        raise_if_not_type("config_word_mask", int)

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
    skip_values = []
    config_word_mask = None
    
    @classmethod
    def in_unimplemented_zone(cls, address):
        for start_reg, end_reg in cls.memory_unwritable_regions:
            if start_reg <= address <= end_reg:
                return True
        return False
    
class PIC10F206(PICModel):
    voltage = 5
    memory_size = 0x400
    memory_unwritable_regions = ((0x240, 0x3FE), # unimplemented zone
                                 (0x3FF, 0x3FF)) # configuration word is also unimplemented when it comes to the programmer
    skip_values = [0xFFFF, 0xFFF]
    config_word_at_address = -1
    config_word_mask = 0xFFF


def wordsFileToWordsList(text):
    result = []
    for num, raw_line in enumerate(text.split("\n")):
        try:
            line = raw_line.strip()
            if (len(line) == 0) or line.startswith("#"):
                pass
            else:
                if '#' in line:
                    line = line.split('#', 1)[0].strip()
                if ':' not in line:
                    raise ValueError('Expected value separator for addresss')
                
                address_str, values_str = line.split(':', 1)
                address = int(address_str.strip(), base=16)
                if address < 0:
                    raise ValueError("invalid address")
                
                values = [int(x, base=16) for x in re.split(r"\s+", values_str.strip())]
                if not all(0 <= x <= 0xFFFF for x in values):
                    raise ValueError("values out of bounds (0x0000-0xFFFF)")
                
                if len(result) < address:
                    result = result + [0xFFFF] * (len(result) - address)
                result = result[:address] + values + result[address+len(values):]
        except ValueError as e:
            raise ValueError("Invalid line {}: {}\n    {}".format(num + 1, str(e), raw_line)) from e
    return result


class ProgrammingError(Exception): pass

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
                        help='the hex-file to use to program the PIC')
                        
    parser.add_argument('--no-configuration-word', 
                        dest="write_config_word",
                        action='store_false',
                        default=True,
                        help='disable writing of the configuration word')

    parsed_args = parser.parse_args(sys.argv[1:])
    
    words = wordsFileToWordsList(parsed_args.hex_file.read())
    if len(words) == 0:
        print("Nothing to program")
        sys.exit(1)
    
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
            
            def issue_command(text, expected="OK"):
                serial_port.write(text.encode("UTF-8"))
                short_answer = serial_port.read(len(expected))
                if short_answer == expected.encode("UTF-8"):
                    if serial_port.read(1) == b"\r":
                        serial_port.read(1) # Munch up a \n as well...
                    return "OK"
                elif short_answer[:2] == "ER".encode("UTF-8"):
                    error_msg = short_answer
                    last_read = 1
                    while last_read > 0:
                        new_data = serial_port.read(1024)
                        error_msg = error_msg + new_data
                        last_read = len(new_data)
                    raise ProgrammingError("PIC programmer reported error:\n{}".format(error_msg.decode("UTF-8")))
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
                if parsed_args.write_config_word:
                    try:
                        config_word = words[pic_model.config_word_at_address]
                    except IndexError:
                        raise ProgrammingError("config word could be found in hex file")
                        
                    config_word &= 0xFFF
                    
                    print("Writing configuration word {:04x}...".format(config_word))
                    issue_command("erase")
                    issue_command("burn {:04x}".format(config_word))
                    
                    word_on_chip = issue_command("read_hex 1")
                    word_on_chip = int(word_on_chip.strip().split(":")[1].strip(), base=16)
                    if word_on_chip != config_word:
                        raise ProgrammingError("writing the configuration word failed."
                                               "Should be {:04x} but was {:04x}".format(
                                                   config_word, word_on_chip))
                    print("Config word written successfully and verified")
                else:
                    issue_command("inc")
                
                # Do the "I can read"-hack
                issue_command("burn 3FFF")
                
                issue_command("erase")
                
                address = 0
                progress_value = -1
                print("Starting programming...", end="")
                while address < min(pic_model.memory_size, len(words)):
                    new_progress_value = address * 10 // (min(pic_model.memory_size, len(words)) - 1)
                    if new_progress_value != progress_value:
                        progress_value = new_progress_value
                        print("{}%...".format(progress_value * 10), end="", flush=True)
                    
                    if words[address] not in pic_model.skip_values:
                        if not pic_model.in_unimplemented_zone(address):
                            issue_command("burn {:04x}".format(words[address]))
                    issue_command("inc", expected="{:x}".format(address + 1))
                    address += 1
                print("finished!")
                while address < pic_model.memory_size:
                    issue_command("inc", expected="{:x}".format(address + 1))
                    address += 1
                
                print(issue_command("read_hex {:04x}".format(pic_model.memory_size)))
            finally:
                issue_command("prog_mode off")
        else:
            print("Could not get a valid answer from PIC programmer")
    finally:
        serial_port.close()
