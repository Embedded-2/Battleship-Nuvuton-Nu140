//------------------------------------------- main.c CODE STARTS ---------------------------------------------------------------------------
#include <stdio.h>
#include "NUC100Series.h"
#include "MCU_init.h"
#include "SYS_init.h"
#include "LCD.h"

#define HXT_CLK 1<<0
#define HXT_STATUS 1<<0
#define BOUNCING_DLY 100000

#define C3_pressed (!(PA->PIN & (1<<0)))		
#define C2_pressed (!(PA->PIN & (1<<1)))
#define C1_pressed (!(PA->PIN & (1<<2)))
#define U11_SEG 0b1000
#define U12_SEG 0b0100
#define U13_SEG 0b0010
#define U14_SEG 0b0001



enum Boolean{true, false};
enum CoordinateType{x, y};							
enum Stage{Welcome, ChooseCoordinate, Shoot, CheckHit, GameOver};

int pattern[] = {
               //   gedbaf_dot_c
                  0b10000010,  //Number 0          // ---a----
                  0b11101110,  //Number 1          // |      |
                  0b00000111,  //Number 2          // f      b
                  0b01000110,  //Number 3          // |      |
                  0b01101010,  //Number 4          // ---g----
                  0b01010010,  //Number 5          // |      |
                  0b00010010,  //Number 6          // e      c
                  0b11100110,  //Number 7          // |      |
                  0b00000010,  //Number 8          // ---d----
                  0b01000010,   //Number 9
                  0b11111111   //Blank LED 
                }; 
enum Stage stage;
enum CoordinateType currCoordinateType = x;
volatile int xCoordinate = 0;
volatile int yCoordinate = 0;
volatile int shootCounter[] = {0,0};
enum Boolean showU13 = true;
volatile int hit = 0;
volatile int shipCoordinates[8][8] = {{0, 0, 0, 0, 0, 0, 0, 0},
																			{0, 1, 1, 0, 0, 0, 0, 0},
																			{0, 0, 0, 0, 0, 0, 1, 0},
																			{0, 0, 0, 0, 0, 0, 1, 0}, 
																			{0, 0, 1, 1, 0, 0, 0, 0},
																			{0, 0, 0, 0, 0, 0, 0, 0},
																			{1, 1, 0, 0, 1, 0, 0, 0},
																			{0, 0, 0, 0, 1, 0, 0, 0}};

volatile int hitCoordinates[8][8] = {{0, 0, 0, 0, 0, 0, 0, 0},
																			{0, 0, 0, 0, 0, 0, 0, 0},
																			{0, 0, 0, 0, 0, 0, 0, 0},
																			{0, 0, 0, 0, 0, 0, 0, 0}, 
																			{0, 0, 0, 0, 0, 0, 0, 0},
																			{0, 0, 0, 0, 0, 0, 0, 0},
																			{0, 0, 0, 0, 0, 0, 0, 0},
																			{0, 0, 0, 0, 0, 0, 0, 0}};

void System_Config(void);
void SPI3_Config(void);
void Input_Config(void);
void Output_Config(void);

void LCD_start(void);
void LCD_command(unsigned char temp);
void LCD_data(unsigned char temp);
void LCD_clear(void);
void LCD_SetAddress(uint8_t PageAddr, uint8_t ColumnAddr);
static void show7Segment(int ledNo, int number);
static void printLCD(char text[]);
static void search_col1(void);
static void search_col2(void);
static void search_col3(void);
static void shoot();
																			
static void turnOffShootCounter() {
	PC->DOUT &= ~(0xF<<4); //reset 7 segment
	TIMER1->TCSR &= ~(1 << 30);// stop timer 1
}

static void resetCoordinate() {
	xCoordinate = 0; //reset coordinate value
	yCoordinate = 0;
	currCoordinateType = x;// change coordinate type back to x
}

static void resetGame() {
	//reset hit map
	for (int column = 0; column<8;column++) {
		for(int row = 0; row<8;row++) {
			hitCoordinates[column][row] = 0;
		}
	}
	hit = 0;
	shootCounter[0] = 0;
	shootCounter[1] = 0;
}

static void drawMap() {
	LCD_clear(); 
	int16_t x = 0;
	int16_t y = 0;
	for (int column = 0; column<8;column++) {
		for(int row = 0; row<8;row++) {
			if (hitCoordinates[column][row] == 1) {
				printS_5x7(x, y, "x");
			} else {
				printS_5x7(x, y, "-");
			}
			x+=10;
		}
		y+=8;
		x=0;
	}
	CLK_SysTickDelay(BOUNCING_DLY/2);
}

