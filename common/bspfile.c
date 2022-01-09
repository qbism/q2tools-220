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
#include "scriplib.h"

qboolean use_qbsp = false; //qb: huge map support
qboolean noskipfix = false;  //qb: warn about SURF_SKIP contents rather than silently changing to zero

void GetLeafNums (void);

//=============================================================================

//qb: add qbsp types

int32_t			nummodels;
dmodel_t	dmodels[MAX_MAP_MODELS_QBSP];

int32_t			visdatasize;
byte		dvisdata[MAX_MAP_VISIBILITY_QBSP];
dvis_t		*dvis = (dvis_t *)dvisdata;

int32_t			lightdatasize;
byte		dlightdata[MAX_MAP_LIGHTING_QBSP];

int32_t			entdatasize;
char		dentdata[MAX_MAP_ENTSTRING_QBSP];

int32_t			numleafs;
dleaf_t	dleafs[MAX_MAP_LEAFS];
dleaf_tx	dleafsX[MAX_MAP_LEAFS_QBSP];

int32_t			numplanes;
dplane_t	dplanes[MAX_MAP_PLANES_QBSP];

int32_t			numvertexes;
dvertex_t	dvertexes[MAX_MAP_VERTS_QBSP];

int32_t			numnodes;
dnode_t	dnodes[MAX_MAP_NODES];
dnode_tx	dnodesX[MAX_MAP_NODES_QBSP];

int32_t			numtexinfo;
texinfo_t	texinfo[MAX_MAP_TEXINFO_QBSP];

int32_t			numfaces;
dface_t	dfaces[MAX_MAP_FACES];
dface_tx	dfacesX[MAX_MAP_FACES_QBSP];

int32_t			numedges;
dedge_t	dedges[MAX_MAP_EDGES];
dedge_tx	dedgesX[MAX_MAP_EDGES_QBSP];

int32_t			numleaffaces;
uint16_t		dleaffaces[MAX_MAP_LEAFFACES];
uint32_t		dleaffacesX[MAX_MAP_LEAFFACES_QBSP];

int32_t			numleafbrushes;
uint16_t		dleafbrushes[MAX_MAP_LEAFBRUSHES];
uint32_t		dleafbrushesX[MAX_MAP_LEAFBRUSHES_QBSP];

int32_t			numsurfedges;
int32_t			dsurfedges[MAX_MAP_SURFEDGES_QBSP];

int32_t			numbrushes;
dbrush_t	dbrushes[MAX_MAP_BRUSHES_QBSP];

int32_t			numbrushsides;
dbrushside_t	dbrushsides[MAX_MAP_BRUSHSIDES];
dbrushside_tx	dbrushsidesX[MAX_MAP_BRUSHSIDES_QBSP];

int32_t			numareas;
darea_t		dareas[MAX_MAP_AREAS];

int32_t			numareaportals;
dareaportal_t	dareaportals[MAX_MAP_AREAPORTALS];

byte		dpop[256];

/*
===============
CompressVis

===============
*/
int32_t CompressVis (byte *vis, byte *dest)
{
    int32_t		j;
    int32_t		rep;
    int32_t		visrow;
    byte	*dest_p;

    dest_p = dest;
//	visrow = (r_numvisleafs + 7)>>3;
    visrow = (dvis->numclusters + 7)>>3;

    for (j=0 ; j<visrow ; j++)
    {
        *dest_p++ = vis[j];
        if (vis[j])
            continue;

        rep = 1;
        for ( j++; j<visrow ; j++)
            if (vis[j] || rep == 255)
                break;
            else
                rep++;
        *dest_p++ = rep;
        j--;
    }

    return dest_p - dest;
}


/*
===================
DecompressVis
===================
*/
void DecompressVis (byte *in, byte *decompressed)
{
    int32_t		c;
    byte	*out;
    int32_t		row;

//	row = (r_numvisleafs+7)>>3;
    row = (dvis->numclusters+7)>>3;
    out = decompressed;

    do
    {
        if (*in)
        {
            *out++ = *in++;
            continue;
        }

        c = in[1];
        if (!c)
            Error ("DecompressVis: 0 repeat");
        in += 2;
        while (c)
        {
            *out++ = 0;
            c--;
        }
    }
    while (out - decompressed < row);
}

