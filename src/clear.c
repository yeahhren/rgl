#include "context.h"
#include "list.h"

#include <math.h>
#include <string.h>

#define CLEAR_BITS (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_ACCUM_BUFFER_BIT)

struct rgl_rect rgl_scissor_rect(const rgl_context *ctx) {
    struct rgl_rect r = {.x0 = 0, .y0 = 0, .x1 = ctx->fb.width, .y1 = ctx->fb.height};

    if (ctx->enable.scissor_test) {
        const struct rgl_scissor *s = &ctx->scissor;
        if (s->x > r.x0)
            r.x0 = s->x;
        if (s->y > r.y0)
            r.y0 = s->y;
        if (s->x + s->width < r.x1)
            r.x1 = s->x + s->width;
        if (s->y + s->height < r.y1)
            r.y1 = s->y + s->height;
    }
    return r;
}

static void clear_color(const rgl_context *ctx, const struct rgl_rect r) {
    GLubyte value[4];
    for (int i = 0; i < 4; i++)
        value[i] = (GLubyte) (ctx->clear.color[i] * 255.0f + 0.5f);

    for (GLint y = r.y0; y < r.y1; y++) {
        GLubyte *p = ctx->fb.color + ((size_t) y * (size_t) ctx->fb.width + (size_t) r.x0) * 4;
        for (GLint x = r.x0; x < r.x1; x++, p += 4) {
            for (int i = 0; i < 4; i++) {
                if (ctx->mask.color[i])
                    p[i] = value[i];
            }
        }
    }
}

static void clear_depth(const rgl_context *ctx, const struct rgl_rect r) {
    if (!ctx->mask.depth)
        return;

    const GLuint value = (GLuint) (ctx->clear.depth * RGL_DEPTH_MAX + 0.5);

    for (GLint y = r.y0; y < r.y1; y++) {
        GLuint *p = ctx->fb.depth + (size_t) y * (size_t) ctx->fb.width;
        for (GLint x = r.x0; x < r.x1; x++)
            p[x] = value;
    }
}

static void clear_accum(const rgl_context *ctx, const struct rgl_rect r) {
    GLshort value[4];
    for (int i = 0; i < 4; i++)
        value[i] = (GLshort) floorf(ctx->clear.accum[i] * RGL_ACCUM_MAX + 0.5f);

    for (GLint y = r.y0; y < r.y1; y++) {
        GLshort *p = ctx->fb.accum + ((size_t) y * (size_t) ctx->fb.width + (size_t) r.x0) * 4;
        for (GLint x = r.x0; x < r.x1; x++, p += 4)
            memcpy(p, value, sizeof(value));
    }
}

static void clear_stencil(const rgl_context *ctx, const struct rgl_rect r) {
    const GLubyte mask = (GLubyte) (ctx->mask.stencil & RGL_STENCIL_MAX);
    const GLubyte value = (GLubyte) ((GLuint) ctx->clear.stencil & mask);

    for (GLint y = r.y0; y < r.y1; y++) {
        GLubyte *p = ctx->fb.stencil + (size_t) y * (size_t) ctx->fb.width;
        for (GLint x = r.x0; x < r.x1; x++)
            p[x] = (GLubyte) ((p[x] & ~mask) | value);
    }
}

void glClear(const GLbitfield mask) {
    GET_CONTEXT(ctx);
    if (SAVE(ctx, RGL_OP_CLEAR, {.u = mask}))
        return;
    OUTSIDE_BEGIN_END(ctx);

    if (mask & ~CLEAR_BITS) {
        rgl_error(ctx, GL_INVALID_VALUE);
        return;
    }

    const struct rgl_rect r = rgl_scissor_rect(ctx);
    if (r.x0 >= r.x1 || r.y0 >= r.y1)
        return;

    if (mask & GL_COLOR_BUFFER_BIT && ctx->color_buffer.draw_buffer != GL_NONE)
        clear_color(ctx, r);
    if (mask & GL_DEPTH_BUFFER_BIT)
        clear_depth(ctx, r);
    if (mask & GL_STENCIL_BUFFER_BIT)
        clear_stencil(ctx, r);
    if (mask & GL_ACCUM_BUFFER_BIT)
        clear_accum(ctx, r);
}

void glClearColor(const GLclampf red, const GLclampf green, const GLclampf blue, const GLclampf alpha) {
    GET_CONTEXT(ctx);
    if (SAVE(ctx, RGL_OP_CLEAR_COLOR, {.f = red}, {.f = green}, {.f = blue}, {.f = alpha}))
        return;
    OUTSIDE_BEGIN_END(ctx);

    ctx->clear.color[0] = rgl_clampf(red, 0.0f, 1.0f);
    ctx->clear.color[1] = rgl_clampf(green, 0.0f, 1.0f);
    ctx->clear.color[2] = rgl_clampf(blue, 0.0f, 1.0f);
    ctx->clear.color[3] = rgl_clampf(alpha, 0.0f, 1.0f);
}

void glClearIndex(const GLfloat c) {
    GET_CONTEXT(ctx);
    if (SAVE(ctx, RGL_OP_CLEAR_INDEX, {.f = c}))
        return;
    OUTSIDE_BEGIN_END(ctx);

    ctx->clear.index = c;
}

void glClearDepth(const GLclampd depth) {
    GET_CONTEXT(ctx);
    if (SAVE(ctx, RGL_OP_CLEAR_DEPTH, {.d = depth}))
        return;
    OUTSIDE_BEGIN_END(ctx);

    ctx->clear.depth = rgl_clampd(depth, 0.0, 1.0);
}

void glClearStencil(const GLint s) {
    GET_CONTEXT(ctx);
    if (SAVE(ctx, RGL_OP_CLEAR_STENCIL, {.i = s}))
        return;
    OUTSIDE_BEGIN_END(ctx);

    ctx->clear.stencil = s;
}

void glClearAccum(const GLfloat red, const GLfloat green, const GLfloat blue, const GLfloat alpha) {
    GET_CONTEXT(ctx);
    if (SAVE(ctx, RGL_OP_CLEAR_ACCUM, {.f = red}, {.f = green}, {.f = blue}, {.f = alpha}))
        return;
    OUTSIDE_BEGIN_END(ctx);

    ctx->clear.accum[0] = rgl_clampf(red, -1.0f, 1.0f);
    ctx->clear.accum[1] = rgl_clampf(green, -1.0f, 1.0f);
    ctx->clear.accum[2] = rgl_clampf(blue, -1.0f, 1.0f);
    ctx->clear.accum[3] = rgl_clampf(alpha, -1.0f, 1.0f);
}
