#!/usr/bin/env python3
##############################################################################
## This file is part of 'pvSave'.
## It is subject to the license terms in the LICENSE.txt file found in the 
## top-level directory of this distribution and at: 
##    https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. 
## No part of 'pvSave', including this file, 
## may be copied, modified, propagated, or distributed except according to 
## the terms contained in the LICENSE.txt file.
##############################################################################

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

if ec != 0:
    print(f'{args.i} and {args.c} differ')
exit(ec)