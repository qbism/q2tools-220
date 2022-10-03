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

#include "qbsp.h"

int32_t nummiptex;
textureref_t textureref[MAX_MAP_TEXTURES];

//==========================================================================

int32_t FindMiptex(char *name) {
    int32_t i, mod_fail;
    char path[1080];
    char pakpath[56];
    miptex_t *mt;

    for (i = 0; i < nummiptex; i++)
        if (!strcmp(name, textureref[i].name)) {
            return i;
        }
    if (nummiptex == MAX_MAP_TEXTURES)
        Error("MAX_MAP_TEXTURES");
    strcpy(textureref[i].name, name);

    mod_fail = true;

    sprintf(pakpath, "textures/%s.wal", name);

    if (moddir[0] != 0) {
        sprintf(path, "%s%s", moddir, pakpath);
        // load the miptex to get the flags and values
        if (TryLoadFile(path, (void **)&mt, false) != -1 ||
            TryLoadFileFromPak(pakpath, (void **)&mt, moddir) != -1) {
            textureref[i].value    = LittleLong(mt->value);
            textureref[i].flags    = LittleLong(mt->flags);
            textureref[i].contents = LittleLong(mt->contents);
            strcpy(textureref[i].animname, mt->animname);
            free(mt);
            mod_fail = false;
        }
    }

    if (mod_fail) {
        // load the miptex to get the flags and values
        sprintf(path, "%s%s", basedir, pakpath);

        if (TryLoadFile(path, (void **)&mt, false) != -1 ||
            TryLoadFileFromPak(pakpath, (void **)&mt, basedir) != -1) {
            textureref[i].value    = LittleLong(mt->value);
            textureref[i].flags    = LittleLong(mt->flags);
            textureref[i].contents = LittleLong(mt->contents);
            strcpy(textureref[i].animname, mt->animname);
            free(mt);
            mod_fail = false;
        }
    }

    if (mod_fail)
        printf("WARNING: couldn't locate texture %s\n", name);

    nummiptex++;

    if (textureref[i].animname[0])
        FindMiptex(textureref[i].animname);

    return i;
}

/*
==================
textureAxisFromPlane
==================
*/
vec3_t baseaxis[18] =
    {
        {0, 0, 1}, {1, 0, 0}, {0, -1, 0}, // floor
        {0, 0, -1},
        {1, 0, 0},
        {0, -1, 0}, // ceiling
        {1, 0, 0},
        {0, 1, 0},
        {0, 0, -1}, // west wall
        {-1, 0, 0},
        {0, 1, 0},
        {0, 0, -1}, // east wall
        {0, 1, 0},
        {1, 0, 0},
        {0, 0, -1}, // south wall
        {0, -1, 0},
        {1, 0, 0},
        {0, 0, -1} // north wall
};

void TextureAxisFromPlane(plane_t *pln, vec3_t xv, vec3_t yv) {
    int32_t bestaxis;
    vec_t dot, best;
    int32_t i;

    best     = 0;
    bestaxis = 0;

    for (i = 0; i < 6; i++) {
        dot = DotProduct(pln->normal, baseaxis[i * 3]);
        if (dot > best) {
            best     = dot;
            bestaxis = i;
        }
    }

    VectorCopy(baseaxis[bestaxis * 3 + 1], xv);
    VectorCopy(baseaxis[bestaxis * 3 + 2], yv);
}

static inline void CheckTexinfoCount() // mxd
{
    if (use_qbsp) {
        if (numtexinfo == MAX_MAP_TEXINFO_QBSP)
            printf("WARNING: texinfo count exceeds qbsp limit (%i).\n", MAX_MAP_TEXINFO_QBSP);
    } else if (numtexinfo == DEFAULT_MAP_TEXINFO)
        printf("WARNING: texinfo count exceeds vanilla limit (%i).\n", DEFAULT_MAP_TEXINFO);
    else if (numtexinfo >= MAX_MAP_TEXINFO)
        Error("ERROR: texinfo count exceeds extended limit (%i).\n", MAX_MAP_TEXINFO);
}

