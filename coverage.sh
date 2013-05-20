#!/bin/bash

for i in src/*.c; do 
	file=gcov/${i#src/}.gcov
	if [ -f $file ]; then
		lines=$(grep "^    #####:" $file | grep -v UNREACHABLE | wc -l)
		if [ $lines -gt 0 ]; then
			echo == $i: $lines ==
			grep "^    #####:" $file | grep -v UNREACHABLE
		fi
	fi
done
