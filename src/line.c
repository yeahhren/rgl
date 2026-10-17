#include "pipeline.h"
#include "list.h"

#include <math.h>
#include <string.h>

static GLboolean stipple_pass(rgl_context *ctx) {
    const GLuint bit = ctx->line.stipple_counter / (GLuint) ctx->line.stipple_factor % 16u;
    ctx->line.stipple_counter++;
    return !ctx->enable.line_stipple || (ctx->line.stipple_pattern >> bit & 1u);
}

static void projected_tex(const struct rgl_vertex *a, const struct rgl_vertex *b, const GLfloat t,
                          GLfloat out[2]) {
    const GLfloat wa = (1.0f - t) * a->inv_w;
    const GLfloat wb = t * b->inv_w;
    const GLfloat q = wa * a->tex_coord[3] + wb * b->tex_coord[3];
    for (int k = 0; k < 2; k++)
        out[k] = q != 0.0f ? (wa * a->tex_coord[k] + wb * b->tex_coord[k]) / q : 0.0f;
}

void rgl_raster_line(rgl_context *ctx, const struct rgl_vertex *a, const struct rgl_vertex *b,
                     const GLfloat *flat_color) {
    const GLfloat dx = b->window[0] - a->window[0];
    const GLfloat dy = b->window[1] - a->window[1];
    if (dx == 0.0f && dy == 0.0f)
        return;

    const int major = fabsf(dx) >= fabsf(dy) ? 0 : 1;
    const int minor = 1 - major;
    const GLfloat ma = a->window[major];
    const GLfloat mb = b->window[major];
    const GLfloat na = a->window[minor];
    const GLfloat nb = b->window[minor];
    const GLint width = rgl_integer_width(ctx->line.width, RGL_MAX_LINE_WIDTH);

    GLint start;
    GLint end;
    GLint step;
    if (mb > ma) {
        start = (GLint) ceilf(ma - 0.5f);
        end = (GLint) ceilf(mb - 0.5f);
        step = 1;
    } else {
        start = (GLint) floorf(ma - 0.5f);
        end = (GLint) floorf(mb - 0.5f);
        step = -1;
    }

    const GLboolean textured = rgl_texturing_enabled(ctx);
    struct rgl_fragment frag;
    memset(frag.tex_dx, 0, sizeof(frag.tex_dx));
    memset(frag.tex_dy, 0, sizeof(frag.tex_dy));

    for (GLint i = start; i != end; i += step) {
        if (!stipple_pass(ctx))
            continue;

        const GLfloat t = ((GLfloat) i + 0.5f - ma) / (mb - ma);
        const GLfloat wa = (1.0f - t) * a->inv_w;
        const GLfloat wb = t * b->inv_w;
        const GLfloat sum = wa + wb;

        frag.z = a->window[2] + t * (b->window[2] - a->window[2]);
        frag.fog_z = (wa * a->fog_z + wb * b->fog_z) / sum;
        for (int k = 0; k < 4; k++) {
            frag.color[k] = flat_color ? flat_color[k] : (wa * a->color[k] + wb * b->color[k]) / sum;
            frag.tex_coord[k] = (wa * a->tex_coord[k] + wb * b->tex_coord[k]) / sum;
        }
        if (textured) {
            GLfloat st[2];
            GLfloat next[2];
            projected_tex(a, b, t, st);
            projected_tex(a, b, t + (GLfloat) step / (mb - ma), next);
            GLfloat *d = major == 0 ? frag.tex_dx : frag.tex_dy;
            d[0] = next[0] - st[0];
            d[1] = next[1] - st[1];
        }

        const GLint first = (GLint) floorf(na + t * (nb - na) - (GLfloat) (width - 1) * 0.5f);
        for (GLint k = 0; k < width; k++) {
            frag.x = major == 0 ? i : first + k;
            frag.y = major == 0 ? first + k : i;
            rgl_write_fragment(ctx, &frag);
        }
    }
}

void glLineWidth(const GLfloat width) {
    GET_CONTEXT(ctx);
    if (SAVE(ctx, RGL_OP_LINE_WIDTH, {.f = width}))
        return;
    OUTSIDE_BEGIN_END(ctx);

    if (width <= 0.0f) {
        rgl_error(ctx, GL_INVALID_VALUE);
        return;
    }
    ctx->line.width = width;
}

void glLineStipple(const GLint factor, const GLushort pattern) {
    GET_CONTEXT(ctx);
    if (SAVE(ctx, RGL_OP_LINE_STIPPLE, {.i = factor}, {.u = pattern}))
        return;
    OUTSIDE_BEGIN_END(ctx);

    ctx->line.stipple_factor = factor < 1 ? 1 : factor > 256 ? 256 : factor;
    ctx->line.stipple_pattern = pattern;
}
