#!/usr/bin/env bash

cd "$(dirname "${BASH_SOURCE[0]}")"

flask --app httpTest run
