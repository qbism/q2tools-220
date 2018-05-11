long lflip(long x)
{
	union {char b[4]; long l;} in, out;
	
	in.l = x;
	out.b[0] = in.b[3];
	out.b[1] = in.b[2];
	out.b[2] = in.b[1];
	out.b[3] = in.b[0];
	
	return out.l;
}

float fflip(float x)
{
	union {char b[4]; float l;} in, out;
	
	in.l = x;
	out.b[0] = in.b[3];
	out.b[1] = in.b[2];
	out.b[2] = in.b[1];
	out.b[3] = in.b[0];
	
	return out.l;
}

int sflip(short x)
{
	union {char b[2]; short i;} in, out;
	
	in.i = x;
	out.b[0] = in.b[1];
	out.b[1] = in.b[0];

	return out.i;
}
