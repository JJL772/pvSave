#!/usr/bin/env bash

cd "$(dirname "${BASH_SOURCE[0]}")/iocBoot/ioc-pvSave-test"

if [ -z "$DEBUGGER" ]; then
	./st.cmd
else
	prog=$(head -n 1 ./st.cmd | sed 's/#!//g')
	$DEBUGGER $prog ./st.cmd
fi
