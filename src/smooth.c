#include "pipeline.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define SAMPLES 4
#define SAMPLE_COUNT (SAMPLES * SAMPLES)

struct shape {
    const GLfloat (*points)[2];
    size_t count;
    GLfloat orientation;
};

typedef GLboolean (*shade_fn)(rgl_context *ctx, const void *data, GLfloat x, GLfloat y, struct rgl_fragment *frag);

static GLfloat smooth_size(const GLfloat size, const GLfloat max) {
    const GLfloat q = floorf(size / RGL_SMOOTH_GRANULARITY + 0.5f) * RGL_SMOOTH_GRANULARITY;
    return rgl_clampf(q, RGL_MIN_SMOOTH_SIZE, max);
}

static GLboolean inside(const struct shape *s, const GLfloat x, const GLfloat y) {
    for (size_t i = 0; i < s->count; i++) {
        const GLfloat *a = s->points[i];
        const GLfloat *b = s->points[(i + 1) % s->count];
        if (((b[0] - a[0]) * (y - a[1]) - (b[1] - a[1]) * (x - a[0])) * s->orientation < 0.0f)
            return GL_FALSE;
    }
    return GL_TRUE;
}

static void include(GLboolean *found, GLfloat *lo, GLfloat *hi, const GLfloat y) {
    if (!*found || y < *lo)
        *lo = y;
    if (!*found || y > *hi)
        *hi = y;
    *found = GL_TRUE;
}

static GLboolean slab_range(const struct shape *s, const GLfloat x0, const GLfloat x1, GLfloat *lo, GLfloat *hi) {
    GLboolean found = GL_FALSE;
    for (size_t i = 0; i < s->count; i++) {
        const GLfloat *a = s->points[i];
        const GLfloat *b = s->points[(i + 1) % s->count];
        if (a[0] >= x0 && a[0] <= x1)
            include(&found, lo, hi, a[1]);

        const GLfloat edges[2] = {x0, x1};
        for (int k = 0; k < 2; k++) {
            const GLfloat x = edges[k];
            if ((a[0] - x) * (b[0] - x) < 0.0f)
                include(&found, lo, hi, a[1] + (x - a[0]) / (b[0] - a[0]) * (b[1] - a[1]));
        }
    }
    return found;
}

static GLfloat coverage(const struct shape *s, const GLint px, const GLint py, GLfloat *sx, GLfloat *sy) {
    int covered = 0;
    GLfloat best = 0.0f;
    for (int j = 0; j < SAMPLES; j++) {
        for (int i = 0; i < SAMPLES; i++) {
            const GLfloat x = (GLfloat) px + ((GLfloat) i + 0.5f) / SAMPLES;
            const GLfloat y = (GLfloat) py + ((GLfloat) j + 0.5f) / SAMPLES;
            if (!inside(s, x, y))
                continue;

            const GLfloat d = (x - (GLfloat) px - 0.5f) * (x - (GLfloat) px - 0.5f) +
                              (y - (GLfloat) py - 0.5f) * (y - (GLfloat) py - 0.5f);
            if (covered == 0 || d < best) {
                best = d;
                *sx = x;
                *sy = y;
            }
            covered++;
        }
    }
    return (GLfloat) covered / SAMPLE_COUNT;
}

static void rasterize(rgl_context *ctx, const struct shape *s, const shade_fn shade, const void *data) {
    GLfloat x_lo = s->points[0][0];
    GLfloat x_hi = s->points[0][0];
    for (size_t i = 1; i < s->count; i++) {
        x_lo = fminf(x_lo, s->points[i][0]);
        x_hi = fmaxf(x_hi, s->points[i][0]);
    }

    const GLint x0 = (GLint) fmaxf(floorf(x_lo), 0.0f);
    const GLint x1 = (GLint) fminf(floorf(x_hi), (GLfloat) (ctx->fb.width - 1));
    struct rgl_fragment frag;

    for (GLint px = x0; px <= x1; px++) {
        GLfloat y_lo;
        GLfloat y_hi;
        if (!slab_range(s, (GLfloat) px, (GLfloat) (px + 1), &y_lo, &y_hi))
            continue;

        const GLint y0 = (GLint) fmaxf(floorf(y_lo), 0.0f);
        const GLint y1 = (GLint) fminf(floorf(y_hi), (GLfloat) (ctx->fb.height - 1));
        for (GLint py = y0; py <= y1; py++) {
            GLfloat sx = 0.0f;
            GLfloat sy = 0.0f;
            const GLfloat cov = coverage(s, px, py, &sx, &sy);
            if (cov <= 0.0f)
                continue;

            memset(&frag, 0, sizeof(frag));
            frag.x = px;
            frag.y = py;
            frag.coverage = cov;
            if (shade(ctx, data, sx, sy, &frag))
                rgl_write_fragment(ctx, &frag);
        }
    }
}

