#define min(a,b)	((a)<(b)?(a):(b))
#define max(a,b)	((a)>(b)?(a):(b))
#define clamp(a,b,c)	min(max(a,b),c)

void *emalloc(ulong);
void *erealloc(void*, ulong);
int eopen(char*, int);
Memimage *eallocmemimage(Rectangle, ulong);
Memimage *ereadmemimage(int);
int ewritememimage(int, Memimage*);
void imgbinop(Memimage*, Memimage*, int(*)(uchar, uchar), int);
void imgunaop(Memimage*, int(*)(uchar), int);
