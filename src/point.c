#include "pipeline.h"
#include "list.h"

#include <math.h>
#include <string.h>

void rgl_raster_point(rgl_context *ctx, const struct rgl_vertex *v, const GLfloat *color) {
    if (ctx->enable.point_smooth) {
        rgl_smooth_point(ctx, v, color);
        return;
    }

    const GLint size = rgl_integer_width(ctx->point.size, RGL_MAX_POINT_SIZE);
    GLint x0;
    GLint y0;
    if (size % 2) {
        x0 = (GLint) floorf(v->window[0]) - (size - 1) / 2;
        y0 = (GLint) floorf(v->window[1]) - (size - 1) / 2;
    } else {
        x0 = (GLint) floorf(v->window[0] + 0.5f) - size / 2;
        y0 = (GLint) floorf(v->window[1] + 0.5f) - size / 2;
    }

    struct rgl_fragment frag;
    frag.z = v->window[2];
    frag.fog_z = v->fog_z;
    frag.coverage = 1.0f;
    memcpy(frag.color, color, sizeof(frag.color));
    memcpy(frag.tex_coord, v->tex_coord, sizeof(frag.tex_coord));
    memset(frag.tex_dx, 0, sizeof(frag.tex_dx));
    memset(frag.tex_dy, 0, sizeof(frag.tex_dy));

    for (GLint y = y0; y < y0 + size; y++) {
        for (GLint x = x0; x < x0 + size; x++) {
            frag.x = x;
            frag.y = y;
            rgl_write_fragment(ctx, &frag);
        }
    }
}

void glPointSize(const GLfloat size) {
    GET_CONTEXT(ctx);
    if (SAVE(ctx, RGL_OP_POINT_SIZE, {.f = size}))
        return;
    OUTSIDE_BEGIN_END(ctx);

    if (size <= 0.0f) {
        rgl_error(ctx, GL_INVALID_VALUE);
        return;
    }
    ctx->point.size = size;
}