//=============================================================================

/*
=============
SwapBSPFile

Byte swaps all data in a bsp file.
=============
*/
void SwapBSPFile (qboolean todisk)
{
    int32_t				i, j;
    dmodel_t		*d;


// models
    for (i=0 ; i<nummodels ; i++)
    {
        d = &dmodels[i];

        d->firstface = LittleLong (d->firstface);
        d->numfaces = LittleLong (d->numfaces);
        d->headnode = LittleLong (d->headnode);

        for (j=0 ; j<3 ; j++)
        {
            d->mins[j] = LittleFloat(d->mins[j]);
            d->maxs[j] = LittleFloat(d->maxs[j]);
            d->origin[j] = LittleFloat(d->origin[j]);
        }
    }

//
// vertexes
//
    for (i=0 ; i<numvertexes ; i++)
    {
        for (j=0 ; j<3 ; j++)
            dvertexes[i].point[j] = LittleFloat (dvertexes[i].point[j]);
    }

//
// planes
//
    for (i=0 ; i<numplanes ; i++)
    {
        for (j=0 ; j<3 ; j++)
            dplanes[i].normal[j] = LittleFloat (dplanes[i].normal[j]);
        dplanes[i].dist = LittleFloat (dplanes[i].dist);
        dplanes[i].type = LittleLong (dplanes[i].type);
    }

//
// texinfos
//
    for (i=0 ; i<numtexinfo ; i++)
    {
        texinfo[i].flags = LittleLong (texinfo[i].flags);
        texinfo[i].value = LittleLong (texinfo[i].value);
        texinfo[i].nexttexinfo = LittleLong (texinfo[i].nexttexinfo);
    }

//
// faces nodes leafs brushsides leafbrushes leaffaces
//
    if (use_qbsp)
    {
        for (i=0 ; i<numfaces ; i++)
        {
            dfacesX[i].texinfo = LittleLong (dfacesX[i].texinfo);
            dfacesX[i].planenum = LittleLong (dfacesX[i].planenum);
            dfacesX[i].side = LittleLong (dfacesX[i].side);
            dfacesX[i].lightofs = LittleLong (dfacesX[i].lightofs);
            dfacesX[i].firstedge = LittleLong (dfacesX[i].firstedge);
            dfacesX[i].numedges = LittleLong (dfacesX[i].numedges);
        }

        for (i=0 ; i<numnodes ; i++)
        {
            dnodesX[i].planenum = LittleLong (dnodesX[i].planenum);
            for (j=0 ; j<3 ; j++)
            {
                dnodesX[i].mins[j] = LittleFloat (dnodesX[i].mins[j]);
                dnodesX[i].maxs[j] = LittleFloat (dnodesX[i].maxs[j]);
            }
            dnodesX[i].children[0] = LittleLong (dnodesX[i].children[0]);
            dnodesX[i].children[1] = LittleLong (dnodesX[i].children[1]);
            dnodesX[i].firstface = LittleLong (dnodesX[i].firstface);
            dnodesX[i].numfaces = LittleLong (dnodesX[i].numfaces);
        }

        for (i=0 ; i<numleafs ; i++)
        {
            dleafsX[i].contents = LittleLong (dleafsX[i].contents);
            dleafsX[i].cluster = LittleLong (dleafsX[i].cluster);
            dleafsX[i].area = LittleLong (dleafsX[i].area);
            for (j=0 ; j<3 ; j++)
            {
                dleafsX[i].mins[j] = LittleFloat (dleafsX[i].mins[j]);
                dleafsX[i].maxs[j] = LittleFloat (dleafsX[i].maxs[j]);
            }
            dleafsX[i].firstleafface = LittleLong (dleafsX[i].firstleafface);
            dleafsX[i].numleaffaces = LittleLong (dleafsX[i].numleaffaces);
            dleafsX[i].firstleafbrush = LittleLong (dleafsX[i].firstleafbrush);
            dleafsX[i].numleafbrushes = LittleLong (dleafsX[i].numleafbrushes);
        }

        for (i=0 ; i<numbrushsides ; i++)
        {
            dbrushsidesX[i].planenum = LittleLong (dbrushsidesX[i].planenum);
            dbrushsidesX[i].texinfo = LittleLong (dbrushsidesX[i].texinfo);
        }

        for (i=0 ; i<numleafbrushes ; i++)
            dleafbrushesX[i] = LittleLong (dleafbrushesX[i]);

        for (i=0 ; i<numleaffaces ; i++)
            dleaffacesX[i] = LittleLong (dleaffacesX[i]);

    }
    else
    {
        for (i=0 ; i<numfaces ; i++)
        {
            dfaces[i].texinfo = LittleShort (dfaces[i].texinfo);
            dfaces[i].planenum = LittleShort (dfaces[i].planenum);
            dfaces[i].side = LittleShort (dfaces[i].side);
            dfaces[i].lightofs = LittleLong (dfaces[i].lightofs);
            dfaces[i].firstedge = LittleLong (dfaces[i].firstedge);
            dfaces[i].numedges = LittleShort (dfaces[i].numedges);
        }

        for (i=0 ; i<numnodes ; i++)
        {
            dnodes[i].planenum = LittleLong (dnodes[i].planenum);
            for (j=0 ; j<3 ; j++)
            {
                dnodes[i].mins[j] = LittleShort (dnodes[i].mins[j]);
                dnodes[i].maxs[j] = LittleShort (dnodes[i].maxs[j]);
            }
            dnodes[i].children[0] = LittleLong (dnodes[i].children[0]);
            dnodes[i].children[1] = LittleLong (dnodes[i].children[1]);
            dnodes[i].firstface = LittleShort (dnodes[i].firstface);
            dnodes[i].numfaces = LittleShort (dnodes[i].numfaces);
        }

        for (i=0 ; i<numleafs ; i++)
        {
            dleafs[i].contents = LittleLong (dleafs[i].contents);
            dleafs[i].cluster = LittleShort (dleafs[i].cluster);
            dleafs[i].area = LittleShort (dleafs[i].area);
            for (j=0 ; j<3 ; j++)
            {
                dleafs[i].mins[j] = LittleShort (dleafs[i].mins[j]);
                dleafs[i].maxs[j] = LittleShort (dleafs[i].maxs[j]);
            }
            dleafs[i].firstleafface = LittleShort (dleafs[i].firstleafface);
            dleafs[i].numleaffaces = LittleShort (dleafs[i].numleaffaces);
            dleafs[i].firstleafbrush = LittleShort (dleafs[i].firstleafbrush);
            dleafs[i].numleafbrushes = LittleShort (dleafs[i].numleafbrushes);
        }

        for (i=0 ; i<numbrushsides ; i++)
        {
            dbrushsides[i].planenum = LittleShort (dbrushsides[i].planenum);
            dbrushsides[i].texinfo = LittleShort (dbrushsides[i].texinfo);
        }

        for (i=0 ; i<numleafbrushes ; i++)
            dleafbrushes[i] = LittleShort (dleafbrushes[i]);

        for (i=0 ; i<numleaffaces ; i++)
            dleaffaces[i] = LittleShort (dleaffaces[i]);

    }


//
// surfedges
//
    for (i=0 ; i<numsurfedges ; i++)
        dsurfedges[i] = LittleLong (dsurfedges[i]);

//
// edges
//
    if(use_qbsp)
        for (i=0 ; i<numedges ; i++)
        {
            dedgesX[i].v[0] = LittleLong (dedgesX[i].v[0]);
            dedgesX[i].v[1] = LittleLong (dedgesX[i].v[1]);
        }
    else
        for (i=0 ; i<numedges ; i++)
        {
            dedges[i].v[0] = LittleShort (dedges[i].v[0]);
            dedges[i].v[1] = LittleShort (dedges[i].v[1]);
        }

//
// brushes
//
    for (i=0 ; i<numbrushes ; i++)
    {
        dbrushes[i].firstside = LittleLong (dbrushes[i].firstside);
        dbrushes[i].numsides = LittleLong (dbrushes[i].numsides);
        dbrushes[i].contents = LittleLong (dbrushes[i].contents);
    }

//
// areas
//
    for (i=0 ; i<numareas ; i++)
    {
        dareas[i].numareaportals = LittleLong (dareas[i].numareaportals);
        dareas[i].firstareaportal = LittleLong (dareas[i].firstareaportal);
    }

//
// areasportals
//
    for (i=0 ; i<numareaportals ; i++)
    {
        dareaportals[i].portalnum = LittleLong (dareaportals[i].portalnum);
        dareaportals[i].otherarea = LittleLong (dareaportals[i].otherarea);
    }

//
// visibility
//
    if (todisk)
        j = dvis->numclusters;
    else
        j = LittleLong(dvis->numclusters);
    dvis->numclusters = LittleLong (dvis->numclusters);
    for (i=0 ; i<j ; i++)
    {
        dvis->bitofs[i][0] = LittleLong (dvis->bitofs[i][0]);
        dvis->bitofs[i][1] = LittleLong (dvis->bitofs[i][1]);
    }
}


