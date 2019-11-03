all: lzwarc lzwarc_

lzwarc: arc.c diter.c diter.h futils.c futils.h lzw.c lzw.h htbl.c htbl.h bitio.c bitio.h
	gcc -O3 -o lzwarc arc.c diter.c futils.c lzw.c htbl.c bitio.c

lzwarc_: arc_.c diter.c diter.h futils.c futils.h lzw.c lzw.h htbl.c htbl.h bitio.c bitio.h queue.c queue.h
	gcc -O3 -o lzwarc_ arc_.c diter.c futils.c lzw.c htbl.c bitio.c queue.c -lpthread

