#include "ch32v003fun.h"
#include <stdio.h>
#include <stdlib.h>
#include "twiddles_RES13.h"
#include "mel_mx.h"  // weights to generate 20 mel-scale bins from 65 FFT128 bins
#include "dctm_20x8_8bit.h" // discrete-cosine transform matrix, 20mel->10cep
#include "codebook.h" // 16-frame-sample-blocks to numbers.

#define FFT 128
#define FF2 65
#define N 64  // 10ms frames
#define MEL 20 // numer of mel-scale bins to generate from FFT data
#define CEPS 8
#define WSIZE 25  // maximum word size in frames
#define MINSIZE 5 // minimum word size in frames

#define RES 8192 // resolution of FFT cos/sine to use...
#define SHIFT 13

#define TOL_ON  200
#define TOL_OFF 150

int16_t buffer[FFT];
int result, samcount=0, total=0, posn=0, count=0, lock, silcount=0;

int re[FFT],im[FFT];
int mel[MEL];
u_int8_t nfloor[MEL];
int cep[CEPS];

int8_t word[CEPS*WSIZE],wsize;


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
    printf("Initializing ADC... (on pin PD4...)\r\n");
    
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
// Handle the timer interrupt....
void TIM1_UP_IRQHandler(void) __attribute__((interrupt));
void TIM1_UP_IRQHandler() {
    if(TIM1->INTFR & TIM_FLAG_Update) {
        TIM1->INTFR = ~TIM_FLAG_Update;
	result= ADC1->RDATAR; result=result-512;
	total+=result; samcount++;
	if (samcount==8) {
	  total>>=3;
	  //total+=127; printf("%c",total);
	  buffer[posn]=(int16_t)total;
	  posn++;
	  if (posn==FFT) {
	     if (lock==1) printf("oops lock\n");
	     for (posn=0; posn<FFT; posn++) {re[posn]=(int)buffer[posn]; im[posn]=0;}
	     	     
	     for (posn=0; posn<FFT-N; posn++) buffer[posn]=buffer[posn+N];
	     posn=FFT-N;   count++;
	  }
	  samcount=0; total=0;
	}
	ADC1->CTLR2 |= ADC_SWSTART; // start next ADC conversion       
    }
}




//-----------------------------------------------------------------------
void simple_int_fft(int size)
{
    unsigned int even, odd, span, log=0, rootindex;    // indexes
    int temp;
    log=0;

    for(span=size>>1; span; span>>=1, log++)   
    {
       
        for(odd=span; odd<size; odd++)         // iterate over the dual nodes
        {
	    
            odd |= span;                    // iterate over odd blocks only
            even = odd ^ span;              // even part of the dual node pair
            //printf("even=%d,odd=%d\n",even,odd);
                       
            temp = re[even] + re[odd];       
            re[odd] = re[even] - re[odd];
            re[even] = temp;
           
            temp = im[even] + im[odd];           
            im[odd] = im[even] - im[odd];
            im[even] = temp;
           
            rootindex = (even<<log) & (size-1); // find root of unity index
            if(rootindex)                    // skip rootindex[0] (has an identity)
            {
	       	//printf("rootindex=%d\n",rootindex);	
                temp=re[odd]*tr[rootindex]/RES+im[odd]*ti[rootindex]/RES;
                im[odd]=im[odd]*tr[rootindex]/RES-re[odd]*ti[rootindex]/RES;

                //temp=((re[odd]*tr[rootindex])>>SHIFT) +((im[odd]*ti[rootindex])>>SHIFT);
                //im[odd]=((im[odd]*tr[rootindex])>>SHIFT) -((re[odd]*ti[rootindex])>>SHIFT);
                re[odd] = temp;
            }
   
        } // end of loop over n
     
     } // end of loop over FFT stages

} //end of function

//-----------------------------------------------------------------------
// returns log2 of input, multiplied by 8.....
const static int first[9]={0,0,8,12,16,18,20,22,24};

unsigned int intlog2_8bit(unsigned int a)
{
  unsigned int b;
  int r; // highest possible log result....
  int l; // log result
  int result;
  
  if (a>8) {
    b=0xffff0000; if ((a&b)==0) r=16; else r=0;
    b=0xff000000; b>>=r; if ((a&b)==0) r+=8;
    b=0xf0000000; b>>=r; if ((a&b)==0) r+=4; 
    b=0xc0000000; b>>=r; if ((a&b)==0) r+=2; 
    b=0x80000000; b>>=r; if ((a&b)==0) r+=1; 
    //printf("log2 a is %i\n",31-r);
    l=31-r; result=l*8;
    a>>=(l-3); result+=a-8;
    return(result);
  } else return(first[a]);
}


