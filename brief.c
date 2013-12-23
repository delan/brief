#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define BF_END_ERROR   'e'
#define BF_END_IGNORE  'i'
#define BF_END_WRAP    'w'
#define BF_OP_VINC     '+'
#define BF_OP_VDEC     '-'
#define BF_OP_PINC     '>'
#define BF_OP_PDEC     '<'
#define BF_OP_LSTART   '['
#define BF_OP_LEND     ']'
#define BF_OP_IN       ','
#define BF_OP_OUT      '.'
#define BF_EOF_ZERO    '0'
#define BF_EOF_MIN     'a'
#define BF_EOF_MAX     'b'
#define BF_EOF_NEGONE  'n'
#define BF_EOF_NOCHG   'x'
#define BF_MODE_DUMP   'd'
#define BF_MODE_RUN    'r'

typedef struct {
	long instruction; /* instruction type */
	long quantity;    /* number of times to run the instruction */
	long loop;        /* index of the loop's matching 'other' instruction */
} instruction;

void die(const char *s, ...) {
	va_list a;
	va_start(a, s);
	fprintf(stderr, "brief: error: ");
	vfprintf(stderr, s, a);
	putchar(10);
	va_end(a);
	exit(1);
}

void help() {
	fputs(
"brief: a flexible brainfuck interpreter\n"
"Usage: brief [options]\n\n"
"Options:\n"
"	-a	minimum cell value (default: 0)\n"
"	-b	maximum cell value (default: 255)\n"
"	-c	number of cells to allocate (default: 30000)\n"
"	-e	value to store upon EOF, which can be one of:\n"
"		0	store a zero in the cell (default)\n"
"		a	store the minimum cell value in the cell\n"
"		b	store the maximum cell value in the cell\n"
"		n	store a negative one in the cell\n"
"		x	do not change the cell's contents\n"
"	-f	source file name (required)\n"
	, stderr);
	fputs(
"	-h	this help output\n"
"	-m	runtime mode, which can be one of:\n"
"		d	dump parsed code\n"
"		r	run normally (default)\n"
"	-v	value overflow/underflow behaviour\n"
"	-w	cell pointer overflow/underflow behaviour\n\n"
"Overflow/underflow behaviours can be one of:\n"
"	e	throw an error and quit upon over/underflow (pointer default)\n"
"	i	do nothing when attempting to over/underflow\n"
"	w	wrap-around to other end upon over/underflow (value default)\n"
	"\n", stderr);
	fputs(
"Cells are 'long int' values, so do not use -a with a value lower than your\n"
"platform's lowest acceptable value for 'long int', and likewise, do not use\n"
"-b with a value higher than LONG_MAX.\n"
	, stderr);
	exit(1);
}

long wrap(long value, long low, long high) {
	return (value - low) % (1 + high - low) + low;
}

