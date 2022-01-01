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

int32_t		c_nofaces;
int32_t		c_facenodes;


/*
=========================================================

ONLY SAVE OUT PLANES THAT ARE ACTUALLY USED AS NODES

=========================================================
*/

int32_t		planeused[MAX_MAP_PLANES_QBSP];

/*
============
EmitPlanes

There is no oportunity to discard planes, because all of the original
brushes will be saved in the map.
============
*/
void EmitPlanes (void)
{
    int32_t			i;
    dplane_t	*dp;
    plane_t		*mp;

    mp = mapplanes;
    for (i=0 ; i<nummapplanes ; i++, mp++)
    {
        dp = &dplanes[numplanes];
        VectorCopy ( mp->normal, dp->normal);
        dp->dist = mp->dist;
        dp->type = mp->type;
        numplanes++;
    }
}


//========================================================

void EmitMarkFace (dleaf_t *leaf_p, face_t *f)
{
    int32_t			i;
    int32_t			facenum;

    while (f->merged)
        f = f->merged;

    if (f->split[0])
    {
        EmitMarkFace (leaf_p, f->split[0]);
        EmitMarkFace (leaf_p, f->split[1]);
        return;
    }

    facenum = f->outputnumber;
    if (facenum == -1)
        return;	// degenerate face

    if (facenum < 0 || facenum >= numfaces)
        Error ("Bad leafface");
    for (i=leaf_p->firstleafface ; i<numleaffaces ; i++)
        if (dleaffaces[i] == facenum)
            break;		// merged out face
    if (i == numleaffaces)
    {
        if (numleaffaces >= MAX_MAP_LEAFFACES)
            Error ("MAX_MAP_LEAFFACES");

        dleaffaces[numleaffaces] =  facenum;
        numleaffaces++;
    }

}

void EmitMarkFaceX (dleaf_tx *leaf_p, face_t *f)
{
    int32_t			i;
    int32_t			facenum;

    while (f->merged)
        f = f->merged;

    if (f->split[0])
    {
        EmitMarkFaceX (leaf_p, f->split[0]);
        EmitMarkFaceX (leaf_p, f->split[1]);
        return;
    }

    facenum = f->outputnumber;
    if (facenum == -1)
        return;	// degenerate face

    if (facenum < 0 || facenum >= numfaces)
        Error ("Bad leafface");
    for (i=leaf_p->firstleafface ; i<numleaffaces ; i++)
        if (dleaffacesX[i] == facenum)
            break;		// merged out face
    if (i == numleaffaces)
    {
        if (numleaffaces >= MAX_MAP_LEAFFACES_QBSP)
            Error ("MAX_MAP_LEAFFACES_QBSP");

        dleaffacesX[numleaffaces] =  facenum;
        numleaffaces++;
    }

}


