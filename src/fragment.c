#include "pipeline.h"

#include <math.h>

GLint rgl_integer_width(const GLfloat width, const GLint max) {
    const GLint w = (GLint) floorf(width + 0.5f);
    return w < 1 ? 1 : w > max ? max : w;
}

static GLboolean is_compare_func(const GLenum func) {
    return func >= GL_NEVER && func <= GL_ALWAYS;
}

static GLboolean compare(const GLenum func, const GLdouble a, const GLdouble b) {
    switch (func) {
        case GL_NEVER:
            return GL_FALSE;
        case GL_LESS:
            return a < b;
        case GL_EQUAL:
            return a == b;
        case GL_LEQUAL:
            return a <= b;
        case GL_GREATER:
            return a > b;
        case GL_NOTEQUAL:
            return a != b;
        case GL_GEQUAL:
            return a >= b;
        default:
            return GL_TRUE;
    }
}

static GLboolean scissor_pass(const rgl_context *ctx, const GLint x, const GLint y) {
    const struct rgl_scissor *s = &ctx->scissor;
    return !ctx->enable.scissor_test ||
           (x >= s->x && x - s->x < s->width && y >= s->y && y - s->y < s->height);
}

static GLuint stencil_ref(const rgl_context *ctx) {
    const GLint ref = ctx->stencil.ref;
    return ref < 0 ? 0u : (GLuint) ref > RGL_STENCIL_MAX ? RGL_STENCIL_MAX : (GLuint) ref;
}

static void stencil_update(rgl_context *ctx, const size_t index, const GLenum op) {
    GLubyte *p = &ctx->fb.stencil[index];
    const GLuint s = *p;
    GLuint value;

    switch (op) {
        case GL_ZERO:
            value = 0;
            break;
        case GL_REPLACE:
            value = stencil_ref(ctx);
            break;
        case GL_INCR:
            value = s < RGL_STENCIL_MAX ? s + 1 : s;
            break;
        case GL_DECR:
            value = s > 0 ? s - 1 : s;
            break;
        case GL_INVERT:
            value = ~s;
            break;
        default:
            return;
    }

    const GLuint mask = ctx->mask.stencil & RGL_STENCIL_MAX;
    *p = (GLubyte) ((s & ~mask) | (value & mask));
}

static void blend_factor(const GLenum factor, const GLfloat *src, const GLfloat *dst, GLfloat out[4]) {
    for (int i = 0; i < 4; i++) {
        switch (factor) {
            case GL_ZERO:
                out[i] = 0.0f;
                break;
            case GL_SRC_COLOR:
                out[i] = src[i];
                break;
            case GL_ONE_MINUS_SRC_COLOR:
                out[i] = 1.0f - src[i];
                break;
            case GL_DST_COLOR:
                out[i] = dst[i];
                break;
            case GL_ONE_MINUS_DST_COLOR:
                out[i] = 1.0f - dst[i];
                break;
            case GL_SRC_ALPHA:
                out[i] = src[3];
                break;
            case GL_ONE_MINUS_SRC_ALPHA:
                out[i] = 1.0f - src[3];
                break;
            case GL_DST_ALPHA:
                out[i] = dst[3];
                break;
            case GL_ONE_MINUS_DST_ALPHA:
                out[i] = 1.0f - dst[3];
                break;
            case GL_SRC_ALPHA_SATURATE:
                out[i] = i == 3 ? 1.0f : src[3] < 1.0f - dst[3] ? src[3] : 1.0f - dst[3];
                break;
            default:
                out[i] = 1.0f;
                break;
        }
    }
}

static void write_color(rgl_context *ctx, const size_t index, const GLfloat *color) {
    GLubyte *p = ctx->fb.color + index * 4;
    GLfloat src[4];
    for (int i = 0; i < 4; i++)
        src[i] = rgl_clampf(color[i], 0.0f, 1.0f);

    if (ctx->enable.blend) {
        GLfloat dst[4];
        GLfloat sf[4];
        GLfloat df[4];
        for (int i = 0; i < 4; i++)
            dst[i] = (GLfloat) p[i] / 255.0f;
        blend_factor(ctx->color_buffer.blend_src, src, dst, sf);
        blend_factor(ctx->color_buffer.blend_dst, src, dst, df);
        for (int i = 0; i < 4; i++)
            src[i] = rgl_clampf(src[i] * sf[i] + dst[i] * df[i], 0.0f, 1.0f);
    }

    for (int i = 0; i < 4; i++) {
        if (ctx->mask.color[i])
            p[i] = (GLubyte) (src[i] * 255.0f + 0.5f);
    }
}

