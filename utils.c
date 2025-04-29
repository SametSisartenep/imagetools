#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>
#include "fns.h"

void *
emalloc(ulong n)
{
	void *p;

	p = malloc(n);
	if(p == nil)
		sysfatal("malloc: %r");
	setmalloctag(p, getcallerpc(&n));
	return p;
}

void *
erealloc(void *p, ulong n)
{
	void *np;

	np = realloc(p, n);
	if(np == nil){
		if(n == 0)
			return nil;
		sysfatal("realloc: %r");
	}
	if(p == nil)
		setmalloctag(np, getcallerpc(&p));
	else
		setrealloctag(np, getcallerpc(&p));
	return np;
}

int
eopen(char *file, int omode)
{
	int fd;

	fd = open(file, omode);
	if(fd < 0)
		sysfatal("open: %r");
	return fd;
}

Memimage *
eallocmemimage(Rectangle r, ulong chan)
{
	Memimage *i;

	i = allocmemimage(r, chan);
	if(i == nil)
		sysfatal("allocmemimage: %r");
	memfillcolor(i, DTransparent);
	return i;
}

Memimage *
ereadmemimage(int fd)
{
	Memimage *i;

	i = readmemimage(fd);
	if(i == nil)
		sysfatal("readmemimage: %r");
	return i;
}

int
ewritememimage(int fd, Memimage *i)
{
	int rc;

	rc = writememimage(fd, i);
	if(rc < 0)
		sysfatal("writememimage: %r");
	return rc;
}

void
imgbinop(Memimage *i1, Memimage *i2, int(*op)(uchar, uchar), int saturate)
{
	uchar *p1, *p1e, *p2;
	int c;

	if(i1->chan != i2->chan || !eqrect(i1->r, i2->r))
		sysfatal("images shapes differ");

	p1 = i1->data->bdata;
	p2 = i2->data->bdata;
	p1e = p1 + Dx(i1->r)*Dy(i1->r)*i1->nchan;
	while(p1 < p1e){
		c = op(*p1, *p2++);
		if(saturate)
			c = clamp(c, 0, 0xFF);
		*p1++ = c;
	}
}

void
imgunaop(Memimage *i1, int(*op)(uchar), int saturate)
{
	uchar *p1, *p1e;
	int c;

	p1 = i1->data->bdata;
	p1e = p1 + Dx(i1->r)*Dy(i1->r)*i1->nchan;
	while(p1 < p1e){
		c = op(*p1);
		if(saturate)
			c = clamp(c, 0, 0xFF);
		*p1++ = c;
	}
}
