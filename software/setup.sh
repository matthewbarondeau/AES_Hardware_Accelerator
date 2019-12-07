#!/bin/sh

if [ -f "/dev/acc_int" ];
   then rm /dev/acc_int
fi

/bin/mknod /dev/acc_int c 244 0

if [ -f "/proc/acc-interrupt" ]; 
     then /sbin/rmmod acc_interrupt
fi

/sbin/insmod acc_interrupt.ko

# /bin/pm 0x43c00000 0x55 > /dev/null
# sleep 1.0
# /bin/pm 0x43c00000 0xaa > /dev/null
# sleep 1.0
cat /proc/interrupts | grep acc

#while (ls > /dev/null) do ./test_dma 0x10 0x3; cat /proc/interrupts | grep xilinx-dma; done

