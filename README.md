# q2tools-220
Q2 compiler tools featuring ability to bsp v220 map format, automatic phong and soft spotlights, and other enhancements.
Includes qbsp3, qvis3, qrad3, and qdata.

Forked from compiler tools supporting the v220 map format by XaeroX and DarkEssence distributed with the J.A.C.K. level editor.


# Enhancements:

BSP
*   Split microbrushes (GDD tools)
*	Fix for scaled textures using an origin brush. (DWH)
*   Fix for rotation using origin brush (MaxEd)
*   Fix microbrush deformation (Jitspoe)
*   Caulk brushes similar to Q3 mapping (kmbsp3)	
*   Texinfo overflow check (MaxEd)
*   Set chop size for surface lights (qbism - idea from JDolan)
		
vis
*   Increase vis data size max. Issue warning for > vanilla limit. (qbism)
		
radiosity
*   Keep emit_surface active for sky planes when sun is active. Adjust with -sunradscale. (qbism)
*   Use any existing TGA replacement textures for radiosity (AA tools)
*   Spotlight center to surface point attenuation (AA tools)
*   Radiosity texture checking (GDD tools)
*   Face extents (quemap)
*   Edge lighting fix (qbism)	
*   Automatic phong smoothing (vluzacn VHLT)
*   Add face for vertex normal (vluzacn VHLT)
*   Add -dice to subdivide patches with a global grid rather than per patch
*	File path determination asumptions (Alien Arena tools):
    *   moddir is parent of whatever directory contains the .map/.bsp
    *   gamedir is parent of moddir
    *   qdir is parent of gamedir	
	
qdata
*	LWO support (KDT)


# Instructions:

qbsp3
*   v220 support- for Trenchbroom, duplicate or modify the Q2 gametype and change the format to valve and add "mapversion" "220" to worldspawn.  JACK does this automatically when saving to v220.  Note that JACK saves a hybrid map format that includes texture flags whereas standard v220 format does not. Current Trenchbroom source adds this hybrid format load/save.
*   -choplight sets the chop size for surface lights.  Lower settings may improve quality of large surface lights, especially when chop is high. Try "-chop 512 -choplight 32" as an example.
*   -noorigfix disables 'origin fix'.


qvis3
*   works the same as always


qrad3
*   -smooth sets the angle (in degrees) for autophong. Corners between (angle) and (180-angle) will not be phonged.  Default is 44, so it will phong a 9-sided or more prism, but not 8-sided.  Set to zero to disable.
*   -maxmapdata sets lightng memory limit.  Original is 0x200000 and it can be set up to 0x800000 (8388608).  Requires an engine that supports the higher limit.
*	-saturation applies to light reflected from surfaces.  Values < 1.0 desaturate.  Values >1.0 oversaturate. 
*   Any tga replacement textures found will be used for radiosity.
*   -basedir sets the base directory.  Use this if modding other than baseq2.  Defaults to baseq2.
*   -sunradscale sets sky radiosity scale when the sun (directional lighting) is active.  Default is 0.5.


qdata
*   help for this tool is scarce, but it runs a script file to convert assets to Q2 data types.  This example creates the colormap:
    
    $load base/pics/pal.pcx
	
	$colormap colormap 


# Build from source:
Linux-  Makefiles for 32-bit and 64-bit builds of Linux and Windows are included. Assuming a 64-bit build environment, packages lib32z1 and lib32z1-dev are needed to build 32-bit.

Cross-compile- Required packages: mingw-w64, mingw-w64-i686-dev, gcc-multilib.  Pre-compiled Windows dependency libraries are included in /mgw-sdk (borrowed from Q2PRO SDK), or download and build them from scratch.

Windows- Visual Studio sln is included. VS compiler fixes (MaxEd)



