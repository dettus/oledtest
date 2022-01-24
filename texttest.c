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


// Configuration starts here
//#define PATCHED_WIRING		// comment in if you did the EXACT SAME wiring as I did ;)
//#define PINOUT_JETSONNANO		// comment in if you connected the SH1106 directly to a Jetson Nano
//#define PINOUT_BANANAPI		// comment in if you connected the SH1106 directly to a Banana Pi Zero
//#define PINOUT_RASPBERRYPI		// comment in if you connected the SH1106 directly to a Raspberry Pi (Zero)
// Configuration ends here


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
#if defined(PINOUT_JETSONNANO)
#warning "This mapping is for the Jetson Nano board."
// Jetson Nano
// source: https://maker.pro/nvidia-jetson/tutorial/how-to-use-gpio-pins-on-jetson-nano-developer-kit
const short physicalmapping[41]={
	0,	// placeholder. there is no PIN0
	 -1, -1,// PIN1 =3.3V DC, PIN2 =5V DC
	 -1, -1,// 3,4
	 -1, -1,// 5,6
	216, -1, //PIN7 = AUDIO_MCLK
	 -1, -1, //9,10
	 50, 79, //PIN11=UART_2_RTS,PIN12=I2S_4_CLK
	 14, -1, //13,14
	194,232, //15,16
	 -1, 15, //17,18
	 16, -1, //19,20
	 17, 13, //21,22
	 18, 19, //23,24
	 -1, 20, //25,26
	 -1, -1, //27,28
	149, -1, //29,30
	200,168, //31,32
	 38, -1, //33,34
	 76, 51, //35,36
	 12, 77, //37,38
	 -1, 78  //39,40
};
#elif defined(PINOUT_BANANAPI)
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
#elif defined(PINOUT_RASPBERRYPI) 
#warning "This mapping is for the Raspberry Pi (Zero) board"
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
#error "The physical pins need to be mapped to the GPIO numbers. For some reason, those numbers are different for each board out there. So please have a look at the sourcecode, lines 21-26, and select the one that works best for you."
const int physicalmapping[41]={0};
#endif

// this is the GPIO pinout for the waveshare OLED 1.3 SH1106 hat
// source: https://www.waveshare.com/1.3inch-oled-hat.htm

#ifdef	PATCHED_WIRING
// Apparently, not every Key was mapped to a working GPIO pin. 
// So I did some rewiring. This is a setup that works
#define	PIN_LEFT	(physicalmapping[ 3])	// OLED pin 29
#define	PIN_UP		(physicalmapping[ 5])	// OLED pin 31
#define	PIN_FIRE	(physicalmapping[ 7])	// OLED pin 33
#define	PIN_DOWN	(physicalmapping[29])	// OLED pin 35
#define	PIN_RIGHT	(physicalmapping[31])	// OLED pin 37

#define	PIN_KEY1	(physicalmapping[27])	// OLED pin 36
#define	PIN_KEY2	(physicalmapping[24])	// OLED pin 38
#define	PIN_KEY3	(physicalmapping[26])	// OLED pin 40

#define	PIN_BL		(physicalmapping[16])	// OLED pin 12
#define	PIN_DC		(physicalmapping[18])
#define	PIN_RST		(physicalmapping[22])
#define PIN_CS		(physicalmapping[28])	// OLED pin 24

#define	PIN_MOSI	(physicalmapping[19])
#define	PIN_MISO	(physicalmapping[21])
#define	PIN_SCLK	(physicalmapping[23])

#else
#define	PIN_LEFT	(physicalmapping[29])
#define	PIN_UP		(physicalmapping[31])
#define	PIN_FIRE	(physicalmapping[33])
#define	PIN_DOWN	(physicalmapping[35])
#define	PIN_RIGHT	(physicalmapping[37])

#define	PIN_KEY1	(physicalmapping[36])
#define	PIN_KEY2	(physicalmapping[38])
#define	PIN_KEY3	(physicalmapping[40])

#define	PIN_BL		(physicalmapping[12])
#define	PIN_DC		(physicalmapping[18])
#define	PIN_RST		(physicalmapping[22])
#define PIN_CS		(physicalmapping[24])

#define	PIN_MOSI	(physicalmapping[19])
#define	PIN_MISO	(physicalmapping[21])
#define	PIN_SCLK	(physicalmapping[23])
#endif



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
	retval|=gpio_export(PIN_RST);	
	retval|=gpio_export(PIN_DC);	
	retval|=gpio_export(PIN_BL);	
	retval|=gpio_export(PIN_CS);	

	retval|=gpio_direction(PIN_RST, GPIO_OUTPUT);
	retval|=gpio_direction(PIN_DC,  GPIO_OUTPUT);
	retval|=gpio_direction(PIN_BL,  GPIO_OUTPUT);
	retval|=gpio_direction(PIN_CS,  GPIO_OUTPUT);


	// continue with the SPI pins
	retval|=gpio_export(PIN_SCLK);
	retval|=gpio_export(PIN_MOSI);
	retval|=gpio_export(PIN_MISO);
	
	retval|=gpio_direction(PIN_SCLK, GPIO_OUTPUT);
	retval|=gpio_direction(PIN_MOSI, GPIO_OUTPUT);
	retval|=gpio_direction(PIN_MISO, GPIO_INPUT);

	return retval;
}