static void checkAccuracy() {
	if (shipCoordinates[yCoordinate-1][xCoordinate-1] == 1) {
		hitCoordinates[yCoordinate-1][xCoordinate-1] = 1;
		hit++;
	}
}

void EINT1_IRQHandler(void){
	//Do some action
	
	switch(stage) {
			case Welcome:
				resetGame();
				stage = ChooseCoordinate;
				break;
			case ChooseCoordinate:
				if (xCoordinate != 0 && yCoordinate != 0) {
					shoot();
					stage = Shoot;
				}
				break;
			case Shoot:
				if((shootCounter[0] == 1 && shootCounter[1] == 6) || hit == 10) {
					stage = GameOver;
				} else {
					stage = ChooseCoordinate;	
				}
				turnOffShootCounter();
				resetCoordinate();
				break;
			case GameOver:
				stage = Welcome;
				break;
			default:
				break;
	}
	
	PB->ISRC |= (1 << 15);
}



int main(void)
{
	//uint8_t pressed_key = 0;
	Output_Config();
	Input_Config();

	//--------------------------------
	//System initialization
	//--------------------------------
	System_Config();

	//--------------------------------
	//SPI3 initialization
	//--------------------------------
	SPI3_Config();

	//--------------------------------
	//LCD initialization
	//--------------------------------
	LCD_start();
	LCD_clear();

	while (1) {
		switch(stage) {
			case Welcome:
				printLCD("Welcome to the game");
				break;
			case ChooseCoordinate:
				drawMap();
				PA->DOUT &= ~(1<<3);
				PA->DOUT &= ~(1<<4);
				PA->DOUT &= ~(1<<5);
				if(C1_pressed){
					search_col1();
				}	else if(C2_pressed){
					search_col2();
				} else if(C3_pressed){
					search_col3();
				} else {}
				break;
			case Shoot:
				drawMap();
				break;
			case GameOver:
				printLCD("Game Over");
				break;
			default:
				break;
		}
	}
}


//------------------------------------------------------------------------------------------------------------------------------------
// Functions definition
//------------------------------------------------------------------------------------------------------------------------------------

void Output_Config(void) {
	PC->PMD &= ~(0b11<<24); // Clear bits
	PC->PMD |= (0b01<<24); // output
}

void Input_Config(void) {
	//Input GPB15 
	PB->PMD &= ~(0b11<<30); // input
	PB->IEN |= (1 << 31); //Trigger on rising edge
	//NVIC interrupt configuration for GPIO-B15 interrupt source
	NVIC->ISER[0] |= 1<<3;
	NVIC->IP[0] &= (~(0b11<<30));
	
	PB->DBEN |= (1<<15);
	
	GPIO->DBNCECON &= ~(1<<4);
	GPIO->DBNCECON |= (1<<4); //choose clk for debounce circuit is LIRC
	
	GPIO->DBNCECON &= ~(0xF<<0);
	GPIO->DBNCECON |= (0b1000<<0); //sample interrupt is once per 256 clk
	
	//Configure GPIO for Key Matrix
	//Rows - outputs
	PA->PMD &= (~(0b11<< 6));
	PA->PMD |= (0b01 << 6);    
	PA->PMD &= (~(0b11<< 8));
  PA->PMD |= (0b01 << 8);  		
	PA->PMD &= (~(0b11<< 10));
  PA->PMD |= (0b01 << 10);  
}

void System_Config(void) {
	SYS_UnlockReg(); // Unlock protected registers
	CLK->PWRCON |= HXT_CLK; //Enable clock source (external high speed crystal)
	while (!(CLK->CLKSTATUS & HXT_STATUS));

	//PLL configuration starts
	CLK->PLLCON &= ~(1 << 19); //0: PLL input is HXT
	CLK->PLLCON &= ~(1 << 16); //PLL in normal mode
	CLK->PLLCON &= (~(0x01FF << 0));
	CLK->PLLCON |= 48;
	CLK->PLLCON &= ~(1 << 18); //0: enable PLLOUT
	while (!(CLK->CLKSTATUS & (0x01ul << 2)));
	//PLL configuration ends

	//clock source selection
	CLK->CLKSEL0 &= (~(0x07 << 0));
	CLK->CLKSEL0 |= (0x02 << 0);
	//clock frequency division
	CLK->CLKDIV &= (~0x0F << 0);

	//enable clock of SPI3
	CLK->APBCLK |= 1 << 15;
	
	//--------------Timer 1 configuration-------------------------
	CLK->CLKSEL1 &= ~(0b111 << 12);//12MHz
	CLK->APBCLK |= (1 << 3); //Enable clock source

	TIMER1->TCSR &= ~(0xFF << 0);//prescaler = 0
	
	//reset Timer 1
	TIMER1->TCSR |= (1 << 26);
	
	//define Timer 1 operation mode
	TIMER1->TCSR &= ~(0b11 << 27);
	TIMER1->TCSR |= (0b01 << 27); //periodic mode
	TIMER1->TCSR &= ~(1 << 24);
	
	//TDR to be updated continuously while timer counter is counting
	TIMER1->TCSR |= (1 << 16);
	
	//TImer interrupt
	//Trigger compare match flag
	TIMER1->TCSR |= (1 << 29);
	NVIC->ISER[0] |= 1<<9; //Timer 1 is given number 9 in the vecter table (section 5.2.7)
	NVIC->IP[2] &= ~(0b11<<14); //Set priority level
	
	TIMER1->TCMPR = 1200-1; // 0.0001/(1/12MHz)
	
	//TIMER1->TCSR |= (1 << 30);

	SYS_LockReg();  // Lock protected registers
}

