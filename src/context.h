#ifndef RGL_CONTEXT_H
#define RGL_CONTEXT_H

#include <rgl/rgl.h>

#include <stddef.h>

#define RGL_MAX_LIGHTS 8
#define RGL_MAX_CLIP_PLANES 6
#define RGL_MAX_EVAL_MAPS 9
#define RGL_MAX_VIEWPORT_DIM 4096
#define RGL_MAX_MODELVIEW_STACK_DEPTH 32
#define RGL_MAX_PROJECTION_STACK_DEPTH 2
#define RGL_MAX_TEXTURE_STACK_DEPTH 2

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

struct rgl_matrix_stack {
    GLfloat items[RGL_MAX_MODELVIEW_STACK_DEPTH][16];
    GLint depth;
    GLint max_depth;
};

struct rgl_transform {
    GLenum matrix_mode;
    struct rgl_matrix_stack modelview;
    struct rgl_matrix_stack projection;
    struct rgl_matrix_stack texture;
};

struct rgl_current_values {
    GLfloat color[4];
    GLfloat index;
    GLfloat normal[3];
    GLfloat tex_coord[4];
    GLboolean edge_flag;
};

struct rgl_vertex {
    GLfloat eye[4];
    GLfloat clip[4];
    GLfloat color[4];
    GLfloat tex_coord[4];
    GLfloat normal[3];
    GLboolean edge_flag;
};

struct rgl_primitive {
    GLenum mode;
    struct rgl_vertex *vertices;
    size_t count;
    size_t capacity;
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
    struct rgl_transform transform;
    struct rgl_current_values current;
    struct rgl_primitive primitive;
};

extern rgl_context *rgl_current;

void rgl_error(rgl_context *ctx, GLenum error);

GLfloat rgl_clampf(GLfloat x, GLfloat lo, GLfloat hi);
GLdouble rgl_clampd(GLdouble x, GLdouble lo, GLdouble hi);

void rgl_matrix_identity(GLfloat m[16]);
void rgl_matrix_transform(const GLfloat m[16], const GLfloat in[4], GLfloat out[4]);
GLfloat *rgl_matrix_top(struct rgl_matrix_stack *stack);

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

#endif
