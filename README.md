## Archiver with LZW compression

Linux only yet...

Usage:

`$ ./lzwarc [-p password] [-h] a <archive-name> <item1> ...`

-- create archive with the listed items (files/directories)

-- `-h` selects Huffman encoder instead of LZW

`$ ./lzwarc [-p password] x <archive-name> [<dest-path> [<item1> ...]]`

-- extract from archive to <dest-path> (default: ./) the listed items (default: all)

`$ ./lzwarc l <arhive-name>`

-- list contents