static GLfloat orientation(const GLfloat (*points)[2], const size_t count) {
    GLdouble area = 0.0;
    for (size_t i = 0; i < count; i++) {
        const GLfloat *a = points[i];
        const GLfloat *b = points[(i + 1) % count];
        area += (GLdouble) a[0] * b[1] - (GLdouble) b[0] * a[1];
    }
    return area > 0.0 ? 1.0f : area < 0.0 ? -1.0f : 0.0f;
}

void rgl_smooth_point(rgl_context *ctx, const struct rgl_vertex *v, const GLfloat *color) {
    const GLfloat r = smooth_size(ctx->point.size, RGL_MAX_POINT_SIZE) * 0.5f;
    const GLfloat cx = v->window[0];
    const GLfloat cy = v->window[1];

    struct rgl_fragment frag;
    memset(&frag, 0, sizeof(frag));
    frag.z = v->window[2];
    frag.fog_z = v->fog_z;
    memcpy(frag.color, color, sizeof(frag.color));
    memcpy(frag.tex_coord, v->tex_coord, sizeof(frag.tex_coord));

    for (GLint py = (GLint) floorf(cy - r); py <= (GLint) floorf(cy + r); py++) {
        for (GLint px = (GLint) floorf(cx - r); px <= (GLint) floorf(cx + r); px++) {
            int covered = 0;
            for (int j = 0; j < SAMPLES; j++) {
                for (int i = 0; i < SAMPLES; i++) {
                    const GLfloat x = (GLfloat) px + ((GLfloat) i + 0.5f) / SAMPLES - cx;
                    const GLfloat y = (GLfloat) py + ((GLfloat) j + 0.5f) / SAMPLES - cy;
                    if (x * x + y * y <= r * r)
                        covered++;
                }
            }
            if (covered == 0)
                continue;

            frag.x = px;
            frag.y = py;
            frag.coverage = (GLfloat) covered / SAMPLE_COUNT;
            rgl_write_fragment(ctx, &frag);
        }
    }
}

struct line_data {
    const struct rgl_vertex *a;
    const struct rgl_vertex *b;
    const GLfloat *flat_color;
    GLfloat ux;
    GLfloat uy;
    GLfloat length;
    GLfloat major;
    GLuint counter;
    GLboolean textured;
};

static void line_tex(const struct line_data *l, const GLfloat t, GLfloat out[2]) {
    const GLfloat wa = (1.0f - t) * l->a->inv_w;
    const GLfloat wb = t * l->b->inv_w;
    const GLfloat q = wa * l->a->tex_coord[3] + wb * l->b->tex_coord[3];
    for (int k = 0; k < 2; k++)
        out[k] = q != 0.0f ? (wa * l->a->tex_coord[k] + wb * l->b->tex_coord[k]) / q : 0.0f;
}

