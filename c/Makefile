lzwarc: arc.o lzw.o huffman.o futils.o diter.o queue.o pathtree.o
	gcc -o lzwarc arc.o lzw.o huffman.o futils.o diter.o queue.o pathtree.o -lpthread

arc.o: arc.c lzw.h huffman.h futils.h diter.h queue.h pathtree.h misc.h
	gcc -O3 -c arc.c

lzw.o: lzw.c lzw.h htbl.h bitio.h
	gcc -O3 -c lzw.c

huffman.o: huffman.c huffman.h pqueue.h bitio.h
	gcc -O3 -c huffman.c

futils.o: futils.c futils.h
	gcc -O3 -c futils.c

diter.o: diter.c diter.h
	gcc -O3 -c diter.c

queue.o: queue.c queue.h
	gcc -O3 -c queue.c

pathtree.o: pathtree.c pathtree.h
	gcc -O3 -c pathtree.c

