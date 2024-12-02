#define TB_LIB_OPTS 1
#define TB_IMPL
#include <termbox2.h>

#include <getopt.h>
#include <float.h>
#include <locale.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define MYSTIFY_TERM_VERSION "0.1.0"
#define MAX_POLYS 16
#define MAX_POINTS 16
#define MAX_TRAILS 64
#define MIN_VEL 0.01f
#define MAX_VEL 10.f

struct point {
    float x;
    float y;
    float vx;
    float vy;
    float trail_x[MAX_TRAILS];
    float trail_y[MAX_TRAILS];
};

struct poly {
    struct point points[MAX_POINTS];
    float hue;
};

int opt_num_polys = 2;
int opt_num_points = 4;
int opt_num_trails = 20;
int opt_fps = 60;
float opt_max_velocity = 1.f;
int opt_show_status = 1;
int opt_trail_incr = 4;

struct poly polys[MAX_POLYS] = {0};
uint32_t *surface = NULL;
struct timeval sleep_tv = {0};

int screen_w = -1;
int screen_h = -1;
int surface_w = -1;
int surface_h = -1;

static uint32_t sextants[] = {
    0x00020 /* */, 0x1fb00 /*🬀*/, 0x1fb01 /*🬁*/, 0x1fb02 /*🬂*/,
    0x1fb03 /*🬃*/, 0x1fb04 /*🬄*/, 0x1fb05 /*🬅*/, 0x1fb06 /*🬆*/,
    0x1fb07 /*🬇*/, 0x1fb08 /*🬈*/, 0x1fb09 /*🬉*/, 0x1fb0a /*🬊*/,
    0x1fb0b /*🬋*/, 0x1fb0c /*🬌*/, 0x1fb0d /*🬍*/, 0x1fb0e /*🬎*/,
    0x1fb0f /*🬏*/, 0x1fb10 /*🬐*/, 0x1fb11 /*🬑*/, 0x1fb12 /*🬒*/,
    0x1fb13 /*🬓*/, 0x0258c /*▌*/, 0x1fb14 /*🬔*/, 0x1fb15 /*🬕*/,
    0x1fb16 /*🬖*/, 0x1fb17 /*🬗*/, 0x1fb18 /*🬘*/, 0x1fb19 /*🬙*/,
    0x1fb1a /*🬚*/, 0x1fb1b /*🬛*/, 0x1fb1c /*🬜*/, 0x1fb1d /*🬝*/,
    0x1fb1e /*🬞*/, 0x1fb1f /*🬟*/, 0x1fb20 /*🬠*/, 0x1fb21 /*🬡*/,
    0x1fb22 /*🬢*/, 0x1fb23 /*🬣*/, 0x1fb24 /*🬤*/, 0x1fb25 /*🬥*/,
    0x1fb26 /*🬦*/, 0x1fb27 /*🬧*/, 0x02590 /*▐*/, 0x1fb28 /*🬨*/,
    0x1fb29 /*🬩*/, 0x1fb2a /*🬪*/, 0x1fb2b /*🬫*/, 0x1fb2c /*🬬*/,
    0x1fb2d /*🬭*/, 0x1fb2e /*🬮*/, 0x1fb2f /*🬯*/, 0x1fb30 /*🬰*/,
    0x1fb31 /*🬱*/, 0x1fb32 /*🬲*/, 0x1fb33 /*🬳*/, 0x1fb34 /*🬴*/,
    0x1fb35 /*🬵*/, 0x1fb36 /*🬶*/, 0x1fb37 /*🬷*/, 0x1fb38 /*🬸*/,
    0x1fb39 /*🬹*/, 0x1fb3a /*🬺*/, 0x1fb3b /*🬻*/, 0x02588 /*█*/,
};


static void usage(FILE *f, int exit_code);
static void parse_opts(int argc, char **argv);
static void init_polys(void);
static void reinit_surface(void);
static int imin(int a, int b);
static int imax(int a, int b);
static float randf(float min, float max);
static uint64_t gettime_us(void);
static uint32_t hsv2rgb(float h, float s, float v);
static void draw_line(float xa, float ya, float xb, float yb, uint32_t color);
static void step(uint64_t frame_num);
static void draw(void);
static void draw_surface(void);
static void draw_screen(void);

