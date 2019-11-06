## Archiver with LZW compression

Linux only yet...

Usage:

`$ ./lzwarc a archivename item1 item2 ...`

`$ ./lzwarc ap password archivename item1 item2 ...`

 -- add listed items (files/directories) to archive

`$ ./lzwarc x archivename [dest_dir/ [pref1 pref2 ...]]`

`$ ./lzwarc xp password archivename [dest_dir/ [pref1 pref2 ...]]`

-- extract files to which internal path begins with one of given prefixes (defaults to any) to dest_dir/ (defaults to current)

`$ ./lzwarc l archivename [pref1 pref2 ...]`

-- list contents to which internal path begins with one of given prefixes (defaults to any)

Underscored one is the multithreaded version.