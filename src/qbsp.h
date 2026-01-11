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

#include "cmdlib.h"
#include "mathlib.h"
#include "scriplib.h"
#include "polylib.h"
#include "threads.h"
#include "bspfile.h"

#define MAX_BRUSH_SIDES 128
#define TEXINFO_NODE    -1 // side is already on a node

typedef struct plane_s {
    vec3_t normal;
    vec_t dist;
    int32_t type;
    struct plane_s *hash_chain;
} plane_t;

typedef struct
{
    vec_t shift[2];
    vec_t rotate;
    vec_t scale[2];
    char name[32];
    int32_t flags;
    int32_t value;
} brush_texture_t;

typedef struct side_s {
    int32_t planenum;
    int32_t texinfo;
    winding_t *winding;
    struct side_s *original; // bspbrush_t sides will reference the mapbrush_t sides
    int32_t contents;        // from miptex
    int32_t surf;            // from miptex
    bool visible;        // choose visble planes first
    bool tested;         // this plane allready checked as a split
    bool bevel;          // don't ever use for bsp splitting
} side_t;

typedef struct brush_s {
    int32_t entitynum;
    int32_t brushnum;

    int32_t contents;

    vec3_t mins, maxs;

    int32_t numsides;
    side_t *original_sides;
} mapbrush_t;

#define PLANENUM_LEAF -1

#define MAXEDGES      20

typedef struct face_s {
    struct face_s *next; // on node

    // the chain of faces off of a node can be merged or split,
    // but each face_t along the way will remain in the chain
    // until the entire tree is freed
    struct face_s *merged;   // if set, this face isn't valid anymore
    struct face_s *split[2]; // if set, this face isn't valid anymore

    struct portal_s *portal;
    int32_t texinfo;
    int32_t planenum;
    int32_t contents; // faces in different contents can't merge
    int32_t outputnumber;
    winding_t *w;
    int32_t numpoints;
    bool badstartvert; // tjunctions cannot be fixed without a midpoint vertex
    int32_t vertexnums[MAXEDGES];
} face_t;

typedef struct bspbrush_s {
    struct bspbrush_s *next;
    vec3_t mins, maxs;
    int32_t side, testside; // side of node during construction
    mapbrush_t *original;
    int32_t numsides;
    side_t sides[6]; // variably sized
} bspbrush_t;

#define MAX_NODE_BRUSHES 8
typedef struct node_s {
    // both leafs and nodes
    int32_t planenum; // -1 = leaf node
    struct node_s *parent;
    vec3_t mins, maxs;  // valid after portalization
    bspbrush_t *volume; // one for each leaf/node

    // nodes only
    bool detail_seperator; // a detail brush caused the split
    side_t *side;              // the side that created the node
    struct node_s *children[2];
    face_t *faces;

    // leafs only
    bspbrush_t *brushlist;    // fragments of all brushes in this leaf
    int32_t contents;         // OR of all brush contents
    int32_t occupied;         // 1 or greater can reach entity
    entity_t *occupant;       // for leak file testing
    int32_t cluster;          // for portalfile writing
    int32_t area;             // for areaportals
    struct portal_s *portals; // also on nodes during construction
} node_t;

typedef struct portal_s {
    plane_t plane;
    node_t *onnode;   // NULL = outside box
    node_t *nodes[2]; // [0] = front side of plane
    struct portal_s *next[2];
    winding_t *winding;

    bool sidefound; // false if ->side hasn't been checked
    side_t *side;       // NULL = non-visible
    face_t *face[2];    // output face in bsp file
} portal_t;

typedef struct
{
    node_t *headnode;
    node_t outside_node;
    vec3_t mins, maxs;
} tree_t;

extern int32_t entity_num;

extern plane_t mapplanes[MAX_MAP_PLANES_QBSP];
extern int32_t nummapplanes;

extern int32_t nummapbrushes;
extern mapbrush_t mapbrushes[MAX_MAP_BRUSHES_QBSP];

extern vec3_t map_mins, map_maxs;

#define MAX_MAP_SIDES (MAX_MAP_BRUSHSIDES_QBSP)

extern int32_t nummapbrushsides;
extern side_t brushsides[MAX_MAP_SIDES];

extern bool noprune;
extern bool nodetail;
extern bool fulldetail;
extern bool nomerge;
extern bool nosubdiv;
extern bool nowater;
extern bool noweld;
extern bool noshare;
extern bool notjunc;
extern bool badnormal_check;
extern float badnormal;
extern bool use_qbsp;
extern bool noskipfix;

extern vec_t microvolume;

extern char outbase[32];

