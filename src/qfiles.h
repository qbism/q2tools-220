/*
===========================================================================
Copyright (C) 1997-2006 Id Software, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
===========================================================================
*/

//
// qfiles.h: file formats
// This file must be identical in the engine and utils directories
//

/*
========================================================================

The .pak files are just a linear collapse of a directory tree

========================================================================
*/

#define IDPAKHEADER (('K' << 24) + ('C' << 16) + ('A' << 8) + 'P')

typedef struct
{
    char name[56];
    int32_t filepos, filelen;
} dpackfile_t;

typedef struct
{
    int32_t ident; // == IDPAKHEADER
    int32_t dirofs;
    int32_t dirlen;
} dpackheader_t;

#define MAX_FILES_IN_PACK 4096

/*
========================================================================

PCX files are used for as many images as possible

========================================================================
*/

typedef struct
{
    char manufacturer;
    char version;
    char encoding;
    char bits_per_pixel;
    uint16_t xmin, ymin, xmax, ymax;
    uint16_t hres, vres;
    uint8_t palette[48];
    char reserved;
    char color_planes;
    uint16_t bytes_per_line;
    uint16_t palette_type;
    char filler[58];
    uint8_t data; // unbounded
} pcx_t;

/*
========================================================================

.MD2 triangle model file format

========================================================================
*/

#define IDALIASHEADER (('2' << 24) + ('P' << 16) + ('D' << 8) + 'I')
#define ALIAS_VERSION 8

#define MAX_TRIANGLES 4096
#define MAX_VERTS     2048
#define MAX_FRAMES    512
#define MAX_MD2SKINS  32
#define MAX_SKINNAME  64

typedef struct
{
    short s;
    short t;
} dstvert_t;

typedef struct
{
    short index_xyz[3];
    short index_st[3];
} dtriangle_t;

typedef struct
{
    byte v[3]; // scaled byte to fit in frame mins/maxs
    byte lightnormalindex;
} dtrivertx_t;

#define DTRIVERTX_V0   0
#define DTRIVERTX_V1   1
#define DTRIVERTX_V2   2
#define DTRIVERTX_LNI  3
#define DTRIVERTX_SIZE 4

typedef struct
{
    float scale[3];       // multiply byte verts by this
    float translate[3];   // then add this
    char name[16];        // frame name from grabbing
    dtrivertx_t verts[1]; // variable sized
} daliasframe_t;

// the glcmd format:
// a positive integer starts a tristrip command, followed by that many
// vertex structures.
// a negative integer starts a trifan command, followed by -x vertexes
// a zero indicates the end of the command list.
// a vertex consists of a floating point s, a floating point t,
// and an integer vertex index.

typedef struct
{
    int32_t ident;
    int32_t version;

    int32_t skinwidth;
    int32_t skinheight;
    int32_t framesize; // byte size of each frame

    int32_t num_skins;
    int32_t num_xyz;
    int32_t num_st; // greater than num_xyz for seams
    int32_t num_tris;
    int32_t num_glcmds; // dwords in strip/fan command list
    int32_t num_frames;

    int32_t ofs_skins;  // each skin is a MAX_SKINNAME string
    int32_t ofs_st;     // byte offset from start for stverts
    int32_t ofs_tris;   // offset for dtriangles
    int32_t ofs_frames; // offset for first frame
    int32_t ofs_glcmds;
    int32_t ofs_end; // end of file

} dmdl_t;

/*
========================================================================

.SP2 sprite file format

========================================================================
*/

#define IDSPRITEHEADER (('2' << 24) + ('S' << 16) + ('D' << 8) + 'I')
// little-endian "IDS2"
#define SPRITE_VERSION 2

typedef struct
{
    int32_t width, height;
    int32_t origin_x, origin_y; // raster coordinates inside pic
    char name[MAX_SKINNAME];    // name of pcx file
} dsprframe_t;

typedef struct {
    int32_t ident;
    int32_t version;
    int32_t numframes;
    dsprframe_t frames[1]; // variable sized
} dsprite_t;

/*
==============================================================================

  .WAL texture file format
  .M8  //qb: Heretic II support from q2map

==============================================================================
*/

#define MIPLEVELS 4
typedef struct miptex_s {
    char name[32];
    unsigned width, height;
    unsigned offsets[MIPLEVELS]; // four mip maps stored
    char animname[32];           // next frame in animation chain
    int32_t flags;
    int32_t contents;
    int32_t value;
} miptex_t;

