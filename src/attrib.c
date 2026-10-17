#include "context.h"
#include "list.h"

#include <string.h>

static void save_texture(const struct rgl_texture *tex, struct rgl_texture_params *p) {
    p->min_filter = tex->min_filter;
    p->mag_filter = tex->mag_filter;
    p->wrap_s = tex->wrap_s;
    p->wrap_t = tex->wrap_t;
    memcpy(p->border_color, tex->border_color, sizeof(p->border_color));
}

static void restore_texture(struct rgl_texture *tex, const struct rgl_texture_params *p) {
    tex->min_filter = p->min_filter;
    tex->mag_filter = p->mag_filter;
    tex->wrap_s = p->wrap_s;
    tex->wrap_t = p->wrap_t;
    memcpy(tex->border_color, p->border_color, sizeof(tex->border_color));
    tex->complete_dirty = GL_TRUE;
}

static void save(const rgl_context *ctx, struct rgl_attrib *a) {
    a->enable = ctx->enable;
    a->clear = ctx->clear;
    a->write_mask = ctx->mask;
    a->viewport = ctx->viewport;
    a->scissor = ctx->scissor;
    a->color_buffer = ctx->color_buffer;
    a->depth = ctx->depth;
    a->stencil = ctx->stencil;
    a->read_buffer = ctx->pixel.read_buffer;
    a->transfer = ctx->pixel.transfer;
    a->zoom_x = ctx->pixel.zoom_x;
    a->zoom_y = ctx->pixel.zoom_y;
    a->point = ctx->point;
    a->line = ctx->line;
    a->polygon = ctx->polygon;
    a->lighting = ctx->lighting;
    a->fog = ctx->fog;
    a->current = ctx->current;
    a->raster = ctx->raster;
    a->hints = ctx->hints;
    a->matrix_mode = ctx->transform.matrix_mode;
    memcpy(a->clip_plane, ctx->transform.clip_plane, sizeof(a->clip_plane));
    a->list_base = ctx->list.base;
    a->grid1_un = ctx->eval.grid1_un;
    a->grid1_domain[0] = ctx->eval.grid1_u1;
    a->grid1_domain[1] = ctx->eval.grid1_u2;
    a->grid2_segments[0] = ctx->eval.grid2_un;
    a->grid2_segments[1] = ctx->eval.grid2_vn;
    a->grid2_domain[0] = ctx->eval.grid2_u1;
    a->grid2_domain[1] = ctx->eval.grid2_u2;
    a->grid2_domain[2] = ctx->eval.grid2_v1;
    a->grid2_domain[3] = ctx->eval.grid2_v2;
    save_texture(&ctx->texture.texture_1d, &a->texture_1d);
    save_texture(&ctx->texture.texture_2d, &a->texture_2d);
    a->env_mode = ctx->texture.env_mode;
    memcpy(a->env_color, ctx->texture.env_color, sizeof(a->env_color));
    memcpy(a->gen, ctx->texture.gen, sizeof(a->gen));
}

