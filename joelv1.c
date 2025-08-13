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

// Custom characters for car animation
const unsigned char car_chars[8][8] = {
    // Character 0: Car body (left part)
    {0x00, 0x0F, 0x1F, 0x1B, 0x1F, 0x0F, 0x00, 0x00},
    
    // Character 1: Car body (right part) 
    {0x00, 0x1E, 0x1F, 0x1B, 0x1F, 0x1E, 0x00, 0x00},
    
    // Character 2: Trail particle (large)
    {0x00, 0x00, 0x0E, 0x1F, 0x1F, 0x0E, 0x00, 0x00},
    
    // Character 3: Trail particle (medium)
    {0x00, 0x00, 0x00, 0x0E, 0x0E, 0x00, 0x00, 0x00},
    
    // Character 4: Trail particle (small)
    {0x00, 0x00, 0x00, 0x04, 0x04, 0x00, 0x00, 0x00},
    
    // Character 5: Empty space
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    
    // Character 6: Car body (full single char - for tight spaces)
    {0x00, 0x0F, 0x1F, 0x1B, 0x1F, 0x0F, 0x00, 0x00},
    
    // Character 7: Road/ground
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x00}
};

unsigned char ScanCode;

static void initlcd();
static void lcd_writecmd(char cmd);
static void LCDprint(char *sptr);
static void lcddata(unsigned char cmd);
static void buzz();
static void create_custom_chars();
static void animate_car_entering();
static void animate_car_exiting();
static void clear_animation_line();

static char detect();
static void buttonOne();
static void buttonTwo();
static void buttonFour();
static void buttonStar();
static void buttonHex();

unsigned char ScanKey();
unsigned char ProcKey();

int inOut = 0; // 0 = in ; 1 = out

int main(int argc, char *argv[])
{    
	CM3DeviceInit();
	system("killall pqiv");
	initlcd();
	create_custom_chars(); // Initialize custom characters
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
    do { usleep(30000); } while (ScanKey() != 0xFF);

    return k;
}

static void buttonOne(){
	int i,j;
	char line1[17], line2[17];

    srand(time(NULL));
    int ticketNum = rand() % 900000 + 100000;

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
	usleep(2000000); // Show ticket info for 2 seconds

	// Clear screen and show car entering animation
	lcd_writecmd(0x01);
	lcd_writecmd(0x80);
	LCDprint("Car Entering...");
	animate_car_entering();

	// Open barrier
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
	
	// Close barrier
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
	create_custom_chars(); // Reinitialize custom characters after LCD reset
	sleep(1);
	lcd_writecmd(0x80);
	LCDprint("Welcome to PTS");
	lcd_writecmd(0xC0);
	LCDprint("1 : Get Ticket");
}

static void buttonTwo(){
	system("DISPLAY=:0.0 pqiv -f /tmp/input.jpg &");
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
	create_custom_chars(); // Reinitialize custom characters

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
	system("DISPLAY=:0.0 pqiv -f /tmp/leave.jpg &");
    sleep(2);
	
	// Show car exiting animation
	initlcd();
	create_custom_chars();
	lcd_writecmd(0x80);
	LCDprint("Car Exiting...");
	animate_car_exiting();

	usleep(1000000);
	CM3_outport(LEDPort,Bin2LED[16]);

	// Open barrier
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
	
	// Close barrier
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
		create_custom_chars();
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
		create_custom_chars();
		lcd_writecmd(0x80);
		LCDprint("Thanks for coming");
		lcd_writecmd(0xC0);
		LCDprint("2 : Proceed");
		CM3_outport(LEDPort,Bin2LED[16]);	
	}
}

static void buttonStar(){
	if (inOut == 0){
		inOut = 1;
		system("DISPLAY=:0.0 pqiv -f /tmp/exit.jpg &");		
		sleep(2);	
		initlcd();
		create_custom_chars();
		lcd_writecmd(0x80);
		LCDprint("Thanks for coming");
		lcd_writecmd(0xC0);
		LCDprint("2 : Proceed");
		CM3_outport(LEDPort,Bin2LED[16]);	
	}
	else if (inOut == 1){
		inOut = 0;
		system("DISPLAY=:0.0 pqiv -f /tmp/entry.jpg &");		
		sleep(2);
		initlcd();
		create_custom_chars();
		lcd_writecmd(0x80);
		LCDprint("Welcome to PTS");
		lcd_writecmd(0xC0);
		LCDprint("1 : Get Ticket");
		CM3_outport(LEDPort,Bin2LED[16]);
	}
}

// Function to create custom characters in LCD memory
static void create_custom_chars(){
    int i, j;
    
    for(i = 0; i < 8; i++){
        // Set CGRAM address (0x40 + character_number * 8)
        lcd_writecmd(0x40 + (i * 8));
        
        // Write 8 bytes for each custom character
        for(j = 0; j < 8; j++){
            lcddata(car_chars[i][j]);
        }
    }
    
    // Return to DDRAM
    lcd_writecmd(0x80);
}

