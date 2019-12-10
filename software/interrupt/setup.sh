#!/bin/sh

if [ -f "/dev/acc_int" ];
   then rm /dev/acc_int
fi

/bin/mknod /dev/acc_int c 244 0

if [ -f "/proc/acc-interrupt" ]; 
     then /sbin/rmmod acc_interrupt
fi

/sbin/insmod acc_interrupt.ko

cat /proc/interrupts | grep acc

