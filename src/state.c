#include "context.h"
#include "list.h"

GLboolean *rgl_cap_flag(rgl_context *ctx, const GLenum cap) {
    struct rgl_enable *e = &ctx->enable;

    if (cap >= GL_CLIP_PLANE0 && cap < GL_CLIP_PLANE0 + RGL_MAX_CLIP_PLANES)
        return &e->clip_plane[cap - GL_CLIP_PLANE0];
    if (cap >= GL_LIGHT0 && cap < GL_LIGHT0 + RGL_MAX_LIGHTS)
        return &e->light[cap - GL_LIGHT0];
    if (cap >= GL_MAP1_COLOR_4 && cap <= GL_MAP1_VERTEX_4)
        return &e->map1[cap - GL_MAP1_COLOR_4];
    if (cap >= GL_MAP2_COLOR_4 && cap <= GL_MAP2_VERTEX_4)
        return &e->map2[cap - GL_MAP2_COLOR_4];
    if (cap >= GL_TEXTURE_GEN_S && cap <= GL_TEXTURE_GEN_Q)
        return &e->texture_gen[cap - GL_TEXTURE_GEN_S];

    switch (cap) {
        case GL_ALPHA_TEST:
            return &e->alpha_test;
        case GL_AUTO_NORMAL:
            return &e->auto_normal;
        case GL_BLEND:
            return &e->blend;
        case GL_COLOR_MATERIAL:
            return &e->color_material;
        case GL_CULL_FACE:
            return &e->cull_face;
        case GL_DEPTH_TEST:
            return &e->depth_test;
        case GL_DITHER:
            return &e->dither;
        case GL_FOG:
            return &e->fog;
        case GL_LIGHTING:
            return &e->lighting;
        case GL_LINE_SMOOTH:
            return &e->line_smooth;
        case GL_LINE_STIPPLE:
            return &e->line_stipple;
        case GL_LOGIC_OP:
            return &e->logic_op;
        case GL_NORMALIZE:
            return &e->normalize;
        case GL_POINT_SMOOTH:
            return &e->point_smooth;
        case GL_POLYGON_SMOOTH:
            return &e->polygon_smooth;
        case GL_POLYGON_STIPPLE:
            return &e->polygon_stipple;
        case GL_SCISSOR_TEST:
            return &e->scissor_test;
        case GL_STENCIL_TEST:
            return &e->stencil_test;
        case GL_TEXTURE_1D:
            return &e->texture_1d;
        case GL_TEXTURE_2D:
            return &e->texture_2d;
        default:
            return NULL;
    }
}

static void set_cap(const GLenum cap, const GLboolean value) {
    GET_CONTEXT(ctx);
    if (SAVE(ctx, value ? RGL_OP_ENABLE : RGL_OP_DISABLE, {.e = cap}))
        return;
    OUTSIDE_BEGIN_END(ctx);

    GLboolean *flag = rgl_cap_flag(ctx, cap);
    if (flag == NULL) {
        rgl_error(ctx, GL_INVALID_ENUM);
        return;
    }
    *flag = value;
    if (cap == GL_COLOR_MATERIAL)
        rgl_update_color_material(ctx);
}

void glEnable(const GLenum cap) {
    set_cap(cap, GL_TRUE);
}

void glDisable(const GLenum cap) {
    set_cap(cap, GL_FALSE);
}

GLboolean glIsEnabled(const GLenum cap) {
    GET_CONTEXT_OR(ctx, GL_FALSE);
    OUTSIDE_BEGIN_END_OR(ctx, GL_FALSE);

    const GLboolean *flag = rgl_cap_flag(ctx, cap);
    if (flag == NULL) {
        rgl_error(ctx, GL_INVALID_ENUM);
        return GL_FALSE;
    }
    return *flag;
}

GLenum glGetError(void) {
    GET_CONTEXT_OR(ctx, GL_NO_ERROR);
    OUTSIDE_BEGIN_END_OR(ctx, GL_NO_ERROR);

    const GLenum error = ctx->error;
    ctx->error = GL_NO_ERROR;
    return error;
}

