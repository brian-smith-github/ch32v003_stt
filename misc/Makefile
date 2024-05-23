OBJS  = make_codebook_h.o make_twiddles_h.o make_mel_mx_h.o make_dctm_h.o


CC=gcc 
CFLAGS= -O2
LDFLAGS=  -O -lm

all:    $(OBJS)
	$(CC) -o make_codebook_h make_codebook_h.o -lm
	$(CC) -o make_twiddles_h make_twiddles_h.o -lm
	$(CC) -o make_mel_mx_h make_mel_mx_h.o -lm
	$(CC) -o make_dctm_h make_dctm_h.o -lm


clean: 
	rm -f *.o *~

.c.o:	$<
	$(CC) $(CFLAGS) -c $<

