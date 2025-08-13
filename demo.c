#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include "library.h"
#include "wiringPi.h"
#include <signal.h>
#include <time.h>

#define LEDPort 0x3A
#define KbdPort 0x3C
#define LCDPort 0x3B
#define SMPort  0x39

#define Col7Lo 0xF7
#define Col6Lo 0xFB
#define Col5Lo 0xFD
#define Col4Lo 0xFE

#define AUDIOFILE "/tmp/shootingstars.raw"

unsigned char ScanCode;		

int open_bar[4] = {0x08, 0x04, 0x02, 0x01};

int close_bar[4] = {0x01, 0x02, 0x04, 0x08};

int NumSteps = 100;


const unsigned char Bin2LED[] =
{
    0x40, 0x79, 0x24, 0x30,
    0x19, 0x12, 0x02, 0x78,
    0x00, 0x18, 0x08, 0x03,
    0x46, 0x21, 0x06, 0x0E,0xFF
};

const unsigned char ScanTable[12] =
{
    0xB7, 0x7E, 0xBE, 0xDE,
    0x7D, 0xBD, 0xDD, 0x7B,
    0xBB, 0xDB, 0x77, 0xD7
};

unsigned char ScanCode;

static void initlcd();
static void lcd_writecmd(char cmd);
static void LCDprint(char *sptr);
static void lcddata(unsigned char cmd);
static void buzz();


static char detect();
static void buttonOne();
static void buttonTwo();
static void buttonFour();
static void buttonStar();
static void buttonFour();
static void buttonHex();



unsigned char ScanKey();
unsigned char ProcKey();


int inOut = 0; // 0 = in ; 1 = out

int main(int argc, char *argv[])
{    
	CM3DeviceInit();
	system("killall pqiv");
	initlcd();
	sleep(1);

	if (inOut == 0){
		system("DISPLAY=:0.0 pqiv -f /tmp/entry.jpg &");		
		sleep(2);	
		lcd_writecmd(0x80);
		LCDprint("Welcome to PTS");
		lcd_writecmd(0xC0);
		LCDprint("1 : Get Ticket");
		CM3_outport(LEDPort,Bin2LED[16]);
	}

	if (inOut == 1){
		system("DISPLAY=:0.0 pqiv -f /tmp/exit.jpg &");		
		sleep(2);
		lcd_writecmd(0x80);
		LCDprint("Thanks for coming");
		lcd_writecmd(0xC0);
		LCDprint("2 : Proceed");
		CM3_outport(LEDPort,Bin2LED[16]);	
	}

	
	

	while(1){
		unsigned char key = detect();

		switch (key)
        {
            case '1':
                buttonOne();
                break;

            case '2':
                buttonTwo();
                break;

			case '4':
                buttonFour();
                break;

            case 'B':
                buttonHex();
                break;

			case 'A':
                buttonStar();
                break;

            default:                        
                break;
        }
	}

    

	CM3DeviceDeInit();   

}



static char detect(){
    unsigned char k;

    do {
        k = ScanKey();
        usleep(1000);
    } while (k == 0xFF);

    unsigned char idx = (k > 0x39) ? k - 0x37 : k - 0x38;
    //lcddata(k);
    do { usleep(30000); } while (ScanKey() != 0xFF);

    return k;

}


static void buttonOne(){
	int i,j;
	char line1[17], line2[17];
	int *hh,*mm,*ss;

    srand(time(NULL));                                 // Seed random number generator
    int ticketNum = rand() % 900000 + 100000;            // Range: 100000 to 999999

	time_t now = time(NULL);
	struct tm *lt = localtime(&now);

	char buf[9];


	if(lt){
		strftime(buf,sizeof buf, "%H:%M:%S", lt);
	}

	
	                       
    snprintf(line1, sizeof(line1), "Ticket: %05d", ticketNum);
    snprintf(line2, sizeof(line2), "Time: %s",buf);
	
	buzz();

    lcd_writecmd(0x01);                                
    lcd_writecmd(0x80);                                
    LCDprint(line1);
    lcd_writecmd(0xC0);                                
    LCDprint(line2);
	usleep(50000);

	for(j = NumSteps; j > 0; j--)
	{
		for(i=0;i<4;i++)
			{
				CM3_outport(SMPort, open_bar[i]);
				usleep(8000);
			}
	}

	system("DISPLAY=:0.0 pqiv -f /tmp/enter.jpg &");
	usleep(5000);
	CM3_outport(LEDPort,Bin2LED[8]);
	usleep(10000000);
	
	
	for(j = NumSteps; j > 0; j--)
	{
		for(i=0;i<4;i++)
			{
				CM3_outport(SMPort, close_bar[i]);
				usleep(8000);
			}
	}

	usleep(100000);
	CM3_outport(LEDPort,Bin2LED[16]);
	system("killall pqiv");
	system("DISPLAY=:0.0 pqiv -f /tmp/entry.jpg &");
	initlcd();
	sleep(1);
	lcd_writecmd(0x80);
	LCDprint("Welcome to PTS");
	lcd_writecmd(0xC0);
	LCDprint("1 : Get Ticket");
	


}


