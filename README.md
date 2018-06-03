# q2tools-220
Q2 compiler tools featuring ability to bsp v220 map format, automatic phong and soft spotlights, and other enhancements.
Includes qbsp3, qvis3, qrad3, and qdata.

Based on modifications supporting the v220 map format by XaeroX and DarkEssence distributed with the J.A.C.K. level editor.


# Enhancements:

*	File path determination asumptions (Alien Arena tools):
    *   moddir is parent of whatever directory contains the .map/.bsp
    *   gamedir is parent of moddir
    *   qdir is parent of gamedir
*   Load files from PAK (GDD tools)
*	VS compiler fixes

BSP
*   Split microbrushes (GDD tools)
*	Fix for scaled textures using an origin brush. (DWH)
*   Fix microbrush deformation (Jitspoe)
*   Caulk brushes similar to Q3 mapping (kmbsp3)	
*   Texinfo overflow check (MaxEd)
		
radiosity
*   Use any existing TGA replacement textures for radiosity (AA tools)
*   Spotlight center to surface point attenuation (AA tools)
*   Radiosity texture checking (GDD tools)
*   Face extents (quemap)
*   Edge lighting fix (qbism)	
*   Automatic phong smoothing (vluzacn VHLT)
*   Add face for vertext normal (vluzacn VHLT)
	
qdata
*	LWO support (KDT)


# Instructions:

qbsp3
*   v220 support- for Trenchbroom, duplicate or modify the Q2 gametype and change the format to valve and add "mapversion" "220" to worldspawn.  JACK does this automatically when saving to v220.

qvis3
*   works the same as always

qrad3
*   -smooth sets the angle (in degrees) for autophong.  Default is 44, so it will phong a 9-sided or more prism, but not 8-sided.  Set to zero to disable.
*   -maxmapdata sets lightng memory limit.  Original is 0x200000 and it can be set up to 0x800000 (8388608).  Requires an engine that supports the higher limit.
*	-saturation applies to light reflected from surfaces.  Values < 1.0 desaturate.  Values >1.0 oversaturate.  
*   Any tga replacement textures found will be used for radiosity.

qdata
*   help for this tool is scarce, but it runs a script file to convert assets to Q2 data types.  This example creates the colormap:
    
    $load base/pics/pal.pcx
    $colormap colormap 

# Build from source:
A Linux makefile and Windows cross-compile from Linux makefiles are included. Download and build the Windows dependency libraries or find pre-compiled libs.  The pre-compiled libs from the Q2PRO SDK for MinGW-w64 do the trick.
Visual Studio sln is included.