dheader_t	*header;

int32_t CopyLump (int32_t lump, void *dest, int32_t size)
{
    int32_t		length, ofs;

    length = header->lumps[lump].filelen;
    ofs = header->lumps[lump].fileofs;

    if (length % size)
        Error ("LoadBSPFile: odd lump size  (length: %i  size: %i  remainder: %i)", length, size, length % size);

    memcpy (dest, (byte *)header + ofs, length);

    return length / size;
}

/*
=============
LoadBSPFile
=============
*/
void	LoadBSPFile (char *filename)
{
    int32_t			i;

//
// load the file header
//
    LoadFile (filename, (void **)&header);

// swap the header
    for (i=0 ; i< sizeof(dheader_t)/4 ; i++)
        ((int32_t *)header)[i] = LittleLong ( ((int32_t *)header)[i]);

//qb: qbsp
    use_qbsp = false;

    switch (header->ident)
    {
    case IDBSPHEADER:
        break;
    case QBSPHEADER:
        use_qbsp = true;
        printf("using QBSP extended limits \n");
        break;
    default:
        Error("%s is not a recognized BSP file (IBSP or QBSP).",
              filename);
    }
    if (header->version != BSPVERSION)
        Error ("%s is version %i, not %i", filename, header->version, BSPVERSION);

    nummodels = CopyLump (LUMP_MODELS, dmodels, sizeof(dmodel_t));
    numvertexes = CopyLump (LUMP_VERTEXES, dvertexes, sizeof(dvertex_t));
    numplanes = CopyLump (LUMP_PLANES, dplanes, sizeof(dplane_t));

    if(use_qbsp)
    {
        numleafs = CopyLump (LUMP_LEAFS, dleafsX, sizeof(dleaf_tx));
        numnodes = CopyLump (LUMP_NODES, dnodesX, sizeof(dnode_tx));
        numtexinfo = CopyLump (LUMP_TEXINFO, texinfo, sizeof(texinfo_t));
        numfaces = CopyLump (LUMP_FACES, dfacesX, sizeof(dface_tx));
        numleaffaces = CopyLump (LUMP_LEAFFACES, dleaffacesX, sizeof(dleaffacesX[0]));
        numleafbrushes = CopyLump (LUMP_LEAFBRUSHES, dleafbrushesX, sizeof(dleafbrushesX[0]));
    }
    else
    {
        numleafs = CopyLump (LUMP_LEAFS, dleafs, sizeof(dleaf_t));
        numnodes = CopyLump (LUMP_NODES, dnodes, sizeof(dnode_t));
        numtexinfo = CopyLump (LUMP_TEXINFO, texinfo, sizeof(texinfo_t));
        numfaces = CopyLump (LUMP_FACES, dfaces, sizeof(dface_t));
        numleaffaces = CopyLump (LUMP_LEAFFACES, dleaffaces, sizeof(dleaffaces[0]));
        numleafbrushes = CopyLump (LUMP_LEAFBRUSHES, dleafbrushes, sizeof(dleafbrushes[0]));
    }

    numsurfedges = CopyLump (LUMP_SURFEDGES, dsurfedges, sizeof(dsurfedges[0]));

    if(use_qbsp)
        numedges = CopyLump (LUMP_EDGES, dedgesX, sizeof(dedge_tx));
    else
        numedges = CopyLump (LUMP_EDGES, dedges, sizeof(dedge_t));

    numbrushes = CopyLump (LUMP_BRUSHES, dbrushes, sizeof(dbrush_t));

    if(use_qbsp)
        numbrushsides = CopyLump (LUMP_BRUSHSIDES, dbrushsidesX, sizeof(dbrushside_tx));
    else
        numbrushsides = CopyLump (LUMP_BRUSHSIDES, dbrushsides, sizeof(dbrushside_t));

    numareas = CopyLump (LUMP_AREAS, dareas, sizeof(darea_t));
    numareaportals = CopyLump (LUMP_AREAPORTALS, dareaportals, sizeof(dareaportal_t));

    visdatasize = CopyLump (LUMP_VISIBILITY, dvisdata, 1);
    lightdatasize = CopyLump (LUMP_LIGHTING, dlightdata, 1);
    entdatasize = CopyLump (LUMP_ENTITIES, dentdata, 1);

    CopyLump (LUMP_POP, dpop, 1);

    free (header);		// everything has been copied out

//
// swap everything
//
    SwapBSPFile (false);
}


