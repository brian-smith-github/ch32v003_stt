// Voice Keyboard. By Brian Smith.
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include "twiddles_RES13.h"
#include "mel_mx.h"
#include "dctm_20x8_8bit.h"  // first and last of 10CEP removed

#define RES 8192
#define SHIFT 13

#define MAXCB 10000 // maximum stored frames (words times average word size)

#define FFT 128
#define FF2 65
#define N 64
#define CHANS 20 // mel banks
#define MEL 20
#define CEPS 8

#define TOL_ON  200
#define TOL_OFF 150

#define MINSIZE 6  // minimum acceptible sample size in frames (x10ms)
#define MAXSIZE 50 // maximum acceptible sample size in frames (x10ms)

//noise floor  (initialized high)
u_int8_t nfloor[CHANS];

// globals...
int  cb[MAXCB][CEPS];
char cbp[MAXCB];
int start[MAXCB],size[MAXCB],cbsize=0;
int parsed=0,trained=0;

int8_t word[500*CEPS];
int wsize=0,safewsize=0;

int re[FFT],im[FFT];

//----------------------------------------------------------------------
// use this to set serial-port into RAW mode and set speed.....
int set_interface_attribs(int fd, int speed)
{
    struct termios tty;

    if (tcgetattr(fd, &tty) < 0) {
        printf("Error from tcgetattr: %s\n", strerror(errno));
        return -1;
    }

    cfsetospeed(&tty, (speed_t)speed);
    cfsetispeed(&tty, (speed_t)speed);

    //tty.c_cflag |= (CLOCAL | CREAD);    /* ignore modem controls */
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;         /* 8-bit characters */
    tty.c_cflag &= ~PARENB;     /* no parity bit */
    tty.c_cflag &= ~CSTOPB;     /* only need 1 stop bit */
    //tty.c_cflag &= ~CRTSCTS;    /* no hardware flowcontrol */

    /* setup for non-canonical mode */
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    tty.c_oflag &= ~OPOST;

    /* fetch bytes as they become available */
    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 1;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        printf("Error from tcsetattr: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

//----------------------------------------------------------------------
// read keyboard, returns -1 if no key, otherwise key code.
int getkey() {
    int character;
    char c;
    struct termios orig_term_attr;
    struct termios new_term_attr;

    /* set the terminal to raw mode */
    tcgetattr(fileno(stdin), &orig_term_attr);
    memcpy(&new_term_attr, &orig_term_attr, sizeof(struct termios));
    new_term_attr.c_lflag &= ~(ECHO|ICANON);
    new_term_attr.c_cc[VTIME] = 0;
    new_term_attr.c_cc[VMIN] = 0;
    tcsetattr(fileno(stdin), TCSANOW, &new_term_attr);

    /* read a character from the stdin stream without blocking */
    /*   returns EOF (-1) if no character is available */
    if (read(0, &c, 1)!=-1) character=(int)c; else character=0;

    /* restore the original terminal attributes */
    tcsetattr(fileno(stdin), TCSANOW, &orig_term_attr);
    return character;
}


//-----------------------------------------------------------------------
void simple_int_fft(int size)
{
    unsigned int even, odd, span, log=0, rootindex;    // indexes
    int temp;
    log=0;
    int i;

    for(span=size>>1; span; span>>=1, log++)   
    {
       
        for(odd=span; odd<size; odd++)         // iterate over the dual nodes
        {
	    
            odd |= span;                    // iterate over odd blocks only
            even = odd ^ span;              // even part of the dual node pair
            //printf("even=%i,odd=%i\n",even,odd);
                       
            temp = re[even] + re[odd];       
            re[odd] = re[even] - re[odd];
            re[even] = temp;
           
            temp = im[even] + im[odd];           
            im[odd] = im[even] - im[odd];
            im[even] = temp;
           
            rootindex = (even<<log) & (size-1); // find root of unity index
            if(rootindex)                    // skip rootindex[0] (has an identity)
            {   
	        //printf("rootindex=%i\n",rootindex);
		// sanity checks....
		//if (re[odd]*tr[rootindex]>(1<<30)) {printf("oops fft\n"); exit(0);}
		//if (re[odd]*ti[rootindex]>(1<<30)) {printf("oops fft\n"); exit(0);}
		//if (im[odd]*tr[rootindex]>(1<<30)) {printf("oops fft\n"); exit(0);}
		//if (im[odd]*ti[rootindex]>(1<<30)) {printf("oops fft\n"); exit(0);}
                temp=re[odd]*tr[rootindex]/RES+im[odd]*ti[rootindex]/RES;
                im[odd]=im[odd]*tr[rootindex]/RES-re[odd]*ti[rootindex]/RES;
                re[odd] = temp;
            }
   
        } // end of loop over n
     
     } // end of loop over FFT stages
     //exit(0);
} //end of function


//-----------------------------------------------------------------------
// returns log2 of a, multiplied by 8.....
const static int first[9]={0,0,8,12,16,18,20,22,24};

int intlog2_8bit(unsigned int a)
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

//---------------------------------------------------------------
// add a word into the codebook.
int train(int p)
{
  FILE *fp;
  int i,a,n,k;
   
  printf("training... wsize=%i\n",wsize);
  for (i=0; i<wsize; i++) for (n=0; n<CEPS; n++) 
       cb[start[cbsize]+i][n]=word[i*CEPS+n];
  for (i=0; i<wsize; i++) {
    for (n=0; n<CEPS; n++) printf("%i,",(int)word[i*CEPS+n]); printf("\n");
  }
  size[cbsize]=wsize; start[cbsize+1]=start[cbsize]+wsize;
  if (p==10) p='R'; // return key
  if (p==127) p='B'; // backspace key
  if (p==32) p='S'; // space key
  cbp[cbsize]=p;  
  fprintf(stderr,"\n*** TRAINED '%c' as cb item %i, size=%i\n",p,cbsize,wsize);
  cbsize++;
  
  // write updated codebook back to disk
  fp=fopen("cb.txt","w");
  for (i=0; i<cbsize; i++) {
    fprintf(fp,"%c,\n",cbp[i]);
    //fprintf(fp,"%i,\n",size[i]);
    for (n=0; n<size[i]; n++) {
      for (k=0; k<CEPS; k++) fprintf(fp,"%i,",cb[start[i]+n][k]);
      fprintf(fp,"\n");
    }
  } 
  fclose(fp); 
  trained++;
  usleep(1000000);
}


//---------------------------------------------------------------
// search codebook, see what word[] most closely matches...
int closest()
{
   FILE *fpout;
   int dist,bestdist,d;
   int best=0,c,a,i,n,k,minsize;
   
    // warp word size to exactly 16 frames....
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

   // no warping for now......
   if (wsize>30) wsize=30; // upper limit
   
   bestdist=99999999;
   for (c=0; c<cbsize; c++) {
     dist=0;
     minsize=wsize; if (size[c]<minsize) minsize=size[c];
     for (i=0; i<minsize; i++) {
       for (n=0; n<CEPS; n++)
       //for (n=0; n<4; n++) // ignore last couple of ceps?
        {d=word[i*CEPS+n]-cb[start[c]+i][n]; dist+=d*d;}
     }
     dist/=minsize; // average out the distance over the framecount
     if (dist<bestdist) {bestdist=dist; best=c;}
   }
   fprintf(stderr,"\n\nBEST=%c, distance=%i, ipsize=%i  cbitem=%i\n",
          cbp[best],bestdist,wsize,best);
   parsed++;
   return(cbp[best]);
}


//---------------------------------------------------------------

int main()
{ 

  FILE *fp;
  int posn=0,fd;
  int f[FFT],f2[FFT];
  int mel[CHANS];
  int cep[CEPS]; //  integer ceps.
  double dctm[CEPS][CHANS],idctm[CEPS][CHANS],x;
 
  int i,j,n,k,c,z,best,key,e,max,min; 
  short s;
  int silcount=0;
  int sound=0; // boolean start/stop
  int frame=0;
  double bestdist,dist; 
  unsigned char cha;
  
  for (i=0; i<CHANS; i++) nfloor[i]=192; // initial high noise floor.
 
  // set up mdct arrays...
  for (i=1; i<CEPS-1; i++) {
    for (j=0,x=1.0; j<CHANS; j++,x+=2.0) {
         dctm[i][j]=cos((double)i*x/CHANS*M_PI/2.0)*sqrtf(2.0/CHANS);
	 idctm[i][j]=dctm[i][j];
	 if (i==0) idctm[i][j]/=2.0;
	 //printf("%.0f,",dctm[i][j]*400.0); // add *256.0 for 16-bit
    }
    //printf("\n");
  }
  //exit(0);

  
  cbsize=0; start[0]=0; size[0]=0; cbp[0]='?';
  if (fp=fopen("cb.txt","r")) {
    while(!feof(fp)) {
      z=fscanf(fp,"%c,\n",&cbp[cbsize]);    
      //z=fscanf(fp,"%i,\n",&size[cbsize]);
      size[cbsize]=16;  // fixed size now 
      for (i=start[cbsize]; i<start[cbsize]+size[cbsize]; i++) {
        for (n=0; n<CEPS; n++) z=fscanf(fp,"%i,",&cb[i][n]); z=fscanf(fp,"\n");
      }
      start[cbsize+1]=start[cbsize]+size[cbsize];  cbsize++; 
    }
    fclose(fp); printf("read codebook, size=%i\n",cbsize);
  }
  
  fd = open("/dev/ttyACM0", O_RDWR | O_NOCTTY | O_SYNC);
  if (fd < 0) {
        printf("Error opening port: %s\n", strerror(errno));
        return -1;
  }
  set_interface_attribs(fd, B230400);
  // usually /dev/ttyACM0 or /tmp/nano1.raw........
  if (!(fp=fopen("/dev/ttyACM0","r"))) {
    printf("oops, unable to open serial port /dev/ttyACM0\n"); exit(0);}
    
    
  //---------------------------------    
  while(1) {   // usually '1'....
    for (i=0; i<FFT-N; i++) f[i]=f[i+N]; // move samples down...
    for (i=FFT-N; i<FFT; i++) {z=fread(&cha,1,1,fp); f[i]=(int)cha;}
    for (i=0; i<FFT; i++) f2[i]=(f[i]);
    for (i=FFT-1; i!=0; i--) f2[i]-=f2[i-1]; f2[0]=0; // pre-emph
    for (i=0; i<FFT; i++) f2[i]<<=8; // scale for FFT
    for (i=0; i<FFT; i++) {re[i]=f2[i]; im[i]=0;}
    simple_int_fft(FFT);
    
    // note, the FFT results are all over the place so need to correct ALL....
    for (i=1; i<FFT; i+=2) if (re[i]>(1<<30)) {printf("oops1!\n"); exit(0);}
    for (i=1; i<FFT; i+=2) if (im[i]>(1<<30)) {printf("oops2!\n"); exit(0);}
    for (i=1; i<FFT; i+=2) {re[i]=(re[i]*re[i]+im[i]*im[i]); re[i]>>=8;}
    for (i=1; i<FFT; i+=2) if (re[i]>(1<<23)) {printf("oops3!\n"); exit(0);}
    
    // now estimate the 20 Mel bins...

     for (i=0; i<MEL; i++) mel[i]=0;
     i=0; n=0; while(n!=MEL) {
         if (mel_mx[i]==0) {i++; n++;}
	 else {mel[n]+=re[mel_mx[i]]*mel_mx[i+1]; i+=2;}
     }
     
    //for (i=0; i<MEL; i++) if (mel[i]>(1<<30)) {printf("oops mel!\n"); exit(0);}
    for (i=0; i<MEL; i++) {
       mel[i]=intlog2_8bit(mel[i]);  
    }
    
    // remove noise floor and adjust it every 4 frames....
    //if (frame%4==0) for (i=0; i<CHANS; i++) {
    if (e<TOL_OFF) for (i=0; i<CHANS; i++) {
       mel[i]-=nfloor[i];
       if (mel[i]>0 && nfloor[i]<255) nfloor[i]++;
       if (mel[i]<0 && nfloor[i]>0) nfloor[i]--;
       if (mel[i]<0) mel[i]=0;     
    } else for (i=0; i<CHANS; i++) { // just remove noise floor....
        mel[i]-=nfloor[i]; 
	if (mel[i]<0) mel[i]=0;   
    }
    //for (i=0; i<CHANS; i++) printf("%i,",mel[i]); printf("\n");
    
    // calculate energy seperate from mdct (just sum-of-mels)
    e=0; for (i=0; i<CHANS; i++) e+=mel[i];
    //printf("e=%i\n",e);
    
    // now try clamping the mel bins and limiting range....
    max=-100; for (i=0; i<CHANS; i++) if (mel[i]>max) max=mel[i];
    //for (i=0; i<CHANS; i++) {mel[i]=mel[i]-max+100; if (mel[i]<0) mel[i]=0;}
    //for (i=0; i<CHANS; i++) printf("%i,",mel[i]); printf("\n");
   
    // calculate  integer ceps....
    for (n=0; n<CEPS; n++) {
      cep[n]=0;
      for (i=0; i<CHANS; i++) cep[n]+=mel[i]*dctm_8bit[n*MEL+i];
      cep[n]>>=9; // ESSENTIAL, word[] is only 8-bit resolution!
      if (cep[n]>127) {printf("oops cep\n"); exit(0);}
      if (cep[i]<-127) {printf("oops cep\n"); exit(0);}
    }
    
    printf("%i: ",e); for (i=0; i<CEPS; i++) printf("%i,",cep[i]); printf("\n");
   
    
    
    
    if (e>TOL_ON) {
      for (i=0; i<CEPS; i++) word[wsize*CEPS+i]=(int8_t)cep[i]; wsize++;
      for (i=0; i<CEPS; i++) printf("%i,",cep[i]); printf("  ");
      silcount=0; printf("wsize now %i\n",wsize);
    }
    if (e<TOL_OFF) silcount++; else silcount=0;
    if (silcount>=20 && wsize!=0) {wsize=0; printf("wsize now 0\n");} // sanity
    if (silcount>=15 && wsize>=MINSIZE) { // finished reading word
       printf("wsize=%i\n",wsize);
       if (wsize>=MINSIZE && wsize<=MAXSIZE) best=closest();
       else  printf("(rejected %i frames)\n",wsize);
       
       for (i=0; i<800000; i++) {
           key=getkey(); if (key>0) printf("key=%i\n",key);
           if (key==27) {printf("\nExiting.\n"); fclose(fp); exit(0); }
           if (key>47 && key<123) {train(key); i=999999999; }
       }
       printf("listening again....\n");
       sound=0;
       silcount=0;
       wsize=0;
    }
    frame++;
  }
}
