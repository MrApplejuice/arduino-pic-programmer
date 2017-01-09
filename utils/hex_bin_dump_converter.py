#!/usr/bin/python3

import sys
import argparse

def mplab_to_hex(data):
    return data

def hex_to_mplab(data):
    return data
    
def bin_to_hex(data):
    return data

def hex_to_bin(data):
    return data

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description='Converts a binary dump into a hex dump or the other way'
                    'around. All files are converted and written to stdout.')
    
    parser.add_argument('filename', 
                        type=str, 
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

    parsed_args = parser.parse_args(sys.argv)
    
    input_files = (sys.stdin,)
    
    for input_file in input_files:
        data = input_file.read()
        
        if parsed_args.bin_to_hex:
            print(bin_to_hex(parsed_args))
        elif parsed_args.hex_to_bin:
            print(bin_to_hex(parsed_args))
        elif parsed_args.hex_to_mplab:
            print(hex_to_mplab(parsed_args))
        elif parsed_args.mplab_to_hex:
            print(mplab_to_hex(parsed_args))
        elif parsed_args.bin_to_mplab:
            print(hex_to_mplab(bin_to_hex(parsed_args)))
        elif parsed_args.mplab_to_bin:
            print(hex_to_bin(mplab_to_hex(parsed_args)))
        else:
            raise ValueError("Invalid set of arguments supplied")
    