/*
=============
LoadBSPFileTexinfo

Only loads the texinfo lump, so 4data can scan for textures
=============
*/
void	LoadBSPFileTexinfo (char *filename)
{
    int32_t			i;
    FILE		*f;
    int32_t		length, ofs;

    header = malloc(sizeof(dheader_t));

    f = fopen (filename, "rb");
    if (!fread (header, sizeof(dheader_t), 1, f))
        Error ("Texinfo header read error");

// swap the header
    for (i=0 ; i< sizeof(dheader_t)/4 ; i++)
        ((int32_t *)header)[i] = LittleLong ( ((int32_t *)header)[i]);

    if (header->ident != IDBSPHEADER && header->ident != QBSPHEADER)
        Error ("%s is not an IBSP or QBSP file", filename);
    if (header->version != BSPVERSION)
        Error ("%s is version %i, not %i", filename, header->version, BSPVERSION);


    length = header->lumps[LUMP_TEXINFO].filelen;
    ofs = header->lumps[LUMP_TEXINFO].fileofs;

    fseek (f, ofs, SEEK_SET);
    if(!fread (texinfo, length, 1, f))
        Error ("Texinfo lump read error");
    fclose (f);

    numtexinfo = length / sizeof(texinfo_t);

    free (header);		// everything has been copied out

    SwapBSPFile (false);
}


