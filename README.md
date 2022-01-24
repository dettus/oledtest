# oledtest
Waveshare SH1106 on a Jetson Nano , Banana Pi Zero, Raspberry Pi Zero


# BACKGROUND
So, I treated myself to a nice little Waveshare OLED 1.3 SH1106 board and tried to get it running
with a BananaPi Zero. (Thus far, I failed.)
The other day, I got my hands on a NVIDIA Jetson Nano, and was able to find a working example. It
was okay, but not of the quality I was expecting. So I decided to come up with my own, and this
is the result.



# COMPILING
Please have a look at the sourcecode. It is imperative that the GPIO numbering from the 
(deprecated) sysfs is being reflected in the physicalmapping[] array. Thus, the first time you 
try to compile it, it will fail. You need to configure lines 21-26. I am really really sorry.
(Yes, three files. oledtest.c and texttest.c as well as keytest.c)

Once you have done this, please run

gcc -O3 -o oledtest.app oledtest.c
gcc -O3 -o keytest.app keytest.c
gcc -O3 -o texttest.app texttest.c

IF YOU HAVE A NEW BOARD, PLEASE DO NOT HESITATE TO SEND ME THE MAPPING.
dettus@dettus.net, make sure to include "OLEDTEST" somewhere in the subject line.

# RUNNING
Just run sudo ./oledtest.app and watch the display. It should show you two alternating bitmaps.
If it complains about a GPIO pin -1, your sysfs mapping does not work with the OLED hat. 
Then you have to do some rewiring. You need to find 7 GPIO pins which are free. 


Or run sudo ./keytest.app and press the buttons. 