static GLboolean shade_line(rgl_context *ctx, const void *data, const GLfloat x, const GLfloat y,
                            struct rgl_fragment *frag) {
    const struct line_data *l = data;
    const struct rgl_vertex *a = l->a;
    const struct rgl_vertex *b = l->b;
    const GLfloat t = rgl_clampf(((x - a->window[0]) * l->ux + (y - a->window[1]) * l->uy) / l->length, 0.0f, 1.0f);

    if (ctx->enable.line_stipple) {
        const GLuint s = l->counter + (GLuint) (t * l->major);
        const GLuint bit = s / (GLuint) ctx->line.stipple_factor % 16u;
        if (!(ctx->line.stipple_pattern >> bit & 1u))
            return GL_FALSE;
    }

    const GLfloat wa = (1.0f - t) * a->inv_w;
    const GLfloat wb = t * b->inv_w;
    const GLfloat sum = wa + wb;
    frag->z = a->window[2] + t * (b->window[2] - a->window[2]);
    frag->fog_z = (wa * a->fog_z + wb * b->fog_z) / sum;
    for (int k = 0; k < 4; k++) {
        frag->color[k] = l->flat_color ? l->flat_color[k] : (wa * a->color[k] + wb * b->color[k]) / sum;
        frag->tex_coord[k] = (wa * a->tex_coord[k] + wb * b->tex_coord[k]) / sum;
    }

    if (l->textured) {
        GLfloat st[2];
        GLfloat next[2];
        line_tex(l, t, st);
        line_tex(l, t + 1.0f / l->length, next);
        frag->tex_dx[0] = next[0] - st[0];
        frag->tex_dx[1] = next[1] - st[1];
    }
    return GL_TRUE;
}

void rgl_smooth_line(rgl_context *ctx, const struct rgl_vertex *a, const struct rgl_vertex *b,
                     const GLfloat *flat_color) {
    const GLfloat dx = b->window[0] - a->window[0];
    const GLfloat dy = b->window[1] - a->window[1];
    const GLfloat length = sqrtf(dx * dx + dy * dy);
    if (length == 0.0f)
        return;

    const GLfloat ux = dx / length;
    const GLfloat uy = dy / length;
    const GLfloat hw = smooth_size(ctx->line.width, RGL_MAX_LINE_WIDTH) * 0.5f;
    const GLfloat nx = -uy * hw;
    const GLfloat ny = ux * hw;
    const GLfloat points[4][2] = {
        {a->window[0] - nx, a->window[1] - ny},
        {b->window[0] - nx, b->window[1] - ny},
        {b->window[0] + nx, b->window[1] + ny},
        {a->window[0] + nx, a->window[1] + ny},
    };

    const struct shape s = {.points = points, .count = 4, .orientation = orientation(points, 4)};
    const struct line_data data = {
        .a = a, .b = b, .flat_color = flat_color, .ux = ux, .uy = uy, .length = length,
        .major = fmaxf(fabsf(dx), fabsf(dy)), .counter = ctx->line.stipple_counter,
        .textured = rgl_texturing_enabled(ctx)
    };
    rasterize(ctx, &s, shade_line, &data);
    ctx->line.stipple_counter += (GLuint) floorf(data.major + 0.5f);
}

struct polygon_data {
    const struct rgl_vertex *vertices;
    size_t count;
    const GLfloat *flat_color;
    GLboolean textured;
};

static GLboolean barycentric(const struct rgl_vertex *v0, const struct rgl_vertex *v1, const struct rgl_vertex *v2,
                             const GLfloat x, const GLfloat y, GLfloat l[3]) {
    const GLfloat x0 = v0->window[0];
    const GLfloat y0 = v0->window[1];
    const GLfloat area = (v1->window[0] - x0) * (v2->window[1] - y0) - (v2->window[0] - x0) * (v1->window[1] - y0);
    if (area == 0.0f)
        return GL_FALSE;

    l[1] = ((x - x0) * (v2->window[1] - y0) - (y - y0) * (v2->window[0] - x0)) / area;
    l[2] = ((v1->window[0] - x0) * (y - y0) - (v1->window[1] - y0) * (x - x0)) / area;
    l[0] = 1.0f - l[1] - l[2];
    return GL_TRUE;
}