int main(int argc, char **argv) {
    setlocale(LC_ALL, "");

    srand(time(NULL));

    parse_opts(argc, argv);

    tb_init();
    tb_set_output_mode(TB_OUTPUT_TRUECOLOR);

    int ttyfd = 0, resizefd = 0;
    tb_get_fds(&ttyfd, &resizefd);

    reinit_surface();
    init_polys();

    uint64_t frame_num = 0;
    int done = 0, pause = 0, single = 0;
    while (!done) {
        uint64_t start_us = gettime_us();
        tb_clear();

        struct tb_event ev;
        if (tb_peek_event(&ev, 0) == TB_OK) {
            switch (ev.type) {
                case TB_EVENT_KEY:
                    switch (ev.ch) {
                        case 'q': done = 1; break;
                        case 'p': pause = 1 - pause; break;
                        case 's': single = 1; pause = 1; break;
                        case 'r': init_polys(); break;
                        case 'w': opt_show_status = 1 - opt_show_status; break;
                        case 'l': opt_num_points = imin(opt_num_points + 1, MAX_POINTS); break;
                        case 'k': opt_num_points = imax(opt_num_points - 1, 3); break;
                        case 'b': opt_trail_incr = imin(opt_trail_incr + 1, MAX_TRAILS); break;
                        case 'v': opt_trail_incr = imax(opt_trail_incr - 1, 1); break;
                        case 'u': opt_num_polys = imin(opt_num_polys + 1, MAX_POLYS); break;
                        case 'y': opt_num_polys = imax(opt_num_polys - 1, 0); break;
                        case 'j': opt_num_trails = imin(opt_num_trails + 1, MAX_TRAILS); break;
                        case 'h': opt_num_trails = imax(opt_num_trails - 1, 0); break;
                        case 'o': opt_fps = imin(opt_fps + 1, 120); break;
                        case 'i': opt_fps = imax(opt_fps - 1, 1); break;
                        case 'm':
                        case 'n': {
                            opt_max_velocity *= (ev.ch == 'm' ? 1.05f : 1.f/1.05f);
                            opt_max_velocity = fminf(fmaxf(opt_max_velocity, MIN_VEL), MAX_VEL);
                            break;
                        }
                    }
                    break;
                case TB_EVENT_RESIZE:
                    reinit_surface();
                    break;
            }
        }

        if (!pause || single) {
            step(frame_num);
            draw();
            tb_present();
            single = 0;
        }

        uint64_t end_us = gettime_us();
        if (end_us >= start_us) {
            uint64_t us_per_frame = (uint64_t)(1000000.f / (float)opt_fps);
            uint64_t elapsed_us = end_us - start_us;
            if (elapsed_us < us_per_frame) {
                uint64_t sleep_us = us_per_frame - elapsed_us;
                sleep_tv.tv_sec = sleep_us / 1000000;
                sleep_tv.tv_usec = sleep_us % 1000000;
                fd_set rfds;
                FD_ZERO(&rfds);
                FD_SET(ttyfd, &rfds);
                FD_SET(resizefd, &rfds);
                int nfds = (resizefd > ttyfd ? resizefd : ttyfd) + 1;
                select(nfds, &rfds, NULL, NULL, &sleep_tv);
            }
        } else {
            tb_shutdown();
            printf("what?\n");
            exit(1);
        }

        frame_num += 1;
    }

    tb_shutdown();
    free(surface);

    return 0;
}

static void usage(FILE *f, int exit_code) {
    fprintf(f, "Usage:\n");
    fprintf(f, "  mystify-term [options]\n");
    fprintf(f, "\n");
    fprintf(f, "Options:\n");
    fprintf(f, "  -h, --help                  Show this help\n");
    fprintf(f, "  -v, --version               Show program version\n");
    fprintf(f, "  -q, --polys=<int>           Set number of polygons (default=%d, max=%d)\n", opt_num_polys, MAX_POLYS);
    fprintf(f, "  -p, --points=<int>          Set number of points per polygon (default=%d, max=%d)\n", opt_num_points, MAX_POINTS);
    fprintf(f, "  -t, --trails=<int>          Set number of trails lines (default=%d, max=%d)\n", opt_num_trails, MAX_TRAILS);
    fprintf(f, "  -f, --fps=<int>             Set frames per second (default=%d)\n", opt_fps);
    fprintf(f, "  -e, --max-velocity=<float>  Set max velocity per point (default=%.2f, min=%.2f, max=%.2f)\n", opt_max_velocity, MIN_VEL, MAX_VEL);
    fprintf(f, "  -s, --no-status             Hide status text\n");
    fprintf(f, "  -i, --trail-incr=<int>      Render every nth trail (default=%d, max=%d)\n", opt_trail_incr, MAX_TRAILS);
    exit(exit_code);
}

