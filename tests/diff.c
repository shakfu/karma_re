// Sample-exact comparator for two scenario capture files.
//   usage: diff <a.bin> <b.bin>
// Format per file: [int64 n_out][double out[n_out]][int64 n_buf][float buf[n_buf]]
// Exit 0 if identical (within --tol), 1 if they differ, 2 on read error.

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <string.h>

typedef struct { int64_t n_out; double *out; int64_t n_buf; float *buf; int64_t n_rep; double *rep; } cap;

static int load(const char *path, cap *c)
{
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); return 0; }
    if (fread(&c->n_out, sizeof(int64_t), 1, f) != 1) goto bad;
    c->out = malloc((size_t)c->n_out * sizeof(double));
    if (fread(c->out, sizeof(double), (size_t)c->n_out, f) != (size_t)c->n_out) goto bad;
    if (fread(&c->n_buf, sizeof(int64_t), 1, f) != 1) goto bad;
    c->buf = malloc((size_t)c->n_buf * sizeof(float));
    if (fread(c->buf, sizeof(float), (size_t)c->n_buf, f) != (size_t)c->n_buf) goto bad;
    c->n_rep = 0; c->rep = NULL;                     // optional report section
    if (fread(&c->n_rep, sizeof(int64_t), 1, f) == 1 && c->n_rep > 0) {
        c->rep = malloc((size_t)c->n_rep * sizeof(double));
        if (fread(c->rep, sizeof(double), (size_t)c->n_rep, f) != (size_t)c->n_rep) goto bad;
    }
    fclose(f); return 1;
bad:
    fprintf(stderr, "short read on %s\n", path); fclose(f); return 0;
}

int main(int argc, char **argv)
{
    double tol = 0.0;
    const char *pa = NULL, *pb = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--tol") && i + 1 < argc) tol = atof(argv[++i]);
        else if (!pa) pa = argv[i];
        else pb = argv[i];
    }
    if (!pa || !pb) { fprintf(stderr, "usage: diff [--tol T] a.bin b.bin\n"); return 2; }

    cap a = {0}, b = {0};
    if (!load(pa, &a) || !load(pb, &b)) return 2;

    int differ = 0;

    // --- output signal ---
    if (a.n_out != b.n_out) { printf("  out length differs: %lld vs %lld\n", (long long)a.n_out, (long long)b.n_out); differ = 1; }
    else {
        int64_t first = -1; double maxd = 0; int64_t ndiff = 0;
        for (int64_t i = 0; i < a.n_out; i++) {
            double d = fabs(a.out[i] - b.out[i]);
            if (d > tol) { if (first < 0) first = i; ndiff++; if (d > maxd) maxd = d; }
        }
        if (first >= 0) {
            differ = 1;
            printf("  out: %lld/%lld samples differ; first @ %lld (%.9g vs %.9g); max|d|=%.3g\n",
                   (long long)ndiff, (long long)a.n_out, (long long)first, a.out[first], b.out[first], maxd);
        } else printf("  out: identical (%lld samples)\n", (long long)a.n_out);
    }

    // --- final buffer ---
    if (a.n_buf != b.n_buf) { printf("  buf length differs: %lld vs %lld\n", (long long)a.n_buf, (long long)b.n_buf); differ = 1; }
    else {
        int64_t first = -1; double maxd = 0; int64_t ndiff = 0;
        for (int64_t i = 0; i < a.n_buf; i++) {
            double d = fabs((double)a.buf[i] - (double)b.buf[i]);
            if (d > tol) { if (first < 0) first = i; ndiff++; if (d > maxd) maxd = d; }
        }
        if (first >= 0) {
            differ = 1;
            printf("  buf: %lld/%lld frames differ; first @ %lld (%.9g vs %.9g); max|d|=%.3g\n",
                   (long long)ndiff, (long long)a.n_buf, (long long)first, a.buf[first], b.buf[first], maxd);
        } else printf("  buf: identical (%lld frames)\n", (long long)a.n_buf);
    }

    // --- data/report outlet (only when both files carry one) ---
    if (a.n_rep > 0 && b.n_rep > 0) {
        double rtol = (tol > 1e-4) ? tol : 1e-4;   // report values are float-rounded
        if (a.n_rep != b.n_rep) { printf("  report length differs: %lld vs %lld\n", (long long)a.n_rep, (long long)b.n_rep); differ = 1; }
        else {
            int64_t first = -1; double maxd = 0;
            for (int64_t i = 0; i < a.n_rep; i++) {
                double d = fabs(a.rep[i] - b.rep[i]);
                if (d > rtol) { if (first < 0) first = i; if (d > maxd) maxd = d; }
            }
            if (first >= 0) {
                differ = 1;
                printf("  report: element %lld differs (%.9g vs %.9g); max|d|=%.3g\n",
                       (long long)first, a.rep[first], b.rep[first], maxd);
            } else printf("  report: identical (%lld elements)\n", (long long)a.n_rep);
        }
    }

    return differ ? 1 : 0;
}