/*
==================
EmitLeaf
==================
*/
void EmitLeaf (node_t *node)
{
    portal_t	*p;
    int32_t			s;
    face_t		*f;
    bspbrush_t	*b;
    int32_t			i;
    int32_t			brushnum;

    // emit a leaf
    if (use_qbsp) //qb: qbsp
    {
        dleaf_tx		*leaf_p;
        if (numleafs >= MAX_MAP_LEAFS_QBSP)
            Error ("MAX_MAP_LEAFS_QBSP");

        leaf_p = &dleafsX[numleafs];
        numleafs++;

        leaf_p->contents = node->contents;
        leaf_p->cluster = node->cluster;
        leaf_p->area = node->area;

        //
        // write bounding box info
        //
        VectorCopy (node->mins, leaf_p->mins);
        VectorCopy (node->maxs, leaf_p->maxs);

        //
        // write the leafbrushes
        //
        leaf_p->firstleafbrush = numleafbrushes;
        for (b=node->brushlist ; b ; b=b->next)
        {
            if (numleafbrushes >= MAX_MAP_LEAFBRUSHES_QBSP)
                Error ("MAX_MAP_LEAFBRUSHES_QBSP");

            brushnum = b->original - mapbrushes;
            for (i=leaf_p->firstleafbrush ; i<numleafbrushes ; i++)
                if (dleafbrushesX[i] == brushnum)
                    break;
            if (i == numleafbrushes)
            {
                dleafbrushesX[numleafbrushes] = brushnum;
                numleafbrushes++;
            }
        }
        leaf_p->numleafbrushes = numleafbrushes - leaf_p->firstleafbrush;

        //
        // write the leaffaces
        //
        if (leaf_p->contents & CONTENTS_SOLID)
            return;		// no leaffaces in solids

        leaf_p->firstleafface = numleaffaces;

        for (p = node->portals ; p ; p = p->next[s])
        {
            s = (p->nodes[1] == node);
            f = p->face[s];
            if (!f)
                continue;	// not a visible portal

            EmitMarkFaceX (leaf_p, f);
        }

        leaf_p->numleaffaces = numleaffaces - leaf_p->firstleafface;
    }

    else
    {
        dleaf_t		*leaf_p;
        if (numleafs >= MAX_MAP_LEAFS)
            Error ("MAX_MAP_LEAFS");

        leaf_p = &dleafs[numleafs];
        numleafs++;

        leaf_p->contents = node->contents;
        leaf_p->cluster = node->cluster;
        leaf_p->area = node->area;

        //
        // write bounding box info
        //
        VectorCopy (node->mins, leaf_p->mins);
        VectorCopy (node->maxs, leaf_p->maxs);

        //
        // write the leafbrushes
        //
        leaf_p->firstleafbrush = numleafbrushes;
        for (b=node->brushlist ; b ; b=b->next)
        {
            if (numleafbrushes >= MAX_MAP_LEAFBRUSHES)
                Error ("MAX_MAP_LEAFBRUSHES");

            brushnum = b->original - mapbrushes;
            for (i=leaf_p->firstleafbrush ; i<numleafbrushes ; i++)
                if (dleafbrushes[i] == brushnum)
                    break;
            if (i == numleafbrushes)
            {
                dleafbrushes[numleafbrushes] = brushnum;
                numleafbrushes++;
            }
        }
        leaf_p->numleafbrushes = numleafbrushes - leaf_p->firstleafbrush;

        //
        // write the leaffaces
        //
        if (leaf_p->contents & CONTENTS_SOLID)
            return;		// no leaffaces in solids

        leaf_p->firstleafface = numleaffaces;

        for (p = node->portals ; p ; p = p->next[s])
        {
            s = (p->nodes[1] == node);
            f = p->face[s];
            if (!f)
                continue;	// not a visible portal

            EmitMarkFace (leaf_p, f);
        }

        leaf_p->numleaffaces = numleaffaces - leaf_p->firstleafface;
    }
}

/*
==================
EmitFace
==================
*/
void EmitFace (face_t *f)
{
    int32_t		i;
    int32_t		e;

    f->outputnumber = -1;

    if (f->numpoints < 3)
    {
        return;		// degenerated
    }
    if (f->merged || f->split[0] || f->split[1])
    {
        return;		// not a final face
    }

    // save output number so leaffaces can use
    f->outputnumber = numfaces;

    if(use_qbsp)
    {
        dface_tx	*df;
        if (numfaces >= MAX_MAP_FACES_QBSP)
            Error ("numfaces == MAX_MAP_FACES_QBSP");
        df = &dfacesX[numfaces];
        numfaces++;

        // planenum is used by qlight, but not the engine
        df->planenum = f->planenum & (~1);
        df->side = f->planenum & 1;

        df->firstedge = numsurfedges;
        df->numedges = f->numpoints;
        df->texinfo = f->texinfo;
        for (i=0 ; i<f->numpoints ; i++)
        {
            e = GetEdge (f->vertexnums[i], f->vertexnums[(i+1)%f->numpoints], f);
            if (numsurfedges >= MAX_MAP_SURFEDGES_QBSP)
                Error ("numsurfedges == MAX_MAP_SURFEDGES_QBSP");
            dsurfedges[numsurfedges] = e;
            numsurfedges++;
        }
    }
    else
    {
        dface_t	*df;
        if (numfaces >= MAX_MAP_FACES)
            Error ("numfaces == MAX_MAP_FACES");
        df = &dfaces[numfaces];
        numfaces++;

        // planenum is used by qlight, but not the engine
        df->planenum = f->planenum & (~1);
        df->side = f->planenum & 1;

        df->firstedge = numsurfedges;
        df->numedges = f->numpoints;
        df->texinfo = f->texinfo;
        for (i=0 ; i<f->numpoints ; i++)
        {
            e = GetEdge (f->vertexnums[i], f->vertexnums[(i+1)%f->numpoints], f);
            if (numsurfedges >= MAX_MAP_SURFEDGES)
                Error ("numsurfedges == MAX_MAP_SURFEDGES");
            dsurfedges[numsurfedges] = e;
            numsurfedges++;
        }
    }
}


