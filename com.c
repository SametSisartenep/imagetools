#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>
#include "fns.h"

static int
opcom(uchar b)
{
	return ~b;
}

static void
usage(void)
{
	fprint(2, "usage: %s\n", argv0);
	exits(nil);
}

void
main(int argc, char *argv[])
{
	Memimage *img;

	ARGBEGIN{
	default: usage();
	}ARGEND;
	if(argc != 0)
		usage();

	img = ereadmemimage(0);
	imgunaop(img, opcom, 0);

	ewritememimage(1, img);
	freememimage(img);

	exits(nil);
}