//============================================================================

FILE		*wadfile;
dheader_t	outheader;

void AddLump (int32_t lumpnum, void *data, int32_t len)
{
    lump_t *lump;

    lump = &header->lumps[lumpnum];

    lump->fileofs = LittleLong( ftell(wadfile) );
    lump->filelen = LittleLong(len);
    SafeWrite (wadfile, data, (len+3)&~3);
}

/*
=============
WriteBSPFile

Swaps the bsp file in place, so it should not be referenced again
=============
*/
void	WriteBSPFile (char *filename)
{

    header = &outheader;
    memset (header, 0, sizeof(dheader_t));

    SwapBSPFile (true);

    if (use_qbsp)
        header->ident = LittleLong (QBSPHEADER);
    else
        header->ident = LittleLong (IDBSPHEADER);

    header->version = LittleLong (BSPVERSION);

    wadfile = SafeOpenWrite (filename);
    SafeWrite (wadfile, header, sizeof(dheader_t));	// overwritten later

    AddLump (LUMP_PLANES, dplanes, numplanes*sizeof(dplane_t));

    if(use_qbsp)
        AddLump (LUMP_LEAFS, dleafsX, numleafs*sizeof(dleaf_tx));
    else
        AddLump (LUMP_LEAFS, dleafs, numleafs*sizeof(dleaf_t));

    AddLump (LUMP_VERTEXES, dvertexes, numvertexes*sizeof(dvertex_t));

    if(use_qbsp)
        AddLump (LUMP_NODES, dnodesX, numnodes*sizeof(dnode_tx));
    else
        AddLump (LUMP_NODES, dnodes, numnodes*sizeof(dnode_t));

    AddLump (LUMP_TEXINFO, texinfo, numtexinfo*sizeof(texinfo_t));

    if(use_qbsp)
        AddLump (LUMP_FACES, dfacesX, numfaces*sizeof(dface_tx));
    else
        AddLump (LUMP_FACES, dfaces, numfaces*sizeof(dface_t));

    AddLump (LUMP_BRUSHES, dbrushes, numbrushes*sizeof(dbrush_t));

    if(use_qbsp)
    {
        AddLump (LUMP_BRUSHSIDES, dbrushsidesX, numbrushsides*sizeof(dbrushside_tx));
        AddLump (LUMP_LEAFFACES, dleaffacesX, numleaffaces*sizeof(dleaffacesX[0]));
        AddLump (LUMP_LEAFBRUSHES, dleafbrushesX, numleafbrushes*sizeof(dleafbrushesX[0]));
    }
    else
    {
        AddLump (LUMP_BRUSHSIDES, dbrushsides, numbrushsides*sizeof(dbrushside_t));
        AddLump (LUMP_LEAFFACES, dleaffaces, numleaffaces*sizeof(dleaffaces[0]));
        AddLump (LUMP_LEAFBRUSHES, dleafbrushes, numleafbrushes*sizeof(dleafbrushes[0]));
    }

    AddLump (LUMP_SURFEDGES, dsurfedges, numsurfedges*sizeof(dsurfedges[0]));

    if(use_qbsp)
        AddLump (LUMP_EDGES, dedgesX, numedges*sizeof(dedge_tx));
    else
        AddLump (LUMP_EDGES, dedges, numedges*sizeof(dedge_t));

    AddLump (LUMP_MODELS, dmodels, nummodels*sizeof(dmodel_t));
    AddLump (LUMP_AREAS, dareas, numareas*sizeof(darea_t));
    AddLump (LUMP_AREAPORTALS, dareaportals, numareaportals*sizeof(dareaportal_t));

    AddLump (LUMP_LIGHTING, dlightdata, lightdatasize);
    AddLump (LUMP_VISIBILITY, dvisdata, visdatasize);
    AddLump (LUMP_ENTITIES, dentdata, entdatasize);
    AddLump (LUMP_POP, dpop, sizeof(dpop));

    fseek (wadfile, 0, SEEK_SET);
    SafeWrite (wadfile, header, sizeof(dheader_t));
    fclose (wadfile);
}

