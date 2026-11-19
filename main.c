#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <mpi.h>


typedef struct {
    int width, height, maxval;
    unsigned char *data;   // width * height * 3
} Image;

static void skip_comments(FILE *f) {
    int c;
    while ((c = fgetc(f)) != EOF) {
        if (c == '#') { while ((c = fgetc(f)) != '\n' && c != EOF); }
        else if (!isspace(c)) { ungetc(c, f); break; }
    }
}

Image *read_ppm(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;

    char magic[3];
    fscanf(f, "%2s", magic);
    skip_comments(f);
    Image *img = malloc(sizeof(Image));
    fscanf(f, "%d %d %d", &img->width, &img->height, &img->maxval);

    int n = img->width * img->height * 3;
    img->data = malloc(n);
    for (int i = 0; i < n; i++) {
        int v; fscanf(f, "%d", &v);
        img->data[i] = (unsigned char)v;
    }
    fclose(f);
    return img;
}

void write_ppm(const char *path, Image *img) {
    FILE *f = fopen(path, "w");
    fprintf(f, "P3\n%d %d\n%d\n", img->width, img->height, img->maxval);
    int n = img->width * img->height * 3;
    for (int i = 0; i < n; i++) {
        fprintf(f, "%d ", img->data[i]);
        if ((i + 1) % 15 == 0) fprintf(f, "\n");
    }
    fclose(f);
}


static unsigned char clamp(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (unsigned char)v;

}

void grayscale(Image *img) {
    int n = img->width * img->height * 3;
    unsigned char *d = img->data;
    for (int i = 0; i < n; i += 3) {
        unsigned char g = (unsigned char)(0.299 * d[i] + 0.587 * d[i+1] + 0.114 * d[i+2] );
        d[i] = d[i+1] = d[i+2] = g;
    }
}

void brightness(Image *img, int delta) {
    int n = img->width * img->height * 3;
    for (int i = 0; i < n; i++)
        img->data[i] = clamp(img->data[i] + delta);
}

void contrast(Image *img, double factor) {
    int n = img->width * img->height * 3;
    for (int i = 0; i < n; i++)
        img->data[i] = clamp((int)(factor * (img->data[i] - 128) + 128));
}

void blur(Image *img) {
    int w = img->width, h = img->height;
    unsigned char *in = img->data;
    unsigned char *out = malloc(w * h * 3 + 1);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            for (int c = 0; c < 3; c++) {
                int sum = 0, cnt = 0;

                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        int ny = y + dy, nx = x + dx;
                        if (ny >= 0 && ny < h && nx >= 0 && nx < w) {
                            sum += in[(ny * w + nx) * 3 + c];
                            cnt++;
                        }
                    }
                }
                out[(y * w + x) * 3 + c] = (unsigned char)(sum / cnt);
            }
        }
    }

    free(img->data);
    img->data = out;
}

void apply_filter(Image *img, const char *filter, int delta, double factor) {
    if (strcmp(filter, "grayscale") == 0)       grayscale(img);
    else if (strcmp(filter, "brightness") == 0) brightness(img, delta);
    else if (strcmp(filter, "contrast") == 0)   contrast(img, factor);
    else if (strcmp(filter, "blur") == 0)       blur(img);
}

//----------------------------------------------------------------

