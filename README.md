This is a command line program that attempts to minimize the size of PNG files. It uses a brute force method to find a PNG filtering that potentially compresses better than using just one filtering method or using a heuristic to find good filtering. Zopfli is used to compress the filtered data to achieve even smaller PNG size.  
Additionally, the PNG filter 0 (no filter) and the default PNG heuristic filtering are tested to see if they would compress better than the brute force method. Zlib compression strategies 2 (HUFFMAN_ONLY) and 3 (RLE) are also tested as those can sometimes achieve better compression than Zopfli and are also very fast to test.

At the moment this software will only recompress the data in the IDAT chunks and leave everything else untouched. Other kinds of optimizations like converting to greyscale/palette and discarding unneeded transparency are not implemented.  
Currently **does not support interlaced** images but all other PNG files should be supported.

This software requires libpng and zlib to compile. It also has optional OpenMP support (compile with "make openmp") to test different filtering and compression options in parallel.

**Usage:**

Give input file as the last argument.

**Compression settings:**  
-p0 -> -p6 settings preset. individual settings can be separately overridden. (default -p3 -w500 -i6 -f12 -f20 -s2)  
-w1 -> -w999... compression testing window in bytes. window is automatically increased to cover at least one scanline  
-i1 -> -i255 iteration count for the Zopfli compressor  
-f10 -> -f14 ; -f20 -> -f24 set the first or second filtering level  
-s0 -> -s3 set the level of tested filtering options and compression settings

Minimum setting is at least level 1 for at least one of the filtering options (f11 or f21)  
This is automatically enforced if selected settings are less than that

**Other options:**  
-b benchmark mode which is slightly faster and writes an arbitrary output file with correct length (forces -k and -o)  
-d dry run that will not write files. overrides -b setting  
-k keep the original file instead of overwriting. new file is named \[name\]_2.png  
-m run only if the input file has multiple IDAT chunks (PNG files tend to have multiple IDATs while PNG optimizers generate only one IDAT)  
-o allow overwriting the output file with -k (still doesn't write if the result is worse)

**Logging level:**  
-l0 no output unless there are errors (errors are always printed)  
-l1 output one line once finished  
-l2 output one line at the beginning and once finished  
-l3 a lot of logging about progress and stats (default)