//============================================================================

/*
=============
PrintBSPFileSizes

Dumps info about current file
=============
*/
void PrintBSPFileSizes (void)
{
    if (!num_entities)
        ParseEntities ();
    printf( "\n<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< FILE STATS >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n" );
    printf ("models:      %7i        size: %7i\n", nummodels, (int32_t)(nummodels*sizeof(dmodel_t)));
    printf ("brushes:     %7i        size: %7i\n", numbrushes, (int32_t)(numbrushes*sizeof(dbrush_t)));

    if (use_qbsp)
    printf ("brushsides:  %7i        size: %7i\n", numbrushsides, (int32_t)(numbrushsides*sizeof(dbrushside_tx)));
    else
    printf ("brushsides:  %7i        size: %7i\n", numbrushsides, (int32_t)(numbrushsides*sizeof(dbrushside_t)));

    printf ("planes:      %7i        size: %7i\n", numplanes, (int32_t)(numplanes*sizeof(dplane_t)));
    printf ("texinfo:     %7i        size: %7i\n", numtexinfo, (int32_t)(numtexinfo*sizeof(texinfo_t)));
    printf ("entdata:     %7i        size: %7i\n", num_entities, entdatasize);

    printf ("vertices:    %7i        size: %7i\n", numvertexes, (int32_t)(numvertexes*sizeof(dvertex_t)));

    if (use_qbsp)
    {
        printf ("nodes:       %7i        size: %7i\n", numnodes, (int32_t)(numnodes*sizeof(dnode_tx)));
        printf ("faces:       %7i        size: %7i\n",numfaces, (int32_t)(numfaces*sizeof(dface_tx)));
        printf ("leafs:       %7i        size: %7i\n", numleafs, (int32_t)(numleafs*sizeof(dleaf_tx)));
        printf ("leaffaces:   %7i        size: %7i\n",numleaffaces, (int32_t)(numleaffaces*sizeof(dleaffacesX[0])));
        printf ("leafbrushes: %7i        size: %7i\n",numleafbrushes, (int32_t)(numleafbrushes*sizeof(dleafbrushesX[0])));
        printf ("edges:       %7i        size: %7i\n",numedges, (int32_t)(numedges*sizeof(dedge_tx)));
    }
    else
    {
        printf ("nodes:       %7i        size: %7i\n", numnodes, (int32_t)(numnodes*sizeof(dnode_t)));
        printf ("faces:       %7i        size: %7i\n", numfaces, (int32_t)(numfaces*sizeof(dface_t)));
        printf ("leafs:       %7i        size: %7i\n", numleafs, (int32_t)(numleafs*sizeof(dleaf_t)));
        printf ("leaffaces:   %7i        size: %7i\n", numleaffaces, (int32_t)(numleaffaces*sizeof(dleaffaces[0])));
        printf ("leafbrushes: %7i        size: %7i\n", numleafbrushes, (int32_t)(numleafbrushes*sizeof(dleafbrushes[0])));
        printf ("edges:       %7i        size: %7i\n", numedges, (int32_t)(numedges*sizeof(dedge_t)));
    }

        printf ("surfedges:   %7i        size: %7i\n",numsurfedges, (int32_t)(numsurfedges*sizeof(dsurfedges[0])));
        printf ("                  lightdata size: %7i\n", lightdatasize);
        printf ("                    visdata size: %7i\n", visdatasize);
}


