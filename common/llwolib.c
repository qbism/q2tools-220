//
// llwolib.c: library for loading triangles from a Lightwave triangle file
//

#include <stdio.h>
#include "cmdlib.h"
#include "mathlib.h"
#include "trilib.h"
#include "llwolib.h"
#include "flipper.h"

#define MAXPOINTS 2000
#define MAXPOLYS 2000

struct {float p[3];} pnt[MAXPOINTS];
struct {short pl[3];} ply[MAXPOLYS];
char trashcan[10000];
char buffer[1000];
long counter;
union {char c[4];float f;long l;} buffer4;
union {char c[2];short i;} buffer2;
FILE *lwo;
short numpoints, numpolys;

void skipchunk()
{
	long chunksize;

	fread(buffer4.c,4,1,lwo);
    counter-=8;
	chunksize=lflip(buffer4.l);
	fread(trashcan,chunksize,1,lwo);
	counter-=chunksize;
};

void getpoints()
{
	long chunksize;
	short i,j;

	fread(buffer4.c,4,1,lwo);
    counter-=8;
	chunksize=lflip(buffer4.l);
	counter-=chunksize;
	numpoints=chunksize/12;
    if (numpoints>MAXPOINTS){fprintf(stderr,"reader: Too many points!!!");exit(0);}
	for (i=0;i<numpoints;i++)
	{
		for (j=0;j<3;j++)
		{
			fread(buffer4.c,4,1,lwo);
			pnt[i].p[j]=fflip(buffer4.f);
		}
	}
}

void getpolys(){
	short temp,i,j;
	short polypoints;
	long chunksize;

	fread(buffer4.c,4,1,lwo);
    counter-=8;
	chunksize=lflip(buffer4.l);
	counter-=chunksize;
	numpolys=0;
	for (i=0;i<chunksize;)
	{
		fread(buffer2.c,2,1,lwo);
		polypoints = sflip(buffer2.i);
		if (polypoints != 3){fprintf(stderr,"reader: Not a triangle!!!");exit(0);}
		i+=10;
		for (j=0;j<3;j++)
		{
			fread(buffer2.c,2,1,lwo);
			temp=sflip(buffer2.i);
			ply[numpolys].pl[j]=temp;
		}
		fread(buffer2.c,2,1,lwo);
		numpolys++;
		if (numpolys>MAXPOLYS){fprintf(stderr,"reader: Too many polygons!!!\n");exit(0);}
	}
}

void LoadLWOTriangleList (char *filename, triangle_t **pptri, int *numtriangles)
{
	int		i, j;
	triangle_t	*ptri;

	if ((lwo = fopen(filename, "rb")) == 0) {
		fprintf(stderr,"reader: could not open file '%s'\n", filename);
		exit(0);
	}

	fread(buffer4.c,4,1,lwo);
	fread(buffer4.c,4,1,lwo);
	counter=lflip(buffer4.l)-4;
	fread(buffer4.c,4,1,lwo);
	while(counter>0)
    {
		fread(buffer4.c,4,1,lwo);
		if (!strncmp(buffer4.c,"PNTS",4))
		{
			getpoints();
		} else {
			if (!strncmp(buffer4.c,"POLS",4))
			{
				getpolys();
			} else {
				skipchunk();
			}
		}
    }
	fclose(lwo);

	ptri = malloc (MAXTRIANGLES * sizeof(triangle_t));
	*pptri = ptri;

	for (i=0; i<numpolys ; i++)
	{
		for (j=0 ; j<3 ; j++)
		{
			ptri[i].verts[j][0] = pnt[ply[i].pl[j]].p[0];
			ptri[i].verts[j][1] = pnt[ply[i].pl[j]].p[2];
			ptri[i].verts[j][2] = pnt[ply[i].pl[j]].p[1];
		}
	}

	*numtriangles = numpolys;
}