static void parse_opts(int argc, char **argv) {
    struct option long_opts[] = {
        { "help",         no_argument,       NULL, 'h' },
        { "version",      no_argument,       NULL, 'v' },
        { "polys",        required_argument, NULL, 'q' },
        { "points",       required_argument, NULL, 'p' },
        { "trails",       required_argument, NULL, 't' },
        { "fps",          required_argument, NULL, 'f' },
        { "max-velocity", required_argument, NULL, 'e' },
        { "no-status",    no_argument,       NULL, 's' },
        { "trail-incr",   required_argument, NULL, 'i' },
        { 0,              0,                 0,    0,  }
    };

    int c;
    while ((c = getopt_long(argc, argv, "hvq:p:t:f:e:si:", long_opts, NULL)) != -1) {
        switch (c) {
            case 'h': usage(stdout, 0); break;
            case 'v': printf("mystify-term v%s\n", MYSTIFY_TERM_VERSION); exit(0); break;
            case 'q': opt_num_polys = imin(imax(atoi(optarg), 0), MAX_POLYS); break;
            case 'p': opt_num_points = imin(imax(atoi(optarg), 0), MAX_POINTS); break;
            case 't': opt_num_trails = imin(imax(atoi(optarg), 0), MAX_TRAILS); break;
            case 'f': opt_fps = imax(atoi(optarg), 1); break;
            case 'e': opt_max_velocity = fminf(fmaxf((float)atof(optarg), MIN_VEL), MAX_VEL); break;
            case 's': opt_show_status = 0; break;
            case 'i': opt_trail_incr = imin(imax(atoi(optarg), 1), MAX_TRAILS); break;
            case '?': usage(stderr, 1); break;
        }
    }
}

static void init_polys(void) {
    int i, j, k;

    for (i = 0; i < MAX_POLYS; i++) {
        struct poly *q = &polys[i];

        q->hue = randf(0.f, 1.f);

        for (j = 0; j < MAX_POINTS; j++) {
            struct point *p = &q->points[j];

            p->x = randf(0.f, (float)(surface_w - 1));
            p->y = randf(0.f, (float)(surface_h - 1));
            p->vx = randf(-1.f, 1.f);
            p->vy = randf(-1.f, 1.f);

            for (k = 0; k < MAX_TRAILS; k++) {
                p->trail_x[k] = -1.f;
                p->trail_y[k] = -1.f;
            }
        }
    }
}

static void reinit_surface(void) {
    int old_screen_w = screen_w;
    int old_screen_h = screen_h;

    screen_w = tb_width();
    screen_h = tb_height();
    surface_w = screen_w * 2;
    surface_h = screen_h * 3;

    surface = realloc(surface, surface_w * surface_h * sizeof(*surface));

    if (old_screen_w <= 0) return;

    float rx = (float)screen_w / (float)old_screen_w;
    float ry = (float)screen_h / (float)old_screen_h;

    int i, j, k;
    for (i = 0; i < opt_num_polys; i++) {
        struct poly *q = &polys[i];
        for (j = 0; j < opt_num_points; j++) {
            struct point *p = &q->points[j];
            p->x *= rx;
            p->y *= ry;
            for (k = 0; k < opt_num_trails; k++) {
                p->trail_x[k] *= rx;
                p->trail_y[k] *= ry;
            }
        }
    }
}

static int imin(int a, int b) {
    return a < b ? a : b;
}

static int imax(int a, int b) {
    return a > b ? a : b;
}

static float randf(float min, float max) {
    float range = max - min;
    return ((float)rand() / (float)RAND_MAX) * range + min;
}

static uint64_t gettime_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_COARSE, &ts);
    return (uint64_t)ts.tv_sec * 1000000llu + (uint64_t)ts.tv_nsec / 1000llu;
}

static uint32_t hsv2rgb(float h, float s, float v) {
    int i = (int)floorf(h * 6.f);
    float f = h * 6.f - i;
    float p = v * (1.f - s);
    float q = v * (1.f - f * s);
    float t = v * (1.f - (1.f - f) * s);

    float r = 0.f, g = 0.f, b = 0.f;
    switch (i % 6) {
        case 0: r = v, g = t, b = p; break;
        case 1: r = q, g = v, b = p; break;
        case 2: r = p, g = v, b = t; break;
        case 3: r = p, g = q, b = v; break;
        case 4: r = t, g = p, b = v; break;
        case 5: r = v, g = p, b = q; break;
    }

    return ((uint32_t)(r * 255.f) << 16)
         + ((uint32_t)(g * 255.f) << 8)
         + (uint32_t)(b * 255.f);
}

