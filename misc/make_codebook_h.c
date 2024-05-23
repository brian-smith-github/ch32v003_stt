// make a 'codebook.h' file from a trained cb.txt file
#include <stdio.h>

#define MAXCB 10000
#define WSIZE 16 // standard number of frames per word in codebook
#define CEPS 8 // size of each frame cepstrum vector

void main()
{
   FILE *fp;
   int  cb[MAXCB][CEPS];
   int start[MAXCB],size[MAXCB],cbsize=0;
   char cbp[MAXCB];
   int i,n,c,z;
   // first, read cb.txt....
   
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
  
  fp=fopen("codebook.h","w");
  fprintf(fp,"const static int8_t cb[]={\n");
  for (c=0; c<cbsize; c++) {
    fprintf(fp,"%c,\n",cbp[c]);
    for (i=0; i<WSIZE; i++) {
      for (n=0; n<CEPS; n++) fprintf(fp,"%i,",cb[start[c]+i][n]);
      fprintf(fp,"\n");
    }
  }
  fprintf(fp,"-1\n};\n"); // end-of-file marker
  fclose(fp);
}