int gpio_pins_down()
{
	int retval;

	retval=RETVAL_OK;
	retval|=gpio_write(PIN_RST,0);
	retval|=gpio_write(PIN_DC,0);
	retval|=gpio_write(PIN_BL,0);
	
	retval|=gpio_unexport(PIN_RST);	
	retval|=gpio_unexport(PIN_DC);	
	retval|=gpio_unexport(PIN_BL);	
	retval|=gpio_unexport(PIN_CS);	

	retval|=gpio_write(PIN_SCLK,0);
	retval|=gpio_write(PIN_MOSI,0);
		
	retval|=gpio_unexport(PIN_SCLK);
	retval|=gpio_unexport(PIN_MOSI);
	retval|=gpio_unexport(PIN_MISO);

	return retval;
}

void spi_writebyte(unsigned char byte,int mode,int msbfirst)
{
	int cpol;
	int cpol0;
	int cpha;
	int bits;

	switch (mode)
	{
		case SPI_MODE0:	cpol=0;cpha=0;break;
		case SPI_MODE1:	cpol=0;cpha=1;break;
		case SPI_MODE2:	cpol=1;cpha=0;break;
		case SPI_MODE3:	cpol=1;cpha=1;break;
	}
	cpol0=cpol;
	gpio_write(PIN_SCLK,cpol);
	if (!cpha)
	{
		cpol=1-cpol;	// when CPHA=1, the new value is being sampled at the first clock edge
		SPI_DELAY;
	}
	for (bits=0;bits<8;bits++)
	{
		int bit;
		if (msbfirst==SPI_MSBFIRST)
		{
			bit=(byte>>7)&1;
			byte<<=1;
		} else {
			bit=(byte)&1;
			byte>>=1;
		}
		gpio_write(PIN_MOSI,bit);	// set the value	
		gpio_write(PIN_SCLK,cpol);	// 1st clock edge
		cpol=1-cpol;
		SPI_DELAY;		
		gpio_write(PIN_SCLK,cpol);	// 2nd clock edge
		cpol=1-cpol;
		SPI_DELAY;
	}
	gpio_write(PIN_SCLK,cpol0);		// make sure that the SPI clk is the same as before
}
void oled_reset()
{
	gpio_write(PIN_DC,0);		
	gpio_write(PIN_RST,1);
	DELAY_MS(200);
	gpio_write(PIN_RST,0);
	DELAY_MS(200);
	gpio_write(PIN_RST,1);
	DELAY_MS(200);
}
void oled_init()
{
	const unsigned char oled_commands[]={
		0xae,0x02,0x10,0x40, 0x81,0xa0,0xc0,0xa6,	// turn off oled panel, set low column addr, set high column addr, set start line, set contrast control register,   set SEG/Column Mapping, Set COM/ROW scan direction, set normal display
		0xa8,0x3f,0xd3,0x00, 0xd5,0x80,0xd9,0xf1,	// set multiplex ration 1:64, 1/64 duty, set display offset, not offset, set display clock, set divide clock as 100frames/sec, set pre-charge period, set pre-chare as 15 clocks & discarge as 1 clock
		0xda,0x12,0xdb,0x40, 0x20,0x02,0xa4,0xa6,	// set com pins hardare, ???, set vcomh, set vcom deselect level, set page addr mode, ???, disable entire display on, disable inverse display on, 
		0xaf	// turn on oled panel
	};
	int i;
	gpio_write(PIN_DC,0);			
	for (i=0;i<sizeof(oled_commands);i++)
	{
		spi_writebyte(oled_commands[i],SPI_MODE0,SPI_MSBFIRST);	
	}
}

