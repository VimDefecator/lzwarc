lzwarc: arc.c diter.c diter.h futils.c futils.h lzw.c lzw.h htbl.c htbl.h queue.c queue.h bitio.h misc.h
	gcc -O3 -o lzwarc arc.c diter.c futils.c lzw.c htbl.c queue.c -lpthread

