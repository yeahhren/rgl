#include "pipeline.h"
#include "list.h"

#include <math.h>

void rgl_fog_fragment(const rgl_context *ctx, const struct rgl_fragment *frag, GLfloat color[4]) {
    if (!ctx->enable.fog)
        return;

    const struct rgl_fog *fog = &ctx->fog;
    const GLfloat z = frag->fog_z;
    GLfloat f;
    switch (fog->mode) {
        case GL_LINEAR:
            f = fog->end != fog->start ? (fog->end - z) / (fog->end - fog->start) : 1.0f;
            break;
        case GL_EXP2:
            f = expf(-(fog->density * z) * (fog->density * z));
            break;
        default:
            f = expf(-fog->density * z);
            break;
    }
    f = rgl_clampf(f, 0.0f, 1.0f);

    for (int k = 0; k < 3; k++)
        color[k] = f * color[k] + (1.0f - f) * fog->color[k];
}

void glFogfv(const GLenum pname, const GLfloat *params) {
    GET_CONTEXT(ctx);
    union rgl_list_arg args[5] = {{.e = pname}};
    for (int k = 0; k < (pname == GL_FOG_COLOR ? 4 : 1); k++)
        args[1 + k].f = params[k];
    if (rgl_save(ctx, RGL_OP_FOG, args, 5, NULL))
        return;
    OUTSIDE_BEGIN_END(ctx);

    struct rgl_fog *fog = &ctx->fog;
    switch (pname) {
        case GL_FOG_MODE: {
            const GLenum mode = (GLenum) (GLint) params[0];
            if (mode != GL_LINEAR && mode != GL_EXP && mode != GL_EXP2)
                break;
            fog->mode = mode;
            return;
        }
        case GL_FOG_DENSITY:
            if (params[0] < 0.0f) {
                rgl_error(ctx, GL_INVALID_VALUE);
                return;
            }
            fog->density = params[0];
            return;
        case GL_FOG_START:
            fog->start = params[0];
            return;
        case GL_FOG_END:
            fog->end = params[0];
            return;
        case GL_FOG_INDEX:
            fog->index = params[0];
            return;
        case GL_FOG_COLOR:
            for (int k = 0; k < 4; k++)
                fog->color[k] = rgl_clampf(params[k], 0.0f, 1.0f);
            return;
        default:
            break;
    }
    rgl_error(ctx, GL_INVALID_ENUM);
}

void glFogf(const GLenum pname, const GLfloat param) {
    if (pname == GL_FOG_COLOR) {
        GET_CONTEXT(ctx);
        rgl_error(ctx, GL_INVALID_ENUM);
        return;
    }
    glFogfv(pname, &param);
}

void glFogiv(const GLenum pname, const GLint *params) {
    GLfloat f[4] = {0};
    if (pname == GL_FOG_COLOR) {
        for (int k = 0; k < 4; k++)
            f[k] = rgl_int_to_float(params[k]);
    } else {
        f[0] = (GLfloat) params[0];
    }
    glFogfv(pname, f);
}

void glFogi(const GLenum pname, const GLint param) {
    glFogf(pname, (GLfloat) param);
}
