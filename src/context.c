#include "context.h"

#include <stdlib.h>
#include <string.h>

rgl_context *rgl_current = NULL;

static void init_stack(struct rgl_matrix_stack *stack, const GLint max_depth) {
    rgl_matrix_identity(stack->items[0]);
    stack->depth = 1;
    stack->max_depth = max_depth;
}

static void init_state(rgl_context *ctx) {
    ctx->error = GL_NO_ERROR;
    ctx->in_begin_end = GL_FALSE;

    ctx->enable.dither = GL_TRUE;

    ctx->clear.depth = 1.0;

    for (int i = 0; i < 4; i++)
        ctx->mask.color[i] = GL_TRUE;
    ctx->mask.index = ~0u;
    ctx->mask.depth = GL_TRUE;
    ctx->mask.stencil = ~0u;

    ctx->viewport.width = ctx->fb.width;
    ctx->viewport.height = ctx->fb.height;
    ctx->viewport.z_far = 1.0;

    ctx->scissor.width = ctx->fb.width;
    ctx->scissor.height = ctx->fb.height;

    ctx->transform.matrix_mode = GL_MODELVIEW;
    init_stack(&ctx->transform.modelview, RGL_MAX_MODELVIEW_STACK_DEPTH);
    init_stack(&ctx->transform.projection, RGL_MAX_PROJECTION_STACK_DEPTH);
    init_stack(&ctx->transform.texture, RGL_MAX_TEXTURE_STACK_DEPTH);

    for (int i = 0; i < 4; i++)
        ctx->current.color[i] = 1.0f;
    ctx->current.index = 1.0f;
    ctx->current.normal[2] = 1.0f;
    ctx->current.tex_coord[3] = 1.0f;
    ctx->current.edge_flag = GL_TRUE;

    ctx->point.size = 1.0f;

    ctx->line.width = 1.0f;
    ctx->line.stipple_factor = 1;
    ctx->line.stipple_pattern = 0xffff;

    ctx->polygon.front_mode = GL_FILL;
    ctx->polygon.back_mode = GL_FILL;
    ctx->polygon.cull_face = GL_BACK;
    ctx->polygon.front_face = GL_CCW;
    memset(ctx->polygon.stipple, 0xff, sizeof(ctx->polygon.stipple));

    ctx->lighting.shade_model = GL_SMOOTH;
}

rgl_context *rgl_create_context(const GLsizei width, const GLsizei height) {
    if (width <= 0 || height <= 0 || width > RGL_MAX_VIEWPORT_DIM || height > RGL_MAX_VIEWPORT_DIM)
        return NULL;

    rgl_context *ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL)
        return NULL;

    const size_t pixels = (size_t) width * (size_t) height;
    ctx->fb.width = width;
    ctx->fb.height = height;
    ctx->fb.color = calloc(pixels, 4);
    ctx->fb.depth = calloc(pixels, sizeof(*ctx->fb.depth));
    ctx->fb.stencil = calloc(pixels, 1);
    if (ctx->fb.color == NULL || ctx->fb.depth == NULL || ctx->fb.stencil == NULL) {
        rgl_destroy_context(ctx);
        return NULL;
    }

    init_state(ctx);
    return ctx;
}

void rgl_destroy_context(rgl_context *ctx) {
    if (ctx == NULL)
        return;
    if (rgl_current == ctx)
        rgl_current = NULL;
    free(ctx->fb.color);
    free(ctx->fb.depth);
    free(ctx->fb.stencil);
    free(ctx->primitive.vertices);
    free(ctx->primitive.clip[0]);
    free(ctx->primitive.clip[1]);
    free(ctx);
}

void rgl_make_current(rgl_context *ctx) {
    rgl_current = ctx;
}

rgl_context *rgl_get_current(void) {
    return rgl_current;
}

const GLubyte *rgl_get_color_buffer(const rgl_context *ctx) {
    return ctx->fb.color;
}

void rgl_error(rgl_context *ctx, GLenum error) {
    if (ctx->error == GL_NO_ERROR)
        ctx->error = error;
}

GLfloat rgl_clampf(const GLfloat x, const GLfloat lo, const GLfloat hi) {
    return x < lo ? lo : x > hi ? hi : x;
}

GLdouble rgl_clampd(const GLdouble x, const GLdouble lo, const GLdouble hi) {
    return x < lo ? lo : x > hi ? hi : x;
}