#define PAL_SIZE        256
#define H2_MIPLEVELS        16

typedef struct palette_s
{
	union
	{
		struct
		{
			byte r,g,b;
		};
	};
} palette_t;

typedef struct miptex_m8_s
{
	int32_t version;
	char name[32];
	unsigned width[H2_MIPLEVELS], height[H2_MIPLEVELS];
	unsigned offsets[H2_MIPLEVELS];         // four mip maps stored
	char animname[32];                  // next frame in animation chain
	palette_t palette[PAL_SIZE];
	int32_t flags;
	int32_t contents;
	int32_t value;
} miptex_m8_t;


#define MIP32_VERSION   4
#define MIP32_NOMIP_FLAG2       0x00000001
#define MIP32_DETAILER_FLAG2        0x00000002
typedef struct miptex_m32_s
{
	int32_t version;
	char name[128];
	char altname[128];                  // texture substitution
	char animname[128];                 // next frame in animation chain
	char damagename[128];               // image that should be shown when damaged
	unsigned width[H2_MIPLEVELS], height[H2_MIPLEVELS];
	unsigned offsets[H2_MIPLEVELS];
	int32_t flags;
	int32_t contents;
	int32_t value;
	float scale_x, scale_y;
	int32_t mip_scale;

	// detail texturing info
	char dt_name[128];              // detailed texture name
	float dt_scale_x, dt_scale_y;
	float dt_u, dt_v;
	float dt_alpha;
	int32_t dt_src_blend_mode, dt_dst_blend_mode;

	int32_t flags2;
	int32_t unused[19];                     // future expansion to maintain compatibility with h2
} miptex_m32_t;


/*
==============================================================================

  .BSP file format

==============================================================================
*/

#define IDBSPHEADER              (('P' << 24) + ('S' << 16) + ('B' << 8) + 'I')
// little-endian "IBSP"

// qb: qbsp
#define QBSPHEADER               ('Q' | ('B' << 8) | ('S' << 16) | ('P' << 24))

#define BSPVERSION               38

// upper design bounds
// leaffaces, leafbrushes, planes, and verts are still bounded by
// 16 bit short limits
#define MAX_MAP_MODELS           1024
#define MAX_MAP_BRUSHES          8192
#define DEFAULT_MAP_ENTITIES     2048 // qb: from kmqbsp3- Knightmare- old limit
#define MAX_MAP_ENTITIES         8192 // qb: KMQ2
#define MAX_MAP_ENTSTRING        262144
#define DEFAULT_MAP_TEXINFO      8192  // mxd: vanilla
#define MAX_MAP_TEXINFO          16384 // mxd: KMQ2

#define MAX_MAP_AREAS            256
#define MAX_MAP_AREAPORTALS      1024
#define MAX_MAP_PLANES           65536
#define MAX_MAP_NODES            65536
#define MAX_MAP_BRUSHSIDES       65536
#define MAX_MAP_LEAFS            65536
#define MAX_MAP_VERTS            65536
#define MAX_MAP_FACES            65536
#define MAX_MAP_LEAFFACES        65536
#define MAX_MAP_LEAFBRUSHES      65536
#define MAX_MAP_PORTALS          65536
#define MAX_MAP_EDGES            128000
#define MAX_MAP_SURFEDGES        256000
#define DEFAULT_MAP_LIGHTING     0x200000 // qb: vanilla
#define MAX_MAP_LIGHTING         0x800000 // qb: q2pro
#define DEFAULT_MAP_VISIBILITY   0x100000
#define MAX_MAP_VISIBILITY       0x400000

// qb: map bounds are +/- MAX
#define DEFAULT_MAP_SIZE         4096
#define MAX_MAP_SIZE             32768
#define MAX_BLOCK_SIZE           8192
#define MAX_POINTS_HASH          MAX_MAP_SIZE / 64

