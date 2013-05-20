#!/bin/bash
total=0
for i in src/*.c; do 
	file=gcov/${i#src/}.gcov
	if [ -f $file ]; then
		lines=$(grep "^    #####:" $file | grep -v UNREACHABLE | wc -l)
		if [ $lines -gt 0 ]; then
			echo == $i: $lines ==
			grep "^    #####:" $file | grep -v UNREACHABLE
			total=$[$total + $lines]
		fi
	fi
done
echo
echo Total untested lines: $total.
