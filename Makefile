lzwarc: arc.c diter.c diter.h futils.c futils.h lzw.c lzw.h queue.c queue.h htbl.h bitio.h misc.h
	gcc -O3 -o lzwarc arc.c diter.c futils.c lzw.c queue.c -lpthread

