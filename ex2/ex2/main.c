//------------------------------------------- main.c CODE STARTS ---------------------------------------------------------------------------
#include <stdio.h>
#include "NUC100Series.h"
#include "MCU_init.h"
#include "SYS_init.h"
#include "LCD.h"

#define HXT_CLK 1<<0
#define HXT_STATUS 1<<0
#define BOUNCING_DLY 100000

enum Boolean {
	true,false
};

volatile char word[100] = "";
volatile char temp[100] = "";
volatile char character;
volatile int wordCount = 0;
volatile char showWord[100] = "";
//enum Boolean showWord = false;
																			
//--------------------functions declaration-------------------------------------
void System_Config(void);
void SPI3_Config(void);
void UART0_Config(void);

void LCD_start(void);
void LCD_command(unsigned char temp);
void LCD_data(unsigned char temp);
void LCD_clear(void);
void LCD_SetAddress(uint8_t PageAddr, uint8_t ColumnAddr);

int checkLatitude(char word[]);
int checkLongitude(char word[]);

int main(void)
{
	PC->PMD &= ~(0b11<<24);
	PC->PMD |= (0b01<<24);
	PC->PMD &= ~(0b11<<26);
	PC->PMD |= (0b01<<26);
	//--------------------------------
	//System initialization
	//--------------------------------
	System_Config();
	UART0_Config();

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
		
	}
}


//------------------------------------------------------------------------------------------------------------------------------------
// Functions definition
//------------------------------------------------------------------------------------------------------------------------------------
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
	
	//--------------Timer 0 configuration-------------------------
	CLK->CLKSEL1 &= ~(0b111 << 12);//12MHz
	CLK->APBCLK |= (1 << 2); //Enable clock source

	TIMER0->TCSR &= ~(0xFF << 0);//prescaler = 0
	
	//reset Timer 0
	TIMER0->TCSR |= (1 << 26);
	
	//define Timer 0 operation mode
	TIMER0->TCSR &= ~(0b11 << 27);
	TIMER0->TCSR |= (0b01 << 27); //periodic mode
	TIMER0->TCSR &= ~(1 << 24);
	
	//TDR to be updated continuously while timer counter is counting
	TIMER0->TCSR |= (1 << 16);
	
	//TImer interrupt
	//Trigger compare match flag
	TIMER0->TCSR |= (1 << 29);
	NVIC->ISER[0] |= 1<<8; //Timer 0 is given number 8 in the vecter table (section 5.2.7)
	NVIC->IP[2] &= ~(0b11<<6); //Set priority level
	
	TIMER0->TCMPR = 2400000-1; // 0.2/(1/12MHz)
	
	TIMER0->TCSR |= (1 << 30);
	
	//-------------------------UART0 Clock selection and configuration---------------------------------------
	CLK->CLKSEL1 |= (0b11 << 24); // UART0 clock source is 22.1184 MHz
	CLK->CLKDIV &= ~(0xF << 8); // clock divider is 1
	CLK->APBCLK |= (1 << 16); // enable UART0 clock

	SYS_LockReg();  // Lock protected registers	
}

void UART0_Config(void) {
	// UART0 pin configuration. PB.1 pin is for UART0 TX
	PB->PMD &= ~(0b11 << 2);
	PB->PMD |= (0b01 << 2); // PB.1 is output pin
	SYS->GPB_MFP |= (1 << 1); // GPB_MFP[1] = 1 -> PB.1 is UART0 TX pin
	
	SYS->GPB_MFP |= (1 << 0); // GPB_MFP[0] = 1 -> PB.0 is UART0 RX pin	
	PB->PMD &= ~(0b11 << 0);	// Set Pin Mode for GPB.0(RX - Input)

	// UART0 operation configuration
	UART0->LCR |= (0b11 << 0); // 8 data bit
	UART0->LCR &= ~(1 << 2); // one stop bit	
	UART0->LCR &= ~(1 << 3); // no parity bit
	UART0->FCR |= (1 << 1); // clear RX FIFO
	UART0->FCR |= (1 << 2); // clear TX FIFO
	UART0->FCR &= ~(0xF << 16); // FIFO Trigger Level is 1 byte]
	UART0->FCR |= (0b0001 << 16);
	
	//Baud rate config: BRD/A = 1, DIV_X_EN=0
	//--> Mode 0, Baud rate = UART_CLK/[16*(A+2)] = 22.1184 MHz/[16*(1+2)]= 460800 bps
	UART0->BAUD &= ~(0b11 << 28); // mode 0	
	UART0->BAUD &= ~(0xFFFF << 0);
	UART0->BAUD |= 70;
	
	//Configure interrupt
	UART0->IER |= (1<<0);
	UART0->FCR &= ~(0xF<<4); //RFITL 1 bytes
	UART0->ISR |= (1<<0); //enable RDA_IF interrupt
	NVIC->ISER[0] |= (1<<12); //enable interrupt at system source level
	NVIC->IP[3] &= ~(0b11<<6); 
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

void UART02_IRQHandler(void){
	character = UART0->DATA; //receive character
	if(character == 32 || character == '\n'){ // ' '
		strcpy(word,""); //reset word
	} else {
		strncat(word, &character, 1); //add character to current word
	}
	
	if (checkLatitude(word) == 1) {
		strcpy(temp, word);// Copy latitude to temp
	} else if (checkLongitude(word) == 1) {
		strcpy(showWord, temp); // COpy temp to showWord
		strcat(showWord, " "); // Add space
		strcat(showWord, word); // Add longtitude to showWord
	}
}

void TMR0_IRQHandler(void) {
	LCD_clear(); //reset LCD
	printS_5x7(0,0, showWord); //display result to LCD
	TIMER0->TISR |= (1<<0);  
}

int checkLatitude(char word[]) {
	int size = strlen(word);
	
	if (size == 8 && word[0] == 'S') { //check if the string starts with 's' and size is 8 => longtitude
		return 1;
	}
	return 0;
}

// 
int checkLongitude(char word[]) { //check if the string starts with 's' and size is 8 => latitude
	int size = strlen(word);
	
	if (size == 9 && word[0] == 'E') {
		return 1;
	}
	return 0;
}

//------------------------------------------- main.c CODE ENDS ---------------------------------------------------------------------------