qboolean TexinfosMatch(texinfo_t t1, texinfo_t t2) // mxd
{
    if (t1.flags != t2.flags || t1.value != t2.value || strcmp(t1.texture, t2.texture))
        return false;

    for (int32_t j = 0; j < 2; j++)
        for (int32_t k = 0; k < 4; k++)
            if ((int32_t)(t1.vecs[j][k] * 100) != (int32_t)(t2.vecs[j][k] * 100)) // qb: round to two decimal places
                return false;

    return true;
}

// mxd. Applies origin brush offset to existing v220 texinfo...
int32_t ApplyTexinfoOffset_UV(int32_t texinfoindex, const brush_texture_t *bt, const vec3_t origin) {
    if ((!origin[0] && !origin[1] && !origin[2]) || texinfoindex < 0)
        return texinfoindex;

    const texinfo_t otx = texinfo[texinfoindex];

    // Copy texinfo
    texinfo_t tx;
    memcpy(&tx, &otx, sizeof(tx));

    // Transform origin to UV space and add it to ST offsets
    tx.vecs[0][3] += origin[0] * tx.vecs[0][0] + origin[1] * tx.vecs[0][1] + origin[2] * tx.vecs[0][2];
    tx.vecs[1][3] += origin[0] * tx.vecs[1][0] + origin[1] * tx.vecs[1][1] + origin[2] * tx.vecs[1][2];

    // Find or replace texinfo
    texinfo_t *tc = texinfo;
    for (texinfoindex = 0; texinfoindex < numtexinfo; texinfoindex++, tc++)
        if (TexinfosMatch(*tc, tx))
            return texinfoindex;

    *tc = tx;
    numtexinfo++;
    CheckTexinfoCount();

    // Repeat for the next animation frame
    const int32_t mt = FindMiptex(tx.texture);
    if (textureref[mt].animname[0]) {
        brush_texture_t anim = *bt;
        strcpy(anim.name, textureref[mt].animname);
        tc->nexttexinfo = ApplyTexinfoOffset_UV(tc->nexttexinfo, &anim, origin);
    } else {
        tc->nexttexinfo = -1;
    }

    // Return new texinfo index.
    return texinfoindex;
}

// DarkEssence: function for new #mapversion with UVaxis
int32_t TexinfoForBrushTexture_UV(brush_texture_t *bt, vec_t *UVaxis) {
    if (!bt->name[0])
        return 0;

    texinfo_t tx;
    memset(&tx, 0, sizeof(tx));
    strcpy(tx.texture, bt->name);

    if (!bt->scale[0])
        bt->scale[0] = 1;
    if (!bt->scale[1])
        bt->scale[1] = 1;

    for (int32_t i = 0; i < 2; i++)
        for (int32_t j = 0; j < 3; j++)
            tx.vecs[i][j] = UVaxis[i * 3 + j] / bt->scale[i];

    tx.vecs[0][3] = bt->shift[0];
    tx.vecs[1][3] = bt->shift[1];
    tx.flags      = bt->flags;
    tx.value      = bt->value;

    // Find or replace texinfo
    int32_t texinfoindex;
    texinfo_t *tc = texinfo;
    for (texinfoindex = 0; texinfoindex < numtexinfo; texinfoindex++, tc++)
        if (TexinfosMatch(*tc, tx))
            return texinfoindex;

    *tc = tx;
    numtexinfo++;
    CheckTexinfoCount(); // mxd

    // Load the next animation
    const int32_t mt = FindMiptex(bt->name);
    if (textureref[mt].animname[0]) {
        brush_texture_t anim = *bt;
        strcpy(anim.name, textureref[mt].animname);
        tc->nexttexinfo = TexinfoForBrushTexture_UV(&anim, UVaxis);
    } else
        tc->nexttexinfo = -1;

    return texinfoindex;
}

