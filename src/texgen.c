#include "pipeline.h"

#include <math.h>
#include <string.h>

static GLfloat dot4(const GLfloat *a, const GLfloat *b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
}

static void sphere_map(const struct rgl_vertex *v, GLfloat out[2]) {
    GLfloat u[3];
    const GLfloat w = v->eye[3] != 0.0f ? v->eye[3] : 1.0f;
    for (int k = 0; k < 3; k++)
        u[k] = v->eye[k] / w;
    const GLfloat len = sqrtf(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);
    if (len > 0.0f) {
        for (int k = 0; k < 3; k++)
            u[k] /= len;
    }

    const GLfloat *n = v->normal;
    const GLfloat n_dot_u = n[0] * u[0] + n[1] * u[1] + n[2] * u[2];
    GLfloat r[3];
    for (int k = 0; k < 3; k++)
        r[k] = u[k] - 2.0f * n[k] * n_dot_u;

    const GLfloat m = 2.0f * sqrtf(r[0] * r[0] + r[1] * r[1] + (r[2] + 1.0f) * (r[2] + 1.0f));
    out[0] = m > 0.0f ? r[0] / m + 0.5f : 0.5f;
    out[1] = m > 0.0f ? r[1] / m + 0.5f : 0.5f;
}

void rgl_texgen(const rgl_context *ctx, const GLfloat object[4], const struct rgl_vertex *v, GLfloat out[4]) {
    memcpy(out, ctx->current.tex_coord, 4 * sizeof(*out));

    GLfloat sphere[2];
    GLboolean sphere_done = GL_FALSE;
    for (int i = 0; i < 4; i++) {
        if (!ctx->enable.texture_gen[i])
            continue;

        const struct rgl_texgen *g = &ctx->texture.gen[i];
        switch (g->mode) {
            case GL_OBJECT_LINEAR:
                out[i] = dot4(g->object_plane, object);
                break;
            case GL_EYE_LINEAR:
                out[i] = dot4(g->eye_plane, v->eye);
                break;
            default:
                if (!sphere_done) {
                    sphere_map(v, sphere);
                    sphere_done = GL_TRUE;
                }
                if (i < 2)
                    out[i] = sphere[i];
                break;
        }
    }
}

GLboolean rgl_texgen_needs_normal(const rgl_context *ctx) {
    for (int i = 0; i < 2; i++) {
        if (ctx->enable.texture_gen[i] && ctx->texture.gen[i].mode == GL_SPHERE_MAP)
            return GL_TRUE;
    }
    return GL_FALSE;
}

static struct rgl_texgen *texgen_for(rgl_context *ctx, const GLenum coord) {
    if (coord < GL_S || coord > GL_Q)
        return NULL;
    return &ctx->texture.gen[coord - GL_S];
}

static void tex_gen(const GLenum coord, const GLenum pname, const GLdouble *params) {
    GET_CONTEXT(ctx);
    OUTSIDE_BEGIN_END(ctx);

    struct rgl_texgen *g = texgen_for(ctx, coord);
    if (g == NULL) {
        rgl_error(ctx, GL_INVALID_ENUM);
        return;
    }

    switch (pname) {
        case GL_TEXTURE_GEN_MODE: {
            const GLenum mode = (GLenum) (GLint) params[0];
            if (mode != GL_OBJECT_LINEAR && mode != GL_EYE_LINEAR &&
                (mode != GL_SPHERE_MAP || coord == GL_R || coord == GL_Q))
                break;
            g->mode = mode;
            return;
        }
        case GL_OBJECT_PLANE:
            for (int k = 0; k < 4; k++)
                g->object_plane[k] = (GLfloat) params[k];
            return;
        case GL_EYE_PLANE: {
            const GLfloat *inv = rgl_modelview_inverse(ctx);
            for (int j = 0; j < 4; j++) {
                g->eye_plane[j] = (GLfloat) (params[0] * inv[j * 4] + params[1] * inv[j * 4 + 1] +
                                             params[2] * inv[j * 4 + 2] + params[3] * inv[j * 4 + 3]);
            }
            return;
        }
        default:
            break;
    }
    rgl_error(ctx, GL_INVALID_ENUM);
}

static void tex_gen_scalar(const GLenum coord, const GLenum pname, const GLdouble param) {
    if (pname != GL_TEXTURE_GEN_MODE) {
        GET_CONTEXT(ctx);
        rgl_error(ctx, GL_INVALID_ENUM);
        return;
    }
    tex_gen(coord, pname, &param);
}

void glTexGend(const GLenum coord, const GLenum pname, const GLdouble param) {
    tex_gen_scalar(coord, pname, param);
}

void glTexGenf(const GLenum coord, const GLenum pname, const GLfloat param) {
    tex_gen_scalar(coord, pname, param);
}

void glTexGeni(const GLenum coord, const GLenum pname, const GLint param) {
    tex_gen_scalar(coord, pname, param);
}

void glTexGendv(const GLenum coord, const GLenum pname, const GLdouble *params) {
    tex_gen(coord, pname, params);
}

void glTexGenfv(const GLenum coord, const GLenum pname, const GLfloat *params) {
    GLdouble d[4] = {0};
    const int n = pname == GL_TEXTURE_GEN_MODE ? 1 : 4;
    for (int k = 0; k < n; k++)
        d[k] = params[k];
    tex_gen(coord, pname, d);
}

void glTexGeniv(const GLenum coord, const GLenum pname, const GLint *params) {
    GLdouble d[4] = {0};
    const int n = pname == GL_TEXTURE_GEN_MODE ? 1 : 4;
    for (int k = 0; k < n; k++)
        d[k] = params[k];
    tex_gen(coord, pname, d);
}

static int get_tex_gen(const GLenum coord, const GLenum pname, GLdouble out[4]) {
    GET_CONTEXT_OR(ctx, 0);
    OUTSIDE_BEGIN_END_OR(ctx, 0);

    const struct rgl_texgen *g = texgen_for(ctx, coord);
    if (g != NULL) {
        switch (pname) {
            case GL_TEXTURE_GEN_MODE:
                out[0] = g->mode;
                return 1;
            case GL_OBJECT_PLANE:
                for (int k = 0; k < 4; k++)
                    out[k] = g->object_plane[k];
                return 4;
            case GL_EYE_PLANE:
                for (int k = 0; k < 4; k++)
                    out[k] = g->eye_plane[k];
                return 4;
            default:
                break;
        }
    }
    rgl_error(ctx, GL_INVALID_ENUM);
    return 0;
}

void glGetTexGendv(const GLenum coord, const GLenum pname, GLdouble *params) {
    GLdouble values[4];
    const int n = get_tex_gen(coord, pname, values);
    for (int k = 0; k < n; k++)
        params[k] = values[k];
}

void glGetTexGenfv(const GLenum coord, const GLenum pname, GLfloat *params) {
    GLdouble values[4];
    const int n = get_tex_gen(coord, pname, values);
    for (int k = 0; k < n; k++)
        params[k] = (GLfloat) values[k];
}

void glGetTexGeniv(const GLenum coord, const GLenum pname, GLint *params) {
    GLdouble values[4];
    const int n = get_tex_gen(coord, pname, values);
    for (int k = 0; k < n; k++)
        params[k] = (GLint) floor(values[k] + 0.5);
}
