Xturos
======
A simple little game engine I've been working for doing Ludum Dare competitions. Nothing too fancy.

Dependancies:
SDL2 2.0.9
STB


When using Nuklear you have to have the rendering in the Process for the screen, not the Draw.

Coordinate System Notes:

         0
         ^
     -,- | +,-
         |
270 <----+----> 90
         |
     -,+ | +,+
	     V
		180

cos(0) = 1
cos(90) = 0
cos(180) = -1
cos(270) = 0

sin(0) = 0
sin(90) = 1
sin(180) = 0
sin(270) = -1

<x,y> = <sin(a), -cos(a)>
0 = <0,-1>
90 = <1,0>
180 = <0,1>
270 = <-1,0>

90 deg rot = <-y,x>
-90 deg rot = <y,-x>