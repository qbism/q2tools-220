# q2tools-220
Quake 2 compiler tools with v220 map support, automatic phong and soft spotlights, other enhancements, and fixes. 

Based on modifications by XaeroX and DarkEssence distributed with J.A.C.K. map editor by Chain Studios.

The code is 'alpha' - not much testing or feedback at this point.  There's an sln for Windows but has not been tried recently.

Features and credit:
Better radiosity, from AA.
Use any existing TGA replacement textures for radiosity, from AA.

Path determination from AAtools. Assumes:
*   moddir is parent of whatever directory contains the .map/.bsp
*   gamedir is parent of moddir
*   qdir is parent of gamedir

Change default max_map_lighting of 0x200000 with -maxdata #.  Some engines use higher values like 0x800000.
* example:  -maxdata 8388608

Fixes:
*   Varous fixes and enhancements from Geoffrey DeWan.
*   Microbrush and others from Jitspoe.
*   DWH- Fix for scaled textures using an origin brush
