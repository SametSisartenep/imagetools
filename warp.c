#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>
#include <geometry.h>
#include "fns.h"

typedef struct Mstk Mstk;
struct Mstk
{
	Matrix3 *items;
	ulong size;
};

static double γ;	/* roll */

static void
pushmat(Mstk *stk, Matrix3 m)
{
	if(stk->size % 4 == 0)
		stk->items = erealloc(stk->items, (stk->size + 4)*sizeof(Matrix3));
	memmove(stk->items[stk->size++], m, sizeof(Matrix3));
}

static void
popmat(Mstk *stk, Matrix3 m)
{
	memmove(m, stk->items[--stk->size], sizeof(Matrix3));
	if(stk->size == 0){
		free(stk->items);
		stk->items = nil;
	}
}

static void
mkrotation(Matrix3 m, double θ, double φ, double γ)
{
	Matrix3 Rx = {
		1, 0, 0, 0,
		0, cos(θ), -sin(θ), 0,
		0, sin(θ),  cos(θ), 0,
		0, 0, 0, 1,
	}, Ry = {
		 cos(φ), 0, sin(φ), 0,
		0, 1, 0, 0,
		-sin(φ), 0, cos(φ), 0,
		0, 0, 0, 1,
	}, Rz = {
		cos(γ), -sin(γ), 0, 0,
		sin(γ),  cos(γ), 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1,
	};
	mulm3(Ry, Rz);
	mulm3(Rx, Ry);
	memmove(m, Rx, sizeof(Matrix3));
}

static void
mkscale(Matrix3 m, double sx, double sy, double sz)
{
	sx = sx == 0? 1: 1/sx;
	sy = sy == 0? 1: 1/sy;
	sz = sz == 0? 1: 1/sz;
	Matrix3 S = {
		sx, 0, 0, 0,
		0, sy, 0, 0,
		0, 0, sz, 0,
		0, 0, 0, 1,
	};
	memmove(m, S, sizeof(Matrix3));
}

static void
mktranslation(Matrix3 m, double tx, double ty, double tz)
{
	Matrix3 T = {
		1, 0, 0, tx,
		0, 1, 0, ty,
		0, 0, 1, tz,
		0, 0, 0, 1,
	};
	memmove(m, T, sizeof(Matrix3));
}

static void
mkshear(Matrix3 m, double shx, double shy, double shz)
{
	Matrix3 Sxy = {
		1, 0, 0, shx,
		0, 1, 0, shy,
		0, 0, 1, 0,
		0, 0, 0, 1,
	}, Syz = {
		1,   0, 0, 0,
		shy, 1, 0, 0,
		shz, 0, 1, 0,
		0,   0, 0, 1,
	}, Sxz = {
		1, shx, 0, 0,
		0,   1, 0, 0,
		0, shz, 1, 0,
		0,   0, 0, 1,
	};
	mulm3(Syz, Sxy);
	mulm3(Sxz, Syz);
	memmove(m, Sxz, sizeof(Matrix3));
}

static void
mkxform(Matrix m, Mstk *stk, Memimage *s)
{
	double d, focal;
	int w, h, i, j;
	Matrix3 U, t;

	w = Dx(s->r);
	h = Dy(s->r);
	d = sqrt(w*w + h*h);
	focal = γ == 0? d: d / 2*sin(γ);

	identity3(U);
	while(stk->size > 0){
		popmat(stk, t);
		mulm3(U, t);
	}

	Matrix3 T = {
		1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, focal,
		0, 0, 0, 1,
	}, A1 = {
		1, 0, -w/2, 0,
		0, 1, -h/2, 0,
		0, 0, 1, 0,
		0, 0, 1, 0,
	}, A2 = {
		focal, 0, w/2, 0,
		0, focal, h/2, 0,
		0, 0, 1, 0,
		0, 0, 0, 0,
	};

	mulm3(U, A1);
	mulm3(T, U);
	mulm3(A2, T);

	for(j = 0; j < 3; j++)
	for(i = 0; i < 3; i++)
		m[j][i] = A2[j][i];
}

static uchar
sample(Memimage *i, Point p, int off)
{
	if(p.x < i->r.min.x || p.y < i->r.min.y
	|| p.x >= i->r.max.x || p.y >= i->r.max.y)
		return 0;	/* edge handler: constant */
	return *(byteaddr(i, p) + off);
}

static Memimage *
imgxform(Memimage *s, Mstk *stk)
{
	Memimage *d;
	Matrix m;
	Point p, p0;
	Point2 p2;
	double Δx, Δy, c[2][2];
	int i;

	d = eallocmemimage(s->r, s->chan);
	mkxform(m, stk, s);

	for(p.y = s->r.min.y; p.y < s->r.max.y; p.y++)
	for(p.x = s->r.min.x; p.x < s->r.max.x; p.x++){
		p2 = xform((Point2){p.x, p.y, 1}, m);
		p2 = mulpt2(p2, p2.w == 0? 1: 1/p2.w);

		p0.x = p2.x;
		p0.y = p2.y;
		Δx = p2.x - p0.x;
		Δy = p2.y - p0.y;

		for(i = 0; i < s->nchan; i++){
			c[0][0] = sample(s, p0, i);
			c[0][1] = sample(s, addpt(p0, (Point){1,0}), i);
			c[1][0] = sample(s, addpt(p0, (Point){0,1}), i);
			c[1][1] = sample(s, addpt(p0, (Point){1,1}), i);

			c[0][0] = flerp(c[0][0], c[0][1], Δx);
			c[0][1] = flerp(c[1][0], c[1][1], Δx);
			c[0][0] = flerp(c[0][0], c[0][1], Δy);

			*(byteaddr(d, p) + i) = clamp(c[0][0], 0, 0xFF);
		}
	}
	return d;
}

static void
usage(void)
{
	fprint(2, "usage: %s [[-s x y z] [-r x y z] [-t x y z] [-S x y z]]...\n", argv0);
	exits(nil);
}

void
main(int argc, char *argv[])
{
	Memimage *img, *warp;
	Mstk stk;
	Matrix3 m;
	double x, y, z;

	memset(&stk, 0, sizeof stk);
	identity3(m);
	ARGBEGIN{
	case 's':
		x = strtod(EARGF(usage()), nil);
		y = strtod(EARGF(usage()), nil);
		z = strtod(EARGF(usage()), nil);
		mkscale(m, x, y, z);
		pushmat(&stk, m);
		break;
	case 'r':
		x = strtod(EARGF(usage()), nil)*DEG;
		y = strtod(EARGF(usage()), nil)*DEG;
		z = strtod(EARGF(usage()), nil)*DEG;
		γ += z;
		mkrotation(m, x, y, z);
		pushmat(&stk, m);
		break;
	case 't':
		x = strtod(EARGF(usage()), nil);
		y = strtod(EARGF(usage()), nil);
		z = strtod(EARGF(usage()), nil);
		mktranslation(m, x, y, z);
		pushmat(&stk, m);
		break;
	case 'S':
		x = strtod(EARGF(usage()), nil);
		y = strtod(EARGF(usage()), nil);
		z = strtod(EARGF(usage()), nil);
		mkshear(m, x, y, z);
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