int main(int argc, char *argv[]) {
    MPI_Init(NULL,NULL);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    /* ---------- 1) الـ arguments (كل process بتفحصهم) ---------- */
    int ok = 0;
    int delta = 0;
    double factor = 1.0;

    if (argc >= 4) {
        const char *flt = argv[3];
        if ((strcmp(flt, "grayscale") == 0 || strcmp(flt, "blur") == 0) && argc == 4) {
            ok = 1;
        } else if (strcmp(flt, "brightness") == 0 && argc == 5) {
            delta = atoi(argv[4]);
            ok = 1;
        } else if (strcmp(flt, "contrast") == 0 && argc == 5) {
            factor = atof(argv[4]);
            ok = 1;
        }
    }
    if (!ok) {
        if (rank == 0) {
           printf("please enter the args correctly");
        }
        MPI_Finalize();
        return 1;
    }
    const char *filter = argv[3];
    int is_blur = (strcmp(filter, "blur") == 0);

    Image *img = NULL;
    int header[4] = {0, 0, 0, 0};        // ok, width, height, maxval

    if (rank == 0) {
        img = read_ppm(argv[1]);
        if (!img) {
            printf("Cannot open file\n");

        } else {
            header[0] = 1;
            header[1] = img->width;
            header[2] = img->height;
            header[3] = img->maxval;
        }
    }
    MPI_Bcast(header, 4, MPI_INT, 0, MPI_COMM_WORLD);
    if (!header[0]) { MPI_Finalize(); return 1; }

    int w = header[1], h = header[2], maxval = header[3];
    int rowbytes = w * 3;

    int base = h / size, rem = h % size;

    int *scounts = malloc(size * sizeof(int));   // عدد البايتس فى كل بروسيس
    int *sdispls = malloc(size * sizeof(int));   // ككل بروسيس هتبدا تاخد بايت منبن فى الاوريجنال
    int *rcounts = malloc(size * sizeof(int));   // ككل بروسيس هتبدا هتبعت كام بايت للروت
    int *rdispls = malloc(size * sizeof(int));   // هتبدا تكتب فبن فى الريسلت
    int *tops    = malloc(size * sizeof(int));   //  بنفع تاخد صف من فوق ولا لا فى البلور
    int *bots    = malloc(size * sizeof(int));



    for (int r = 0; r < size; r++) {
        int rows  = base + (r < rem ? 1 : 0);
        int start = r * base + (r < rem ? r : rem);

        int top = 0, bot = 0;
        if (is_blur && rows > 0) {
            if (start > 0)         top = 1;
            if (start + rows < h)  bot = 1;
        }
        tops[r] = top;
        bots[r] = bot;

        scounts[r] = (rows + top + bot) * rowbytes;
        sdispls[r] = (start - top) * rowbytes;
        rcounts[r] = rows * rowbytes;
        rdispls[r] = start * rowbytes;
    }

    int my_rows = rcounts[rank] / rowbytes;
    int my_top  = tops[rank];
    int local_rows = my_rows + my_top + bots[rank];

    Image local;
    local.width  = w;
    local.height = local_rows;
    local.maxval = maxval;
    local.data   = malloc(local_rows * rowbytes + 1);

    Image result;
    result.width = w; result.height = h; result.maxval = maxval;
    result.data = NULL;
    if (rank == 0) result.data = malloc(h * rowbytes);

    MPI_Barrier(MPI_COMM_WORLD);
    double t0 = MPI_Wtime();
    // توزيع الصفوف
    MPI_Scatterv(rank == 0 ? img->data : NULL, scounts, sdispls, MPI_UNSIGNED_CHAR,
                 local.data, scounts[rank], MPI_UNSIGNED_CHAR,
                 0, MPI_COMM_WORLD);

    // كل process تطبق الفلتر على جزئها
    apply_filter(&local, filter, delta, factor);

    MPI_Gatherv(local.data + my_top * rowbytes, rcounts[rank], MPI_UNSIGNED_CHAR,
                result.data, rcounts, rdispls, MPI_UNSIGNED_CHAR,
                0, MPI_COMM_WORLD);

    MPI_Barrier(MPI_COMM_WORLD);
    double t_par = MPI_Wtime() - t0;

    // if (rank == 0) {
        write_ppm(argv[2], &result);

        Image seq;
        seq.width = w; seq.height = h; seq.maxval = maxval;
        seq.data = malloc(h * rowbytes);
        memcpy(seq.data, img->data, h * rowbytes);

        double s0 = MPI_Wtime();
        apply_filter(&seq, filter, delta, factor);
        double t_seq = MPI_Wtime() - s0;

        int same = (memcmp(seq.data, result.data, h * rowbytes) == 0);

        printf("Image         : %d x %d\n", w, h);
        printf("Processes     : %d\n", size);
        printf("Sequential    : %.6f s\n", t_seq);
        printf("Parallel      : %.6f s\n", t_par);
        printf("Speedup       : %.2f\n", t_par > 0 ? t_seq / t_par : 0.0);
        printf("Results match : %s\n", same ? "yes" : "NO (bug!)");

        free(seq.data);
        free(result.data);
        free(img->data);
        free(img);


    free(local.data);
    free(scounts); free(sdispls);
    free(rcounts); free(rdispls);
    free(tops);    free(bots);

    MPI_Finalize();
    return 0;
}