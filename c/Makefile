lzwarc: arc.o lzw.o huffman.o futils.o diter.o queue.o pathtree.o
	clang -o lzwarc arc.o lzw.o huffman.o futils.o diter.o queue.o pathtree.o -lpthread

arc.o: arc.c ../common/lzw.h ../common/huffman.h ../common/futils.h diter.h queue.h pathtree.h misc.h
	clang -O3 -c arc.c

lzw.o: ../common/lzw.c ../common/lzw.h ../common/htbl.h ../common/bitio.h
	clang -O3 -c ../common/lzw.c

huffman.o: ../common/huffman.c ../common/huffman.h ../common/pqueue.h ../common/bitio.h
	clang -O3 -c ../common/huffman.c

futils.o: ../common/futils.c ../common/futils.h
	clang -O3 -c ../common/futils.c

diter.o: diter.c diter.h
	clang -O3 -c diter.c

queue.o: queue.c queue.h
	clang -O3 -c queue.c

pathtree.o: pathtree.c pathtree.h
	clang -O3 -c pathtree.c