// qb: qbsp limits
#define WARN_MAP_MODELS_QBSP     32768
#define MAX_MAP_MODELS_QBSP      131072
#define MAX_MAP_BRUSHES_QBSP     1048576
#define WARN_MAP_ENTITIES_QBSP   32768
#define MAX_MAP_ENTITIES_QBSP    131072
#define MAX_MAP_ENTSTRING_QBSP   13631488
#define MAX_MAP_TEXINFO_QBSP     1048576
#define MAX_MAP_PLANES_QBSP      1048576
#define MAX_MAP_NODES_QBSP       1048576
#define MAX_MAP_LEAFS_QBSP       1048576
#define MAX_MAP_VERTS_QBSP       4194304
#define MAX_MAP_FACES_QBSP       1048576
#define MAX_MAP_LEAFFACES_QBSP   1048576
#define MAX_MAP_LEAFBRUSHES_QBSP 1048576
#define MAX_MAP_EDGES_QBSP       1048576
#define MAX_MAP_BRUSHSIDES_QBSP  4194304
#define MAX_MAP_PORTALS_QBSP     1048576
#define MAX_MAP_SURFEDGES_QBSP   4194304
#define MAX_MAP_LIGHTING_QBSP    54525952
#define MAX_MAP_VISIBILITY_QBSP  0x8000000

// key / value pair sizes

#define MAX_KEY                  32
#define MAX_VALUE                1024

//=============================================================================

typedef struct
{
    int32_t fileofs, filelen;
} lump_t;

#define LUMP_ENTITIES    0
#define LUMP_PLANES      1
#define LUMP_VERTEXES    2
#define LUMP_VISIBILITY  3
#define LUMP_NODES       4
#define LUMP_TEXINFO     5
#define LUMP_FACES       6
#define LUMP_LIGHTING    7
#define LUMP_LEAFS       8
#define LUMP_LEAFFACES   9
#define LUMP_LEAFBRUSHES 10
#define LUMP_EDGES       11
#define LUMP_SURFEDGES   12
#define LUMP_MODELS      13
#define LUMP_BRUSHES     14
#define LUMP_BRUSHSIDES  15
#define LUMP_POP         16
#define LUMP_AREAS       17
#define LUMP_AREAPORTALS 18
#define HEADER_LUMPS     19

typedef struct
{
    int32_t ident;
    int32_t version;
    lump_t lumps[HEADER_LUMPS];
} dheader_t;

typedef struct
{
    float mins[3], maxs[3];
    float origin[3]; // for sounds or lights
    int32_t headnode;
    int32_t firstface, numfaces; // submodels just draw faces
                                 // without walking the bsp tree
} dmodel_t;

typedef struct
{
    float point[3];
} dvertex_t;

// 0-2 are axial planes
#define PLANE_X    0
#define PLANE_Y    1
#define PLANE_Z    2

// 3-5 are non-axial planes snapped to the nearest
#define PLANE_ANYX 3
#define PLANE_ANYY 4
#define PLANE_ANYZ 5

// planes (x&~1) and (x&~1)+1 are allways opposites

typedef struct
{
    float normal[3];
    float dist;
    int32_t type; // PLANE_X - PLANE_ANYZ ?remove? trivial to regenerate
} dplane_t;

// contents flags are seperate bits
// a given brush can contribute multiple content bits
// multiple brushes can be in a single leaf

// these definitions also need to be in q_shared.h!

// lower bits are stronger, and will eat weaker brushes completely
#define CONTENTS_SOLID        1 // an eye is never valid in a solid
#define CONTENTS_WINDOW       2 // translucent, but not watery
#define CONTENTS_AUX          4
#define CONTENTS_LAVA         8
#define CONTENTS_SLIME        16
#define CONTENTS_WATER        32
#define CONTENTS_MIST         64
#define LAST_VISIBLE_CONTENTS 64 // was 64

// remaining contents are non-visible, and don't eat brushes

#define CONTENTS_AREAPORTAL   0x8000

#define CONTENTS_PLAYERCLIP   0x10000
#define CONTENTS_MONSTERCLIP  0x20000

// currents can be added to any other contents, and may be mixed
#define CONTENTS_CURRENT_0    0x40000
#define CONTENTS_CURRENT_90   0x80000
#define CONTENTS_CURRENT_180  0x100000
#define CONTENTS_CURRENT_270  0x200000
#define CONTENTS_CURRENT_UP   0x400000
#define CONTENTS_CURRENT_DOWN 0x800000

#define CONTENTS_ORIGIN       0x1000000 // removed before bsping an entity

#define CONTENTS_MONSTER      0x2000000 // should never be on a brush, only in game
#define CONTENTS_DEADMONSTER  0x4000000
#define CONTENTS_DETAIL       0x8000000  // brushes to be added after vis leafs
#define CONTENTS_TRANSLUCENT  0x10000000 // auto set if any surface has trans
#define CONTENTS_LADDER       0x20000000

#define SURF_LIGHT            0x1 // value will hold the light strength

#define SURF_SLICK            0x2 // effects game physics