//============================================

int32_t			num_entities;
entity_t	entities[MAX_MAP_ENTITIES_QBSP];

void StripTrailing (char *e)
{
    char	*s;

    s = e + strlen(e)-1;
    while (s >= e && *s <= 32)
    {
        *s = 0;
        s--;
    }
}

/*
=================
ParseEpair
=================
*/
epair_t *ParseEpair (void)
{
    epair_t	*e;

    e = malloc (sizeof(epair_t));
    memset (e, 0, sizeof(epair_t));

    if (strlen(token) >= MAX_KEY-1)
        Error ("ParseEpar: token too long");
    e->key = copystring(token);
    GetToken (false);
    if (strlen(token) >= MAX_VALUE-1)
        Error ("ParseEpar: token too long");
    e->value = copystring(token);

    // strip trailing spaces
    StripTrailing (e->key);
    StripTrailing (e->value);

    return e;
}


/*
================
ParseEntity
================
*/
qboolean	ParseEntity (void)
{
    epair_t		*e;
    entity_t	*mapent;

    if (!GetToken (true))
        return false;

    if (strcmp (token, "{") )
        Error ("ParseEntity: { not found");

    if (use_qbsp)
    {
        if (num_entities == MAX_MAP_ENTITIES_QBSP)
            Error ("num_entities == MAX_MAP_ENTITIES_QBSP  (%i)", MAX_MAP_ENTITIES_QBSP);
    }
    else if (num_entities == MAX_MAP_ENTITIES)
        Error ("num_entities == MAX_MAP_ENTITIES  (%i)", MAX_MAP_ENTITIES);

    mapent = &entities[num_entities];
    num_entities++;

    do
    {
        if (!GetToken (true))
            Error ("ParseEntity: EOF without closing brace");
        if (!strcmp (token, "}") )
            break;
        e = ParseEpair ();
        e->next = mapent->epairs;
        mapent->epairs = e;
    }
    while (1);

    return true;
}

