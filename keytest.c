/*
MIT No Attribution

Copyright 2022 Thomas Dettbarn (dettus@dettus.net)

Permission is hereby granted, free of charge, to any person obtaining a copy of this
software and associated documentation files (the "Software"), to deal in the Software
without restriction, including without limitation the rights to use, copy, modify,
merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>

#define	RETVAL_OK	0
#define	RETVAL_NOK	-1
#define	GPIO_INPUT	0
#define	GPIO_OUTPUT	1


#define	MAXBUFLEN	64

#define	DELAY_US(us)	usleep((us));
#define	DELAY_MS(ms)	usleep((ms)*1000);

#define	SPI_MODE0	0
#define	SPI_MODE1	1
#define	SPI_MODE2	2
#define	SPI_MODE3	3

#define	SPI_LSBFIRST	0
#define	SPI_MSBFIRST	1
#define SPI_DELAY	//DELAY_US(1)

#define	BITMAP_HEIGHT	64
#define	BITMAP_WIDTH	128


//// depends on the development board

// the way I understand this is, that the physical pins are mapped into
// the system memory. to accces those, there is some sort of memory
// offset maybe
#if 0
#warning "This mapping is for the Jetson Nano board."
// Jetson Nano
// source: https://maker.pro/nvidia-jetson/tutorial/how-to-use-gpio-pins-on-jetson-nano-developer-kit
const short physicalmapping[41]={
	0,	// placeholder. there is no PIN0
	 -1, -1,// PIN1 =3.3V DC, PIN2 =5V DC
	 -1, -1,
	 -1, -1,
	216, -1, //PIN7 = AUDIO_MCLK
	 -1, -1,
	 50, 79, //PIN11=UART_2_RTS,PIN12=I2S_4_CLK
	 14, -1, 
	194,232,
	 -1, 15,
	 16, -1,
	 17, 13,
	 18, 19,
	 -1, 20,
	 -1, -1,
	149, -1,
	200,168,
	 38, -1,
	 76, 51,
	 12, 77,
	 -1, 78
};
#elif 0
#warning "This mapping is for the Banana Pi board."
// source: https://wiki.fw-web.de/doku.php?id=en:bpi-r64:gpio
const short physicalmapping[41]={
	0,	// placeholder. there is no PIN0
	 -1, -1,// PIN1 =3.3V DC, PIN2 =5V DC
	 56, -1,// 3,4
	 55, -1,// 5,6
	101, 59, //PIN7 = PWM7,8
	 -1, 60,// 9,10
	 51, -1, //PIN11=UART_2_RTS,PIN12=I2S_4_CLK
	 52, -1, //13,14
	 98, 61, //15,16
	 -1, 62, //17,18
	 68, -1, //19,20
	 69, 63, //21,22
	 67, 70, //23,24
	 -1, 66, //25,26
	 58, 57, //27,28
	 85, -1, //29,30
	 82, 64, //31,32
	 -1, -1, //33,34
	 -1, 65, //35,36
	 86, -1, //37,38
	 -1, -1  //39,40
};
#elif 1
#warning "This mapping is for the Raspberry Pi Zero W 1.1 board"
// source: https://www.theengineeringprojects.com/wp-content/uploads/2021/03/raspberry-pi-zero-5.png
// NOTE: GPIO16 was actually missing in the diagram.
const short physicalmapping[41]={
	0,	// placeholder. there is no PIN0
	 -1, -1,// PIN1 =3.3V DC, PIN2 =5V DC
	  2, -1,// 3,4
	  3, -1,// 5,6
	  4, 14, //PIN7 = PWM7,8
	 -1, 15,// 9,10
	 17, 18, //PIN11=UART_2_RTS,PIN12=I2S_4_CLK
	 27, -1, //13,14
	 22, 23, //15,16
	 -1, 24, //17,18
	 10, -1, //19,20
	  9, 25, //21,22
	 11,  8, //23,24
	 -1,  7, //25,26
	  0,  1, //27,28
	  5, -1, //29,30
	  6, 12, //31,32
	 13, -1, //33,34
	 19, 16, //35,36	
	 26, 20, //37,38
	 -1, 21  //39,40
};
#else
#error "Please have a look at the source code and select one of the mappings from the sysfs numbering to the physical pins"
#endif

// this is the GPIO pinout for the waveshare OLED 1.3 SH1106 hat
// source: https://www.waveshare.com/1.3inch-oled-hat.htm
#define	PIN_LEFT	(physicalmapping[29])
#define	PIN_UP		(physicalmapping[31])
#define	PIN_FIRE	(physicalmapping[33])
#define	PIN_DOWN	(physicalmapping[35])
#define	PIN_RIGHT	(physicalmapping[37])

#define	PIN_KEY1	(physicalmapping[36])
#define	PIN_KEY2	(physicalmapping[38])
#define	PIN_KEY3	(physicalmapping[40])

#define	PIN_BL	(physicalmapping[12])
#define	PIN_DC	(physicalmapping[18])
#define	PIN_RST	(physicalmapping[22])
#define PIN_CS	(physicalmapping[24])

#define	PIN_MOSI	(physicalmapping[19])
#define	PIN_MISO	(physicalmapping[21])
#define	PIN_SCLK	(physicalmapping[23])


int gpio_export(int pin)
{
	int fd;
	char buffer[MAXBUFLEN];
	int len;
	fd=open("/sys/class/gpio/export", O_WRONLY);
	if (fd<0)
	{
		fprintf(stderr,"GPIO export for pin %d failed\n",pin);
		return RETVAL_NOK;
	}
	len=snprintf(buffer,MAXBUFLEN,"%d",pin);
	write(fd,buffer,len);
	close(fd);
	return RETVAL_OK;
}


int gpio_unexport(int pin)
{
	int fd;
	char buffer[MAXBUFLEN];
	int len;
	fd=open("/sys/class/gpio/unexport", O_WRONLY);
	if (fd<0)
	{
		fprintf(stderr,"GPIO unexport for pin %d failed\n",pin);
		return RETVAL_NOK;
	}
	len=snprintf(buffer,MAXBUFLEN,"%d",pin);
	write(fd,buffer,len);
	close(fd);
	return RETVAL_OK;
}

int gpio_direction(int pin,int direction)
{
	int fd;
	char buffer[MAXBUFLEN];
	snprintf(buffer,MAXBUFLEN,"/sys/class/gpio/gpio%d/direction",pin);
	fd=open(buffer,O_WRONLY);
	if (fd<0)
	{
		fprintf(stderr,"GPIO direction for pin %d cannot be set\n",pin);
		return RETVAL_NOK;
	}
	if (direction==GPIO_INPUT)
	{
		write(fd,"in",2);
	} else if (direction==GPIO_OUTPUT) {
		write(fd,"out",3);
	} else {
		fprintf(stderr,"Type error for direction for pin %d (%d unknown)\n",pin,direction);
	}
	close(fd);
		
	
	return RETVAL_OK;
}
int gpio_write(int pin,int value)
{
	int fd;
	char buffer[MAXBUFLEN];
	snprintf(buffer,MAXBUFLEN,"/sys/class/gpio/gpio%d/value",pin);
	fd=open(buffer,O_WRONLY);
	if (fd<0)
	{
		fprintf(stderr,"Cannot access GPIO pin %d\n",pin);
		return RETVAL_NOK;
	}
	write(fd,value?"1":"0",1);
	close(fd);
	return RETVAL_OK;
}

int gpio_read(int pin,int* value)
{
	int fd;
	char buffer[MAXBUFLEN];
	int len;
	snprintf(buffer,MAXBUFLEN,"/sys/class/gpio/gpio%d/value",pin);
	fd=open(buffer,O_RDONLY);
	if (fd<0)
	{
		fprintf(stderr,"Cannot access GPIO pin %d\n",pin);
		return RETVAL_NOK;
	}
	len=read(fd,buffer,3);
	if (len<0)
	{
		fprintf(stderr,"Unable to read from pin %d\n",pin);
		close(fd);
		return RETVAL_NOK;		
	}
	*value=atoi(buffer);
	close(fd);
	return RETVAL_OK;
}

int gpio_pins_up()
{
	int retval;

	retval=RETVAL_OK;
	// start with the GPIO configuration
	retval|=gpio_export(PIN_LEFT);
	retval|=gpio_export(PIN_UP);
	retval|=gpio_export(PIN_FIRE);
	retval|=gpio_export(PIN_DOWN);
	retval|=gpio_export(PIN_RIGHT);
	retval|=gpio_export(PIN_KEY1);
	retval|=gpio_export(PIN_KEY2);
	retval|=gpio_export(PIN_KEY3);

	retval|=gpio_direction(PIN_LEFT,  GPIO_INPUT);
	retval|=gpio_direction(PIN_UP,    GPIO_INPUT);
	retval|=gpio_direction(PIN_FIRE,  GPIO_INPUT);
	retval|=gpio_direction(PIN_DOWN,  GPIO_INPUT);
	retval|=gpio_direction(PIN_RIGHT, GPIO_INPUT);
	retval|=gpio_direction(PIN_KEY1,  GPIO_INPUT);
	retval|=gpio_direction(PIN_KEY2,  GPIO_INPUT);
	retval|=gpio_direction(PIN_KEY3,  GPIO_INPUT);

	return retval;
}

int gpio_pins_down()
{
	int retval;

	retval=RETVAL_OK;
	retval|=gpio_unexport(PIN_LEFT);
	retval|=gpio_unexport(PIN_UP);
	retval|=gpio_unexport(PIN_FIRE);
	retval|=gpio_unexport(PIN_DOWN);
	retval|=gpio_unexport(PIN_RIGHT);
	retval|=gpio_unexport(PIN_KEY1);
	retval|=gpio_unexport(PIN_KEY2);
	retval|=gpio_unexport(PIN_KEY3);


	return retval;
}

int sh1106_up()
{
	int retval;
	retval=RETVAL_OK;
	retval|=gpio_pins_up();
	return retval;
}
int sh1106_down()
{
	return gpio_pins_down();
}
void graceFulExit(int signal_number)
{
	printf("shutting down ...\n");
	sh1106_down();
	exit(0);
}
int main(int argc,char** argv)
{
	char *names[8]={"LEFT","UP","FIRE","DOWN","RIGHT","KEY1","KEY2","KEY3"};
	int pins[8]={PIN_LEFT,PIN_UP,PIN_FIRE,PIN_DOWN,PIN_RIGHT,PIN_KEY1,PIN_KEY2,PIN_KEY3};
	int lastval[8]={0};
	signal(SIGINT, graceFulExit);
	if (sh1106_up())
	{
		fprintf(stderr,"unable to start up pins. sorry.\n");
		return 1;
	}
	while (1)
	{
		int i;
		for (i=0;i<8;i++) 
		{
			int val;
			gpio_read(pins[i],&val);
			if (val!=lastval[i])
			{
				printf("%8s %d->%d\n",names[i],lastval[i],val);
			}
			lastval[i]=val;
		}
	}	
	graceFulExit(0);

	return 0;	
}
