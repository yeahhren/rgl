#ifndef RGL_CONTEXT_H
#define RGL_CONTEXT_H

#include <rgl/rgl.h>

#include <stddef.h>

#define RGL_MAX_LIGHTS 8
#define RGL_MAX_CLIP_PLANES 6
#define RGL_MAX_EVAL_MAPS 9
#define RGL_MAX_VIEWPORT_DIM 4096

#define RGL_DEPTH_BITS 24
#define RGL_STENCIL_BITS 8
#define RGL_DEPTH_MAX ((1u << RGL_DEPTH_BITS) - 1u)
#define RGL_STENCIL_MAX ((1u << RGL_STENCIL_BITS) - 1u)

struct rgl_framebuffer {
    GLsizei width;
    GLsizei height;
    GLubyte *color;
    GLuint *depth;
    GLubyte *stencil;
};

struct rgl_enable {
    GLboolean alpha_test;
    GLboolean auto_normal;
    GLboolean blend;
    GLboolean clip_plane[RGL_MAX_CLIP_PLANES];
    GLboolean color_material;
    GLboolean cull_face;
    GLboolean depth_test;
    GLboolean dither;
    GLboolean fog;
    GLboolean light[RGL_MAX_LIGHTS];
    GLboolean lighting;
    GLboolean line_smooth;
    GLboolean line_stipple;
    GLboolean logic_op;
    GLboolean map1[RGL_MAX_EVAL_MAPS];
    GLboolean map2[RGL_MAX_EVAL_MAPS];
    GLboolean normalize;
    GLboolean point_smooth;
    GLboolean polygon_smooth;
    GLboolean polygon_stipple;
    GLboolean scissor_test;
    GLboolean stencil_test;
    GLboolean texture_1d;
    GLboolean texture_2d;
    GLboolean texture_gen[4];
};

struct rgl_clear {
    GLclampf color[4];
    GLfloat index;
    GLclampd depth;
    GLint stencil;
    GLfloat accum[4];
};

struct rgl_mask {
    GLboolean color[4];
    GLuint index;
    GLboolean depth;
    GLuint stencil;
};

struct rgl_viewport {
    GLint x;
    GLint y;
    GLsizei width;
    GLsizei height;
    GLclampd z_near;
    GLclampd z_far;
};

struct rgl_scissor {
    GLint x;
    GLint y;
    GLsizei width;
    GLsizei height;
};

struct rgl_context {
    GLenum error;
    GLboolean in_begin_end;
    struct rgl_framebuffer fb;
    struct rgl_enable enable;
    struct rgl_clear clear;
    struct rgl_mask mask;
    struct rgl_viewport viewport;
    struct rgl_scissor scissor;
};

extern rgl_context *rgl_current;

void rgl_error(rgl_context *ctx, GLenum error);

#define GET_CONTEXT(ctx) \
    struct rgl_context *ctx = rgl_current; \
    if (ctx == NULL) \
        return

#define GET_CONTEXT_OR(ctx, ret) \
    struct rgl_context *ctx = rgl_current; \
    if (ctx == NULL) \
        return (ret)

#define OUTSIDE_BEGIN_END(ctx) \
    do { \
        if ((ctx)->in_begin_end) { \
            rgl_error((ctx), GL_INVALID_OPERATION); \
            return; \
        } \
    } while (0)

#define OUTSIDE_BEGIN_END_OR(ctx, ret) \
    do { \
        if ((ctx)->in_begin_end) { \
            rgl_error((ctx), GL_INVALID_OPERATION); \
            return (ret); \
        } \
    } while (0)

static GLfloat rgl_clampf(const GLfloat x, const GLfloat lo, const GLfloat hi) {
    return x < lo ? lo : x > hi ? hi : x;
}

static GLdouble rgl_clampd(const GLdouble x, const GLdouble lo, const GLdouble hi) {
    return x < lo ? lo : x > hi ? hi : x;
}

#endif
