#!/usr/bin/python3

import sys
import argparse

def mplab_to_words(text):
    current_address = 0
    result = []

    for num, raw_line in enumerate(text.split("\n")):
        try:
            line = raw_line.strip()
            if line.startswith(":"):
                line = line[1:].replace(" ", "")
                if len(line) % 2 != 0:
                    raise ValueError("line does not contain bytes")
                if len(line) < 2 + 4 + 2 + 2: # bytes, address, type, checksum
                    raise ValueError("valid lines must contain at least 5 bytes")
                
                check_sum = 0
                for i in range(0, len(line), 2):
                    check_sum = (check_sum + int(line[i:i+2], base=16)) % 0x100
                if check_sum != 0:
                    raise ValueError("invalid checksum")
                
                nbytes_str = line[0:2]
                address_str = line[2:6]
                type_str = line[6:8]
                trailing_str = line[8:]
                
                nbytes = int(nbytes_str, base=16)
                address = int(address_str, base=16)
                current_address = (current_address // 0x10000 * 0x10000) | address
                
                if len(trailing_str) != nbytes * 2 + 2:
                    raise ValueError("invalid number of bytes in line")
                    
                if type_str == "00":
                    new_words = [int(trailing_str[i+2:i+4] + trailing_str[i:i+2], base=16)
                                 for i in range(0, len(trailing_str) - 2, 4)]
                    
                    if len(result) < current_address + len(new_words):
                        result += [0xFFFF] * (current_address + len(new_words) - len(result))
                    result = result[:current_address] + new_words + result[current_address+len(new_words):]
                elif type_str == "01":
                    break
                elif type_str == "04":
                    current_address = current_address & 0xFFFF + 0x10000 * int(trailing_str[:-2], base=16)
                else:
                    raise ValueError("Non-supported mplab-hex value: {}".format(type_str))
        except ValueError as e:
            raise ValueError("Invalid line {}: {}\n    {}".format(num + 1, str(e), raw_line)) from e
    return result

def words_to_hex(data):
    result = ""

    spacing = 16

    for address in range(0, len(data), spacing):
        result += hex(address) + ": "
        result += " ".join(format(v, 'x') for v in data[address:address + spacing])
        result += "\n"
    return result

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description='Converts a binary dump into a hex dump or the other way'
                    'around. All files are converted and written to stdout.')
    
    parser.add_argument('filename', 
                        type=argparse.FileType('r'), 
                        nargs="*", 
                        help='the file(s) that should be converted. If none is defined, data is read from stdin')
    
    direction_group = parser.add_mutually_exclusive_group(required=True)
    direction_group.add_argument('--bin_to_hex', 
                                 dest='bin_to_hex', 
                                 action='store_true', 
                                 default=False,
                                 help='convert the file from binary to hex representation')
    direction_group.add_argument('--hex_to_bin', 
                                 dest='hex_to_bin', 
                                 action='store_true', 
                                 default=False,
                                 help='convert the file from hex to binary representation')
    direction_group.add_argument('--hex_to_mplabhex', 
                                 dest='hex_to_mplab', 
                                 action='store_true', 
                                 default=False,
                                 help='convert the file from hex to the mplab hex representation')
    direction_group.add_argument('--mplabhex_to_hex', 
                                 dest='mplab_to_hex', 
                                 action='store_true', 
                                 default=False,
                                 help='convert the file from mplab hex to the hex representation')
    direction_group.add_argument('--mplabhex_to_bin', 
                                 dest='mplab_to_bin', 
                                 action='store_true', 
                                 default=False,
                                 help='convert the file from mplab hex to the binary representation')
    direction_group.add_argument('--bin_to_mplabhex', 
                                 dest='bin_to_mplab', 
                                 action='store_true', 
                                 default=False,
                                 help='convert the file from binary to mplab hex representation')

    parsed_args = parser.parse_args(sys.argv[1:])
    
    input_files = (sys.stdin,)
    if parsed_args.filename:
        input_files = list(parsed_args.filename)
    
    result = ""
    for input_file in input_files:
        data = input_file.read()
    
            
        if parsed_args.bin_to_hex:
            result += bin_to_hex(data)
        elif parsed_args.hex_to_bin:
            result += bin_to_hex(data)
        elif parsed_args.hex_to_mplab:
            result += hex_to_mplab(data)
        elif parsed_args.mplab_to_hex:
            result += words_to_hex(mplab_to_words(data))
        elif parsed_args.bin_to_mplab:
            result += hex_to_mplab(bin_to_hex(data))
        elif parsed_args.mplab_to_bin:
            result += hex_to_bin(mplab_to_hex(data))
        else:
            raise ValueError("Invalid set of arguments supplied")
    
    print(result, end="")