static void polygon_tex(const struct rgl_vertex *const v[3], const GLfloat l[3], GLfloat out[2]) {
    GLfloat st[2] = {0.0f, 0.0f};
    GLfloat q = 0.0f;
    for (int i = 0; i < 3; i++) {
        const GLfloat p = l[i] * v[i]->inv_w;
        st[0] += p * v[i]->tex_coord[0];
        st[1] += p * v[i]->tex_coord[1];
        q += p * v[i]->tex_coord[3];
    }
    out[0] = q != 0.0f ? st[0] / q : 0.0f;
    out[1] = q != 0.0f ? st[1] / q : 0.0f;
}

static GLboolean shade_polygon(rgl_context *ctx, const void *data, const GLfloat x, const GLfloat y,
                               struct rgl_fragment *frag) {
    const struct polygon_data *p = data;
    if (!rgl_polygon_stipple_pass(ctx, frag->x, frag->y))
        return GL_FALSE;

    const struct rgl_vertex *v[3] = {NULL, NULL, NULL};
    GLfloat l[3] = {0.0f, 0.0f, 0.0f};
    GLfloat best = -INFINITY;
    for (size_t k = 1; k + 1 < p->count; k++) {
        GLfloat c[3];
        if (!barycentric(&p->vertices[0], &p->vertices[k], &p->vertices[k + 1], x, y, c))
            continue;

        const GLfloat worst = fminf(c[0], fminf(c[1], c[2]));
        if (worst > best) {
            best = worst;
            v[0] = &p->vertices[0];
            v[1] = &p->vertices[k];
            v[2] = &p->vertices[k + 1];
            memcpy(l, c, sizeof(l));
        }
        if (worst >= 0.0f)
            break;
    }
    if (v[0] == NULL)
        return GL_FALSE;

    GLfloat w[3];
    GLfloat sum = 0.0f;
    for (int i = 0; i < 3; i++) {
        w[i] = l[i] * v[i]->inv_w;
        sum += w[i];
    }

    frag->z = l[0] * v[0]->window[2] + l[1] * v[1]->window[2] + l[2] * v[2]->window[2];
    frag->fog_z = (w[0] * v[0]->fog_z + w[1] * v[1]->fog_z + w[2] * v[2]->fog_z) / sum;
    for (int k = 0; k < 4; k++) {
        const GLfloat color = w[0] * v[0]->color[k] + w[1] * v[1]->color[k] + w[2] * v[2]->color[k];
        const GLfloat tex = w[0] * v[0]->tex_coord[k] + w[1] * v[1]->tex_coord[k] + w[2] * v[2]->tex_coord[k];
        frag->color[k] = p->flat_color ? p->flat_color[k] : color / sum;
        frag->tex_coord[k] = tex / sum;
    }

    if (p->textured) {
        GLfloat lx[3];
        GLfloat ly[3];
        GLfloat st[2];
        GLfloat sx[2];
        GLfloat sy[2];
        barycentric(v[0], v[1], v[2], x + 1.0f, y, lx);
        barycentric(v[0], v[1], v[2], x, y + 1.0f, ly);
        polygon_tex(v, l, st);
        polygon_tex(v, lx, sx);
        polygon_tex(v, ly, sy);
        for (int k = 0; k < 2; k++) {
            frag->tex_dx[k] = sx[k] - st[k];
            frag->tex_dy[k] = sy[k] - st[k];
        }
    }
    return GL_TRUE;
}

void rgl_smooth_polygon(rgl_context *ctx, const struct rgl_vertex *vertices, const size_t count,
                        const GLfloat *flat_color) {
    GLfloat (*points)[2] = malloc(count * sizeof(*points));
    if (points == NULL) {
        rgl_error(ctx, GL_OUT_OF_MEMORY);
        return;
    }
    for (size_t i = 0; i < count; i++) {
        points[i][0] = vertices[i].window[0];
        points[i][1] = vertices[i].window[1];
    }

    const struct shape s = {.points = (const GLfloat (*)[2]) points, .count = count,
                            .orientation = orientation((const GLfloat (*)[2]) points, count)};
    if (s.orientation != 0.0f) {
        const struct polygon_data data = {
            .vertices = vertices, .count = count, .flat_color = flat_color, .textured = rgl_texturing_enabled(ctx)
        };
        rasterize(ctx, &s, shade_polygon, &data);
    }
    free(points);
}
