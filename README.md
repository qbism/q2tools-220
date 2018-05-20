# q2tools-220
Q2 compiler tools featuring ability to bsp v220 map format, automatic phong and soft spotlights, and other enhancements.
Includes qbsp3, qvis3, qrad3, and qdata.

Based on modifications supporting the v220 map format by XaeroX and DarkEssence distributed with the J.A.C.K. level editor.

The code is 'alpha' - not much testing or feedback at this point.

# Fixes and enhancements:
*   AA tools (Alien Arena)- File path determination asumptions:
    *   moddir is parent of whatever directory contains the .map/.bsp
    *   gamedir is parent of moddir
    *   qdir is parent of gamedir

*   AA tools
    *   Use any existing TGA replacement textures for radiosity
    *   Spotlight center to surface point attenuation

*   DWH- Fix for scaled textures using an origin brush.

*   GDD tools (Geoffrey DeWan)
    *   Load file from PAK
    *   Split microbrushes
    *   Radiosity texture checking
    *   Update thread handling

*   jit (Jitspoe)
    *   Fix microbrush deformation

*   KDT- qdata LWO support

*   kmbsp3 (Knightmare bsp tool)
    *   Caulk

*   MaxEd
    *   Texinfo overflow check
    *   VS compiler fixes

*   qbism
    * edge lighting fix 

*   quemap
    *   Face extents

*   VHLT (vluzacn)
    *   Automatic phong smoothing
    *   Add face for vertext normal


# Instructions:

qbsp3
*   v220 support- for Trenchbroom, duplicate or modify the Q2 gametype and change the format to the V-word and add "mapversion" "220" to worldspawn.  JACK does this automatically when saving to v220.

qvis3
*   works the same as always

qrad3
*   -smooth sets the angle (in degrees) for autophong.  Default is 44, so it will phong a 9-sided or more prism, but not 8-sided.  Set to zero to disable.
*   -maxmapdata sets lightng memory limit.  Original is 0x200000 and it can be set up to 0x800000 (8388608).  Requires an engine that supports the higher limit.
*   Any tga replacement textures found will be used for radiosity.


# Build from source:
A Linux makefile and Windows cross-compile from Linux makefiles are included.   Download and build the Windows dependency libraries or find pre-compiled libs.  The pre-compiled libs from the Q2PRO SDK for MinGW-w64 do the trick.