void SPI3_Config(void) {
	SYS->GPD_MFP |= 1 << 11; //1: PD11 is configured for alternative function
	SYS->GPD_MFP |= 1 << 9; //1: PD9 is configured for alternative function
	SYS->GPD_MFP |= 1 << 8; //1: PD8 is configured for alternative function

	SPI3->CNTRL &= ~(1 << 23); //0: disable variable clock feature
	SPI3->CNTRL &= ~(1 << 22); //0: disable two bits transfer mode
	SPI3->CNTRL &= ~(1 << 18); //0: select Master mode
	SPI3->CNTRL &= ~(1 << 17); //0: disable SPI interrupt
	SPI3->CNTRL |= 1 << 11; //1: SPI clock idle high
	SPI3->CNTRL &= ~(1 << 10); //0: MSB is sent first
	SPI3->CNTRL &= ~(3 << 8); //00: one transmit/receive word will be executed in one data transfer

	SPI3->CNTRL &= ~(31 << 3); //Transmit/Receive bit length
	SPI3->CNTRL |= 9 << 3;     //9: 9 bits transmitted/received per data transfer

	SPI3->CNTRL |= (1 << 2);  //1: Transmit at negative edge of SPI CLK
	SPI3->DIVIDER = 0; // SPI clock divider. SPI clock = HCLK / ((DIVIDER+1)*2). HCLK = 50 MHz
}

void LCD_start(void)
{
	LCD_command(0xE2); // Set system reset
	LCD_command(0xA1); // Set Frame rate 100 fps
	LCD_command(0xEB); // Set LCD bias ratio E8~EB for 6~9 (min~max)
	LCD_command(0x81); // Set V BIAS potentiometer
	LCD_command(0xA0); // Set V BIAS potentiometer: A0 ()
	LCD_command(0xC0);
	LCD_command(0xAF); // Set Display Enable
}

void LCD_command(unsigned char temp)
{
	SPI3->SSR |= 1 << 0;
	SPI3->TX[0] = temp;
	SPI3->CNTRL |= 1 << 0;
	while (SPI3->CNTRL & (1 << 0));
	SPI3->SSR &= ~(1 << 0);
}

void LCD_data(unsigned char temp)
{
	SPI3->SSR |= 1 << 0;
	SPI3->TX[0] = 0x0100 + temp;
	SPI3->CNTRL |= 1 << 0;
	while (SPI3->CNTRL & (1 << 0));
	SPI3->SSR &= ~(1 << 0);
}

void LCD_clear(void)
{
	int16_t i;
	LCD_SetAddress(0x0, 0x0);
	for (i = 0; i < 132 * 8; i++)
	{
		LCD_data(0x00);
	}
}

void LCD_SetAddress(uint8_t PageAddr, uint8_t ColumnAddr)
{
	LCD_command(0xB0 | PageAddr);
	LCD_command(0x10 | (ColumnAddr >> 4) & 0xF);
	LCD_command(0x00 | (ColumnAddr & 0xF));
}

void TMR1_IRQHandler(void)
{
	//PC->DOUT ^= 1<<12;
	(showU13 == true) ? (show7Segment(U13_SEG, shootCounter[0])) : (show7Segment(U14_SEG, shootCounter[1]));
	(showU13 == true) ? (showU13 = false) : (showU13 = true);
	TIMER1->TISR |= (1<<0); 
}

