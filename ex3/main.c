//------------------------------------------- main.c CODE STARTS ---------------------------------------------------------------------------
#include <stdio.h>
#include "NUC100Series.h"

volatile int buzz = 0;
volatile int press = 0;

#define BUZZER_BEEP_TIME 6
#define BUZZER_BEEP_DELAY 2000000

#define HXT_STATUS 1<<0 //12MHz high speed crystal clock source
#define PLL_STATUS 1<<2 //PLL clock source
#define LXT_STATUS 1<<1 //32.768kHz low speed crystal clock source
#define LIRC_STATUS 1<<3 //10kHz low speed oscillator
#define HIRC_STATUS 1<<4 //22.1184MHz high speed oscillator clock source
#define HTX_CLK 1<<0 //for enable 12MHz crystal high speed clock source at PWRCON
#define LXT_CLK 1<<1 //for enable 32.768kHz crystal low speed clock source at PWRCON
#define LIRC_CLK 1<<3 //for enable 10kHz low speed oscillator at PWRCON
#define HIRC_CLK 1<<2 //for enable 22.1184 MHz high speed oscillator at PWRCON
#define HTX_HCLK_S_VAL 0b000<<0 //choose clock source is 12MHz crystal high speed clock source at the MUX
#define LXT_HCLK_S_VAL 0b001<<0 //choose clock source is 32.768kHz crystal low speed clock source at the MUX
#define PLL_HCLK_S_VAL 0b010<<0 //choose clock source is PLL clock at the MUX
#define LIRC_HCLK_S_VAL 0b011<<0 // choose clock source is 10kHz low speed oscillator at the MUX
#define HIRC_HCLK_S_VAL 0b111<<0 //choose clock source is 22.1184 MHz high speed oscillator at the MUX
#define PLLCON_FB_DV_VAL 34 //Fout=36Mhz
#define NUC_CLK 3
#define TIMER_COUNTS 1999999
#define beep_time 6 
//7 segment LED 
#define U11_SEG 0b1000
#define U12_SEG 0b0100
#define U13_SEG 0b0010
#define U14_SEG 0b0001
//Macro define when there are keys pressed in each column
#define C3_pressed (!(PA->PIN & (1<<0)))		
#define C2_pressed (!(PA->PIN & (1<<1)))
#define C1_pressed (!(PA->PIN & (1<<2)))

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

enum Boolean{true, false};
enum CoordinateType{x, y};
enum Stage{Welcome, ChooseCoordinate, Shoot, CheckHit, GameOver};

enum Stage stage = Welcome;

enum CoordinateType currCoordinateType = x;
volatile int xCoor = 0;
volatile int yCoor = 0;
enum Boolean gameOver = false;
enum Boolean gameStart = false;
volatile int shootCounter[] = {0,0};
enum Boolean showU13 = true;

static void handleShootCounter() {
	shootCounter[1]++;
	if(shootCounter[1] == 10) {
		shootCounter[0]++;
		shootCounter[1] = 0;
	}
}

// Function to display 7 segment led
static void show7Segment(int ledNo, int number) {
	// Control which led to turn on
	PC->DOUT &= ~(0xF<<4);
	PC->DOUT |= ledNo<<4;
	//(currCoordinateType == x) ? (PC->DOUT |= U11_SEG<<4) : (PC->DOUT |= U12_SEG<<4);
	
	// Dislay number
	PE->DOUT = pattern[number];
}

static void shoot() {
	//shoot
	handleShootCounter();
	SysTick->CTRL |= 1<<0;
}

void EINT1_IRQHandler(void){
	//Do some action
	
	switch(stage) {
			case Welcome:
				stage = ChooseCoordinate;
				break;
			case ChooseCoordinate:
				/*if (xCoor != 0 && yCoor != 0) {
					SysTick->CTRL |= 1<<0;
					stage = Shoot;
				}*/
				//SysTick->CTRL |= 1<<0;
				stage = Shoot;
				break;
			case Shoot:
				SysTick->CTRL |= 1<<0;
				//if (shootCounter[0] == 1 && shootCounter[1]==7) {
					//shootCounter[0] = 0;
					//shootCounter[1] = 0;
					//game over
				//}
				break;
			case CheckHit:
				break;
			case GameOver:
				break;
			default:
				break;
	}
	
	PB->ISRC |= (1 << 15);
}

static void addCoordinateValue(int number) {
	(currCoordinateType == x) ? (show7Segment(U11_SEG, number)) : (show7Segment(U12_SEG, number));
	(currCoordinateType == x) ? (xCoor = number) : (yCoor = number);
}

