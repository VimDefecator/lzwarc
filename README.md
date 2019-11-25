## Archiver with LZW compression

Linux only.

Usage:

`$ ./lzwarc [-p password] [-h] a <archive-name> <item1> ...`

-- create archive with the listed files/directories or add to existing one

-- `-h` selects Huffman encoder instead of LZW (ignored if archive-name exists)

`$ ./lzwarc [-p password] x <archive-name> [<dest-path> [<item1> ...]]`

-- extract from archive to dest-path (default: ./) the listed items (default: all)

-- specify empty string `""` as item to start visual selection (VT100 required)

`$ ./lzwarc l <arhive-name>`

-- list contents
