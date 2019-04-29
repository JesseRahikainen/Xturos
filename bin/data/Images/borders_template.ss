1
borders_template.png
48
  0   0 32 32
 32   0 32 32
 64   0 32 32
 96   0 32 32
128   0 32 32
160   0 32 32
192   0 32 32
224   0 32 32

  0  32 32 32
 32  32 32 32
 64  32 32 32
 96  32 32 32
128  32 32 32
160  32 32 32
192  32 32 32
224  32 32 32

  0  64 32 32
 32  64 32 32
 64  64 32 32
 96  64 32 32
128  64 32 32
160  64 32 32
192  64 32 32
224  64 32 32

  0  96 32 32
 32  96 32 32
 64  96 32 32
 96  96 32 32
128  96 32 32
160  96 32 32
192  96 32 32
224  96 32 32

  0 128 32 32
 32 128 32 32
 64 128 32 32
 96 128 32 32
128 128 32 32
160 128 32 32
192 128 32 32
224 128 32 32

  0 160 32 32
 32 160 32 32
 64 160 32 32
 96 160 32 32
128 160 32 32
160 160 32 32
192 160 32 32
224 160 32 32


comments:
tiles are 32x32
https://gamedevelopment.tutsplus.com/tutorials/how-to-use-tile-bitmasking-to-auto-tile-your-level-layouts--cms-25673
they have two copies of the open tile in the example

Each bit represents a filled spot following this format. The template is layed over the map where a filled spot is
and the number is constructed by looking at the adjacent spots.
726
1x0
534
So we can query the neighbors and get a number in this format
0b76543210
This can then be filtered to have redudant data removed if necessary, corner pieces only
matter if the two cardinal pieces surrounding it are also filled, and then a look up table
can be used to determine the index in the sprite list.

The last one is a special case, that is when the center spot is not filled.

0    1    2    3    4    5*1  6*1  7*3
000  000  000  000  010  010  010  010
0x0  0x1  1x0  1x1  0x0  0x1  1x0  1x1
000  000  000  000  000  000  000  000

8    9*1  10*1 11*3 12   13*  14*  15*
000  000  000  000  010  010  010  010
0x0  0x1  1x0  1x1  0x0  0x1  1x0  1x1
010  010  010  010  010  010  010  010

25*  27*  29*  31   42*  43*  46   47
000  000  010  010  000  000  010  010
0x1  1x1  0x1  1x1  1x0  1x1  1x0  1x1
011  011  011  011  110  110  110  110

59*  63   69*  71*  77*  79   93*  95
000  010  011  011  011  011  011  011
1x1  1x1  0x1  1x1  0x1  1x1  0x1  1x1
111  111  000  000  010  010  011  011

111  127  134* 135* 142  143  159  174
011  011  110  110  110  110  110  110
1x1  1x1  1x0  1x1  1x0  1x1  1x1  1x0
110  111  000  000  010  010  011  110

175  191  199* 207  223  239  255
110  110  111  111  111  111  111
1x1  1x1  1x1  1x1  1x1  1x1  1x1
110  111  000  010  011  110  111