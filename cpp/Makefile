lzwarc: arc.o lzw.o huffman.o futils.o
	clang++ -o lzwarc arc.o lzw.o huffman.o futils.o -lpthread -lstdc++fs

arc.o: arc.cc lzw.h huffman.h futils.h tqueue.h misc.h
	clang++ -std=c++17 -c arc.cc

lzw.o: lzw.c lzw.h htbl.h bitio.h
	clang -c lzw.c

huffman.o: huffman.c huffman.h pqueue.h bitio.h
	clang -c huffman.c

futils.o: futils.c futils.h
	clang -c futils.c