void oled_text(char *text,int line,int inverted)
{
	const unsigned long long font[95]={
		0x0000000000000000,//  
		0x0000065f5f060000,// !
		0x0000030300030300,// "
		0x00147f7f147f7f14,// #
		0x0000123a6b6b2e24,// $
		0x0062660c18306646,// %
		0x00487a375d4f7a30,// &
		0x0000000000030704,// '
		0x00000041633e1c00,// (
		0x0000001c3e634100,// )
		0x082a3e1c1c3e2a08,// *
		0x000008083e3e0808,// +
		0x0000000060e08000,// ,
		0x0000080808080808,// -
		0x0000000060600000,// .
		0x000103060c183060,// /
		0x003e7f4d59717f3e,// 0
		0x000040407f7f4240,// 1
		0x0000666f49597362,// 2
		0x0000367f49496322,// 3
		0x00507f7f53161c18,// 4
		0x0000397d45456727,// 5
		0x00003079494b7e3c,// 6
		0x0000070f79710303,// 7
		0x0000367f49497f36,// 8
		0x00001e3f69494f06,// 9
		0x0000000066660000,// :
		0x0000000066e68000,// ;
		0x0000004163361c08,// <
		0x0000242424242424,// =
		0x0000081c36634100,// >
		0x0000060f59510302,// ?
		0x001e1f5d5d417f3e,// @
		0x00007c7e13137e7c,// A
		0x00367f49497f7f41,// B
		0x0022634141633e1c,// C
		0x001c3e63417f7f41,// D
		0x0063415d497f7f41,// E
		0x0003011d497f7f41,// F
		0x0072735141633e1c,// G
		0x00007f7f08087f7f,// H
		0x000000417f7f4100,// I
		0x00013f7f41407030,// J
		0x0063771c087f7f41,// K
		0x00706040417f7f41,// L
		0x007f7f0e1c0e7f7f,// M
		0x007f7f180c067f7f,// N
		0x001c3e6341633e1c,// O
		0x00060f09497f7f41,// P
		0x00005e7f71213f1e,// Q
		0x00667f19097f7f41,// R
		0x00003273594d6f26,// S
		0x000003417f7f4103,// T
		0x00007f7f40407f7f,// U
		0x00001f3f60603f1f,// V
		0x007f7f3018307f7f,// W
		0x0043673c183c6743,// X
		0x0000074f78784f07,// Y
		0x0073674d59716347,// Z
		0x00000041417f7f00,// [
		0x006030180c060301,// 
		0x0000007f7f414100,// ]
		0x00080c0603060c08,// ^
		0x8080808080808080,// _
		0x0000000407030000,// `
		0x0040783c54547420,// a
		0x00307848483f7f41,// b
		0x0000286c44447c38,// c
		0x00407f3f49487830,// d
		0x0000185c54547c38,// e
		0x00000203497f7e48,// f
		0x00047cf8a4a4bc98,// g
		0x00787c04087f7f41,// h
		0x000000407d7d4400,// i
		0x00007dfd8080e060,// j
		0x00446c38107f7f41,// k
		0x000000407f7f4100,// l
		0x00787c1c38187c7c,// m
		0x0000787c04047c7c,// n
		0x0000387c44447c38,// o
		0x00183c24a4f8fc84,// p
		0x0084fcf8a4243c18,// q
		0x00181c044c787c44,// r
		0x0000247454545c48,// s
		0x000024447f3e0400,// t
		0x00407c3c40407c3c,// u
		0x00001c3c60603c1c,// v
		0x003c7c7038707c3c,// w
		0x00446c3810386c44,// x
		0x00007cfca0a0bc9c,// y
		0x0000644c5c74644c,// z
		0x00004141773e0808,// {
		0x0000007777000000,// |
		0x000008083e774141,// }
		0x0001030203010302,// ~
	};
	#define	TEXT_WIDTH	16
	#define	FONT_XRES	8
	int i;
	int j;

	gpio_write(PIN_DC,0);			// write command
	spi_writebyte(0xb0+line,SPI_MODE0,SPI_MSBFIRST);	// set page address
	spi_writebyte(0x02,SPI_MODE0,SPI_MSBFIRST);	// set low column address
	spi_writebyte(0x10,SPI_MODE0,SPI_MSBFIRST);	// set high column address
	gpio_write(PIN_DC,1);			// write data
	
	for (i=0;i<TEXT_WIDTH;i++)
	{
		unsigned long long x;
		x=font[text[i]-' '];
		if (inverted) x=~x;
		for (j=0;j<FONT_XRES;j++)
		{
			spi_writebyte(x&0xff,SPI_MODE0,SPI_MSBFIRST);
			x>>=8;
		}
	}
}


int sh1106_up()
{
	int retval;
	retval=RETVAL_OK;
	retval|=gpio_pins_up();
	// spi mode 0
	// spi master
	// spi clock div 2
	// spi msbfirst

	// ?? tell the device it should take orders from SPI??
	retval|=gpio_write(PIN_BL,1);
	retval|=gpio_write(PIN_CS,0);
	
	if (retval==RETVAL_OK)
	{	
		oled_reset();
		oled_init();
	}
	return retval;
}
int sh1106_down()
{
	return gpio_pins_down();
}
void graceFulExit(int signal_number)
{
	printf("shutting down display...\n");
	sh1106_down();
	exit(0);
}
int main(int argc,char** argv)
{
	int i;
	char buf[16];
	
	signal(SIGINT, graceFulExit);
	if (sh1106_up())
	{
		fprintf(stderr,"unable to start up display. sorry");
		return 1;
	}

	oled_text("----------------",0,0);
	oled_text("-     HELL0    -",1,0);
	oled_text("-     WORLD    -",2,0);
	oled_text("----------------",3,0);
	oled_text("-Get vaccinated-",4,0);
	oled_text("-Better safe   -",5,0);
	oled_text("-than sorry.   -",6,0);
	oled_text("----------------",7,0);
// make one line blink
	for (i=0;i<100;i++)
	{
		oled_text("-Get vaccinated-",4,1);
		oled_text("-Get vaccinated-",4,0);
		printf("%2d\n",i);
	}
	printf("press Enter to quit\n");
	
	fgets(buf,sizeof(buf),stdin);	
	graceFulExit(0);

	return 0;	
}
