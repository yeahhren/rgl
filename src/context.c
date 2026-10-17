#include "context.h"

#include <math.h>
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

    ctx->color_buffer.alpha_func = GL_ALWAYS;
    ctx->color_buffer.blend_src = GL_ONE;
    ctx->color_buffer.blend_dst = GL_ZERO;
    ctx->color_buffer.logic_op = GL_COPY;
    ctx->color_buffer.draw_buffer = GL_FRONT;

    ctx->depth.func = GL_LESS;

    ctx->stencil.func = GL_ALWAYS;
    ctx->stencil.value_mask = ~0u;
    ctx->stencil.fail = GL_KEEP;
    ctx->stencil.zfail = GL_KEEP;
    ctx->stencil.zpass = GL_KEEP;

    ctx->pixel.read_buffer = GL_FRONT;
    ctx->pixel.pack.alignment = 4;
    ctx->pixel.unpack.alignment = 4;
    for (int i = 0; i < 4; i++)
        ctx->pixel.transfer.scale[i] = 1.0f;
    ctx->pixel.transfer.depth_scale = 1.0f;
    for (int i = 0; i < RGL_PIXEL_MAPS; i++)
        ctx->pixel.maps[i].size = 1;
    ctx->pixel.zoom_x = 1.0f;
    ctx->pixel.zoom_y = 1.0f;

    ctx->raster.clip_w = 1.0f;
    for (int i = 0; i < 4; i++)
        ctx->raster.color[i] = 1.0f;
    ctx->raster.index = 1.0f;
    ctx->raster.tex_coord[3] = 1.0f;
    ctx->raster.valid = GL_TRUE;

    ctx->transform.matrix_mode = GL_MODELVIEW;
    init_stack(&ctx->transform.modelview, RGL_MAX_MODELVIEW_STACK_DEPTH);
    init_stack(&ctx->transform.projection, RGL_MAX_PROJECTION_STACK_DEPTH);
    init_stack(&ctx->transform.texture, RGL_MAX_TEXTURE_STACK_DEPTH);
    ctx->transform.modelview_inverse_dirty = GL_TRUE;

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

    rgl_init_lighting(ctx);
    rgl_init_texture(ctx);

    ctx->fog.mode = GL_EXP;
    ctx->fog.density = 1.0f;
    ctx->fog.end = 1.0f;
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
    rgl_free_texture(ctx);
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

GLfloat rgl_int_to_float(const GLint c) {
    return (GLfloat) ((2.0 * (GLdouble) c + 1.0) / 4294967295.0);
}

GLint rgl_float_to_int(const GLfloat c) {
    return (GLint) floor((4294967295.0 * c - 1.0) / 2.0 + 0.5);
}