/*
================
ParseEntities

Parses the dentdata string into entities
================
*/
void ParseEntities (void)
{
    num_entities = 0;
    ParseFromMemory (dentdata, entdatasize);

    while (ParseEntity ())
    {
    }
}


/*
================
UnparseEntities

Generates the dentdata string from all the entities
================
*/
void UnparseEntities (void)
{
    char	*buf, *end;
    epair_t	*ep;
    char	line[2060];
    int32_t		i;
    char	key[1024], value[1024];

    buf = dentdata;
    end = buf;
    *end = 0;

    for (i=0 ; i<num_entities ; i++)
    {
        ep = entities[i].epairs;
        if (!ep)
            continue;	// ent got removed

        strcat (end,"{\n");
        end += 2;

        for (ep = entities[i].epairs ; ep ; ep=ep->next)
        {
            strcpy (key, ep->key);
            StripTrailing (key);
            strcpy (value, ep->value);
            StripTrailing (value);

            sprintf (line, "\"%s\" \"%s\"\n", key, value);
            strcat (end, line);
            end += strlen(line);
        }
        strcat (end,"}\n");
        end += 2;

        if (use_qbsp)
        {
            if (end > buf + MAX_MAP_ENTSTRING_QBSP)
                Error ("QBSP Entity text too long");
        }
        else if (end > buf + MAX_MAP_ENTSTRING)
            Error ("Entity text too long");
    }
    entdatasize = end - buf + 1;
}

void PrintEntity (entity_t *ent)
{
    epair_t	*ep;

    printf ("------- entity %p -------\n", ent);
    for (ep=ent->epairs ; ep ; ep=ep->next)
    {
        printf ("%s = %s\n", ep->key, ep->value);
    }

}

void 	SetKeyValue (entity_t *ent, char *key, char *value)
{
    epair_t	*ep;

    for (ep=ent->epairs ; ep ; ep=ep->next)
        if (!strcmp (ep->key, key) )
        {
            free (ep->value);
            ep->value = copystring(value);
            return;
        }
    ep = malloc (sizeof(*ep));
    if (ep) //qb: gcc -fanalyzer: could be NULL
    {
        ep->next = ent->epairs;
        ent->epairs = ep;
        ep->key = copystring(key);
        ep->value = copystring(value);
    }
}

char 	*ValueForKey (entity_t *ent, char *key)
{
    epair_t	*ep;

    for (ep=ent->epairs ; ep ; ep=ep->next)
        if (!strcmp (ep->key, key) )
            return ep->value;
    return "";
}

vec_t	FloatForKey (entity_t *ent, char *key)
{
    char	*k;

    k = ValueForKey (ent, key);
    return atof(k);
}

void 	GetVectorForKey (entity_t *ent, char *key, vec3_t vec)
{
    char	*k;
    double	v1, v2, v3;

    k = ValueForKey (ent, key);
// scanf into doubles, then assign, so it is vec_t size independent
    v1 = v2 = v3 = 0;
    sscanf (k, "%lf %lf %lf", &v1, &v2, &v3);
    vec[0] = v1;
    vec[1] = v2;
    vec[2] = v3;
}

void RemoveLastEpair( entity_t *ent )
{
    epair_t	*e = ent->epairs->next;

    if ( ent->epairs->key )
        free( ent->epairs->key );
    if ( ent->epairs->value )
        free( ent->epairs->value );

    free( ent->epairs );
    ent->epairs = e;
}