/*
============
EmitDrawingNode_r
============
*/
int32_t EmitDrawNode_r (node_t *node)
{
    face_t	*f;
    int32_t		i;

    if (node->planenum == PLANENUM_LEAF)
    {
        EmitLeaf (node);
        return -numleafs;
    }

// emit a node
    if (use_qbsp)
    {
        dnode_tx	*n;
        if (numnodes == MAX_MAP_NODES_QBSP)
            Error ("MAX_MAP_NODES_QBSP");
        n = &dnodesX[numnodes];
        numnodes++;

        VectorCopy (node->mins, n->mins);
        VectorCopy (node->maxs, n->maxs);

        planeused[node->planenum]++;
        planeused[node->planenum^1]++;

        if (node->planenum & 1)
            Error ("WriteDrawNodes_r: odd planenum");
        n->planenum = node->planenum;
        n->firstface = numfaces;

        if (!node->faces)
            c_nofaces++;
        else
            c_facenodes++;

        for (f=node->faces ; f ; f=f->next)
            EmitFace (f);

        n->numfaces = numfaces - n->firstface;

        // recursively output the other nodes
        for (i=0 ; i<2 ; i++)
        {
            if (node->children[i]->planenum == PLANENUM_LEAF)
            {
                n->children[i] = -(numleafs + 1);
                EmitLeaf (node->children[i]);
            }
            else
            {
                n->children[i] = numnodes;
                EmitDrawNode_r (node->children[i]);
            }
        }

        return n - dnodesX;
    }

    else  //ibsp
    {
        dnode_t	*n;
        if (numnodes == MAX_MAP_NODES)
            Error ("MAX_MAP_NODES");
        n = &dnodes[numnodes];
        numnodes++;

        VectorCopy (node->mins, n->mins);
        VectorCopy (node->maxs, n->maxs);

        planeused[node->planenum]++;
        planeused[node->planenum^1]++;

        if (node->planenum & 1)
            Error ("WriteDrawNodes_r: odd planenum");
        n->planenum = node->planenum;
        n->firstface = numfaces;

        if (!node->faces)
            c_nofaces++;
        else
            c_facenodes++;

        for (f=node->faces ; f ; f=f->next)
            EmitFace (f);

        n->numfaces = numfaces - n->firstface;

        // recursively output the other nodes
        for (i=0 ; i<2 ; i++)
        {
            if (node->children[i]->planenum == PLANENUM_LEAF)
            {
                n->children[i] = -(numleafs + 1);
                EmitLeaf (node->children[i]);
            }
            else
            {
                n->children[i] = numnodes;
                EmitDrawNode_r (node->children[i]);
            }
        }

        return n - dnodes;
    }
}

//=========================================================


/*
============
WriteBSP
============
*/
void WriteBSP (node_t *headnode)
{
    int32_t		oldfaces;

    c_nofaces = 0;
    c_facenodes = 0;

    qprintf ("--- WriteBSP ---\n");
    oldfaces = numfaces;
    dmodels[nummodels].headnode = EmitDrawNode_r (headnode);
    EmitAreaPortals (headnode);

    qprintf ("%5i nodes with faces\n", c_facenodes);
    qprintf ("%5i nodes without faces\n", c_nofaces);
    qprintf ("%5i faces\n", numfaces-oldfaces);
}