static void restore(rgl_context *ctx, const struct rgl_attrib *a) {
    const GLbitfield mask = a->mask;
    struct rgl_enable *e = &ctx->enable;
    const struct rgl_enable *s = &a->enable;

    if (mask & GL_CURRENT_BIT) {
        ctx->current = a->current;
        ctx->raster = a->raster;
    }
    if (mask & GL_POINT_BIT) {
        e->point_smooth = s->point_smooth;
        ctx->point = a->point;
    }
    if (mask & GL_LINE_BIT) {
        e->line_smooth = s->line_smooth;
        e->line_stipple = s->line_stipple;
        ctx->line.width = a->line.width;
        ctx->line.stipple_factor = a->line.stipple_factor;
        ctx->line.stipple_pattern = a->line.stipple_pattern;
    }
    if (mask & GL_POLYGON_BIT) {
        e->cull_face = s->cull_face;
        e->polygon_smooth = s->polygon_smooth;
        e->polygon_stipple = s->polygon_stipple;
        ctx->polygon.front_mode = a->polygon.front_mode;
        ctx->polygon.back_mode = a->polygon.back_mode;
        ctx->polygon.cull_face = a->polygon.cull_face;
        ctx->polygon.front_face = a->polygon.front_face;
    }
    if (mask & GL_POLYGON_STIPPLE_BIT)
        memcpy(ctx->polygon.stipple, a->polygon.stipple, sizeof(ctx->polygon.stipple));
    if (mask & GL_PIXEL_MODE_BIT) {
        ctx->pixel.read_buffer = a->read_buffer;
        ctx->pixel.transfer = a->transfer;
        ctx->pixel.zoom_x = a->zoom_x;
        ctx->pixel.zoom_y = a->zoom_y;
    }
    if (mask & GL_LIGHTING_BIT) {
        e->lighting = s->lighting;
        e->color_material = s->color_material;
        memcpy(e->light, s->light, sizeof(e->light));
        ctx->lighting = a->lighting;
    }
    if (mask & GL_FOG_BIT) {
        e->fog = s->fog;
        ctx->fog = a->fog;
    }
    if (mask & GL_DEPTH_BUFFER_BIT) {
        e->depth_test = s->depth_test;
        ctx->depth = a->depth;
        ctx->clear.depth = a->clear.depth;
        ctx->mask.depth = a->write_mask.depth;
    }
    if (mask & GL_ACCUM_BUFFER_BIT)
        memcpy(ctx->clear.accum, a->clear.accum, sizeof(ctx->clear.accum));
    if (mask & GL_STENCIL_BUFFER_BIT) {
        e->stencil_test = s->stencil_test;
        ctx->stencil = a->stencil;
        ctx->clear.stencil = a->clear.stencil;
        ctx->mask.stencil = a->write_mask.stencil;
    }
    if (mask & GL_VIEWPORT_BIT)
        ctx->viewport = a->viewport;
    if (mask & GL_TRANSFORM_BIT) {
        ctx->transform.matrix_mode = a->matrix_mode;
        memcpy(ctx->transform.clip_plane, a->clip_plane, sizeof(ctx->transform.clip_plane));
        memcpy(e->clip_plane, s->clip_plane, sizeof(e->clip_plane));
        e->normalize = s->normalize;
    }
    if (mask & GL_ENABLE_BIT)
        *e = *s;
    if (mask & GL_COLOR_BUFFER_BIT) {
        e->alpha_test = s->alpha_test;
        e->blend = s->blend;
        e->dither = s->dither;
        e->logic_op = s->logic_op;
        ctx->color_buffer = a->color_buffer;
        memcpy(ctx->clear.color, a->clear.color, sizeof(ctx->clear.color));
        ctx->clear.index = a->clear.index;
        memcpy(ctx->mask.color, a->write_mask.color, sizeof(ctx->mask.color));
        ctx->mask.index = a->write_mask.index;
    }
    if (mask & GL_HINT_BIT)
        ctx->hints = a->hints;
    if (mask & GL_EVAL_BIT) {
        memcpy(e->map1, s->map1, sizeof(e->map1));
        memcpy(e->map2, s->map2, sizeof(e->map2));
        e->auto_normal = s->auto_normal;
        ctx->eval.grid1_un = a->grid1_un;
        ctx->eval.grid1_u1 = a->grid1_domain[0];
        ctx->eval.grid1_u2 = a->grid1_domain[1];
        ctx->eval.grid2_un = a->grid2_segments[0];
        ctx->eval.grid2_vn = a->grid2_segments[1];
        ctx->eval.grid2_u1 = a->grid2_domain[0];
        ctx->eval.grid2_u2 = a->grid2_domain[1];
        ctx->eval.grid2_v1 = a->grid2_domain[2];
        ctx->eval.grid2_v2 = a->grid2_domain[3];
    }
    if (mask & GL_LIST_BIT)
        ctx->list.base = a->list_base;
    if (mask & GL_TEXTURE_BIT) {
        e->texture_1d = s->texture_1d;
        e->texture_2d = s->texture_2d;
        memcpy(e->texture_gen, s->texture_gen, sizeof(e->texture_gen));
        restore_texture(&ctx->texture.texture_1d, &a->texture_1d);
        restore_texture(&ctx->texture.texture_2d, &a->texture_2d);
        ctx->texture.env_mode = a->env_mode;
        memcpy(ctx->texture.env_color, a->env_color, sizeof(ctx->texture.env_color));
        memcpy(ctx->texture.gen, a->gen, sizeof(ctx->texture.gen));
    }
    if (mask & GL_SCISSOR_BIT) {
        e->scissor_test = s->scissor_test;
        ctx->scissor = a->scissor;
    }
}

void glPushAttrib(const GLbitfield mask) {
    GET_CONTEXT(ctx);
    if (SAVE(ctx, RGL_OP_PUSH_ATTRIB, {.u = mask}))
        return;
    OUTSIDE_BEGIN_END(ctx);

    if (ctx->attrib_depth == RGL_MAX_ATTRIB_STACK_DEPTH) {
        rgl_error(ctx, GL_STACK_OVERFLOW);
        return;
    }

    struct rgl_attrib *a = &ctx->attrib[ctx->attrib_depth++];
    a->mask = mask;
    save(ctx, a);
}

void glPopAttrib(void) {
    GET_CONTEXT(ctx);
    if (SAVE0(ctx, RGL_OP_POP_ATTRIB))
        return;
    OUTSIDE_BEGIN_END(ctx);

    if (ctx->attrib_depth == 0) {
        rgl_error(ctx, GL_STACK_UNDERFLOW);
        return;
    }
    restore(ctx, &ctx->attrib[--ctx->attrib_depth]);
}

void glHint(const GLenum target, const GLenum mode) {
    GET_CONTEXT(ctx);
    if (SAVE(ctx, RGL_OP_HINT, {.e = target}, {.e = mode}))
        return;
    OUTSIDE_BEGIN_END(ctx);

    if (mode != GL_DONT_CARE && mode != GL_FASTEST && mode != GL_NICEST) {
        rgl_error(ctx, GL_INVALID_ENUM);
        return;
    }

    struct rgl_hints *h = &ctx->hints;
    switch (target) {
        case GL_PERSPECTIVE_CORRECTION_HINT:
            h->perspective_correction = mode;
            break;
        case GL_POINT_SMOOTH_HINT:
            h->point_smooth = mode;
            break;
        case GL_LINE_SMOOTH_HINT:
            h->line_smooth = mode;
            break;
        case GL_POLYGON_SMOOTH_HINT:
            h->polygon_smooth = mode;
            break;
        case GL_FOG_HINT:
            h->fog = mode;
            break;
        default:
            rgl_error(ctx, GL_INVALID_ENUM);
            break;
    }
}
