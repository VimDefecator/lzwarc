lzwarc: arc.o lzw.o huffman.o futils.o
	clang++ -o lzwarc arc.o lzw.o huffman.o futils.o -lpthread -lstdc++fs

arc.o: arc.cc ../common/lzw.h ../common/huffman.h ../common/futils.h tqueue.h
	clang++ -std=c++17 -O3 -c arc.cc

lzw.o: ../common/lzw.c ../common/lzw.h ../common/htbl.h ../common/bitio.h
	clang -O3 -c ../common/lzw.c

huffman.o: ../common/huffman.c ../common/huffman.h ../common/pqueue.h ../common/bitio.h
	clang -O3 -c ../common/huffman.c

futils.o: ../common/futils.c ../common/futils.h
	clang -O3 -c ../common/futils.c

