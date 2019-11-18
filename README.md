## Archiver with LZW compression

Linux only yet...

Installation:

`
$ make

$ ./install.sh
`

Usage:

`$ lzwarc [-p password] [-h] a <archive-name> <item1> ...`

-- create archive with the listed files/directories or add to existing one

-- `-h` selects Huffman encoder instead of LZW (ignored if archive-name exists)

`$ lzwarc [-p password] x <archive-name> [<dest-path> [<item1> ...]]`

-- extract from archive to dest-path (default: ./) the listed items (default: all)

`$ lzwarc l <arhive-name>`

-- list contents
