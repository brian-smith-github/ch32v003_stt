// generate dctm_20x8_8bit.h file (a MDCT matrix)
#include <stdio.h>
#include <math.h>

#define MEL 20  // number of MEL bins as input to matrix
#define CEPS 8  // number of cepstrum bins as output from matrix

void main()
{

  FILE *fp;
  double dctm[CEPS+1][MEL],x;
  int i,j;
  
  // set up mdct arrays...
  for (i=0; i<=CEPS; i++) {
    for (j=0,x=1.0; j<MEL; j++,x+=2.0) {
         dctm[i][j]=cos((double)i*x/MEL*M_PI/2.0)*sqrtf(2.0/MEL);
	 dctm[i][j]*=400.0; // scale up for integer conversion
	 //printf("%.0f,",dctm[i][j]); // add *256.0 for 16-bit
    }
    //printf("\n");
  }
  fp=fopen("dctm_20x8_8bit.h","w");
  fprintf(fp,"const static int dctm_8bit[%i]={\n",MEL*CEPS);
  for (i=1; i<=CEPS; i++) {
    for (j=0; j<MEL; j++) fprintf(fp,"%i,",(int)(dctm[i][j]));
    fprintf(fp,"\n");
  }
  fprintf(fp,"};\n");
  fclose(fp);
  


}

