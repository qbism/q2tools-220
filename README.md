# q2tools-220
Q2 compiler tools featuring ability to bsp v220 map format, automatic phong and soft spotlights, and other enhancements.
Includes 4bsp, 4vis, 4rad, and 4data.

Forked from compiler tools supporting the v220 map format by XaeroX and DarkEssence distributed with the J.A.C.K. level editor.

# Test feature:  
Extended map limits for very large maps patterned after the Quake BSP2 format.  Several 16-bit data types are changed to 32-bit.  This feature requires a supporting engine.  See common/qfiles.h for differences in limits.

Usage:  Add -qbsp to the 4bsp command line.  4vis and 4rad will detect QBSP automatically.



# Enhancements:

bsp
*   Split microbrushes (GDD tools)
*	Fix for scaled textures using an origin brush. (DWH)
*   Fix for rotation using origin brush (MaxEd)
*   Fix microbrush deformation (Jitspoe)
*   Caulk brushes similar to Q3 mapping (kmbsp3)	
*   Texinfo overflow check (MaxEd)
*   Set chop size for surface lights (qbism - idea from JDolan)
*   Increased limits within standard file format.  Requires a supporting engine. (kmqbsp3)
*   Fix so SURF_NODRAW flagged faces are not included in texture processing. (Paril)
*   Feature: set CONTENTS_AUX flag with CONTENTS_MIST to disable rendering of mist backfaces.
*   Extreme limits with extended file format. Future supporting engine. (ideas from motorsep, Paril, Knightmare)
		
vis
*   Increase vis data size max. Issue warning for > vanilla limit. (qbism)
		
radiosity
*   Keep emit_surface active for sky planes when sun is active. Adjust with -sunradscale. (qbism)
*   Use any existing TGA replacement textures for radiosity (AA tools)
*   Spotlight center to surface point attenuation (AA tools)
*   Add \_falloff property for point lights (blarghrad)
*   Radiosity texture checking (GDD tools)
*   Face extents (quemap)
*   Edge lighting fix (qbism)	
*   Automatic phong smoothing (vluzacn VHLT)
*   Add face for vertex normal (vluzacn VHLT)
*   Add -nudge to adjust sample nudge distance for -extra (qbism)
*   Add -dice to subdivide patches with a global grid rather than per patch
*	File path determination asumptions (Alien Arena tools):
    *   moddir is parent of whatever directory contains the .map/.bsp
    *   gamedir is parent of moddir
    *   qdir is parent of gamedir	
	
4data
*	LWO support (KDT)


# Instructions:

4bsp
*   v220 support- for Trenchbroom, duplicate or modify the Q2 gametype and change the format to valve and add "mapversion" "220" to worldspawn.  JACK does this automatically when saving to v220.  Note that JACK saves a hybrid map format that includes texture flags whereas standard v220 format does not. Current Trenchbroom source adds this hybrid format load/save.
*   -choplight sets the chop size for surface lights.  Lower settings may improve quality of large surface lights, especially when chop is high. Try "-chop 512 -choplight 32" as an example.
*   -noorigfix disables 'origin fix'.
*   -largebounds: Increase max map size for supporting engines.
*   -moreents: Increase max number of entities for supporting engines.


4vis
*   It works the same as always. -fast for a quick single pass.


4rad
*   -smooth sets the angle (in degrees) for autophong. Applies to convex and concave corners. Corners between (angle) and (180-angle) will not be phonged.  Default is 44, so it will phong a 9-sided or more prism, but not 8-sided.  Set to zero to disable.
*   -maxmapdata sets lightng memory limit.  Original is 0x200000 and it can be set up to 0x800000 (8388608).  Requires an engine that supports the higher limit.
*	-saturation applies to light reflected from surfaces.  Values < 1.0 desaturate.  Values >1.0 oversaturate. 
*   Any tga replacement textures found will be used for radiosity.
*   -basedir sets the base directory.  Use this if modding other than baseq2.  Defaults to baseq2.
*   -sunradscale sets sky radiosity scale when the sun (directional lighting) is active.  Default is 0.5.
*   -nudge sets the fractional distance from face center when extra lighting samples are used (-extra).  Default is 0.25.
*   _falloff property values; intensity - distance), 1 (inverse; intensity/distance), 2 (inverse-square; intensity/dist*dist)  default: 0  Note that inverse and inverse-square falloff require very high brightness values to be visible.


4data
*   Runs a script file to convert assets to Q2 data types.  This example creates the colormap:
    
    $load base/pics/pal.pcx
	$colormap colormap 

Compile a model from individual .tri, .3ds, or .lwo frames. Example:
    $cd monsters/berserk
    $origin 0 0 24
    $base base
    $skin skin
    $skin pain

    //idle
    $frame stand1 stand2 stand3 stand4 stand5
    $frame standb1 standb2 standb3 standb4 standb5 standb6 standb7 standb8 standb9 standb10
    $frame standb11 standb12 standb13 standb14 standb15 standb16 standb17 standb18 standb19 standb20

    //walk
    //$frame walk1 walk2 walk3 walk4 walk5 walk6 walk7 walk8 walk9 walk10
    ...etc.


# Build from source:
Linux-  Makefiles for 32-bit and 64-bit builds of Linux and Windows are included. Assuming a 64-bit build environment, packages lib32z1 and lib32z1-dev are needed to build 32-bit.

Cross-compile- Required packages: mingw-w64, mingw-w64-i686-dev, gcc-multilib.  Pre-compiled Windows dependency libraries are included in /mgw-sdk (borrowed from Q2PRO SDK), or download and build them from scratch.

Windows- Visual Studio sln is included. VS compiler fixes (Paril, MaxEd)