//-----------------------------------------------------------------------------------
int main() {
    SystemInit();

    init_adc();
    init_timer();
   
    int e,k=10,i,n,lastcount=0; 
    int d,dist,bestdist,best,c;
    
    for (i=0; i<MEL; i++) nfloor[i]=192; // high initial noise floor

    while(1) {
       while(count!=0 && lastcount==count) k++;  // wait till 10ms of samples read....
       if (count-lastcount!=1) printf("oops framecount\n");
       lock=1;
       //lock=0;  // ignore locking for now when doing trace stuff.
       
       for (i=FFT-1,n=FFT-2; i!=0; i--,n--) re[i]=re[i]-re[n];  re[0]=0; // 1.0 pre-emph?
       for (i=0; i<FFT; i++) re[i]<<=8; // scale for the FFT
       simple_int_fft(FFT);
       for (i=1; i<FFT; i+=2) {re[i]=(re[i]*re[i]+im[i]*im[i]); re[i]>>=8;}
       //for (i=1; i<FFT; i+=2) if (re[i]>(1<<23)) printf("oops!\n");
       
       for (i=0; i<MEL; i++) mel[i]=0;      
       i=0; n=0; while(n!=MEL) {
         if (mel_mx[i]==0) {i++; n++;}
	 else {mel[n]+=(unsigned int)re[mel_mx[i]]*mel_mx[i+1]; i+=2;}
       }
      
       for (i=0; i<MEL; i++) mel[i]=intlog2_8bit(mel[i]);
       
       // remove noise floor and adjust it?....
       if (e<TOL_OFF) for (i=0; i<MEL; i++) {
         mel[i]-=nfloor[i];
         if (mel[i]>0 && nfloor[i]<255) nfloor[i]++;
         if (mel[i]<0 && nfloor[i]>0) nfloor[i]--;
         if (mel[i]<0) mel[i]=0;     
       } else for (i=0; i<MEL; i++) { // just remove noise floor....
        mel[i]-=nfloor[i]; 
	if (mel[i]<0) mel[i]=0;   
       }
            
       // calculate energy seperate from mdct (just sum-of-mels)
       e=0; for (i=0; i<MEL; i++) e+=mel[i];
       //printf("e=%d\n",e);
       
       
       for (n=0; n<CEPS; n++) {
         cep[n]=0;
	 for (i=0; i<MEL; i++) cep[n]+=mel[i]*dctm_8bit[n*MEL+i];
	 cep[n]>>=9;  // only enough memory for buffering 8-bit ceps
       }
      
       if (e>TOL_ON) {  // capture (another) audio frame...
          for (i=0; i<CEPS; i++) word[wsize*CEPS+i]=(int8_t)cep[i];
	   wsize++; silcount=0;
       }
       if (e<TOL_OFF) silcount++; // silent frame, end of sample?
       if ( silcount>=20 && wsize<MINSIZE) {silcount=0; wsize=0;}
       lock=0;
             
       if (wsize==WSIZE || (silcount>=15 && wsize>=MINSIZE)) {
         // warp sample size to exactly 16 frames....
	 if (wsize<16) {
         for (i=15; i>=0; i--) {
	    k=i*wsize/16;
	    for (n=0; n<CEPS; n++) word[i*CEPS+n]=word[k*CEPS+n];
	  }
         }
         if (wsize>16) {
           for (i=0; i<16; i++) {
	     k=i*wsize/16;
	     for (n=0; n<CEPS; n++) word[i*CEPS+n]=word[k*CEPS+n];
           }
         }
         wsize=16; // standardized now!
	 
	 // search codebook for best match and print it!
	 bestdist=999999999; c=0;
	 while(cb[c]!=-1) {
	   k=cb[c]; c++; dist=0; // printf("k=%d\n",k);
	   for (i=0; i<16; i++) {	     
	     for (n=0; n<CEPS; n++,c++) {d=word[i*CEPS+n]-cb[c]; dist+=d*d;}	     
	   }
	   if (dist<bestdist) {bestdist=dist; best=k;}
	 }
	 printf("best match = %d, bestdist=%d\n",best,bestdist);
	 wsize=0; silcount=0; posn=0;
      }       
      lastcount=count;  
    }
}
