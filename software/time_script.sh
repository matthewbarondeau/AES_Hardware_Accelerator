#!/bin/bash

while true; do
	for (( n=1; n<100000; n++))
do
	echo "$n" >> numbers.txt
	dd bs=1 count=$n if=/dev/urandom of=random.txt
	(time openssl enc -aes-256-ecb -K \
		3030303030303030303030303030303030303030303030303030303030303030 \
		-in random.txt -out mesg.enc) |& \
		awk '\
		/real/ {print $2 >> "open_ssl.txt"}'
	
	(time ./aes_test -f enc random.txt key_file.txt -o output.txt) |& \
		awk '\
		/real/ {print $2 >> "aes_time.txt"}'
done
done
