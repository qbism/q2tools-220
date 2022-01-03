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
#include "bspfile.h"
#include "polylib.h"
#include "threads.h"
#include "lbmlib.h"

#ifdef WIN32
#include <windows.h>
#endif

typedef enum
{
	emit_surface,
	emit_point,
	emit_spotlight,
    emit_sky
} emittype_t;



typedef struct directlight_s
{
	struct directlight_s *next;
	emittype_t	type;

	float		intensity;
	int32_t			style;
    float       wait;
    float       adjangle;
    int32_t         falloff;
	vec3_t		origin;
	vec3_t		color;
	vec3_t		normal;		// for surfaces and spotlights
	float		stopdot;		// for spotlights
    dplane_t    *plane;
    dleaf_t     *leaf;
    dleaf_tx     *leafX;
    int32_t			nodenum;
} directlight_t;


// the sum of all tranfer->transfer values for a given patch
// should equal exactly 0x10000, showing that all radiance
// reaches other patches
typedef struct
{
	uint16_t	patch;
	uint16_t	transfer;
} transfer_t;


#define	MAX_PATCHES	          65535
#define	MAX_PATCHES_QBSP     4000000 //qb: extended limit

#define LMSTEP 16 //qb: lightmap step.  Default generates 1/16 of texture scale.
#define QBSP_LMSTEP 4 //qb: higher res lightmap

#define DEFAULT_SMOOTHING_VALUE     44.0
#define DEFAULT_NUDGE_VALUE     0.25

typedef struct patch_s
{
	winding_t	*winding;
	struct patch_s		*next;		// next in face
	int32_t			numtransfers;
	transfer_t	*transfers;
    byte *trace_hit;

    int32_t			nodenum;

	int32_t			cluster;			// for pvs checking
	vec3_t		origin;
	dplane_t	*plane;

	qboolean	sky;

	vec3_t		totallight;			// accumulated by radiosity
									// does NOT include light
									// accounted for by direct lighting
	float		area;
    	int32_t         faceNumber;

	// illuminance * reflectivity = radiosity
	vec3_t		reflectivity;
	vec3_t		baselight;			// emissivity only

	// each style 0 lightmap sample in the patch will be
	// added up to get the average illuminance of the entire patch
	vec3_t		samplelight;
	int32_t			samples;		// for averaging direct light
} patch_t;

extern	patch_t		*face_patches[MAX_MAP_FACES_QBSP];
extern	entity_t	*face_entity[MAX_MAP_FACES_QBSP];
extern	vec3_t		face_offset[MAX_MAP_FACES_QBSP];		// for rotating bmodels
extern	patch_t		patches[MAX_PATCHES_QBSP];
extern	unsigned	num_patches;

extern	int32_t		leafparents[MAX_MAP_LEAFS_QBSP];
extern	int32_t		nodeparents[MAX_MAP_NODES_QBSP];

extern	float	lightscale;

extern char		basedir[64];

extern qboolean use_qbsp;

void MakeShadowSplits (void);

//==============================================

void BuildVisMatrix (void);
qboolean CheckVisBit (unsigned p1, unsigned p2);

//==============================================

extern	float ambient, maxlight;

void LinkPlaneFaces (void);

extern float grayscale;
extern float saturation;
extern	qboolean	extrasamples;
extern	qboolean	dicepatches;
extern int32_t numbounce;
extern qboolean noblock;
extern qboolean lightwarp;

extern	directlight_t	*directlights[MAX_MAP_LEAFS_QBSP];

extern	byte	nodehit[MAX_MAP_NODES_QBSP];

void BuildLightmaps (void);

void BuildFacelights (int32_t facenum);

void FinalLightFace (int32_t facenum);
qboolean PvsForOrigin (vec3_t org, byte *pvs);

int32_t	PointInNodenum (vec3_t point);
int32_t TestLine (vec3_t start, vec3_t stop);
int32_t TestLine_color (int32_t node, vec3_t start, vec3_t stop, vec3_t occluded);
int32_t TestLine_r (int32_t node, vec3_t start, vec3_t stop);

void CreateDirectLights (void);

dleaf_t		*PointInLeaf (vec3_t point);
dleaf_tx		*PointInLeafX (vec3_t point);


extern	dplane_t	backplanes[MAX_MAP_PLANES_QBSP];
extern	int32_t			fakeplanes;					// created planes for origin offset
extern  int32_t		maxdata, step;

extern	float	subdiv;

extern	float	direct_scale;
extern	float	entity_scale;

extern qboolean sun;
extern qboolean sun_alt_color;
extern vec3_t sun_pos;
extern float sun_main;
extern float sun_ambient;
extern vec3_t sun_color;

extern float    smoothing_threshold;
extern float    smoothing_value;
extern float    sample_nudge;
extern int32_t  num_smoothing;

extern int32_t	refine_amt, refine_setting;
extern int32_t	PointInLeafnum (vec3_t point);
extern void MakeTnodes (dmodel_t *bm);
extern void MakePatches (void);
extern void SubdividePatches (void);
extern void PairEdges (void);
extern void CalcTextureReflectivity (void);
extern byte	*dlightdata_ptr;
extern byte	dlightdata_raw[MAX_MAP_LIGHTING_QBSP];

extern	float sunradscale;
