# q2tools-220
Q2 compiler tools featuring ability to bsp v220 map format, automatic phong and soft spotlights, and other enhancements.

Forked from compiler tools supporting the v220 map format by XaeroX and DarkEssence distributed with the J.A.C.K. level editor.

Additional documentation, notes, and examples on social media:  https://3v.is/notice/ASqLl2Veho7QL0eX5c

Automated development builds for Linux, Windows, Mac:  green checkmark -> details (for any build) ->summary


# Usage: q2tool [mode] [options] [file]

```
    -moddir [path]: Set a mod directory. Default is parent dir of map file.
    -basedir [path]: Set the directory for assets not in moddir. Default is moddir.
    -gamedir [path]: Set game directory, the folder with game executable.
    -v: Display more verbose output.
    -threads #: number of CPU threads to use

BSP pass:
    -bsp: enable bsp pass, requires a .map file as input
    -chop #: Subdivide size.
        Default: 240  Range: 32-1024
    -choplight #: Subdivide size for surface lights.
        Default: 240  Range: 32-1024
    -largebounds or -lb: Increase max map size for supporting engines.
    -micro #: Minimum microbrush size. Default: 0.02
        Suggested range: 0.02 - 1.0
    -nosubdiv: Disable subdivision.
    -qbsp: Greatly expanded map and entity limits for supporting engines.
bsp debugging options:
    -block # #: Division tree block size, square
    -blocks # # # #: Div tree block size, rectangular
    -blocksize: map cube size for processing. Default: 1024
    -fulldetail: Change most brushes to detail.
    -leaktest: Perform leak test only.
    -nocsg: No constructive solid geometry.
    -nodetail: No detail brushes.
    -nomerge: Don't merge visible faces per node.
    -noorigfix: Disable texture fix for origin offsets.
    -noprune: Disable node pruning.
    -noshare: Don't look for shared edges on save.
    -noskipfix: Do not automatically set skip contents to zero.
    -notjunc: Disable edge cleanup.
    -nowater: Ignore warp surfaces.
    -noweld: Disable vertex welding.
    -onlyents: Grab the entites and resave.

VIS pass:
    -vis: enable vis pass, requires a .bsp file as input or bsp pass enabled
    -fast: fast single vis pass

RAD pass:
    -rad: enable rad pass, requires a .bsp file as input or bsp and vis passes enabled
    -ambient #: Minimum light level.
         range:  0 to 255.
    -moddir [path]: Set a mod directory. Default is parent dir of map file.
    -basedir [path]: Set the directory for assets not in moddir. Default is moddir.
    -gamedir [path]: Set game directory, the folder with game executable.
    -bounce #: Max number of light bounces for radiosity.
    -dice: Subdivide patches with a global grid rather than per patch.
    -direct #: Direct light scale factor.
    -entity #: Entity light scale factor.
    -extra: Use extra samples to smooth lighting.
    -maxdata #: 2097152 is default max. Not needed for QBSP format.
         Increase requires a supporting engine.
    -maxlight #: Maximium light level.
         range:  0 to 255.
    -noedgefix: disable dark edges at sky fix. More of a hack, really.
    -nudge #: Nudge factor for samples. Distance fraction from center.
    -saturate #: Saturation factor of light bounced off surfaces.
    -scale #: Light intensity multiplier.
    -smooth #: Threshold angle (# and 180deg - #) for phong smoothing.
    -subdiv #: Maximum patch size.  Default: 64
    -sunradscale #: Sky light intensity scale when sun is active.
    -threads #:  Number of CPU cores to use.
rad debugging options:
    -dump: Dump patches to a text file.
    -noblock: Brushes don't block lighting path.
    -nopvs:  Don't do potential visibility set check.
    -savetrace: Test traces and report errors.

```

# Enhancements:

