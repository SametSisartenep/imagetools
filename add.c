#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>
#include "fns.h"

static int saturate;

static int
opadd(uchar b1, uchar b2)
{
	return b1 + b2;
}

static void
usage(void)
{
	fprint(2, "usage: %s [-s] img1 img2 [imgs...]\n", argv0);
	exits(nil);
}

void
main(int argc, char *argv[])
{
	Memimage *imgs[2];
	int i, j, fd;

	ARGBEGIN{
	case 's': saturate++; break;
	default: usage();
	}ARGEND;
	if(argc < 2)
		usage();

	for(i = 0; i < argc; i++){
		j = i != 0;
		fd = eopen(argv[i], OREAD);
		imgs[j] = ereadmemimage(fd);
		close(fd);

		if(j){
			imgbinop(imgs[0], imgs[1], opadd, saturate);
			freememimage(imgs[1]);
		}
	}

	ewritememimage(1, imgs[0]);
	freememimage(imgs[0]);

	exits(nil);
}