static void draw_line(float xa, float ya, float xb, float yb, uint32_t color) {
    float x1, y1, x2, y2;
    if (xa > xb) {
        x1 = xb, y1 = yb, x2 = xa, y2 = ya;
    } else {
        x1 = xa, y1 = ya, x2 = xb, y2 = yb;
    }

    float dx = x2 - x1;
    float dy = y2 - y1;
    float m = dy / dx;
    float lx_delta = fmaxf(fminf(1.f / fabsf(m), 1.f), 0.001f);

    float lx, ly;
    for (lx = x1; lx <= x2; lx += lx_delta) {
        ly = m * (lx - x1) + y1;

        int idx = imin(
            (int)ly * surface_w + (int)lx,
            surface_w * surface_h - 1
        );

        surface[idx] = color;
    }
}

static void step(uint64_t frame_num) {
    int i, j, k;
    for (i = 0; i < opt_num_polys; i++) {
        struct poly *q = &polys[i];

        for (j = 0; j < opt_num_points; j++) {
            struct point *p = &q->points[j];

            for (k = opt_num_trails - 1; k > 0; k--) {
                p->trail_x[k] = p->trail_x[k - 1];
                p->trail_y[k] = p->trail_y[k - 1];
            }
            p->trail_x[0] = p->x;
            p->trail_y[0] = p->y;

            float scaled_vx = p->vx * opt_max_velocity;
            float scaled_vy = p->vy * opt_max_velocity;

            p->x += scaled_vx;
            p->y += scaled_vy;

            if (p->x < 0.f || p->x >= (float)surface_w) {
                p->vx *= -1.f;
                p->x += -2.f * scaled_vx;
            }

            if (p->y < 0.f || p->y >= (float)surface_h) {
                p->vy *= -1.f;
                p->y += -2.f * scaled_vy;
            }

            if (p->x < 0.f)               p->x = 0.f;
            if (p->x >= (float)surface_w) p->x = (float)(surface_w - 1);
            if (p->y < 0.f)               p->y = 0.f;
            if (p->y >= (float)surface_h) p->y = (float)(surface_h - 1);
        }
    }
}

static void draw(void) {
    draw_surface();
    draw_screen();
    if (opt_show_status) {
        int dy = 7;
        tb_printf(screen_w - 19, screen_h - dy--, 0, 0, "   fps (i o): %-7d", opt_fps);
        tb_printf(screen_w - 19, screen_h - dy--, 0, 0, " polys (y u): %-7d", opt_num_polys);
        tb_printf(screen_w - 19, screen_h - dy--, 0, 0, "points (k l): %-7d", opt_num_points);
        tb_printf(screen_w - 19, screen_h - dy--, 0, 0, "trails (h j): %-7d", opt_num_trails);
        tb_printf(screen_w - 19, screen_h - dy--, 0, 0, " max_v (n m): %-7.2f", opt_max_velocity);
        tb_printf(screen_w - 19, screen_h - dy--, 0, 0, "  incr (v b): %-7d", opt_trail_incr);
        tb_printf(screen_w - 45, screen_h - dy--, 0, 0, "(p=pause, s=step, r=reinit, w=status, q=quit)");
    }
}

static void draw_surface(void) {
    int i, j, k;

    memset(surface, 0, surface_w * surface_h * sizeof(*surface));

    for (i = 0; i < opt_num_polys; i++) {
        struct poly *q = &polys[i];

        for (j = 0; j < opt_num_points; j++) {
            struct point *a = &q->points[j];
            struct point *b = &q->points[(j + 1) % opt_num_points];

            for (k = 0; k < opt_num_trails; k += opt_trail_incr) {
                if (a->trail_x[k] < 0.f || b->trail_x[k] < 0.f) break;

                float v = fminf((opt_num_trails - k) / (opt_num_trails - 1.f), 1.f);
                uint32_t color = hsv2rgb(q->hue, 0.85f, v);

                draw_line(a->trail_x[k], a->trail_y[k], b->trail_x[k], b->trail_y[k], color);
            }

            draw_line(a->x, a->y, b->x, b->y, hsv2rgb(q->hue, 1.f, 1.f));
        }
    }
}

static void draw_screen(void) {
    int x, y;

    for (y = 0; y < screen_h; y++) {
        for (x = 0; x < screen_w; x++) {
            uint8_t sextant_idx = 0;

            uint32_t color = 0;
            int bit = 0;

            int dx, dy;
            for (dy = 0; dy < 3; dy++) {
                for (dx = 0; dx < 2; dx++) {

                    int px = x * 2 + dx;
                    int py = y * 3 + dy;
                    uint32_t pixel = surface[py * surface_w + px];

                    if (pixel) {
                        color = pixel;
                        sextant_idx |= 1 << bit;
                    }

                    bit += 1;
                }
            }
            uint32_t ch = sextants[sextant_idx & 0x3f];
            tb_set_cell(x, y, ch, color, 0);
        }
    }
}
