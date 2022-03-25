2
borders_template.png
bt00   0   0 32 32
bt01  32   0 32 32
bt02  64   0 32 32
bt03  96   0 32 32
bt04 128   0 32 32
bt05 160   0 32 32
bt06 192   0 32 32
bt07 224   0 32 32

bt08   0  32 32 32
bt09  32  32 32 32
bt10  64  32 32 32
bt11  96  32 32 32
bt12 128  32 32 32
bt13 160  32 32 32
bt14 192  32 32 32
bt15 224  32 32 32

bt16   0  64 32 32
bt17  32  64 32 32
bt18  64  64 32 32
bt19  96  64 32 32
bt20 128  64 32 32
bt21 160  64 32 32
bt22 192  64 32 32
bt23 224  64 32 32

bt24   0  96 32 32
bt25  32  96 32 32
bt26  64  96 32 32
bt27  96  96 32 32
bt28 128  96 32 32
bt29 160  96 32 32
bt30 192  96 32 32
bt31 224  96 32 32

bt32   0 128 32 32
bt33  32 128 32 32
bt34  64 128 32 32
bt35  96 128 32 32
bt36 128 128 32 32
bt37 160 128 32 32
bt38 192 128 32 32
bt39 224 128 32 32

bt40   0 160 32 32
bt41  32 160 32 32
bt42  64 160 32 32
bt43  96 160 32 32
bt44 128 160 32 32
bt45 160 160 32 32
bt46 192 160 32 32
bt47 224 160 32 32


# comments:
# tiles are 32x32
# https://gamedevelopment.tutsplus.com/tutorials/how-to-use-tile-bitmasking-to-auto-tile-your-level-layouts--cms-25673
# they have two copies of the open tile in the example
# 
# Each bit represents a filled spot following this format. The template is layed over the map where a filled spot is
# and the number is constructed by looking at the adjacent spots.
# 726
# 1x0
# 534
# So we can query the neighbors and get a number in this format
# 0b76543210
# This can then be filtered to have redudant data removed if necessary, corner pieces only
# matter if the two cardinal pieces surrounding it are also filled, and then a look up table
# can be used to determine the index in the sprite list.
# 
# The last one is a special case, that is when the center spot is not filled.
# 
# 0    1    2    3    4    5*1  6*1  7*3
# 000  000  000  000  010  010  010  010
# 0x0  0x1  1x0  1x1  0x0  0x1  1x0  1x1
# 000  000  000  000  000  000  000  000
# 
# 8    9*1  10*1 11*3 12   13*  14*  15*
# 000  000  000  000  010  010  010  010
# 0x0  0x1  1x0  1x1  0x0  0x1  1x0  1x1
# 010  010  010  010  010  010  010  010
# 
# 25*  27*  29*  31   42*  43*  46   47
# 000  000  010  010  000  000  010  010
# 0x1  1x1  0x1  1x1  1x0  1x1  1x0  1x1
# 011  011  011  011  110  110  110  110
# 
# 59*  63   69*  71*  77*  79   93*  95
# 000  010  011  011  011  011  011  011
# 1x1  1x1  0x1  1x1  0x1  1x1  0x1  1x1
# 111  111  000  000  010  010  011  011
# 
# 111  127  134* 135* 142  143  159  174
# 011  011  110  110  110  110  110  110
# 1x1  1x1  1x0  1x1  1x0  1x1  1x1  1x0
# 110  111  000  000  010  010  011  110
# 
# 175  191  199* 207  223  239  255
# 110  110  111  111  111  111  111
# 1x1  1x1  1x1  1x1  1x1  1x1  1x1
# 110  111  000  010  011  110  111
