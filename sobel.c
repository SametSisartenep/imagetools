#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>
#include "fns.h"

static int saturate;
static int byangle;

static int
opsobelΔ(uchar b1, uchar b2)
{
	return sqrt(b1*b1 + b2*b2);
}

static int
opsobelΘ(uchar b1, uchar b2)
{
	return atan2(b2, b1);
}

static void
usage(void)
{
	fprint(2, "usage: %s [-sa] Gx Gy\n", argv0);
	exits(nil);
}

void
main(int argc, char *argv[])
{
	Memimage *imgs[2];
	int i, fd;

	ARGBEGIN{
	case 's': saturate++; break;
	case 'a': byangle++; break;
	default: usage();
	}ARGEND;
	if(argc != 2)
		usage();

	for(i = 0; i < argc; i++){
		fd = eopen(argv[i], OREAD);
		imgs[i] = ereadmemimage(fd);
		close(fd);
	}

	imgbinop(imgs[0], imgs[1], byangle? opsobelΘ: opsobelΔ, saturate);
	ewritememimage(1, imgs[0]);

	freememimage(imgs[1]);
	freememimage(imgs[0]);
	exits(nil);
}