static void search_col1(void){
  // Drive ROW1 output pin as LOW. Other ROW pins as HIGH
	PA->DOUT &= ~(1<<3);
	PA->DOUT |= (1<<4);
	PA->DOUT |= (1<<5);
  if (C1_pressed){
		CLK_SysTickDelay(100000);
		addCoordinateValue(1);
		return;	
	} else {		
    // Drive ROW2 output pin as LOW. Other ROW pins as HIGH
		PA->DOUT |= (1<<3);
		PA->DOUT &= ~(1<<4);
		PA->DOUT |= (1<<5);	
    if (C1_pressed){
        // If column1 is LOW, detect key press as K4 (KEY 4)
			CLK_SysTickDelay(100000);
			addCoordinateValue(4);
      return;
    } else {
			 
    // Drive ROW3 output pin as LOW. Other ROW pins as HIGH
			PA->DOUT |= (1<<3);
			PA->DOUT |= (1<<4);		
			PA->DOUT &= ~(1<<5);	
			if (C1_pressed) {
				// If column1 is LOW, detect key press as K7 (KEY 7)
				CLK_SysTickDelay(100000);
				addCoordinateValue(7);
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
		CLK_SysTickDelay(100000);
		addCoordinateValue(2);
    return;
	} else {  
    // Drive ROW2 output pin as LOW. Other ROW pins as HIGH
		PA->DOUT |= (1<<3);
		PA->DOUT &= ~(1<<4);
		PA->DOUT |= (1<<5);
    if (C2_pressed) {
      // If column2 is LOW, detect key press as K5 (KEY 5)
			CLK_SysTickDelay(100000);
			addCoordinateValue(5);
      return;
    }	else {
			// Drive ROW3 output pin as LOW. Other ROW pins as HIGH
			PA->DOUT |= (1<<3);
			PA->DOUT |= (1<<4);		
			PA->DOUT &= ~(1<<5);
			if (C2_pressed) {
        // If column3 is LOW, detect key press as K8 (KEY 8)
				CLK_SysTickDelay(100000);
				addCoordinateValue(8);
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
		CLK_SysTickDelay(100000);
		addCoordinateValue(3);
    return;
	} else {
		// Drive ROW2 output pin as LOW. Other ROW pins as HIGH
		PA->DOUT |= (1<<3);
		PA->DOUT &= ~(1<<4);
		PA->DOUT |= (1<<5);
		if (C3_pressed) {
			// If column3 is LOW, detect key press as K6 (KEY 6)
			CLK_SysTickDelay(100000);
			addCoordinateValue(6);
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

int main(void)
{ 
	//-------------System initialization start-------------------
	SYS_UnlockReg(); // Unlock protected registers

	CLK->PWRCON |= 1<<0; //Enable clock source (external high speed crystal)
	while (!(CLK->CLKSTATUS & HXT_STATUS));
	

	
	//-----------------------Configure pin----------------------------- 
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
	
	//Enable debounce for button k1 and k9
	//PA->DBEN |= (1<<0);
	//PA->DBEN |= (1<<1);
	//PA->DBEN |= (1<<2);
	//GPIO->DBNCECON &= ~(1<<4);
	//GPIO->DBNCECON |= (1<<4); //choose clk for debounce circuit is LIRC
	//GPIO->DBNCECON &= ~(0xF<<0);
	//sample interrupt is once per 256 clk which means (256)*(1/(10*1000))=0.03s
	//GPIO->DBNCECON |= (0b1000<<0); 
	
	//GPC12
	PC->PMD &= ~(0b11<<24); //clear bits
	PC->PMD |= (0b01<<24); //output
	
	//Buzzer
	PB->PMD &= ~(0b11<<22); //clear bits
	PB->PMD |= (0b01<<22); //output
	
	//7 Segment
	//GPC5 to GPC7 (to control which leds to turn on and off)
	PC->PMD &= ~(0xFF<< 8);
	PC->PMD |= 0b01010101 << 8; //Set all 4 pins as push-pull output 
	
	//COnfigure 7 segmnet LED (GPE[7:0]) (to control which number to turn on)
	PE->PMD &= ~(0xFFFF<<0);
	PE->PMD |= 0b0101010101010101<<0; //Set output mode for pins[7:0] (2 pins per mode)
	
	//----------SYSTICK CONFIGURATION------------------------
	CLK->CLKSEL0 &= ~(0b111<<3); //12Mhz
	
	//Reload value
	SysTick->LOAD=1200-1; //0.0001s
	
	//Set priority
	SCB->SHP[1] &= ~(0b11<<30);
	
	//Initial value
	SysTick->VAL=0;
	
	//Enable systick core clk 
	SysTick->CTRL &= ~(1<<2);
	
	SysTick->CTRL |= 1<<1;
	
	//Start SysTick
	//SysTick->CTRL |= 1<<0;
	
	SYS_LockReg();  // Lock protected registers
	
	while(1){
		if (stage == ChooseCoordinate) {
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
		}
		
		
	}
}

void SysTick_Handler(void) {
	PC->DOUT ^= 1<<12;
	//(showU13 == true) ? (show7Segment(U13_SEG, shootCounter[0])) : (show7Segment(U14_SEG, shootCounter[1]));
	//(showU13 == true) ? (showU13 = false) : (showU13 = true);
}
		

	
	

//------------------------------------------- main.c CODE ENDS ---------------------------------------------------------------------------
