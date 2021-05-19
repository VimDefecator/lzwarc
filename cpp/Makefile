lzwarc: arc.o lzw.o huffman.o futils.o queue.o
	clang++ -o lzwarc arc.o lzw.o huffman.o futils.o queue.o -lpthread -lstdc++fs

arc.o: arc.cc lzw.h huffman.h futils.h queue.h misc.h
	clang++ -std=c++17 -c arc.cc

lzw.o: lzw.c lzw.h htbl.h bitio.h
	clang -c lzw.c

huffman.o: huffman.c huffman.h pqueue.h bitio.h
	clang -c huffman.c

futils.o: futils.c futils.h
	clang -c futils.c

queue.o: queue.c queue.h
	clang -c queue.c