extern qboolean origfix;

int32_t TexinfoForBrushTexture(plane_t *plane, brush_texture_t *bt, vec3_t origin) {
    if (!bt->name[0])
        return 0;

    texinfo_t tx;
    memset(&tx, 0, sizeof(tx));
    strcpy(tx.texture, bt->name);

    vec3_t vecs[2];
    TextureAxisFromPlane(plane, vecs[0], vecs[1]);

    /* Originally:
    shift[0] = DotProduct (origin, vecs[0]);
    shift[1] = DotProduct (origin, vecs[1]);
    */

    if (!bt->scale[0])
        bt->scale[0] = 1;
    if (!bt->scale[1])
        bt->scale[1] = 1;

    // DWH: Fix for scaled textures using an origin brush
    float shift[2];
    vec3_t scaled_origin;
    if (origfix) {
        VectorScale(origin, 1.0 / bt->scale[0], scaled_origin);
        shift[0] = DotProduct(scaled_origin, vecs[0]);
        VectorScale(origin, 1.0 / bt->scale[1], scaled_origin);
        shift[1] = DotProduct(scaled_origin, vecs[1]);
    } else {
        shift[0] = DotProduct(origin, vecs[0]);
        shift[1] = DotProduct(origin, vecs[1]);
    }

    // Rotate axis
    vec_t sinv, cosv;
    if (bt->rotate == 0) {
        sinv = 0;
        cosv = 1;
    } else if (bt->rotate == 90) {
        sinv = 1;
        cosv = 0;
    } else if (bt->rotate == 180) {
        sinv = 0;
        cosv = -1;
    } else if (bt->rotate == 270) {
        sinv = -1;
        cosv = 0;
    } else {
        const vec_t ang = bt->rotate / 180 * Q_PI;
        sinv            = sin(ang);
        cosv            = cos(ang);
    }

    // DWH: and again...
    if (origfix) {
        const vec_t ns = cosv * shift[0] - sinv * shift[1];
        const vec_t nt = sinv * shift[0] + cosv * shift[1];
        shift[0]       = ns;
        shift[1]       = nt;
    }

    int32_t sv, tv;
    if (vecs[0][0])
        sv = 0;
    else if (vecs[0][1])
        sv = 1;
    else
        sv = 2;

    if (vecs[1][0])
        tv = 0;
    else if (vecs[1][1])
        tv = 1;
    else
        tv = 2;

    for (int32_t i = 0; i < 2; i++) {
        const vec_t ns = cosv * vecs[i][sv] - sinv * vecs[i][tv];
        const vec_t nt = sinv * vecs[i][sv] + cosv * vecs[i][tv];
        vecs[i][sv]    = ns;
        vecs[i][tv]    = nt;
    }

    for (int32_t i = 0; i < 2; i++)
        for (int32_t j = 0; j < 3; j++)
            tx.vecs[i][j] = vecs[i][j] / bt->scale[i];

    tx.vecs[0][3] = bt->shift[0] + shift[0];
    tx.vecs[1][3] = bt->shift[1] + shift[1];
    tx.flags      = bt->flags;
    tx.value      = bt->value;

    // Find or replace texinfo
    int32_t texinfoindex;
    texinfo_t *tc = texinfo;
    for (texinfoindex = 0; texinfoindex < numtexinfo; texinfoindex++, tc++)
        if (TexinfosMatch(*tc, tx))
            return texinfoindex;

    *tc = tx;
    numtexinfo++;
    CheckTexinfoCount(); // mxd

    // load the next animation
    const int32_t mt = FindMiptex(bt->name);
    if (textureref[mt].animname[0]) {
        brush_texture_t anim = *bt;
        strcpy(anim.name, textureref[mt].animname);
        tc->nexttexinfo = TexinfoForBrushTexture(plane, &anim, origin);
    } else
        tc->nexttexinfo = -1;

    return texinfoindex;
}