extern char source[1024];

extern int32_t max_entities; // qb: from kmqbsp3-  Knightmare- adjustable entity limit
extern int32_t max_bounds;   // Knightmare- adjustable max bounds

void LoadMapFile(char *filename);
int32_t FindFloatPlane(vec3_t normal, vec_t dist, int32_t bnum);

//=============================================================================

// textures.c

typedef struct
{
    char name[64];
    int32_t flags;
    int32_t value;
    int32_t contents;
    char animname[64];
} textureref_t;

#define MAX_MAP_TEXTURES 1024

extern textureref_t textureref[MAX_MAP_TEXTURES];

int32_t FindMiptex(char *name);

int32_t TexinfoForBrushTexture(plane_t *plane, brush_texture_t *bt, vec3_t origin);
// DarkEssence: function TexinfoForBrushTexture_UV for #mapversion 220
int32_t TexinfoForBrushTexture_UV(brush_texture_t *bt, vec_t *UVaxis);
// mxd: Applies origin brush offset to existing #mapversion 220 texinfo
int32_t ApplyTexinfoOffset_UV(int32_t texinfoindex, const brush_texture_t *bt, const vec3_t origin);

//=============================================================================

void FindGCD(int32_t *v);

mapbrush_t *Brush_LoadEntity(entity_t *ent);
int32_t PlaneTypeForNormal(vec3_t normal);
bool MakeBrushPlanes(mapbrush_t *b);
int32_t FindIntPlane(int32_t *inormal, int32_t *iorigin);
void CreateBrush(int32_t brushnum);

//=============================================================================

// draw.c

extern vec3_t draw_mins, draw_maxs;

void Draw_ClearWindow(void);
void DrawWinding(winding_t *w);

//=============================================================================

// csg

bspbrush_t *MakeBspBrushList(int32_t startbrush, int32_t endbrush,
                             vec3_t clipmins, vec3_t clipmaxs);
bspbrush_t *ChopBrushes(bspbrush_t *head);
bspbrush_t *InitialBrushList(bspbrush_t *list);
bspbrush_t *OptimizedBrushList(bspbrush_t *list);

void WriteBrushMap(char *name, bspbrush_t *list);

//=============================================================================

// brushbsp

bspbrush_t *CopyBrush(bspbrush_t *brush);

void SplitBrush(bspbrush_t *brush, int32_t planenum,
                bspbrush_t **front, bspbrush_t **back);

tree_t *AllocTree(void);
node_t *AllocNode(void);
bspbrush_t *AllocBrush(int32_t numsides);
int32_t CountBrushList(bspbrush_t *brushes);
void FreeBrush(bspbrush_t *brushes);
vec_t BrushVolume(bspbrush_t *brush);

void BoundBrush(bspbrush_t *brush);
void FreeBrushList(bspbrush_t *brushes);

tree_t *BrushBSP(bspbrush_t *brushlist, vec3_t mins, vec3_t maxs);

//=============================================================================

// portals.c

int32_t VisibleContents(int32_t contents);

void MakeHeadnodePortals(tree_t *tree);
void MakeNodePortal(node_t *node);
void SplitNodePortals(node_t *node);

bool Portal_VisFlood(portal_t *p);

bool FloodEntities(tree_t *tree);
void FillOutside(node_t *headnode);
void FloodAreas(tree_t *tree);
void MarkVisibleSides(tree_t *tree, int32_t start, int32_t end);
void FreePortal(portal_t *p);
void EmitAreaPortals(node_t *headnode);

void MakeTreePortals(tree_t *tree);

//=============================================================================

// leakfile.c

void LeakFile(tree_t *tree);

//=============================================================================

// prtfile.c

void WritePortalFile(tree_t *tree);

//=============================================================================

// writebsp.c

void SetModelNumbers(void);
void SetLightStyles(void);

void BeginBSPFile(void);
void WriteBSP(node_t *headnode);
void EndBSPFile(void);
void BeginModel(void);
void EndModel(void);

//=============================================================================

// faces.c

void MakeFaces(node_t *headnode);
void FixTjuncs(node_t *headnode);
int32_t GetEdge(int32_t v1, int32_t v2, face_t *f);

face_t *AllocFace(void);
void FreeFace(face_t *f);

void MergeNodeFaces(node_t *node);

//=============================================================================

// tree.c

void FreeTree(tree_t *tree);
void FreeTree_r(node_t *node);
void PrintTree_r(node_t *node, int32_t depth);
void FreeTreePortals_r(node_t *node);
void PruneNodes_r(node_t *node);
void PruneNodes(node_t *node);
