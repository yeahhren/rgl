#include "pipeline.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#define SUBPIXEL_ONE (1 << RGL_SUBPIXEL_BITS)
#define SUBPIXEL_HALF (SUBPIXEL_ONE / 2)

static int64_t to_fixed(const GLfloat v) {
    return (int64_t) floor((GLdouble) v * SUBPIXEL_ONE + 0.5);
}

static int64_t floor_div(const int64_t a, const int64_t b) {
    return a >= 0 ? a / b : -((-a + b - 1) / b);
}

static int64_t min3(const int64_t a, const int64_t b, const int64_t c) {
    const int64_t m = a < b ? a : b;
    return m < c ? m : c;
}

static int64_t max3(const int64_t a, const int64_t b, const int64_t c) {
    const int64_t m = a > b ? a : b;
    return m > c ? m : c;
}

static GLboolean stipple_pass(const rgl_context *ctx, const GLint x, const GLint y) {
    if (!ctx->enable.polygon_stipple)
        return GL_TRUE;
    const GLubyte byte = ctx->polygon.stipple[(y & 31) * 4 + (x & 31) / 8];
    return byte >> (7 - (x & 7)) & 1u;
}

static void triangle(rgl_context *ctx, const struct rgl_vertex *v0, const struct rgl_vertex *v1,
                     const struct rgl_vertex *v2, const GLfloat *flat_color) {
    const struct rgl_vertex *v[3] = {v0, v1, v2};
    int64_t x[3];
    int64_t y[3];
    for (int i = 0; i < 3; i++) {
        x[i] = to_fixed(v[i]->window[0]);
        y[i] = to_fixed(v[i]->window[1]);
    }

    int64_t area = (x[1] - x[0]) * (y[2] - y[0]) - (x[2] - x[0]) * (y[1] - y[0]);
    if (area == 0)
        return;
    if (area < 0) {
        const struct rgl_vertex *tv = v[1];
        v[1] = v[2];
        v[2] = tv;
        int64_t t = x[1];
        x[1] = x[2];
        x[2] = t;
        t = y[1];
        y[1] = y[2];
        y[2] = t;
        area = -area;
    }

    GLint x_min = (GLint) floor_div(min3(x[0], x[1], x[2]), SUBPIXEL_ONE);
    GLint x_max = (GLint) floor_div(max3(x[0], x[1], x[2]), SUBPIXEL_ONE);
    GLint y_min = (GLint) floor_div(min3(y[0], y[1], y[2]), SUBPIXEL_ONE);
    GLint y_max = (GLint) floor_div(max3(y[0], y[1], y[2]), SUBPIXEL_ONE);
    if (x_min < 0)
        x_min = 0;
    if (y_min < 0)
        y_min = 0;
    if (x_max > ctx->fb.width - 1)
        x_max = ctx->fb.width - 1;
    if (y_max > ctx->fb.height - 1)
        y_max = ctx->fb.height - 1;
    if (x_min > x_max || y_min > y_max)
        return;

    int64_t ex[3];
    int64_t ey[3];
    int64_t bias[3];
    for (int i = 0; i < 3; i++) {
        const int a = (i + 1) % 3;
        const int b = (i + 2) % 3;
        ex[i] = x[b] - x[a];
        ey[i] = y[b] - y[a];
        bias[i] = ey[i] < 0 || (ey[i] == 0 && ex[i] < 0) ? 0 : -1;
    }

    const int64_t sx0 = (int64_t) x_min * SUBPIXEL_ONE + SUBPIXEL_HALF;
    struct rgl_fragment frag;

    for (GLint py = y_min; py <= y_max; py++) {
        const int64_t sy = (int64_t) py * SUBPIXEL_ONE + SUBPIXEL_HALF;
        int64_t w[3];
        for (int i = 0; i < 3; i++) {
            const int a = (i + 1) % 3;
            w[i] = ex[i] * (sy - y[a]) - ey[i] * (sx0 - x[a]);
        }

        for (GLint px = x_min; px <= x_max; px++) {
            if (w[0] + bias[0] >= 0 && w[1] + bias[1] >= 0 && w[2] + bias[2] >= 0 && stipple_pass(ctx, px, py)) {
                const GLdouble l0 = (GLdouble) w[0] / (GLdouble) area;
                const GLdouble l1 = (GLdouble) w[1] / (GLdouble) area;
                const GLdouble l2 = (GLdouble) w[2] / (GLdouble) area;
                const GLdouble p0 = l0 * v[0]->inv_w;
                const GLdouble p1 = l1 * v[1]->inv_w;
                const GLdouble p2 = l2 * v[2]->inv_w;
                const GLdouble sum = p0 + p1 + p2;

                frag.x = px;
                frag.y = py;
                frag.z = (GLfloat) (l0 * v[0]->window[2] + l1 * v[1]->window[2] + l2 * v[2]->window[2]);
                for (int k = 0; k < 4; k++) {
                    frag.color[k] = flat_color
                                        ? flat_color[k]
                                        : (GLfloat) ((p0 * v[0]->color[k] + p1 * v[1]->color[k] +
                                                      p2 * v[2]->color[k]) / sum);
                    frag.tex_coord[k] = (GLfloat) ((p0 * v[0]->tex_coord[k] + p1 * v[1]->tex_coord[k] +
                                                    p2 * v[2]->tex_coord[k]) / sum);
                }
                rgl_write_fragment(ctx, &frag);
            }
            for (int i = 0; i < 3; i++)
                w[i] -= ey[i] * SUBPIXEL_ONE;
        }
    }
}