static void buttonTwo(){

	system("DISPLAY=:0.0 pqiv -f /tmp/input.jpg &");		 //toggle GUI to show leaving payment and back options 
	usleep(100000);
    lcd_writecmd(0x01);                                 
    lcd_writecmd(0x80);                                 
    LCDprint("# : Payment");
	lcd_writecmd(0xC0);                                
    LCDprint("* : Back ");

	while (1)
	{
		unsigned char key = detect();
		if(key == 'B'){
			break;
		}

		if(key != 0){
			lcddata('*');
		}
	
	
	}

    
}

static void buttonFour(){

	system("DISPLAY=:0.0 pqiv -f /tmp/contact.jpg &");

    lcd_writecmd(0x01);                                 
    lcd_writecmd(0x80);                                 
    LCDprint("P: 1800 696 6969");
	lcd_writecmd(0xC0);                                
    LCDprint("www.parkingson.com");

	usleep(10000000);
	CM3_outport(LEDPort,Bin2LED[16]);
	system("killall pqiv");
	initlcd();

	if (inOut == 0){
		system("DISPLAY=:0.0 pqiv -f /tmp/entry.jpg &");		
		sleep(2);	
		lcd_writecmd(0x80);
		LCDprint("Welcome to PTS");
		lcd_writecmd(0xC0);
		LCDprint("1 : Get Ticket");
		CM3_outport(LEDPort,Bin2LED[16]);
	}

	if (inOut == 1){
		system("DISPLAY=:0.0 pqiv -f /tmp/exit.jpg &");		
		sleep(2);
		lcd_writecmd(0x80);
		LCDprint("Thanks for coming");
		lcd_writecmd(0xC0);
		LCDprint("2 : Proceed");
		CM3_outport(LEDPort,Bin2LED[16]);	
	}


    
}


static void buttonHex(){
	int i,j;
	system("DISPLAY=:0.0 pqiv -f /tmp/leave.jpg &");			//Enter Ticket ID, Display Ticket Paid, Open Gantry, Back to entry GUI
    sleep(2);
	initlcd();
	lcd_writecmd(0x80);
	LCDprint("Thanks for coming");

	usleep(1000000);
	CM3_outport(LEDPort,Bin2LED[16]);



	for(j = NumSteps; j > 0; j--)
	{
		for(i=0;i<4;i++)
			{
				CM3_outport(SMPort, open_bar[i]);
				usleep(8000);
			}
	}

	usleep(5000);
	CM3_outport(LEDPort,Bin2LED[8]);
	usleep(10000000);
	
	
	for(j = NumSteps; j > 0; j--)
	{
		for(i=0;i<4;i++)
			{
				CM3_outport(SMPort, close_bar[i]);
				usleep(8000);
			}
	}

	usleep(100000);
	CM3_outport(LEDPort,Bin2LED[16]);

	if (inOut == 0){
		system("DISPLAY=:0.0 pqiv -f /tmp/entry.jpg &");		
		sleep(2);	
		initlcd();
		lcd_writecmd(0x80);
		LCDprint("Welcome to PTS");
		lcd_writecmd(0xC0);
		LCDprint("1 : Get Ticket");
		CM3_outport(LEDPort,Bin2LED[16]);
	}

	if (inOut == 1){
		system("DISPLAY=:0.0 pqiv -f /tmp/exit.jpg &");		
		sleep(2);
		initlcd();
		lcd_writecmd(0x80);
		LCDprint("Thanks for coming");
		lcd_writecmd(0xC0);
		LCDprint("2 : Proceed");
		CM3_outport(LEDPort,Bin2LED[16]);	
	}

}




static void buttonStar(){					// Hidden toggle in out button 
	
	if (inOut == 0){
		inOut =1;
		system("DISPLAY=:0.0 pqiv -f /tmp/entry.jpg &");		
		sleep(2);	
		initlcd();
		lcd_writecmd(0x80);
		LCDprint("Welcome to PTS");
		lcd_writecmd(0xC0);
		LCDprint("1 : Get Ticket");
		CM3_outport(LEDPort,Bin2LED[16]);
	}

	if (inOut == 1){
		inOut =0;
		system("DISPLAY=:0.0 pqiv -f /tmp/exit.jpg &");		
		sleep(2);
		lcd_writecmd(0x80);
		initlcd();
		LCDprint("Thanks for coming");
		lcd_writecmd(0xC0);
		LCDprint("2 : Proceed");
		CM3_outport(LEDPort,Bin2LED[16]);	
	}
    
}





