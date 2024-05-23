// generate the twiddles_RES13.h tables of integer cos/sin values
#include <stdio.h>
#include <math.h>

#define FFT 128 // size of FFT
#define RES 13 // bits of resolution of the twiddles 
#define  MUL (1<<RES)

void main()
{
    FILE *fp;
    static int tr[FFT/2],ti[FFT/2];
    unsigned int n;

    for(n=1; n<(FFT/2); n++)
    {
       tr[n] = (int)(cos(2.0*M_PI*n/FFT)*MUL);
       ti[n] = (int)(sin(2.0*M_PI*n/FFT)*MUL);
    }
    fp=fopen("twiddles_RES13.h","w");
    fprintf(fp,"const static int tr[%i]={\n",FFT/2);
    for (n=0; n<FFT/2; n++) fprintf(fp,"%i,\n",tr[n]);
    fprintf(fp,"};\nconst static int ti[%i]={\n",FFT/2);
    for (n=0; n<FFT/2; n++) fprintf(fp,"%i,\n",ti[n]);
    fprintf(fp,"};\n");
    fclose(fp);
}