//===========================================================

/*
============
SetModelNumbers
============
*/
void SetModelNumbers (void)
{
    int32_t		i;
    int32_t		models;
    char	value[12];

    models = 1;
    for (i=1 ; i<num_entities ; i++)
    {
        if (entities[i].numbrushes)
        {
            sprintf (value, "*%i", models);
            models++;
            SetKeyValue (&entities[i], "model", value);
        }
    }

}

/*
============
SetLightStyles
============
*/
#define	MAX_SWITCHED_LIGHTS	32
void SetLightStyles (void)
{
    int32_t		stylenum;
    char	*t;
    entity_t	*e;
    int32_t		i, j;
    char	value[11];
    char	lighttargets[MAX_SWITCHED_LIGHTS][64];


    // any light that is controlled (has a targetname)
    // must have a unique style number generated for it

    stylenum = 0;
    for (i=1 ; i<num_entities ; i++)
    {
        e = &entities[i];

        t = ValueForKey (e, "classname");
        if (Q_strncasecmp (t, "light", 5))
            continue;
        t = ValueForKey (e, "targetname");
        if (!t[0])
            continue;

        // find this targetname
        for (j=0 ; j<stylenum ; j++)
            if (!strcmp (lighttargets[j], t))
                break;
        if (j == stylenum)
        {
            if (stylenum == MAX_SWITCHED_LIGHTS)
                Error ("stylenum == MAX_SWITCHED_LIGHTS");
            strcpy (lighttargets[j], t);
            stylenum++;
        }
        sprintf (value, "%i", 32 + j);
        SetKeyValue (e, "style", value);
    }

}

//===========================================================

/*
============
EmitBrushes
============
*/
void EmitBrushes (void)
{
    int32_t			i, j, bnum, s, x;
    dbrush_t	*db;
    mapbrush_t		*b;
    vec3_t		normal;
    vec_t		dist;
    int32_t			planenum;

    numbrushsides = 0;
    numbrushes = nummapbrushes;

    for (bnum=0 ; bnum<nummapbrushes ; bnum++)
    {
        b = &mapbrushes[bnum];
        db = &dbrushes[bnum];

        db->contents = b->contents;
        db->firstside = numbrushsides;
        db->numsides = b->numsides;

        if(use_qbsp)
        {
            dbrushside_tx	*cp;
            for (j=0 ; j<b->numsides ; j++)
            {
                if (numbrushsides == MAX_MAP_BRUSHSIDES_QBSP)
                    Error ("MAX_MAP_BRUSHSIDES_QBSP");
                cp = &dbrushsidesX[numbrushsides];
                numbrushsides++;
                cp->planenum = b->original_sides[j].planenum;
                cp->texinfo = b->original_sides[j].texinfo;
            }
        }
        else
        {
            for (j=0 ; j<b->numsides ; j++)
            {
                dbrushside_t	*cp;
                if (numbrushsides == MAX_MAP_BRUSHSIDES)
                    Error ("MAX_MAP_BRUSHSIDES");
                cp = &dbrushsides[numbrushsides];
                numbrushsides++;
                cp->planenum = b->original_sides[j].planenum;
                cp->texinfo = b->original_sides[j].texinfo;
            }
        }

        // add any axis planes not contained in the brush to bevel off corners
        for (x=0 ; x<3 ; x++)
            for (s=-1 ; s<=1 ; s+=2)
            {
                // add the plane
                VectorCopy (vec3_origin, normal);
                normal[x] = s;
                if (s == -1)
                    dist = -b->mins[x];
                else
                    dist = b->maxs[x];
                planenum = FindFloatPlane (normal, dist, b->brushnum);
                for (i=0 ; i<b->numsides ; i++)
                    if (b->original_sides[i].planenum == planenum)
                        break;
                if (i == b->numsides)
                {
                    if (use_qbsp)
                    {
                        if (numbrushsides >= MAX_MAP_BRUSHSIDES_QBSP)
                            Error ("MAX_MAP_BRUSHSIDES_QBSP");
                        dbrushsidesX[numbrushsides].planenum = planenum;
                        dbrushsidesX[numbrushsides].texinfo = dbrushsidesX[numbrushsides-1].texinfo;

                    }
                    else
                    {
                        if (numbrushsides >= MAX_MAP_BRUSHSIDES)
                            Error ("MAX_MAP_BRUSHSIDES");
                        dbrushsides[numbrushsides].planenum = planenum;
                        dbrushsides[numbrushsides].texinfo = dbrushsides[numbrushsides-1].texinfo;
                    }
                    numbrushsides++;
                    db->numsides++;
                }
            }
    }

}