void rgl_write_fragment(rgl_context *ctx, const struct rgl_fragment *frag) {
    const struct rgl_framebuffer *fb = &ctx->fb;
    if (frag->x < 0 || frag->y < 0 || frag->x >= fb->width || frag->y >= fb->height)
        return;
    if (!scissor_pass(ctx, frag->x, frag->y))
        return;
    if (ctx->enable.alpha_test &&
        !compare(ctx->color_buffer.alpha_func, rgl_clampf(frag->color[3], 0.0f, 1.0f), ctx->color_buffer.alpha_ref))
        return;

    const size_t index = (size_t) frag->y * (size_t) fb->width + (size_t) frag->x;

    if (ctx->enable.stencil_test) {
        const GLuint mask = ctx->stencil.value_mask;
        if (!compare(ctx->stencil.func, stencil_ref(ctx) & mask, fb->stencil[index] & mask)) {
            stencil_update(ctx, index, ctx->stencil.fail);
            return;
        }
    }

    if (ctx->enable.depth_test) {
        const GLuint z = (GLuint) (rgl_clampd(frag->z, 0.0, 1.0) * RGL_DEPTH_MAX + 0.5);
        if (!compare(ctx->depth.func, z, fb->depth[index])) {
            if (ctx->enable.stencil_test)
                stencil_update(ctx, index, ctx->stencil.zfail);
            return;
        }
        if (ctx->mask.depth)
            fb->depth[index] = z;
    }

    if (ctx->enable.stencil_test)
        stencil_update(ctx, index, ctx->stencil.zpass);

    if (ctx->color_buffer.draw_buffer != GL_NONE)
        write_color(ctx, index, frag->color);
}

void glAlphaFunc(const GLenum func, const GLclampf ref) {
    GET_CONTEXT(ctx);
    OUTSIDE_BEGIN_END(ctx);

    if (!is_compare_func(func)) {
        rgl_error(ctx, GL_INVALID_ENUM);
        return;
    }
    ctx->color_buffer.alpha_func = func;
    ctx->color_buffer.alpha_ref = rgl_clampf(ref, 0.0f, 1.0f);
}

void glStencilFunc(const GLenum func, const GLint ref, const GLuint mask) {
    GET_CONTEXT(ctx);
    OUTSIDE_BEGIN_END(ctx);

    if (!is_compare_func(func)) {
        rgl_error(ctx, GL_INVALID_ENUM);
        return;
    }
    ctx->stencil.func = func;
    ctx->stencil.ref = ref;
    ctx->stencil.value_mask = mask;
}

static GLboolean is_stencil_op(const GLenum op) {
    return op == GL_KEEP || op == GL_ZERO || op == GL_REPLACE || op == GL_INCR || op == GL_DECR ||
           op == GL_INVERT;
}

void glStencilOp(const GLenum fail, const GLenum zfail, const GLenum zpass) {
    GET_CONTEXT(ctx);
    OUTSIDE_BEGIN_END(ctx);

    if (!is_stencil_op(fail) || !is_stencil_op(zfail) || !is_stencil_op(zpass)) {
        rgl_error(ctx, GL_INVALID_ENUM);
        return;
    }
    ctx->stencil.fail = fail;
    ctx->stencil.zfail = zfail;
    ctx->stencil.zpass = zpass;
}

void glDepthFunc(const GLenum func) {
    GET_CONTEXT(ctx);
    OUTSIDE_BEGIN_END(ctx);

    if (!is_compare_func(func)) {
        rgl_error(ctx, GL_INVALID_ENUM);
        return;
    }
    ctx->depth.func = func;
}

static GLboolean is_blend_common(const GLenum factor) {
    return factor == GL_ZERO || factor == GL_ONE || factor == GL_SRC_ALPHA || factor == GL_ONE_MINUS_SRC_ALPHA ||
           factor == GL_DST_ALPHA || factor == GL_ONE_MINUS_DST_ALPHA;
}

void glBlendFunc(const GLenum sfactor, const GLenum dfactor) {
    GET_CONTEXT(ctx);
    OUTSIDE_BEGIN_END(ctx);

    const GLboolean src_ok = is_blend_common(sfactor) || sfactor == GL_DST_COLOR ||
                             sfactor == GL_ONE_MINUS_DST_COLOR || sfactor == GL_SRC_ALPHA_SATURATE;
    const GLboolean dst_ok = is_blend_common(dfactor) || dfactor == GL_SRC_COLOR || dfactor == GL_ONE_MINUS_SRC_COLOR;
    if (!src_ok || !dst_ok) {
        rgl_error(ctx, GL_INVALID_ENUM);
        return;
    }
    ctx->color_buffer.blend_src = sfactor;
    ctx->color_buffer.blend_dst = dfactor;
}

void glLogicOp(const GLenum opcode) {
    GET_CONTEXT(ctx);
    OUTSIDE_BEGIN_END(ctx);

    if (opcode < GL_CLEAR || opcode > GL_SET) {
        rgl_error(ctx, GL_INVALID_ENUM);
        return;
    }
    ctx->color_buffer.logic_op = opcode;
}