const GLubyte *glGetString(const GLenum name) {
    GET_CONTEXT_OR(ctx, NULL);
    OUTSIDE_BEGIN_END_OR(ctx, NULL);

    switch (name) {
        case GL_VENDOR:
            return (const GLubyte *) "rgl";
        case GL_RENDERER:
            return (const GLubyte *) "rgl software rasterizer";
        case GL_VERSION:
            return (const GLubyte *) "1.0 rgl";
        case GL_EXTENSIONS:
            return (const GLubyte *) "";
        default:
            rgl_error(ctx, GL_INVALID_ENUM);
            return NULL;
    }
}

void glFlush(void) {
    GET_CONTEXT(ctx);
    OUTSIDE_BEGIN_END(ctx);
}

void glFinish(void) {
    GET_CONTEXT(ctx);
    OUTSIDE_BEGIN_END(ctx);
}

void glViewport(const GLint x, const GLint y, const GLsizei width, const GLsizei height) {
    GET_CONTEXT(ctx);
    if (SAVE(ctx, RGL_OP_VIEWPORT, {.i = x}, {.i = y}, {.i = width}, {.i = height}))
        return;
    OUTSIDE_BEGIN_END(ctx);

    if (width < 0 || height < 0) {
        rgl_error(ctx, GL_INVALID_VALUE);
        return;
    }
    ctx->viewport.x = x;
    ctx->viewport.y = y;
    ctx->viewport.width = width < RGL_MAX_VIEWPORT_DIM ? width : RGL_MAX_VIEWPORT_DIM;
    ctx->viewport.height = height < RGL_MAX_VIEWPORT_DIM ? height : RGL_MAX_VIEWPORT_DIM;
}

void glDepthRange(const GLclampd zNear, const GLclampd zFar) {
    GET_CONTEXT(ctx);
    if (SAVE(ctx, RGL_OP_DEPTH_RANGE, {.d = zNear}, {.d = zFar}))
        return;
    OUTSIDE_BEGIN_END(ctx);

    ctx->viewport.z_near = rgl_clampd(zNear, 0.0, 1.0);
    ctx->viewport.z_far = rgl_clampd(zFar, 0.0, 1.0);
}

void glScissor(const GLint x, const GLint y, const GLsizei width, const GLsizei height) {
    GET_CONTEXT(ctx);
    if (SAVE(ctx, RGL_OP_SCISSOR, {.i = x}, {.i = y}, {.i = width}, {.i = height}))
        return;
    OUTSIDE_BEGIN_END(ctx);

    if (width < 0 || height < 0) {
        rgl_error(ctx, GL_INVALID_VALUE);
        return;
    }
    ctx->scissor.x = x;
    ctx->scissor.y = y;
    ctx->scissor.width = width;
    ctx->scissor.height = height;
}

void glColorMask(const GLboolean red, const GLboolean green, const GLboolean blue, const GLboolean alpha) {
    GET_CONTEXT(ctx);
    if (SAVE(ctx, RGL_OP_COLOR_MASK, {.i = red}, {.i = green}, {.i = blue}, {.i = alpha}))
        return;
    OUTSIDE_BEGIN_END(ctx);

    ctx->mask.color[0] = red ? GL_TRUE : GL_FALSE;
    ctx->mask.color[1] = green ? GL_TRUE : GL_FALSE;
    ctx->mask.color[2] = blue ? GL_TRUE : GL_FALSE;
    ctx->mask.color[3] = alpha ? GL_TRUE : GL_FALSE;
}

void glIndexMask(const GLuint mask) {
    GET_CONTEXT(ctx);
    if (SAVE(ctx, RGL_OP_INDEX_MASK, {.u = mask}))
        return;
    OUTSIDE_BEGIN_END(ctx);

    ctx->mask.index = mask;
}

void glDepthMask(const GLboolean flag) {
    GET_CONTEXT(ctx);
    if (SAVE(ctx, RGL_OP_DEPTH_MASK, {.i = flag}))
        return;
    OUTSIDE_BEGIN_END(ctx);

    ctx->mask.depth = flag ? GL_TRUE : GL_FALSE;
}

void glStencilMask(const GLuint mask) {
    GET_CONTEXT(ctx);
    if (SAVE(ctx, RGL_OP_STENCIL_MASK, {.u = mask}))
        return;
    OUTSIDE_BEGIN_END(ctx);

    ctx->mask.stencil = mask;
}
