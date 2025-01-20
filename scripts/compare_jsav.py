#!/usr/bin/env python3

import json
import argparse

parser = argparse.ArgumentParser()
parser.add_argument('-i', type=str, required=True, help='Input to compare against the other file')
parser.add_argument('-c', type=str, required=True, help='File to compare')
args = parser.parse_args()

a: dict = {}
b: dict = {}
with open(args.i, 'r') as fp:
    a = json.load(fp)

with open(args.c, 'r') as fp:
    b = json.load(fp)

ec = 0
for k,v in a.items():
    if str(b[k]) != str(v):
        ec = 1
        print(f'{k}: a=({v}) != b({b[k]})')

exit(ec)