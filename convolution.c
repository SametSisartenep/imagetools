#include <u.h>
#include <libc.h>
#include <bio.h>
#include <draw.h>
#include <memdraw.h>
#include "fns.h"

static int dim;
static int saturate;

static void
fprintm(int fd, double *m, int dim)
{
	int i, j;

	for(j = 0; j < dim; j++)
	for(i = 0; i < dim; i++)
		fprint(fd, "%g%c", m[j*dim+i], i == dim-1? '\n': '\t');
}

static char *
getline(Biobuf *b)
{
	char *line;

	if((line = Brdline(b, '\n')) != nil)
		line[Blinelen(b)-1] = 0;
	return line;
}

static double *
readkernel(int fd)
{
	Biobuf *bin;
	double *kern;
	char *line, *f[10];
	int nf, i, j;

	bin = Bfdopen(fd, OREAD);
	if(bin == nil)
		sysfatal("Bfdopen: %r");
	do{
		line = getline(bin);
		if(line == nil)
			sysfatal("Brdline: %r");
		dim = tokenize(line, f, nelem(f));
	}while(dim < 1);
	kern = emalloc(dim*dim*sizeof(double));
	for(j = i = 0; i < dim; i++)
		kern[j*dim+i] = strtod(f[i], nil);
	j++;

	while((line = getline(bin)) != nil){
		if((nf = tokenize(line, f, nelem(f))) < 1)
			continue;
		if(nf != dim)
			sysfatal("expected a %dx%d matrix", dim, dim);
		for(i = 0; i < dim; i++)
			kern[j*dim+i] = strtod(f[i], nil);
		j++;
	}
	Bterm(bin);

	if(j != dim)
		sysfatal("expected a %dx%d matrix", dim, dim);

	return kern;
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
normalize(double *k, int dim)
{
	double denom;
	int i, j;

	denom = coeffsum(k, dim);
	if(denom == 0)
		return;
	for(j = 0; j < dim; j++)
	for(i = 0; i < dim; i++)
		k[j*dim+i] /= denom;
}

static double *
reverse(double *k, int dim)
{
	double *ck;
	int i, j;

	ck = emalloc(dim*dim*sizeof(double));

	for(j = 0; j < dim; j++)
	for(i = 0; i < dim; i++)
		ck[j*dim+i] = k[(dim-j-1)*dim+(dim-i-1)];
	return ck;
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
	int i, j;
	double r;

	modulate(d, s, dim);
	r = 0;
	for(j = 0; j < dim; j++)
	for(i = 0; i < dim; i++)
		r += d[j*dim+i];
	return r;
}

static uchar
sample(Memimage *i, Point p, int off)
{
	if(p.x < i->r.min.x || p.y < i->r.min.y
	|| p.x >= i->r.max.x || p.y >= i->r.max.y)
		return 0;	/* edge handler: constant */
	return *(byteaddr(i, p) + off);
}

static void
mksubrects(Rectangle *sr, int nsr, Rectangle *r)
{
	int i, Δy;

	sr[0] = *r;
	Δy = Dy(sr[0])/nsr;
	sr[0].max.y = sr[0].min.y + Δy;
	for(i = 1; i < nsr; i++)
		sr[i] = rectaddpt(sr[i-1], Pt(0,Δy));
	if(sr[nsr-1].max.y < r->max.y)
		sr[nsr-1].max.y = r->max.y;
}

static void
subimgconvolution(Memimage *d, Memimage *s, Rectangle *r, double *k, int dim, double denom)
{
	double **im;
	Point imc, p, cp;
	double c;
	int i;

	im = emalloc(d->nchan*sizeof(double*));
	for(i = 0; i < d->nchan; i++)
		im[i] = emalloc(dim*dim*sizeof(double));

	imc = Pt(dim/2, dim/2);

	for(p.y = r->min.y; p.y < r->max.y; p.y++)
	for(p.x = r->min.x; p.x < r->max.x; p.x++){
		for(cp.y = 0; cp.y < dim; cp.y++)
		for(cp.x = 0; cp.x < dim; cp.x++){
			for(i = 0; i < d->nchan; i++)
				im[i][cp.y*dim + cp.x] = sample(s, addpt(p, subpt(cp, imc)), i);
		}
		for(i = 0; i < d->nchan; i++){
			c = fabs(convolve(im[i], k, dim) * denom);
			if(saturate)
				c = clamp(c, 0, 0xFF);
			*(byteaddr(d, p) + i) = c;
		}
	}

	for(i = 0; i < d->nchan; i++)
		free(im[i]);
	free(im);
}

static void
imgconvolution(Memimage *d, Memimage *s, double *k, int dim)
{
	double denom;
	Rectangle *subr;
	char *nprocs;
	int nproc, i;

	denom = coeffsum(k, dim);
	denom = denom == 0? 1: 1/denom;

	nprocs = getenv("NPROC");
	if(nprocs == nil || (nproc = strtoul(nprocs, nil, 10)) < 2)
		nproc = 1;
	free(nprocs);

	subr = emalloc(nproc*sizeof(*subr));
	mksubrects(subr, nproc, &s->r);

	for(i = 0; i < nproc; i++){
		switch(rfork(RFPROC|RFMEM)){
		case -1:
			sysfatal("rfork: %r");
		case 0:
			subimgconvolution(d, s, &subr[i], k, dim, denom);
			exits(nil);
		}
	}
	while(waitpid() != -1)
		;

	free(subr);
}


static void
usage(void)
{
	fprint(2, "usage: %s [-s] kernfile\n", argv0);
	exits("usage");
}

void
main(int argc, char *argv[])
{
	Memimage *in, *out;
	double *kern, *ckern;
	int fd;

	ARGBEGIN{
	case 's': saturate++; break;
	default: usage();
	}ARGEND;
	if(argc != 1)
		usage();

	fd = eopen(argv[0], OREAD);
	kern = readkernel(fd);
	close(fd);
	ckern = reverse(kern, dim);
	free(kern);
	kern = ckern;

	in = ereadmemimage(0);
	out = eallocmemimage(in->r, in->chan);

	imgconvolution(out, in, kern, dim);
	ewritememimage(1, out);

	freememimage(out);
	freememimage(in);
	free(kern);
	exits(nil);
}
