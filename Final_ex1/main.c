#include <stdio.h>
#include "NUC100Series.h"

#define ADC_Voltage_2V 1638 // resolution is 2V/(1.221*(10^-3)) = 1638;
#define digits_1 2 //ascii 50
#define digits_2 0 //ascii 48
#define digits_3 2 //ascii 50
#define digits_4 2 //ascii 50

void System_Config(void);
void SPI2_Config(void);
void ADC7_Config(void);
void data_sending(uint8_t temp);

int main(void)
{
	uint32_t ADC_7val;
	
	System_Config();
	SPI2_Config();
	ADC7_Config();
	
	ADC->ADCR |= (1 << 11); // start ADC channel 7 conversion
	
	while(1){
		while(!(ADC->ADSR & (1 << 0))); // wait until conversion is completed (ADF=1)
		ADC->ADSR |= (1 << 0); // write 1 to clear ADF
		ADC_7val = ADC->ADDR[7] & 0x0000FFFF;
		if (ADC_7val > ADC_Voltage_2V) {
				data_sending('2');
				data_sending('0');
				data_sending('2');
				data_sending('2');
		} else {
		}
	}
}
//}
//---------------------------------------------------------------------------------------
// Functions definition
//---------------------------------------------------------------------------------------

void data_sending(uint8_t temp)
{
	SPI2->SSR |= 1 << 0;
	SPI2->TX[0] = temp;
	SPI2->CNTRL |= 1 << 0;
	while(SPI2->CNTRL & (1 << 0));
	SPI2->SSR &= ~(1 << 0);
}

void System_Config (void){
	SYS_UnlockReg(); // Unlock protected registers
	CLK->PWRCON |= (1 << 0);
	while(!(CLK->CLKSTATUS & (1 << 0)));
	//PLL configuration starts
	CLK->PLLCON &= ~(1<<19); //0: PLL input is HXT
	CLK->PLLCON &= ~(1<<16); //PLL in normal mode
	CLK->PLLCON &= (~(0x01FF << 0));
	CLK->PLLCON |= 48;
	CLK->PLLCON &= ~(1<<18); //0: enable PLLOUT
	while(!(CLK->CLKSTATUS & (1 << 2)));
	//PLL configuration ends
	//clock source selection
	CLK->CLKSEL0 &= (~(0b111 << 0));
	CLK->CLKSEL0 |= (0b010 << 0);
	//clock frequency division
	CLK->CLKDIV &= ~(0b1111 << 0);
	// SPI2 clock enable
	CLK->APBCLK |= 1 << 14;
	//ADC Clock selection and configuration
	CLK->CLKSEL1 &= ~(0b011 << 2); // ADC clock source is 12 MHz
	CLK->CLKDIV &= ~(0x0FF << 16);
	CLK->CLKDIV |= (0x0B << 16); // ADC clock divider is (11+1) --> ADC clock is 12/12 = 1 MHz
	CLK->APBCLK |= (1 << 28); // enable ADC clock
	SYS_LockReg(); // Lock protected registers
}

void SPI2_Config (void){
	SYS->GPD_MFP |= 1 << 0; //1: PD0 is configured for SPI2
	SYS->GPD_MFP |= 1 << 1; //1: PD1 is configured for SPI2
	SYS->GPD_MFP |= 1 << 3; //1: PD3 is configured for SPI2
	
	SPI2->CNTRL &= ~(1 << 23); //0: disable variable clock feature
	SPI2->CNTRL &= ~(1 << 22); //0: disable two bits transfer mode
	SPI2->CNTRL &= ~(1 << 18); //0: select Master mode
	SPI2->CNTRL &= ~(1 << 17); //0: disable SPI interrupt
	SPI2->CNTRL |= 1 << 11; //1: SPI clock idle high
	SPI2->CNTRL |= (1 << 10); //1: LSB is sent first
	SPI2->CNTRL &= ~(3 << 8); //00: one transmit/receive word will be executed in one data transfer
	SPI2->CNTRL &= ~(31 << 3); //Transmit/Receive bit length
	SPI2->CNTRL |= 8 << 3; //8: 8 bits transmitted per data transfer
	SPI2->CNTRL &= ~(1 << 2); //0: Transmit at positive edge of SPI CLK
	SPI2->DIVIDER = 24; // SPI clock divider. SPI clock = HCLK / ((DIVIDER+1)*2) = 50/((24+1)2) = 1MHz
}

void ADC7_Config(void) {
	PA->PMD &= ~(0b11 << 14); // PA.7 is input pin
	PA->OFFD |= (0b01 << 23); // PA.7 digital input path is disabled
	SYS->GPA_MFP |= (0b01 << 7); // GPA_MFP[7] = 1 for ADC7
	SYS->ALT_MFP &= ~(0b01 << 11); //ALT_MFP[11] = 0 for ADC7
	//ADC operation configuration
	ADC->ADCR |= (0b11 << 2); // continuous scan mode
	ADC->ADCR &= ~(1 << 1); // ADC interrupt is disabled
	ADC->ADCR |= (1 << 0); // ADC is enabled
	ADC->ADCHER &= ~(0b11 << 8); // ADC7 input source is external pin
	ADC->ADCHER |= (1 << 7); // ADC channel 7 is enabled.
}
