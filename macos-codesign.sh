#!/bin/bash

if [ -d "./build/bin" ]; then
	for bin_file in `ls ./build/bin`
	do
		codesign -s - -f --entitlements gammon.entitlements ./build/bin/$bin_file
	done
fi
