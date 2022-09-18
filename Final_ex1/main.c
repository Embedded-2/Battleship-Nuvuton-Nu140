//------------------------------------------- main.c CODE STARTS ---------------------------------------------------------------------------
#include <stdio.h>
#include "NUC100Series.h"
#include "LCD.h"

#define ADC_Voltage_2V 1638 // resolution is 2V/(1.221*(10^-3)) = 1638;
#define HXT_STATUS 1<<0
#define PLL_STATUS 1<<2

void System_Config(void);
void SPI3_Config(void);
void ADC7_Config(void);

void SPI2_Config(void);
void data_sending(uint8_t temp);


void LCD_start(void);
void LCD_command(unsigned char temp);
void LCD_data(unsigned char temp);
void LCD_clear(void);
void LCD_SetAddress(uint8_t PageAddr, uint8_t ColumnAddr);
uint32_t ADC_7val;
char adc7_val_s[4] = "0000";

int main(void)
{

    System_Config();
    SPI3_Config();
		SPI2_Config();
    ADC7_Config();

    LCD_start();
   LCD_clear();

    //--------------------------------
    //LCD static content
    //--------------------------------

    ADC->ADCR |= (1 << 11); // start ADC channel 7 conversion

    while (1) {
        while (!(ADC->ADSR & (1 << 0))); // wait until conversion is completed (ADF=1)
        ADC->ADSR |= (1 << 0); // write 1 to clear ADF
        ADC_7val = ADC->ADDR[7] & 0x0000FFFF;
				if (ADC_7val > ADC_Voltage_2V) {
					data_sending('0');
					data_sending('0');
					data_sending('2');
					data_sending('2');
		}
		//CLK_SysTickDelay(10);
	}
}

//------------------------------------------------------------------------------------------------------------------------------------
// Functions definition
//------------------------------------------------------------------------------------------------------------------------------------



void TMR0_IRQHandler(void) {
	LCD_clear(); //reset LCD
	printS_5x7(2, 20, "A/D value:");
	sprintf(adc7_val_s, "%d", ADC_7val);
	printS_5x7(4 + 5 * 10, 20, "    ");
  printS_5x7(4 + 5 * 10, 20, adc7_val_s);
	TIMER0->TISR |= (1<<0); 
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

void System_Config(void) {
    SYS_UnlockReg(); // Unlock protected registers
    
    CLK->PWRCON |= (1 << 0);
		while(!(CLK->CLKSTATUS & HXT_STATUS));
    //PLL configuration starts
    CLK->PLLCON &= ~(1 << 19); //0: PLL input is HXT
    CLK->PLLCON &= ~(1 << 16); //PLL in normal mode
    CLK->PLLCON &= (~(0x01FF << 0));
    CLK->PLLCON |= 48;
    CLK->PLLCON &= ~(1 << 18); //0: enable PLLOUT
    while(!(CLK->CLKSTATUS & PLL_STATUS));
    //PLL configuration ends
    
    //clock source selection
    CLK->CLKSEL0 &= (~(0x07 << 0));
    CLK->CLKSEL0 |= (0x02 << 0);
    //clock frequency division
    CLK->CLKDIV &= (~0x0F << 0);
    
    // SPI3 clock enable
    CLK->APBCLK |= 1 << 15;
    
		// SPI2 clock enable
		CLK->APBCLK |= 1 << 14;
		
    //ADC Clock selection and configuration
    CLK->CLKSEL1 &= ~(0x03 << 2); // ADC clock source is 12 MHz
    CLK->CLKDIV &= ~(0x0FF << 16);
    CLK->CLKDIV |= (0x0B << 16); // ADC clock divider is (11+1) --> ADC clock is 12/12 = 1 MHz
    CLK->APBCLK |= (0x01 << 28); // enable ADC clock
    
		
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
	
		TIMER0->TCMPR = 15000000-1; // 1.25/(1/12MHz)
	
		TIMER0->TCSR |= (1 << 30);
		
    SYS_LockReg();  // Lock protected registers    
}

void SPI3_Config(void) {
    SYS->GPD_MFP |= 1 << 11; //1: PD11 is configured for SPI3
    SYS->GPD_MFP |= 1 << 9; //1: PD9 is configured for SPI3
    SYS->GPD_MFP |= 1 << 8; //1: PD8 is configured for SPI3

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
    SPI3->DIVIDER = 0; // SPI clock divider. SPI clock = HCLK / ((DIVID-ER+1)*2). HCLK = 50 MHz
}

void SPI2_Config (void){
	SYS->GPD_MFP |= 1 << 0; //1: PD0 is configured for SPI2
	SYS->GPD_MFP |= 1 << 1; //1: PD1 is configured for SPI2
	SYS->GPD_MFP |= 1 << 3; //1: PD3 is configured for SPI2
	
	SPI2->CNTRL &= ~(1 << 23); //0: disable variable clock feature
	SPI2->CNTRL &= ~(1 << 22); //0: disable two bits transfer mode
	SPI2->CNTRL &= ~(1 << 18); //0: select Master mode
	SPI2->CNTRL &= ~(3 << 19);
	SPI2->CNTRL &= ~(1 << 17); //0: disable SPI interrupt
	SPI2->CNTRL |= 1 << 11; //1: SPI clock idle high
	SPI2->CNTRL |= (1 << 10); //1: LSB is sent first
	//SPI2->CNTRL &= ~(3 << 8); //00: one transmit/receive word will be executed in one data transfer
	SPI2->CNTRL &= ~(3 << 8);
	
	SPI2->CNTRL &= ~(31 << 3); //Transmit/Receive bit length
	SPI2->CNTRL |= 8 << 3; //8: 8 bits transmitted per data transfer
	SPI2->CNTRL &= ~(1 << 2); //0: Transmit at positive edge of SPI CLK
	SPI2->DIVIDER = 24; // SPI clock divider. SPI clock = HCLK / ((DIVIDER+1)*2) = 50/((24+1)2) = 1MHz
}

void data_sending(uint8_t temp)
{
	SPI2->SSR |= 1 << 0;
	SPI2->TX[0] = temp;
	SPI2->CNTRL |= 1 << 0;
	while(SPI2->CNTRL & (1 << 0));
	SPI2->SSR &= ~(1 << 0);
}

void ADC7_Config(void) {
    PA->PMD &= ~(0b11 << 14);
    PA->PMD |= (0b01 << 14); // PA.7 is input pin
    PA->OFFD |= (0x01 << 7); // PA.7 digital input path is disabled
    SYS->GPA_MFP |= (1 << 7); // GPA_MFP[7] = 1 for ADC7
    SYS->ALT_MFP &= ~(1 << 11); //ALT_MFP[11] = 0 for ADC7

    //ADC operation configuration
    ADC->ADCR |= (0b11 << 2);  // continuous scan mode
    ADC->ADCR &= ~(1 << 1); // ADC interrupt is disabled
    ADC->ADCR |= (0x01 << 0); // ADC is enabled
    ADC->ADCHER &= ~(0b11 << 8); // ADC7 input source is external pin
    ADC->ADCHER |= (1 << 7); // ADC channel 7 is enabled.
}
//------------------------------------------- main.c CODE ENDS ---------------------------------------------------------------------------


