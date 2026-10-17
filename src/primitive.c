#include "pipeline.h"
#include "list.h"

#include <stdlib.h>

static const GLfloat *flat_color(const rgl_context *ctx, const struct rgl_vertex *provoking) {
    return ctx->lighting.shade_model == GL_FLAT ? provoking->color : NULL;
}

static const struct rgl_vertex *flat_vertex(const rgl_context *ctx, const struct rgl_vertex *provoking) {
    return ctx->lighting.shade_model == GL_FLAT ? provoking : NULL;
}

static void triangle(rgl_context *ctx, const struct rgl_vertex *a, const struct rgl_vertex *b,
                     const struct rgl_vertex *c, const GLboolean use_edge_flags, const struct rgl_vertex *provoking) {
    const struct rgl_vertex *v[3] = {a, b, c};
    rgl_clip_polygon(ctx, v, 3, use_edge_flags, flat_vertex(ctx, provoking));
}

static void quad(rgl_context *ctx, const struct rgl_vertex *a, const struct rgl_vertex *b,
                 const struct rgl_vertex *c, const struct rgl_vertex *d, const GLboolean use_edge_flags,
                 const struct rgl_vertex *provoking) {
    const struct rgl_vertex *v[4] = {a, b, c, d};
    rgl_clip_polygon(ctx, v, 4, use_edge_flags, flat_vertex(ctx, provoking));
}

static void polygon(rgl_context *ctx, const struct rgl_vertex *vertices, const size_t count) {
    if (count < 3)
        return;

    const struct rgl_vertex **v = malloc(count * sizeof(*v));
    if (v == NULL) {
        rgl_error(ctx, GL_OUT_OF_MEMORY);
        return;
    }
    for (size_t i = 0; i < count; i++)
        v[i] = &vertices[i];
    rgl_clip_polygon(ctx, v, count, GL_TRUE, flat_vertex(ctx, &vertices[0]));
    free(v);
}

void rgl_render_primitive(rgl_context *ctx) {
    const struct rgl_vertex *v = ctx->primitive.vertices;
    const size_t n = ctx->primitive.count;

    ctx->line.stipple_counter = 0;

    switch (ctx->primitive.mode) {
        case GL_POINTS:
            for (size_t i = 0; i < n; i++)
                rgl_clip_point(ctx, &v[i]);
            break;
        case GL_LINES:
            for (size_t i = 0; i + 1 < n; i += 2) {
                ctx->line.stipple_counter = 0;
                rgl_clip_line(ctx, &v[i], &v[i + 1], flat_color(ctx, &v[i + 1]));
            }
            break;
        case GL_LINE_STRIP:
        case GL_LINE_LOOP:
            for (size_t i = 0; i + 1 < n; i++)
                rgl_clip_line(ctx, &v[i], &v[i + 1], flat_color(ctx, &v[i + 1]));
            if (ctx->primitive.mode == GL_LINE_LOOP && n > 1)
                rgl_clip_line(ctx, &v[n - 1], &v[0], flat_color(ctx, &v[0]));
            break;
        case GL_TRIANGLES:
            for (size_t i = 0; i + 2 < n; i += 3)
                triangle(ctx, &v[i], &v[i + 1], &v[i + 2], GL_TRUE, &v[i + 2]);
            break;
        case GL_TRIANGLE_STRIP:
            for (size_t i = 0; i + 2 < n; i++) {
                if (i % 2 == 0)
                    triangle(ctx, &v[i], &v[i + 1], &v[i + 2], GL_FALSE, &v[i + 2]);
                else
                    triangle(ctx, &v[i + 1], &v[i], &v[i + 2], GL_FALSE, &v[i + 2]);
            }
            break;
        case GL_TRIANGLE_FAN:
            for (size_t i = 1; i + 1 < n; i++)
                triangle(ctx, &v[0], &v[i], &v[i + 1], GL_FALSE, &v[i + 1]);
            break;
        case GL_QUADS:
            for (size_t i = 0; i + 3 < n; i += 4)
                quad(ctx, &v[i], &v[i + 1], &v[i + 2], &v[i + 3], GL_TRUE, &v[i + 3]);
            break;
        case GL_QUAD_STRIP:
            for (size_t i = 0; i + 3 < n; i += 2)
                quad(ctx, &v[i], &v[i + 1], &v[i + 3], &v[i + 2], GL_FALSE, &v[i + 3]);
            break;
        case GL_POLYGON:
            polygon(ctx, v, n);
            break;
        default:
            break;
    }
}

void glShadeModel(const GLenum mode) {
    GET_CONTEXT(ctx);
    if (SAVE(ctx, RGL_OP_SHADE_MODEL, {.e = mode}))
        return;
    OUTSIDE_BEGIN_END(ctx);

    if (mode != GL_FLAT && mode != GL_SMOOTH) {
        rgl_error(ctx, GL_INVALID_ENUM);
        return;
    }
    ctx->lighting.shade_model = mode;
}
