#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>
#include "fns.h"

static Memimage *
genmask(Memimage *s, ulong clr)
{
	Memimage *m;
	Point p;
	int i;
	uchar b, sh;

	m = eallocmemimage(s->r, GREY1);

	for(p.y = s->r.min.y; p.y < s->r.max.y; p.y++)
	for(p.x = s->r.min.x; p.x < s->r.max.x; p.x++){
		b = 0;
		for(i = 0; i < s->nchan; i++)
			if(*(byteaddr(s, p) + i) != (clr>>i*8 & 0xFF)){
				b = 1;
				break;
			}

		sh = (p.y*Dx(m->r) + p.x) & 7;
		*byteaddr(m, p) |= b << (7-sh);
	}

	return m;
}

static void
usage(void)
{
	fprint(2, "usage: %s clrcol\n", argv0);
	exits(nil);
}

void
main(int argc, char *argv[])
{
	Memimage *img, *mask;
	ulong clrcol;

	ARGBEGIN{
	default: usage();
	}ARGEND;
	if(argc != 1)
		usage();

	clrcol = strtoul(argv[0], nil, 0);
	img = ereadmemimage(0);
	mask = genmask(img, clrcol);
	freememimage(img);

	ewritememimage(1, mask);
	freememimage(mask);

	exits(nil);
}