int main(int argc, char **argv) {
	long
		ce = BF_EOF_ZERO,   /* EOF behaviour */
		ci = 0,             /* current cell index */
		cn = 30000,         /* number of cells to allocate */
		cw = BF_END_ERROR,  /* cell wrap behaviour */
		ia = 4096,          /* number of allocated instructions */
		ii = 0,             /* current instruction index */
		in = 0,             /* number of used instructions */
		la = 4096,          /* loop stack allocation */
		ln = 0,             /* loop stack used */
		mode = BF_MODE_RUN, /* runtime mode */
		va = 0,	            /* minimum value */
		vb = 255,           /* maximum value */
		vw = BF_END_WRAP,   /* value wrap behaviour */
		*cm = NULL,         /* cell memory */
		i, j = 0, k;        /* counters */
	instruction *im = malloc(sizeof(instruction) * ia); /* inst. memory */
	long *ls = malloc(sizeof(long) * la);               /* loop stack */
	FILE *fp = NULL;
	while ((i = getopt(argc, argv, "a:b:c:e:f:hm:v:w:")) != -1) {
		j++;
		switch (i) {
		case 'a': va = atol(optarg); break;
		case 'b': vb = atol(optarg); break;
		case 'c': cn = atol(optarg); break;
		case 'd': mode = BF_MODE_DUMP; break;
		case 'e': ce = optarg[0]; break;
		case 'f':
			fp = fopen(optarg, "r");
			if (!fp)
				die("%s: %s", optarg, strerror(errno));
			break;
		case 'h': help(); break;
		case 'm': mode = optarg[0]; break;
		case 'v': vw = optarg[0]; break;
		case 'w': cw = optarg[0]; break;
		default: break;
		}
	}
	if (!j)
		help();
	if (!fp)
		die("no source file specified; use -f");
	for (ii = 0; (i = getc(fp)) != EOF; ) {
		switch (i) {
		case BF_OP_VINC:
		case BF_OP_VDEC:
		case BF_OP_PINC:
		case BF_OP_PDEC:
		case BF_OP_IN:
		case BF_OP_OUT:
			if (in && i == im[in - 1].instruction) {
				++im[in - 1].quantity;
			} else {
				if (++in > ia)
					im = realloc(im, sizeof(instruction) *
						(ia *= 2));
				im[in - 1].instruction = i;
				im[in - 1].quantity = 1;
				++ii;
			}
			break;
		case BF_OP_LSTART:
			if (++in > ia)
				im = realloc(im, sizeof(instruction) *
					(ia *= 2));
			im[in - 1].instruction = i;
			if (++ln > la)
				ls = realloc(ls, sizeof(long) * (la *= 2));
			ls[ln - 1] = ii;
			++ii;
			break;
		case BF_OP_LEND:
			if (++in > ia)
				im = realloc(im, sizeof(instruction) *
					(ia *= 2));
			im[in - 1].instruction = i;
			im[in - 1].loop = ls[ln - 1];
			im[ls[ln - 1]].loop = ii;
			--ln;
			++ii;
			break;
		default: break;
		}
	}
	free(ls);
	fclose(fp);
	switch (mode) {
	case BF_MODE_DUMP:
		for (ii = 0, j = 0; ii < in; ii++) {
			printf("%c %ld", (int) im[ii].instruction,
				im[ii].quantity);
			if (++j % 8)	putchar(9);
			else			putchar(10);
		}
		putchar(10);
		break;
	case BF_MODE_RUN:
		cm = memset(malloc(cn * sizeof(long)), 0, cn * sizeof(long));
		for (ii = 0; ii < in; ii++)
			switch (im[ii].instruction) {
			case BF_OP_VINC:
				if (cm[ci] + im[ii].quantity > vb)
					switch (vw) {
					case BF_END_ERROR:
						die("value overflow");
						break;
					case BF_END_IGNORE:
						cm[ci] = vb;
						break;
					case BF_END_WRAP:
						cm[ci] += im[ii].quantity;
						cm[ci] = wrap(cm[ci], va, vb);
						break;
					default:
					die("invalid value-end behaviour");
					}
				else cm[ci] += im[ii].quantity;
				break;
			case BF_OP_VDEC:
				if (cm[ci] - im[ii].quantity < va)
					switch (vw) {
					case BF_END_ERROR:
						die("value underflow");
						break;
					case BF_END_IGNORE:
						cm[ci] = va;
						break;
					case BF_END_WRAP:
						cm[ci] -= im[ii].quantity;
						cm[ci] = wrap(cm[ci], va, vb);
						break;
					default:
					die("invalid value-end behaviour");
					}
				else cm[ci] -= im[ii].quantity;
				break;
			case BF_OP_PINC:
				if (ci + im[ii].quantity > cn - 1)
					switch (cw) {
					case BF_END_ERROR:
						die("cell index overflow");
						break;
					case BF_END_IGNORE:
						ci = cn - 1;
						break;
					case BF_END_WRAP:
						ci += im[ii].quantity;
						ci = wrap(ci, 0, cn);
						break;
					default:
					die("invalid cell-end behaviour");
					}
				else ci += im[ii].quantity;
				break;
			case BF_OP_PDEC:
				if (ci - im[ii].quantity < 0)
					switch (cw) {
					case BF_END_ERROR:
						die("cell index underflow");
						break;
					case BF_END_IGNORE:
						ci = 0;
						break;
					case BF_END_WRAP:
						ci -= im[ii].quantity;
						ci = wrap(ci, 0, cn);
						break;
					default:
					die("invalid cell-end behaviour");
					}
				else ci -= im[ii].quantity;
				break;
			case BF_OP_IN:
				for (j = im[ii].quantity; j; j--) {
					k = getchar();
					if (k == EOF)
						switch (ce) {
						case BF_EOF_ZERO: k = 0; break;
						case BF_EOF_NEGONE:
							k = -1;
							break;
						case BF_EOF_MIN: k = va; break;
						case BF_EOF_MAX: k = vb; break;
						case BF_EOF_NOCHG: break;
						default:
						die("invalid EOF behaviour");
						}
					cm[ci] = k;
				}
				break;
			case BF_OP_OUT:
				for (j = im[ii].quantity; j; j--)
					putchar(cm[ci]);
				break;
			case BF_OP_LSTART:
				if (!cm[ci])
					ii = im[ii].loop;
				break;
			case BF_OP_LEND:
				if (cm[ci])
					ii = im[ii].loop;
				break;
			default: break;
			}
		free(cm);
		break;
	default: die("invalid mode"); break;
	}
	free(im);
	return 0;
}
