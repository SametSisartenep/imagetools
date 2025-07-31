#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>
#include <geometry.h>
#include "fns.h"

typedef struct Mstk Mstk;
struct Mstk
{
	Matrix *items;
	ulong size;
};

static void
pushmat(Mstk *stk, Matrix m)
{
	if(stk->size % 4 == 0)
		stk->items = erealloc(stk->items, (stk->size + 4)*sizeof(Matrix));
	memmove(stk->items[stk->size++], m, sizeof(Matrix));
}

static void
popmat(Mstk *stk, Matrix m)
{
	memmove(m, stk->items[--stk->size], sizeof(Matrix));
	if(stk->size == 0){
		free(stk->items);
		stk->items = nil;
	}
}

static void
mkrotation(Matrix m, double θ)
{
	double c, s;

	c = cos(θ);
	s = sin(θ);
	Matrix R = {
		c, -s, 0,
		s,  c, 0,
		0,  0, 1,
	};
	memmove(m, R, sizeof(Matrix));
}

static void
mkscale(Matrix m, double sx, double sy)
{
	Matrix S = {
		sx, 0, 0,
		0, sy, 0,
		0, 0, 1,
	};
	memmove(m, S, sizeof(Matrix));
}

static void
mktranslation(Matrix m, double tx, double ty)
{
	Matrix T = {
		1, 0, tx,
		0, 1, ty,
		0, 0, 1,
	};
	memmove(m, T, sizeof(Matrix));
}

static void
mkshear(Matrix m, double shx, double shy)
{
	Matrix Sxy = {
		1, shx, 0,
		shy, 1, 0,
		0,   0, 1,
	};
	memmove(m, Sxy, sizeof(Matrix));
}

static void
mkxform(Matrix m, Mstk *stk)
{
	Matrix t;

	identity(m);
	while(stk->size > 0){
		popmat(stk, t);
		mulm(m, t);
	}
}

static uchar
edgehandler(Rectangle *r, Point *p)
{
	USED(r, p);
	return 0;	/* constant */
}

static uchar
sample(Memimage *i, Point p, int off)
{
	if(!ptinrect(p, i->r))
		return edgehandler(&i->r, &p);
	return *(byteaddr(i, p) + off);
}

static Memimage *
imgxform(Memimage *s, Mstk *stk)
{
	Memimage *d;
	Matrix m;
	Point sp, dp;
	Point2 p2;
	double Δx, Δy, c[2][2];
	int i;

	d = eallocmemimage(s->r, s->chan);
	mkxform(m, stk);
	invm(m);

	for(dp.y = d->r.min.y; dp.y < d->r.max.y; dp.y++)
	for(dp.x = d->r.min.x; dp.x < d->r.max.x; dp.x++){
		p2 = xform((Point2){dp.x, dp.y, 1}, m);
		p2 = mulpt2(p2, p2.w == 0? 1: 1/p2.w);

		sp.x = p2.x;
		sp.y = p2.y;
		Δx = p2.x - sp.x;
		Δy = p2.y - sp.y;

		for(i = 0; i < s->nchan; i++){
			c[0][0] = sample(s, sp, i);
			c[0][1] = sample(s, addpt(sp, (Point){1,0}), i);
			c[1][0] = sample(s, addpt(sp, (Point){0,1}), i);
			c[1][1] = sample(s, addpt(sp, (Point){1,1}), i);

			c[0][0] = flerp(c[0][0], c[0][1], Δx);
			c[0][1] = flerp(c[1][0], c[1][1], Δx);
			c[0][0] = flerp(c[0][0], c[0][1], Δy);

			*(byteaddr(d, dp) + i) = clamp(c[0][0], 0, 0xFF);
		}
	}
	return d;
}

static void
usage(void)
{
	fprint(2, "usage: %s [[-s x y] [-r θ] [-t x y] [-S x y]]...\n", argv0);
	exits(nil);
}

void
main(int argc, char *argv[])
{
	Memimage *img, *warp;
	Mstk stk;
	Matrix m;
	double x, y, θ;

	memset(&stk, 0, sizeof stk);
	identity(m);
	ARGBEGIN{
	case 's':
		x = strtod(EARGF(usage()), nil);
		y = strtod(EARGF(usage()), nil);
		mkscale(m, x, y);
		pushmat(&stk, m);
		break;
	case 'r':
		θ = strtod(EARGF(usage()), nil)*DEG;
		mkrotation(m, θ);
		pushmat(&stk, m);
		break;
	case 't':
		x = strtod(EARGF(usage()), nil);
		y = strtod(EARGF(usage()), nil);
		mktranslation(m, x, y);
		pushmat(&stk, m);
		break;
	case 'S':
		x = strtod(EARGF(usage()), nil);
		y = strtod(EARGF(usage()), nil);
		mkshear(m, x, y);
		pushmat(&stk, m);
		break;
	default: usage();
	}ARGEND;
	if(argc != 0)
		usage();

	img = ereadmemimage(0);
	warp = imgxform(img, &stk);
	freememimage(img);

	ewritememimage(1, warp);
	freememimage(warp);

	exits(nil);
}