-bsp
*   Alphatest surface flag (fence textures) for supporting engines
*   N64 style scroll flags for supporting engines	
*   Caulk brushes similar to Q3 mapping (kmbsp3)	
*   Caulk brushes similar to Q3 mapping (kmbsp3)	
*   Set chop size for surface lights (qbism - idea from JDolan)
*   Fix so SURF_NODRAW flagged faces are not included in texture processing. (Paril)
*   Feature: set CONTENTS_AUX flag with CONTENTS_MIST to disable rendering of mist backfaces.
*   Increased limits within standard file format.  Requires a supporting engine. (kmqbsp3)
*   Increased map limits with extended file format. Future supporting engine. (ideas from motorsep, Paril, Knightmare)
*   Split microbrushes (GDD tools)
*   Fix for scaled textures using an origin brush. (DWH)
*   Fix for rotation using origin brush (MaxEd)
*   Fix microbrush deformation (Jitspoe)
*   Texinfo overflow check (MaxEd)
		
-vis
*   Increase vis data size max. Issue warning for > vanilla limit. (qbism)
		
-rad
*   Reduce dark banding at sky: Faces are never behind the sky.  (idea from ericw-tools)
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
*	File path determination asumptions:
    *   moddir is parent of whatever directory contains the .map/.bsp
    *   basedir is moddir unless set explicitly
    *   gamedir is parent of moddir
    *   qdir is parent of gamedir	
	
-data
*	LWO support (KDT)

Directory commands (applies to all tools)
*   -moddir:  Set a mod directory.  Default is parent directory of the map file.
*   -basedir: Set the base data directory for assets not found in moddir.  Default is moddir.
*   -gamedir: Set game directory, the folder with game executable.  Default is parent of basedir.

Based on an example provided by aapokaapo:
My editor is in C:/Games/Trenchbroom/
My '.map'-files are in C:/Games/Trenchbroom/maps
My compilers are in C:/Games/Trenchbroom/tools/
My game/modfiles are in C:/Games/Paintball2/pball/ (my baseq2 dir)

../Trenchbroom/tools/q2tool -rad -basedir C:/Games/Paintball2/pball -gamedir C:/Games/Paintball2 ../Trenchbroom/maps/mymap
=> Compiler finds all the game files and compiles the map correctly


# Notes:

bsp
*   v220 (Valve) support: Trenchbroom and JACK editors can open and save a hybrid format that preserves texture flags.
*   chop:  Usually, higher is better and faster.  Start at 1024 and work down if any issues.  
*   choplight: Set the chop size independetly for surface lights.  Lower settings may improve quality of large surface lights when chop is high. Try "choplight 16".
*   -largebounds: Increase max map size for supporting engines.
*   -moreents: Increase max number of entities for supporting engines.

vis
*   It works the same as always. -fast for a quick single pass.

rad
*   -smooth sets the angle (in degrees) for autophong. Applies to convex and concave corners. Corners between (angle) and (180-angle) will not be phonged.  Default is 44, so it will phong a 9-sided or more prism, but not 8-sided.  Set to zero to disable.
*   -maxmapdata sets lightng memory limit.  Original is 0x200000 and it can be set up to 0x800000 (8388608).  Requires an engine that supports the higher limit.
*	-saturation applies to light reflected from surfaces.  Values < 1.0 desaturate.  Values >1.0 oversaturate. 
*   Any tga replacement textures found will be used for radiosity.
*   -sunradscale sets sky radiosity scale when the sun (directional lighting) is active.  Default is 0.5.
*   -nudge sets the fractional distance from face center when extra lighting samples are used (-extra).  Default is 0.25.
*   _falloff property values; intensity - distance), 1 (inverse; intensity/distance), 2 (inverse-square; intensity/dist*dist)  default: 0  Note that inverse and inverse-square falloff require very high brightness values to be visible.

data
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

# qbsp format:  
Extended map limits for larger or more detailed maps.  Several 16-bit data types are changed to 32-bit.  This feature requires a supporting engine.  See common/qfiles.h for differences in limits.

Usage:  Add -qbsp to the bsp command line.  vis and rad will detect QBSP automatically.  No released engine supports this yet.  See https://github.com/qbism/qb2 for prototype code.


# Build from source in Linux:
Linux-  
mkdir build  
cd build  
cmake ..  

Windows-  
mkdir buildwin  
cd buildwin  
cmake -DCMAKE_TOOLCHAIN_FILE=../win64.cmake .. 

Cross-compile requires packages: mingw-w64, mingw-w64-i686-dev, gcc-multilib, and libz-mingw-w64-dev.  

Testing Windows in Linux with wine if default is 32-bit:  
WINEARCH=win64 WINEPREFIX=~/64bitprefix wine q2tool.exe
