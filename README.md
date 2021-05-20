## Archiver with LZW compression

C version requires Linux

Usage:

`$ ./lzwarc [-p password] [-h[8]] a <archive-name> <item1> ...`

-- create archive with the listed files/directories or add to existing one

-- `-h` selects Huffman encoder instead of LZW (ignored if archive-name exists)

-- `-h8` combines LZ8 and Huffman encoders (only in C version)

`$ ./lzwarc [-p password] x <archive-name> [<dest-path> [<item1> ...]]`

-- extract from archive to dest-path (default: ./) the listed items (default: all)

-- specify empty string `""` as item to start visual selection (only in C version)

`$ ./lzwarc l <arhive-name>`

-- list contents