//------------------------------------------Functions-----------------------------------------------------------
static void show7Segment(int ledNo, int number) {
	// Control which led to turn on
	PC->DOUT &= ~(0xF<<4);
	PC->DOUT |= ledNo<<4;

	PE->DOUT = pattern[number];
}

static void printLCD(char text[]) {
	CLK_SysTickDelay(BOUNCING_DLY);
	LCD_clear(); 
	print_Line(0, text);
}

static void changeCoordinateValue(int number) {
	CLK_SysTickDelay(BOUNCING_DLY);
	(currCoordinateType == x) ? (show7Segment(U11_SEG, number)) : (show7Segment(U12_SEG, number));
	(currCoordinateType == x) ? (xCoordinate = number) : (yCoordinate = number);
}

static void search_col1(void){
  // Drive ROW1 output pin as LOW. Other ROW pins as HIGH
	PA->DOUT &= ~(1<<3);
	PA->DOUT |= (1<<4);
	PA->DOUT |= (1<<5);
  if (C1_pressed){
		changeCoordinateValue(1);
		return;	
	} else {		
    // Drive ROW2 output pin as LOW. Other ROW pins as HIGH
		PA->DOUT |= (1<<3);
		PA->DOUT &= ~(1<<4);
		PA->DOUT |= (1<<5);	
    if (C1_pressed){
        // If column1 is LOW, detect key press as K4 (KEY 4)
			changeCoordinateValue(4);
      return;
    } else {
			 
    // Drive ROW3 output pin as LOW. Other ROW pins as HIGH
			PA->DOUT |= (1<<3);
			PA->DOUT |= (1<<4);		
			PA->DOUT &= ~(1<<5);	
			if (C1_pressed) {
				// If column1 is LOW, detect key press as K7 (KEY 7)
				changeCoordinateValue(7);
				return;
			} else
				return;
		}
	}
}		

static void search_col2(void) {
    // Drive ROW1 output pin as LOW. Other ROW pins as HIGH
	PA->DOUT &= ~(1<<3);
	PA->DOUT |= (1<<4);
	PA->DOUT |= (1<<5);
	if (C2_pressed) {
    // If column2 is LOW, detect key press as K2 (KEY 2)
		changeCoordinateValue(2);
    return;
	} else {  
    // Drive ROW2 output pin as LOW. Other ROW pins as HIGH
		PA->DOUT |= (1<<3);
		PA->DOUT &= ~(1<<4);
		PA->DOUT |= (1<<5);
    if (C2_pressed) {
      // If column2 is LOW, detect key press as K5 (KEY 5)
			changeCoordinateValue(5);
      return;
    }	else {
			// Drive ROW3 output pin as LOW. Other ROW pins as HIGH
			PA->DOUT |= (1<<3);
			PA->DOUT |= (1<<4);		
			PA->DOUT &= ~(1<<5);
			if (C2_pressed) {
        // If column3 is LOW, detect key press as K8 (KEY 8)
				changeCoordinateValue(8);
        return;
			} else	
				return;
		}
	}
}	

static void search_col3(void) {
    // Drive ROW1 output pin as LOW. Other ROW pins as HIGH
	PA->DOUT &= ~(1<<3);
	PA->DOUT |= (1<<4);
	PA->DOUT |= (1<<5);
     
	if (C3_pressed) {
    // If column3 is LOW, detect key press as K3 (KEY 3)
		changeCoordinateValue(3);
    return;
	} else {
		// Drive ROW2 output pin as LOW. Other ROW pins as HIGH
		PA->DOUT |= (1<<3);
		PA->DOUT &= ~(1<<4);
		PA->DOUT |= (1<<5);
		if (C3_pressed) {
			// If column3 is LOW, detect key press as K6 (KEY 6)
			changeCoordinateValue(6);
			return;
		} else {
			// Drive ROW3 output pin as LOW. Other ROW pins as HIGH
			PA->DOUT |= (1<<3);
			PA->DOUT |= (1<<4);		
			PA->DOUT &= ~(1<<5);		
			if (C3_pressed) {
				// If column3 is LOW, detect key press as K9 (KEY 9)
				CLK_SysTickDelay(100000);
				(currCoordinateType == x) ? (currCoordinateType = y) : (currCoordinateType = x); //change coordinate type
				return;
			} else
				return;
		}
	}
}	

static void handleShootCounter() {
	shootCounter[1]++;
	if(shootCounter[1] == 10) {
		shootCounter[0]++;
		shootCounter[1] = 0;
	}
}

static void shoot() {
	handleShootCounter();
	checkAccuracy();
	TIMER1->TCSR |= (1 << 30);
}



//------------------------------------------- main.c CODE ENDS ---------------------------------------------------------------------------
