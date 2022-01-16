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
#else
#error "Please have a look at the source code and select one of the mappings from the sysfs numbering to the physical pins"
#endif

// this is the GPIO pinout for the waveshare OLED 1.3 SH1106 hat
#define PIN_CS	(physicalmapping[24])
#define	PIN_RST	(physicalmapping[22])
#define	PIN_DC	(physicalmapping[18])
#define	PIN_BL	(physicalmapping[12])

#define	PIN_SCLK	(physicalmapping[23])
#define	PIN_MOSI	(physicalmapping[19])
#define	PIN_MISO	(physicalmapping[21])


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
	fd=open(buffer,O_WRONLY);
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
void oled_draw(unsigned char* bitmap)
{
	#define	CANVAS_WIDTH	128
	#define	CANVAS_PAGES	8	
	unsigned char canvas[CANVAS_WIDTH*CANVAS_PAGES];
	int i;
	for (i=0;i<CANVAS_WIDTH*CANVAS_PAGES;i++)
	{
		// each byte in the canvas represents a column of 8 bits from the bitmap
		// 0000
		// 1111
		// 2222
		// 3333
		// 4444
		// ...
	
		int j;
		unsigned char byte;
		int x,y;
		byte=0;
		x=i%BITMAP_WIDTH;
		y=(i/BITMAP_WIDTH)*8;
		for (j=0;j<8;j++)
		{
			int bit;
			bit=bitmap[x+(y+j)*BITMAP_WIDTH]?0x80:0x00;
			byte>>=1;
			byte|=bit;
		}
		canvas[i]=byte;
	}

	for (i=0;i<CANVAS_PAGES;i++)
	{
		int j;
		gpio_write(PIN_DC,0);			// write command
		spi_writebyte(0xb0+i,SPI_MODE0,SPI_MSBFIRST);	// set page address
		spi_writebyte(0x02,SPI_MODE0,SPI_MSBFIRST);	// set low column address
		spi_writebyte(0x10,SPI_MODE0,SPI_MSBFIRST);	// set high column address
		gpio_write(PIN_DC,1);			// write data
		for (j=0;j<CANVAS_WIDTH;j++)
		{
			spi_writebyte(canvas[i*CANVAS_WIDTH+j],SPI_MODE0,SPI_MSBFIRST);
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
	unsigned char bitmap[BITMAP_WIDTH*BITMAP_HEIGHT]={0};
	unsigned char bitmap2[BITMAP_WIDTH*BITMAP_HEIGHT]={0};
	int i;
	
	signal(SIGINT, graceFulExit);
	if (sh1106_up())
	{
		fprintf(stderr,"unable to start up display. sorry");
		return 1;
	}
	for (i=0;i<BITMAP_WIDTH*BITMAP_HEIGHT;i++)
	{
		if ((i%10)<5) bitmap[i]=1;else bitmap[i]=0;	
	}
	for (i=0;i<BITMAP_HEIGHT;i++)
	{
		bitmap2[i+i*BITMAP_WIDTH]=1;
	}
	// draw the two bitmaps, one after the other
	for (i=0;i<10;i++)
	{
		oled_draw(bitmap);
		printf("%d\n",i);
		oled_draw(bitmap2);
	}
	printf("press Enter to quit\n");
	fgets(bitmap,sizeof(bitmap),stdin);	
	graceFulExit(0);

	return 0;	
}
