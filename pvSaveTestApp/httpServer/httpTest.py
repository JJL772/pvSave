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
from flask import Flask, request
import os
import json

app = Flask(__name__)

db = {}

def _db_path() -> str:
    return os.path.dirname(__file__) + '/db.json'

def _save_db():
    with open(_db_path(), 'w') as fp:
        json.dump(db, fp)

def main():
    global db
    if os.path.exists(_db_path()):
        with open(_db_path(), 'r') as fp:
            db = json.load(fp)


@app.route('/pvget', methods=['GET'])
def _pvget():
    result = ''
    for v,k in request.args.items():
        if v != 'pv': continue
        if k not in db: continue
        result += f'{k} {k["type"]} {k["value"]}\n'
    return result

@app.route('/pvput', methods=['POST'])
def _pvput():
    for k,v in request.form.items():
        s = k.split(' ')
        db[s[0]] = {
            'type': s[1],
            'value': s[2].removesuffix('\n')
        }
    print(db)
    _save_db()
    return 'OK'

main()