#include <u.h>
#include <libc.h>
#include <bio.h>
#include <draw.h>
#include <memdraw.h>
#include "fns.h"

static int saturate = 1;

static double
lanczos(double x, double a)
{
	if(x == 0) return 1;
	if(x < -a || x >= a) return 0;
	return a * sin(PI*x) * sin(PI*x/a)/(PI*PI * x*x);
}

static double
lanczos2(double x, double y, double a)
{
	return lanczos(x, a)*lanczos(y, a);
}

static double
coeffsum(double *k, int dim)
{
	double s;
	int i, j;

	s = 0;
	for(j = 0; j < dim; j++)
	for(i = 0; i < dim; i++)
		s += k[j*dim+i];
	return s;
}

static void
modulate(double *d, double *s, int dim)
{
	int i, j;

	for(j = 0; j < dim; j++)
	for(i = 0; i < dim; i++)
		d[j*dim+i] *= s[j*dim+i];
}

static double
convolve(double *d, double *s, int dim)
{
	modulate(d, s, dim);
	return coeffsum(d, dim);
}

static uchar
sample(Memimage *i, Point p, int off)
{
	if(!ptinrect(p, i->r))
		return 0;	/* edge handler: constant */
	return *(byteaddr(i, p) + off);
}

static Memimage *
resample(Memimage *s, int dx, int dy, int a)
{
	Memimage *d;
	double *k, **im, c, denom;
	Point imc, sp, dp, cp, p;
	struct {
		double x, y;
	} dp2;
	int i;

	d = eallocmemimage(Rect(0, 0, dx, dy), s->chan);
	k = emalloc(2*a*2*a*sizeof(double));
	im = emalloc(s->nchan*sizeof(double*));
	for(i = 0; i < s->nchan; i++)
		im[i] = emalloc(2*a*2*a*sizeof(double));
	imc = Pt(a, a);

	for(dp.y = d->r.min.y; dp.y < d->r.max.y; dp.y++){
		dp2.y = (double)dp.y*Dy(s->r)/dy;
	for(dp.x = d->r.min.x; dp.x < d->r.max.x; dp.x++){
		dp2.x = (double)dp.x*Dx(s->r)/dx;
		p = Pt(floor(dp2.x), floor(dp2.y));

		for(cp.y = 1; cp.y <= 2*a; cp.y++)
		for(cp.x = 1; cp.x <= 2*a; cp.x++){
			sp = addpt(p, subpt(cp, imc));
			for(i = 0; i < d->nchan; i++)
				im[i][(cp.y-1)*2*a + cp.x - 1] = sample(s, sp, i);
			/* TODO precompute this kernel */
			k[(cp.y-1)*2*a + cp.x - 1] = lanczos2(dp2.x - sp.x, dp2.y - sp.y, a);
		}

//static int g;
//if(g++ == dx*dy/2)
//fprintm(2, k, 2*a);

		denom = coeffsum(k, 2*a);
		denom = denom == 0? 1: 1/denom;

		for(i = 0; i < d->nchan; i++){
			c = fabs(convolve(im[i], k, 2*a) * denom);
			if(saturate)
				c = clamp(c, 0, 0xFF);
			*(byteaddr(d, dp) + i) = c;
		}
	}
	}

	for(i = 0; i < s->nchan; i++)
		free(im[i]);
	free(im);
	free(k);

	return d;
}

_Noreturn static void
usage(void)
{
	fprint(2, "usage: %s [-s] [-x size] [-y size] [-w window] [file]\n", argv0);
	exits("usage");
}

void
main(int argc, char *argv[])
{
	Memimage *in, *out;
	char *infile, *s;
	int fd, dx, dy, percx, percy, w;

	infile = "/fd/0";
	dx = dy = 0;
	percx = percy = 0;
	w = 3;
	ARGBEGIN{
	case 's':
		saturate--;
		break;
	case 'x':
		dx = strtoul(EARGF(usage()), &s, 10);
		if(*s == '%')
			percx++;
		break;
	case 'y':
		dy = strtoul(EARGF(usage()), &s, 10);
		if(*s == '%')
			percy++;
		break;
	case 'w':
		w = strtoul(EARGF(usage()), nil, 10);
		break;
	default: usage();
	}ARGEND;
	if(argc == 1)
		infile = argv[0];
	else if(argc > 0)
		usage();

	fd = eopen(infile, OREAD);
	in = ereadmemimage(fd);
	close(fd);

	if(percx)
		dx = Dx(in->r)*dx/100;
	if(percy)
		dy = Dy(in->r)*dy/100;

	if(dx == 0 || dy == 0)
		usage();
	else if(dx == Dx(in->r) && dy == Dy(in->r))
		out = in;
	else
		out = resample(in, dx, dy, w);

	ewritememimage(1, out);
	exits(nil);
}
