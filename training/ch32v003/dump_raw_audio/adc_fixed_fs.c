/*
 * Example for using ADC with DMA and timer for fixed sampling rate
 * by eeucalyptus
 */

#include "ch32v003fun.h"
#include <stdio.h>
#include <stdlib.h>

int result,total=0,count=0,b=0;

//-----------------------------------------------------------------------------------
void init_timer() {
    // TIMER
    printf("Initializing timer...\r\n");
    RCC->APB2PCENR |= RCC_APB2Periph_TIM1;
    TIM1->CTLR1 |= TIM_CounterMode_Up | TIM_CKD_DIV1;
    TIM1->CTLR2 = TIM_MMS_1;
    TIM1->ATRLR = 80;   // lower = higher sample rate. 800 for 6400sam/sec
    TIM1->PSC = 10-1;
    TIM1->RPTCR = 0;
    TIM1->SWEVGR = TIM_PSCReloadMode_Immediate;

    NVIC_EnableIRQ(TIM1_UP_IRQn);
    TIM1->INTFR = ~TIM_FLAG_Update;
    TIM1->DMAINTENR |= TIM_IT_Update;
    TIM1->CTLR1 |= TIM_CEN;
}

//-----------------------------------------------------------------------------------
void init_adc() {
    printf("Initializing ADC... (bjs pin PD4 I think...)\r\n");
    
    // ADCCLK = 24 MHz => RCC_ADCPRE divide by 2
    RCC->CFGR0 &= ~(0x1F<<11);
    
    // Enable GPIOD and ADC
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOD | RCC_APB2Periph_ADC1;
    
    // PD4 is analog input chl 7
    GPIOD->CFGLR &= ~(0xf<<(4*4));    // pin D4 analog-in  now
   
    // Reset the ADC to init all regs
    RCC->APB2PRSTR |= RCC_APB2Periph_ADC1;
    RCC->APB2PRSTR &= ~RCC_APB2Periph_ADC1;

  // Set sequencer to channel 7 only
    ADC1->RSQR1 = 0;
    ADC1->RSQR2 = 0;
    ADC1->RSQR3 = 7;
    
     // set sampling time for chl 7
     ADC1->SAMPTR2 &= ~(ADC_SMP0<<(3*7));
     ADC1->SAMPTR2 |= 7<<(3*7);	// 0:7 => 3/9/15/30/43/57/73/241 cycles
		 
    
    // Keep CALVOL register with initial value
    ADC1->CTLR2  = ADC_ADON  | ADC_EXTSEL;
  
    // Reset calibrate
    printf("Calibrating ADC...\r\n");
    ADC1->CTLR2 |= ADC_RSTCAL;
    while(ADC1->CTLR2 & ADC_RSTCAL);   
    // Calibrate
    ADC1->CTLR2 |= ADC_CAL;
    while(ADC1->CTLR2 & ADC_CAL);
    
    printf("Calibrating done...\r\n");
}


//-----------------------------------------------------------------------------------
// Handle the Interrup....

void TIM1_UP_IRQHandler(void) __attribute__((interrupt));
void TIM1_UP_IRQHandler() {
    if(TIM1->INTFR & TIM_FLAG_Update) {
        TIM1->INTFR = ~TIM_FLAG_Update;
	result= ADC1->RDATAR; result-=512;
	
	total+=result; count++;
	if (count==8) {
	  total>>=3; 	
	  total+=127;
	  printf("%c",total);   // averaged
	  total=0; count=0;
	}
	ADC1->CTLR2 |= ADC_SWSTART; // start next ADC conversion
        
    }
}

//-----------------------------------------------------------------------------------
int main() {
    SystemInit();

    init_adc();
    init_timer();
   
    int a,i=12;
    unsigned char c;
    printf("here %d\n",i);
    
    while(1) {
    }
}
