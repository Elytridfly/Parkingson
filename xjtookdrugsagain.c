#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include "library.h"
#include "wiringPi.h"
#include <signal.h>
#include <time.h>
#include <pthread.h>


#define LEDPort 0x3A
#define KbdPort 0x3C
#define LCDPort 0x3B
#define SMPort  0x39

#define Col7Lo 0xF7
#define Col6Lo 0xFB
#define Col5Lo 0xFD
#define Col4Lo 0xFE

#define AUDIOFILE "/tmp/jingle.raw"
#define CSVFILE "/tmp/tickets.csv"

pthread_t dac_id;
pthread_mutex_t daclock = PTHREAD_MUTEX_INITIALIZER;  
volatile bool stop_audio = false;

unsigned char ScanCode;		

int open_bar[4] = {0x01,0x02,0x04,0x08}; //{0x08, 0x04, 0x02, 0x01};
int close_bar[4] = {0x08, 0x04, 0x02, 0x01};//{0x01, 0x02, 0x04, 0x08};
int NumSteps = 12;

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
    
    // Character 6: Car (improved design)
    {0x00, 0x04, 0x0E, 0x1F, 0x1B, 0x1F, 0x0A, 0x00},
    
    // Character 7: Road/ground
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x00}
};

unsigned char ScanCode;

static void initlcd();
static void lcd_writecmd(char cmd);
static void LCDprint(char *sptr);
static void lcddata(unsigned char cmd);
static void create_custom_chars();
static void animate_car_entering();
static void animate_car_exiting();
static void clear_animation_line();
static void write_ticket_to_csv(int ticket, char* action);
static void delete_ticket_from_csv(int ticket);
static int find_ticket_in_csv(int ticket);
static int get_ticket_input();
static void start_buzz();
static void stop_buzz();
void* buzz(void* arg);

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
    int mutex_check_dac;
	pthread_mutex_lock(&daclock);

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

    snprintf(line1, sizeof(line1), "Ticket: %06d", ticketNum);
    snprintf(line2, sizeof(line2), "Time: %s",buf);
	
	// Write ticket to CSV
	write_ticket_to_csv(ticketNum, "ENTRY");
	
	start_buzz();

	system("DISPLAY=:0.0 pqiv -f /tmp/printing.jpg &");
	usleep(5000);
    lcd_writecmd(0x01);
    lcd_writecmd(0x80);
    LCDprint(line1);
    lcd_writecmd(0xC0);
    LCDprint(line2);
	usleep(2000000); // Show ticket info for 2 seconds

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
	lcd_writecmd(0x01);
	lcd_writecmd(0x80);
	LCDprint("Car Entering...");
	animate_car_entering();
	usleep(5000000);
	
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
	initlcd();
    lcd_writecmd(0x01);
    lcd_writecmd(0x80);
    LCDprint("# : Continue");
	lcd_writecmd(0xC0);
    LCDprint("* : Back ");

	int enteredTicket = get_ticket_input();
	
	if(enteredTicket == -1){
		return;
	}
	if(find_ticket_in_csv(enteredTicket)){
		delete_ticket_from_csv(enteredTicket);
		
		lcd_writecmd(0x01);
		lcd_writecmd(0x80);
		LCDprint("Payment received");
		lcd_writecmd(0xC0);
		LCDprint("Ticket Resolved");
		usleep(2000000);
		lcd_writecmd(0x01);
		lcd_writecmd(0x80);
		system("DISPLAY=:0.0 pqiv -f /tmp/process.jpg &");		
		LCDprint("#: Open Barricade");
		
	} else {
		initlcd();
		lcd_writecmd(0x01);
		lcd_writecmd(0x80);
		LCDprint("Ticket not found");
		enteredTicket = 0;
		usleep(300000);
		initlcd();
		lcd_writecmd(0x01);
		lcd_writecmd(0x80);
		LCDprint("Please try again");
		lcd_writecmd(0xC0);
		LCDprint("2 : Proceed");
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

	
	// Open barrier
	for(j = NumSteps; j > 0; j--)
	{
		for(i=0;i<4;i++)
			{
				CM3_outport(SMPort, open_bar[i]);
				usleep(8000);
			}
	}
	
	// Show car exiting animation
	initlcd();
	create_custom_chars();
	lcd_writecmd(0x80);
	LCDprint("Car Exiting...");
	animate_car_exiting();

	usleep(1000000);
	CM3_outport(LEDPort,Bin2LED[16]);


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
		system("killall pqiv");
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
		system("killall pqiv");
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
		system("killall pqiv");
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
		system("killall pqiv");
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
        lcd_writecmd(0x40 + (i * 8));
        
        for(j = 0; j < 8; j++){
            lcddata(car_chars[i][j]);
        }
		usleep(10000);
    }
    lcd_writecmd(0x80);
}