//===========================================================

/*
==================
BeginBSPFile
==================
*/
void BeginBSPFile (void)
{
    // these values may actually be initialized
    // if the file existed when loaded, so clear them explicitly
    nummodels = 0;
    numfaces = 0;
    numnodes = 0;
    numbrushsides = 0;
    numvertexes = 0;
    numleaffaces = 0;
    numleafbrushes = 0;
    numsurfedges = 0;

    // edge 0 is not used, because 0 can't be negated
    numedges = 1;

    // leave vertex 0 as an error
    numvertexes = 1;

    // leave leaf 0 as an error
    numleafs = 1;
    dleafs[0].contents = CONTENTS_SOLID;
    dleafsX[0].contents = CONTENTS_SOLID;
}


/*
============
EndBSPFile
============
*/
void EndBSPFile (void)
{
    char	path[1030];

    EmitBrushes ();
    EmitPlanes ();
    UnparseEntities ();

    // load the pop
#if 0
    sprintf (path, "%s/pics/pop.lmp", gamedir);
    len = LoadFile (path, &buf);
    memcpy (dpop, buf, sizeof(dpop));
    free (buf);
#endif

    // write the map
    sprintf (path, "%s.bsp", source);
    WriteBSPFile (path);
}


/*
==================
BeginModel
==================
*/
int32_t	firstmodleaf;
extern	int32_t firstmodeledge;
extern	int32_t	firstmodelface;
void BeginModel (void)
{
    dmodel_t	*mod;
    int32_t			start, end;
    mapbrush_t	*b;
    int32_t			j;
    entity_t	*e;
    vec3_t		mins, maxs;

    if (use_qbsp)
    {
        if (nummodels == WARN_MAP_MODELS_QBSP)
            printf ("WARNING: nummodels may exceed protocol limit (%i)\n", WARN_MAP_MODELS_QBSP);
        if (nummodels == MAX_MAP_MODELS_QBSP)
            Error ("nummodels exceeds MAX_MAP_MODELS_QBSP");
    }
    else if (nummodels == MAX_MAP_MODELS)
        Error ("nummodels exceeds MAX_MAP_MODELS");

    mod = &dmodels[nummodels];

    mod->firstface = numfaces;

    firstmodleaf = numleafs;
    firstmodeledge = numedges;
    firstmodelface = numfaces;

    //
    // bound the brushes
    //
    e = &entities[entity_num];

    start = e->firstbrush;
    end = start + e->numbrushes;
    ClearBounds (mins, maxs);

    for (j=start ; j<end ; j++)
    {
        b = &mapbrushes[j];
        if (!b->numsides)
            continue;	// not a real brush (origin brush)
        AddPointToBounds (b->mins, mins, maxs);
        AddPointToBounds (b->maxs, mins, maxs);
    }

    VectorCopy (mins, mod->mins);
    VectorCopy (maxs, mod->maxs);
}


/*
==================
EndModel
==================
*/
void EndModel (void)
{
    dmodel_t	*mod;

    mod = &dmodels[nummodels];

    mod->numfaces = numfaces - mod->firstface;

    nummodels++;
}

