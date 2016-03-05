#!/bin/sh

#sox -d -c 1 -e signed-integer -r 48000 -b 16 -t raw - | ./swiper -c 0 -n 2 -b -stdin


#debug
#sox -d -c 1 -e signed-integer -r 48000 -b 16 -t raw - | ./swiper -c 0 -n 1 -p -stdin > out.txt

sox -d -c 1 -e signed-integer -r 48000 -b 16 -t raw - 2>log.txt | ./swiper -c 0 -n 1 -b