// Animation for car entering (left to right)
static void animate_car_entering(){
    int pos;
    
    lcd_writecmd(0xC0);
    
    for(int i = 0; i < 16; i++){
        lcddata(' ');
    }
    
    for(pos = 0; pos < 14; pos++){
        lcd_writecmd(0xC0);
        for(int i = 0; i < 16; i++){
            lcddata(' ');
        }
        lcd_writecmd(0xC0 + pos);
        lcddata(6); // Custom character 6 (car)
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
        
        usleep(50000); // Delay for animation speed
    }
    
    clear_animation_line();
}

// Animation for car exiting (right to left)
static void animate_car_exiting(){
    int pos;
    
    lcd_writecmd(0xC0);
    
    for(int i = 0; i < 16; i++){
        lcddata(' ');
    }
    
    for(pos = 15; pos >= 1; pos--){
        lcd_writecmd(0xC0);
        for(int i = 0; i < 16; i++){
            lcddata(' ');
        }
        lcd_writecmd(0xC0 + pos);
        lcddata(6); // Custom character 6 (car)
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
        
        usleep(50000); // Delay for animation speed
    }
    
    clear_animation_line();
}

// Function to clear the animation line
static void clear_animation_line(){
    lcd_writecmd(0xC0); // Move to second line
    for(int i = 0; i < 16; i++){
        lcddata(' ');
    }
}

// Function to find ticket in CSV file
static int find_ticket_in_csv(int ticket){
    FILE *file = fopen(CSVFILE, "r");
    if(file == NULL){
		lcd_writecmd(0xC0);
		usleep(2000000);
        return 0; // File doesn't exist, ticket not found
		
    }
    
    char line[100];
    int found_ticket;
    char action[20];
    
    while(fgets(line, sizeof(line), file)){
        if(sscanf(line, "%d", &found_ticket) == 1){
            if(found_ticket == ticket){
                fclose(file);
                return 1; // Ticket found and is active (ENTRY status)
            }
        }
    }
    fclose(file);
    return 0; // Ticket not found
}

// Function to delete ticket from CSV file
static void delete_ticket_from_csv(int ticket){
    FILE *file = fopen(CSVFILE, "r");
    if(file == NULL){
        return;
    }
    
    FILE *temp = fopen("/tmp/temp_tickets.csv", "w");
    if(temp == NULL){
        fclose(file);
        return;
    }
    
    char line[100];
    int found_ticket;
    char action[20];
    char timestamp[30];
    
    while(fgets(line, sizeof(line), file)){
        if(sscanf(line, "%d", &found_ticket)==1){
            if(!(found_ticket == ticket)){
                fputs(line, temp);
            }
        }
    }
    
    fclose(file);
    fclose(temp);
    
    system("mv /tmp/temp_tickets.csv " CSVFILE);
}

static void write_ticket_to_csv(int ticket, char* action){
    FILE *file = fopen(CSVFILE, "a");
    if(file == NULL){
        return; // Fail silently
    }
    
    time_t now = time(NULL);
    struct tm *lt = localtime(&now);
    char timestamp[20];
    
    if(lt){
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", lt);
    }
    
    fprintf(file, "%06d,%s,%s\n", ticket, action, timestamp);
    fclose(file);
}

// Function to get 6-digit ticket input from keypad
static int get_ticket_input(){
    char input_str[7] = {0};
    int digit_count = 0;
    unsigned char key;
    
    while(digit_count < 6){
        key = detect();
        
        if(key >= '0' && key <= '9'){
            input_str[digit_count] = key;
            lcddata('*');
            digit_count++;
        }
        else if(key == 'A' && digit_count > 0){
            digit_count--;
            input_str[digit_count] = '\0';
            lcd_writecmd(0xC0 + 9 + digit_count);
            lcddata(' ');
            lcd_writecmd(0xC0 + 9 + digit_count);
        }
        else if(key == 'B'){
            return -1; // Cancel
        }
    }
    
    while(1){
        key = detect();
        if(key == 'B'){
            return atoi(input_str);
        }
        else if(key == 'A'){
            return -1; // Cancel
        }
    }
}

void* buzz(void* arg)
{
    dac_id = pthread_self();
    unsigned char buffer[1];
    int fileend;
    FILE *ptr;
    
    pthread_mutex_lock(&daclock);
    stop_audio = false;

    CM3DeviceInit();
    CM3PortInit(5);

    ptr = fopen(AUDIOFILE, "rb");
    if (ptr == NULL) {
        perror(AUDIOFILE);
        printf("File cannot be found\n");
        pthread_mutex_unlock(&daclock);
        return NULL;
    }

    while ((fileend = fgetc(ptr)) != EOF && !stop_audio) {
        if (fread(buffer, sizeof(buffer), 1, ptr) > 0) {
            CM3PortWrite(3, buffer[0]);
        }
    }

    fclose(ptr);
    pthread_mutex_unlock(&daclock);
    return NULL;
}

static void start_buzz()
{
    if (pthread_create(&dac_id, NULL, buzz, NULL) != 0) {
        perror("Failed to create buzz thread");
    }
}

static void stop_buzz()
{
    stop_audio = true;
    pthread_join(dac_id, NULL);
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