// Animation for car entering (left to right)
static void animate_car_entering(){
    int pos;
    
    // Move to second line for animation
    lcd_writecmd(0xC0);
    
    // Clear the line first
    for(int i = 0; i < 16; i++){
        lcddata(' ');
    }
    
    // Animate car moving from left to right
    for(pos = 0; pos < 14; pos++){
        // Move cursor to animation line
        lcd_writecmd(0xC0);
        
        // Clear previous frame
        for(int i = 0; i < 16; i++){
            lcddata(' ');
        }
        
        // Move cursor to current car position
        lcd_writecmd(0xC0 + pos);
        
        // Draw car (using custom character 6 for single character car)
        lcddata(6); // Custom character 6 (car)
        
        // Draw trail particles behind the car
        if(pos > 0){
            lcd_writecmd(0xC0 + pos - 1);
            lcddata(2); // Large particle
        }
        if(pos > 1){
            lcd_writecmd(0xC0 + pos - 2);
            lcddata(3); // Medium particle
        }
        if(pos > 2){
            lcd_writecmd(0xC0 + pos - 3);
            lcddata(4); // Small particle
        }
        
        usleep(200000); // Delay for animation speed
    }
    
    // Clear animation line
    clear_animation_line();
}

// Animation for car exiting (right to left)
static void animate_car_exiting(){
    int pos;
    
    // Move to second line for animation
    lcd_writecmd(0xC0);
    
    // Clear the line first
    for(int i = 0; i < 16; i++){
        lcddata(' ');
    }
    
    // Animate car moving from right to left
    for(pos = 15; pos >= 1; pos--){
        // Move cursor to animation line
        lcd_writecmd(0xC0);
        
        // Clear previous frame
        for(int i = 0; i < 16; i++){
            lcddata(' ');
        }
        
        // Move cursor to current car position
        lcd_writecmd(0xC0 + pos);
        
        // Draw car (using custom character 6)
        lcddata(6); // Custom character 6 (car)
        
        // Draw trail particles ahead of the car (since it's moving left)
        if(pos < 15){
            lcd_writecmd(0xC0 + pos + 1);
            lcddata(2); // Large particle
        }
        if(pos < 14){
            lcd_writecmd(0xC0 + pos + 2);
            lcddata(3); // Medium particle
        }
        if(pos < 13){
            lcd_writecmd(0xC0 + pos + 3);
            lcddata(4); // Small particle
        }
        
        usleep(200000); // Delay for animation speed
    }
    
    // Clear animation line
    clear_animation_line();
}

// Function to clear the animation line
static void clear_animation_line(){
    lcd_writecmd(0xC0); // Move to second line
    for(int i = 0; i < 16; i++){
        lcddata(' ');
    }
}

static void initlcd(void)
{
	usleep(20000);
	lcd_writecmd(0x30);
	usleep(20000);
	lcd_writecmd(0x30);   
  	usleep(20000);
	lcd_writecmd(0x30);

	lcd_writecmd(0X02);
	lcd_writecmd(0X28);
	lcd_writecmd(0X01);
	lcd_writecmd(0x0c);
	lcd_writecmd(0x14);
}

static void lcd_writecmd(char cmd)
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

static void LCDprint(char *sptr)
{
	while (*sptr != 0)
	{
		int i=1;
        lcddata(*sptr);
		++sptr;
	}
}

static void lcddata(unsigned char cmd)
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

unsigned char ScanKey()
{
	CM3_outport(KbdPort, Col7Lo);
	ScanCode = CM3_inport(KbdPort);
	ScanCode |= 0x0F;
	ScanCode &= Col7Lo;
	if (ScanCode != Col7Lo)
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
	unsigned char j;
	for (j = 0 ; j <= 12 ; j++)
	if (ScanCode == ScanTable [j])
	{
	   if(j > 9) {
		   j = j + 0x37;
	   } else {
		   j = j + 0x30;
	   }
	   return j;
	}

	if (j == 12)
	{
		return 0xFF;
	}

	return (0);
}

void buzz(void)
{
    unsigned char data[2];
    float gain = 255.0f;
    float phase = 0.0f;
    float bias = 255.0f;
    float freq = 2.0f * 3.14159f / 4.0f;
    unsigned char buffer[1];
    int fileend;
    FILE *ptr;

    CM3DeviceInit();
    CM3PortInit(5);

    ptr = fopen(AUDIOFILE, "rb");
    if (ptr == NULL) {
        perror(AUDIOFILE);
        printf("File cannot be found\n");
        return;
    }

    while ((fileend = fgetc(ptr)) != EOF) {
        fread(buffer, sizeof(buffer), 2, ptr);
        for (int i = 0; i < 1; i++) {
            CM3PortWrite(3, buffer[i]);
        }
    }

    fclose(ptr);
}