#include "context.h"
#include "list.h"

#include <math.h>

static GLshort to_accum(const GLfloat v) {
    return (GLshort) floorf(rgl_clampf(v, -1.0f, 1.0f) * RGL_ACCUM_MAX + 0.5f);
}

static GLfloat from_accum(const GLshort v) {
    return (GLfloat) v / RGL_ACCUM_MAX;
}

void glAccum(const GLenum op, const GLfloat value) {
    GET_CONTEXT(ctx);
    if (SAVE(ctx, RGL_OP_ACCUM, {.e = op}, {.f = value}))
        return;
    OUTSIDE_BEGIN_END(ctx);

    if (op < GL_ACCUM || op > GL_ADD) {
        rgl_error(ctx, GL_INVALID_ENUM);
        return;
    }
    if (ctx->fb.accum == NULL) {
        rgl_error(ctx, GL_INVALID_OPERATION);
        return;
    }

    const struct rgl_rect r = rgl_scissor_rect(ctx);
    const GLboolean draw = ctx->color_buffer.draw_buffer != GL_NONE;

    for (GLint y = r.y0; y < r.y1; y++) {
        const size_t row = (size_t) y * (size_t) ctx->fb.width;
        for (GLint x = r.x0; x < r.x1; x++) {
            GLshort *a = ctx->fb.accum + (row + (size_t) x) * 4;
            GLubyte *c = ctx->fb.color + (row + (size_t) x) * 4;

            for (int k = 0; k < 4; k++) {
                const GLfloat color = (GLfloat) c[k] / 255.0f;
                switch (op) {
                    case GL_ACCUM:
                        a[k] = to_accum(from_accum(a[k]) + value * color);
                        break;
                    case GL_LOAD:
                        a[k] = to_accum(value * color);
                        break;
                    case GL_RETURN:
                        if (draw && ctx->mask.color[k])
                            c[k] = (GLubyte) (rgl_clampf(value * from_accum(a[k]), 0.0f, 1.0f) * 255.0f + 0.5f);
                        break;
                    case GL_MULT:
                        a[k] = to_accum(from_accum(a[k]) * value);
                        break;
                    default:
                        a[k] = to_accum(from_accum(a[k]) + value);
                        break;
                }
            }
        }
    }
}