void rgl_raster_polygon(rgl_context *ctx, const struct rgl_vertex *vertices, const size_t count,
                        const GLfloat *flat_color) {
    GLdouble area = 0.0;
    for (size_t i = 0; i < count; i++) {
        const GLfloat *a = vertices[i].window;
        const GLfloat *b = vertices[(i + 1) % count].window;
        area += (GLdouble) a[0] * b[1] - (GLdouble) b[0] * a[1];
    }

    const GLboolean front = ctx->polygon.front_face == GL_CCW ? area > 0.0 : area < 0.0;
    if (ctx->enable.cull_face) {
        const GLenum cull = ctx->polygon.cull_face;
        if (cull == GL_FRONT_AND_BACK || (cull == GL_FRONT && front) || (cull == GL_BACK && !front))
            return;
    }

    switch (front ? ctx->polygon.front_mode : ctx->polygon.back_mode) {
        case GL_POINT:
            for (size_t i = 0; i < count; i++) {
                if (vertices[i].edge_flag)
                    rgl_raster_point(ctx, &vertices[i], flat_color ? flat_color : vertices[i].color);
            }
            break;
        case GL_LINE:
            ctx->line.stipple_counter = 0;
            for (size_t i = 0; i < count; i++) {
                if (vertices[i].edge_flag)
                    rgl_raster_line(ctx, &vertices[i], &vertices[(i + 1) % count], flat_color);
            }
            break;
        default:
            for (size_t i = 1; i + 1 < count; i++)
                triangle(ctx, &vertices[0], &vertices[i], &vertices[i + 1], flat_color);
            break;
    }
}

void glPolygonMode(const GLenum face, const GLenum mode) {
    GET_CONTEXT(ctx);
    OUTSIDE_BEGIN_END(ctx);

    if ((face != GL_FRONT && face != GL_BACK && face != GL_FRONT_AND_BACK) ||
        (mode != GL_POINT && mode != GL_LINE && mode != GL_FILL)) {
        rgl_error(ctx, GL_INVALID_ENUM);
        return;
    }
    if (face != GL_BACK)
        ctx->polygon.front_mode = mode;
    if (face != GL_FRONT)
        ctx->polygon.back_mode = mode;
}

void glCullFace(const GLenum mode) {
    GET_CONTEXT(ctx);
    OUTSIDE_BEGIN_END(ctx);

    if (mode != GL_FRONT && mode != GL_BACK && mode != GL_FRONT_AND_BACK) {
        rgl_error(ctx, GL_INVALID_ENUM);
        return;
    }
    ctx->polygon.cull_face = mode;
}

void glFrontFace(const GLenum mode) {
    GET_CONTEXT(ctx);
    OUTSIDE_BEGIN_END(ctx);

    if (mode != GL_CW && mode != GL_CCW) {
        rgl_error(ctx, GL_INVALID_ENUM);
        return;
    }
    ctx->polygon.front_face = mode;
}

// assumes default unpack state until `glPixelStore` is called
void glPolygonStipple(const GLubyte *mask) {
    GET_CONTEXT(ctx);
    OUTSIDE_BEGIN_END(ctx);

    memcpy(ctx->polygon.stipple, mask, sizeof(ctx->polygon.stipple));
}

void glGetPolygonStipple(GLubyte *mask) {
    GET_CONTEXT(ctx);
    OUTSIDE_BEGIN_END(ctx);

    memcpy(mask, ctx->polygon.stipple, sizeof(ctx->polygon.stipple));
}