static void initlcd(void)                               // function to initialise LCD
{
	usleep(20000);
	lcd_writecmd(0x30);									// Initialise
	usleep(20000);
	lcd_writecmd(0x30);   
  	usleep(20000);
	lcd_writecmd(0x30);

	lcd_writecmd(0X02);  								// return cursor to home; return display to orig position 
	lcd_writecmd(0X28);  								// 4 bit mode, 2 line,  5*7 dots
	lcd_writecmd(0X01);  								// clear screen
	lcd_writecmd(0x0c);  								// dis on cur off
	lcd_writecmd(0x14);  								// inc cur
	//lcd_writecmd();								// line 1
}

static void lcd_writecmd(char cmd)                      // function to output hex as commands to LCD display
{
	char data;

	data = (cmd & 0xf0);
	CM3_outport(LCDPort, data | 0x04);
	usleep(10);
	CM3_outport(LCDPort, data);

	usleep(200);

	data = (cmd & 0x0f) << 4;
	CM3_outport(LCDPort, data | 0x04);
	usleep(10);
	CM3_outport(LCDPort, data);

	usleep(2000);
}

static void LCDprint(char *sptr)                        // function to print string to LCD
{
	while (*sptr != 0)
	{
		int i=1;
        lcddata(*sptr);
		++sptr;
	}
}

static void lcddata(unsigned char cmd)					// fuction to print single int or char to LCD
{

	char data;

	data = (cmd & 0xf0);
	CM3_outport(LCDPort, data | 0x05);
	usleep(10);
	CM3_outport(LCDPort, data);

	usleep(200);

	data = (cmd & 0x0f) << 4;
	CM3_outport(LCDPort, data | 0x05);
	usleep(10);
	CM3_outport(LCDPort, data);

	usleep(2000);
}

//----------- Keypad Functions ----------------

unsigned char ScanKey()
{
	CM3_outport(KbdPort, Col7Lo);					// bit 7 low 
	ScanCode = CM3_inport(KbdPort);					// Read
	ScanCode |= 0x0F;								// high nybble to 1
	ScanCode &= Col7Lo;								// AND back scan value
	if (ScanCode != Col7Lo)							// in <> out get key and display
	{
	    return ProcKey();
	}

	CM3_outport(KbdPort, Col6Lo);
	ScanCode = CM3_inport(KbdPort);
	ScanCode |= 0x0F;
	ScanCode &= Col6Lo;
	if (ScanCode != Col6Lo)
	{
	    return ProcKey();
	}

	CM3_outport(KbdPort, Col5Lo);
	ScanCode = CM3_inport(KbdPort);
	ScanCode |= 0x0F;
	ScanCode &= Col5Lo;
	if (ScanCode != Col5Lo)
	{
	    return ProcKey();
	}

	CM3_outport(KbdPort, Col4Lo);
	ScanCode = CM3_inport(KbdPort);
	ScanCode |= 0x0F;
	ScanCode &= Col4Lo;
	if (ScanCode != Col4Lo)
	{
	    return ProcKey();
	}

	return 0xFF;
}

unsigned char ProcKey()
{
	unsigned char j;								// index of scan code returned
	for (j = 0 ; j <= 12 ; j++)
	if (ScanCode == ScanTable [j])					// search in table
	{
	   if(j > 9) {
		   j = j + 0x37;
	   } else {
		   j = j + 0x30;
	   }
	   return j;                           			// exit loop if found
	}

	if (j == 12)
	{
		return 0xFF;                           		// if not found, return 0xFF
	}

	return (0);
}


void buzz(void)
{
    unsigned char data[2];
    float gain = 255.0f;
    float phase = 0.0f;
    float bias = 255.0f;  // Previously 1024.0f
    float freq = 2.0f * 3.14159f / 4.0f;
    unsigned char buffer[1];
    int fileend;
    FILE *ptr;

    // Initialize device and DAC
    CM3DeviceInit();															
    CM3PortInit(5);																// initialise DAC

    ptr = fopen(AUDIOFILE, "rb");
    if (ptr == NULL) {
        perror(AUDIOFILE);
        printf("File cannot be found\n");
        return;
    }

    while ((fileend = fgetc(ptr)) != EOF) {
        fread(buffer, sizeof(buffer), 2, ptr);  // Read 2 bytes, store in buffer
        for (int i = 0; i < 1; i++) {
            CM3PortWrite(3, buffer[i]);
        }

        // You can add delay here if needed
        // usleep(100);
    }

    fclose(ptr);
}