#define SURF_SKY              0x4  // don't draw, but add to skybox
#define SURF_WARP             0x8  // turbulent water warp
#define SURF_TRANS33          0x10 // 33% alpha blend
#define SURF_TRANS66          0x20 // 66% alpha blend
#define SURF_FLOWING          0x40 // scroll towards angle
#define SURF_NODRAW           0x80 // don't bother referencing the texture

#define SURF_HINT             0x0100 // make a bsp splitter
#define SURF_SKIP             0x0200 // ignore surface to make non-closed brushes
#define SURF_ALPHATEST        0x02000000 //alpha test flag

// qb: qbsp types - dnode_tx, dedge_tx, dface_tx, dleaf_tx, dbrushside_tx

typedef struct
{
    int32_t planenum;
    int32_t children[2]; // negative numbers are -(leafs+1), not nodes
    short mins[3];       // for frustom culling
    short maxs[3];
    uint16_t firstface;
    uint16_t numfaces; // counting both sides
} dnode_t;

typedef struct
{
    int32_t planenum;
    int32_t children[2]; // negative numbers are -(leafs+1), not nodes
    float mins[3];       // for frustom culling
    float maxs[3];
    uint32_t firstface;
    uint32_t numfaces; // counting both sides
} dnode_tx;            // qb: qbsp

typedef struct texinfo_s {
    float vecs[2][4];    // [s/t][xyz offset]
    int32_t flags;       // miptex flags + overrides
    int32_t value;       // light emission, etc
    char texture[32];    // texture name (textures/*.wal)
    int32_t nexttexinfo; // for animations, -1 = end of chain
} texinfo_t;

// note that edge 0 is never used, because negative edge nums are used for
// counterclockwise use of the edge in a face
typedef struct
{
    uint16_t v[2]; // vertex numbers
} dedge_t;

typedef struct
{
    uint32_t v[2]; // vertex numbers
} dedge_tx;        // qb: qbsp

#define MAXLIGHTMAPS 4
typedef struct
{
    uint16_t planenum;
    short side;

    int32_t firstedge; // we must support > 64k edges
    short numedges;
    short texinfo;

    // lighting info
    byte styles[MAXLIGHTMAPS];
    int32_t lightofs; // start of [numstyles*surfsize] samples
} dface_t;

typedef struct
{
    uint32_t planenum;
    int32_t side;

    int32_t firstedge; // we must support > 64k edges
    int32_t numedges;
    int32_t texinfo;

    // lighting info
    byte styles[MAXLIGHTMAPS];
    int32_t lightofs; // start of [numstyles*surfsize] samples
} dface_tx;           // qb: qbsp

typedef struct
{
    int32_t contents; // OR of all brushes (not needed?)

    short cluster;
    short area;

    short mins[3]; // for frustum culling
    short maxs[3];

    uint16_t firstleafface;
    uint16_t numleaffaces;

    uint16_t firstleafbrush;
    uint16_t numleafbrushes;
} dleaf_t;

typedef struct
{
    int32_t contents; // OR of all brushes (not needed?)

    int32_t cluster;
    int32_t area;

    float mins[3]; // for frustum culling
    float maxs[3];

    uint32_t firstleafface;
    uint32_t numleaffaces;

    uint32_t firstleafbrush;
    uint32_t numleafbrushes;
} dleaf_tx; // qb: qbsp

typedef struct
{
    uint16_t planenum; // facing out of the leaf
    short texinfo;
} dbrushside_t;

typedef struct
{
    uint32_t planenum; // facing out of the leaf
    int32_t texinfo;
} dbrushside_tx; // qb: qbsp

// qbsp /////////////////////////

typedef struct
{
    int32_t firstside;
    int32_t numsides;
    int32_t contents;
} dbrush_t;

#define ANGLE_UP   -1
#define ANGLE_DOWN -2

// the visibility lump consists of a header with a count, then
// byte offsets for the PVS and PHS of each cluster, then the raw
// compressed bit vectors
#define DVIS_PVS   0
#define DVIS_PHS   1
typedef struct
{
    int32_t numclusters;
    int32_t bitofs[8][2]; // bitofs[numclusters][2]
} dvis_t;

// each area has a list of portals that lead into other areas
// when portals are closed, other areas may not be visible or
// hearable even if the vis info says that it should be
typedef struct
{
    int32_t portalnum;
    int32_t otherarea;
} dareaportal_t;

typedef struct
{
    uint32_t numareaportals;
    uint32_t firstareaportal;
} darea_t;
