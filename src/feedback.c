#include "pipeline.h"
#include "list.h"

void rgl_reset_line_stipple(rgl_context *ctx) {
    ctx->line.stipple_counter = 0;
    ctx->line.reset_token = GL_TRUE;
}

void rgl_select_hit(rgl_context *ctx, const GLfloat z) {
    struct rgl_select *s = &ctx->select;
    const GLfloat clamped = rgl_clampf(z, 0.0f, 1.0f);
    if (!s->hit) {
        s->hit = GL_TRUE;
        s->min_z = clamped;
        s->max_z = clamped;
        return;
    }
    if (clamped < s->min_z)
        s->min_z = clamped;
    if (clamped > s->max_z)
        s->max_z = clamped;
}

static void select_value(struct rgl_select *s, const GLuint value) {
    if (s->count < s->size)
        s->buffer[s->count++] = value;
    else
        s->overflow = GL_TRUE;
}

static void write_hit(rgl_context *ctx) {
    struct rgl_select *s = &ctx->select;
    if (!s->hit)
        return;

    select_value(s, (GLuint) s->depth);
    select_value(s, (GLuint) ((GLdouble) s->min_z * 4294967295.0 + 0.5));
    select_value(s, (GLuint) ((GLdouble) s->max_z * 4294967295.0 + 0.5));
    for (GLint i = 0; i < s->depth; i++)
        select_value(s, s->names[i]);
    s->hits++;
    s->hit = GL_FALSE;
}

static void feedback_value(rgl_context *ctx, const GLfloat value) {
    struct rgl_feedback *f = &ctx->feedback;
    if (f->count < f->size)
        f->buffer[f->count++] = value;
    else
        f->overflow = GL_TRUE;
}

static void feedback_vertex(rgl_context *ctx, const GLfloat window[3], const GLfloat clip_w, const GLfloat *color,
                            const GLfloat *tex_coord) {
    const GLenum type = ctx->feedback.type;

    feedback_value(ctx, window[0]);
    feedback_value(ctx, window[1]);
    if (type != GL_2D)
        feedback_value(ctx, window[2]);
    if (type == GL_4D_COLOR_TEXTURE)
        feedback_value(ctx, clip_w);
    if (type == GL_3D_COLOR || type == GL_3D_COLOR_TEXTURE || type == GL_4D_COLOR_TEXTURE) {
        for (int k = 0; k < 4; k++)
            feedback_value(ctx, color[k]);
    }
    if (type == GL_3D_COLOR_TEXTURE || type == GL_4D_COLOR_TEXTURE) {
        for (int k = 0; k < 4; k++)
            feedback_value(ctx, tex_coord[k]);
    }
}

static void vertex_out(rgl_context *ctx, const struct rgl_vertex *v, const GLfloat *flat_color) {
    feedback_vertex(ctx, v->window, 1.0f / v->inv_w, flat_color ? flat_color : v->color, v->tex_coord);
}

void rgl_feedback_point(rgl_context *ctx, const struct rgl_vertex *v) {
    if (ctx->render_mode == GL_SELECT) {
        rgl_select_hit(ctx, v->window[2]);
        return;
    }
    feedback_value(ctx, (GLfloat) GL_POINT_TOKEN);
    vertex_out(ctx, v, NULL);
}

void rgl_feedback_line(rgl_context *ctx, const struct rgl_vertex *a, const struct rgl_vertex *b,
                       const GLfloat *flat_color) {
    if (ctx->render_mode == GL_SELECT) {
        rgl_select_hit(ctx, a->window[2]);
        rgl_select_hit(ctx, b->window[2]);
        return;
    }
    feedback_value(ctx, (GLfloat) (ctx->line.reset_token ? GL_LINE_RESET_TOKEN : GL_LINE_TOKEN));
    ctx->line.reset_token = GL_FALSE;
    vertex_out(ctx, a, flat_color);
    vertex_out(ctx, b, flat_color);
}

void rgl_feedback_polygon(rgl_context *ctx, const struct rgl_vertex *vertices, const size_t count,
                          const GLfloat *flat_color, const GLenum mode) {
    if (ctx->render_mode == GL_SELECT) {
        for (size_t i = 0; i < count; i++)
            rgl_select_hit(ctx, vertices[i].window[2]);
        return;
    }

    switch (mode) {
        case GL_POINT:
            for (size_t i = 0; i < count; i++) {
                if (!vertices[i].edge_flag)
                    continue;
                feedback_value(ctx, (GLfloat) GL_POINT_TOKEN);
                vertex_out(ctx, &vertices[i], flat_color);
            }
            break;
        case GL_LINE:
            rgl_reset_line_stipple(ctx);
            for (size_t i = 0; i < count; i++) {
                if (vertices[i].edge_flag)
                    rgl_feedback_line(ctx, &vertices[i], &vertices[(i + 1) % count], flat_color);
            }
            break;
        default:
            feedback_value(ctx, (GLfloat) GL_POLYGON_TOKEN);
            feedback_value(ctx, (GLfloat) count);
            for (size_t i = 0; i < count; i++)
                vertex_out(ctx, &vertices[i], flat_color);
            break;
    }
}

GLboolean rgl_feedback_raster(rgl_context *ctx, const GLenum token) {
    if (ctx->render_mode == GL_RENDER)
        return GL_FALSE;

    const struct rgl_raster_pos *r = &ctx->raster;
    if (!r->valid)
        return GL_TRUE;

    if (ctx->render_mode == GL_SELECT) {
        rgl_select_hit(ctx, r->window[2]);
    } else {
        feedback_value(ctx, (GLfloat) token);
        feedback_vertex(ctx, r->window, r->clip_w, r->color, r->tex_coord);
    }
    return GL_TRUE;
}

GLint glRenderMode(const GLenum mode) {
    GET_CONTEXT_OR(ctx, 0);
    OUTSIDE_BEGIN_END_OR(ctx, 0);

    if (mode != GL_RENDER && mode != GL_SELECT && mode != GL_FEEDBACK) {
        rgl_error(ctx, GL_INVALID_ENUM);
        return 0;
    }
    if ((mode == GL_SELECT && !ctx->select.buffer_set) || (mode == GL_FEEDBACK && !ctx->feedback.buffer_set)) {
        rgl_error(ctx, GL_INVALID_OPERATION);
        return 0;
    }

    GLint result = 0;
    if (ctx->render_mode == GL_SELECT) {
        write_hit(ctx);
        result = ctx->select.overflow ? -1 : ctx->select.hits;
    } else if (ctx->render_mode == GL_FEEDBACK) {
        result = ctx->feedback.overflow ? -1 : ctx->feedback.count;
    }

    if (mode == GL_SELECT) {
        struct rgl_select *s = &ctx->select;
        s->count = 0;
        s->hits = 0;
        s->overflow = GL_FALSE;
        s->hit = GL_FALSE;
        s->depth = 0;
    } else if (mode == GL_FEEDBACK) {
        ctx->feedback.count = 0;
        ctx->feedback.overflow = GL_FALSE;
    }

    ctx->render_mode = mode;
    return result;
}

void glSelectBuffer(const GLsizei size, GLuint *buffer) {
    GET_CONTEXT(ctx);
    OUTSIDE_BEGIN_END(ctx);

    if (size < 0) {
        rgl_error(ctx, GL_INVALID_VALUE);
        return;
    }
    if (ctx->render_mode == GL_SELECT) {
        rgl_error(ctx, GL_INVALID_OPERATION);
        return;
    }
    ctx->select.buffer = buffer;
    ctx->select.size = size;
    ctx->select.buffer_set = GL_TRUE;
}

void glFeedbackBuffer(const GLsizei size, const GLenum type, GLfloat *buffer) {
    GET_CONTEXT(ctx);
    OUTSIDE_BEGIN_END(ctx);

    if (type < GL_2D || type > GL_4D_COLOR_TEXTURE) {
        rgl_error(ctx, GL_INVALID_ENUM);
        return;
    }
    if (size < 0) {
        rgl_error(ctx, GL_INVALID_VALUE);
        return;
    }
    if (ctx->render_mode == GL_FEEDBACK) {
        rgl_error(ctx, GL_INVALID_OPERATION);
        return;
    }
    ctx->feedback.buffer = buffer;
    ctx->feedback.size = size;
    ctx->feedback.type = type;
    ctx->feedback.buffer_set = GL_TRUE;
}

void glPassThrough(const GLfloat token) {
    GET_CONTEXT(ctx);
    if (SAVE(ctx, RGL_OP_PASS_THROUGH, {.f = token}))
        return;
    OUTSIDE_BEGIN_END(ctx);

    if (ctx->render_mode != GL_FEEDBACK)
        return;
    feedback_value(ctx, (GLfloat) GL_PASS_THROUGH_TOKEN);
    feedback_value(ctx, token);
}

void glInitNames(void) {
    GET_CONTEXT(ctx);
    if (SAVE0(ctx, RGL_OP_INIT_NAMES))
        return;
    OUTSIDE_BEGIN_END(ctx);

    if (ctx->render_mode != GL_SELECT)
        return;
    write_hit(ctx);
    ctx->select.depth = 0;
}

void glLoadName(const GLuint name) {
    GET_CONTEXT(ctx);
    if (SAVE(ctx, RGL_OP_LOAD_NAME, {.u = name}))
        return;
    OUTSIDE_BEGIN_END(ctx);

    if (ctx->render_mode != GL_SELECT)
        return;
    if (ctx->select.depth == 0) {
        rgl_error(ctx, GL_INVALID_OPERATION);
        return;
    }
    write_hit(ctx);
    ctx->select.names[ctx->select.depth - 1] = name;
}

void glPushName(const GLuint name) {
    GET_CONTEXT(ctx);
    if (SAVE(ctx, RGL_OP_PUSH_NAME, {.u = name}))
        return;
    OUTSIDE_BEGIN_END(ctx);

    if (ctx->render_mode != GL_SELECT)
        return;
    write_hit(ctx);
    if (ctx->select.depth == RGL_MAX_NAME_STACK_DEPTH) {
        rgl_error(ctx, GL_STACK_OVERFLOW);
        return;
    }
    ctx->select.names[ctx->select.depth++] = name;
}

void glPopName(void) {
    GET_CONTEXT(ctx);
    if (SAVE0(ctx, RGL_OP_POP_NAME))
        return;
    OUTSIDE_BEGIN_END(ctx);

    if (ctx->render_mode != GL_SELECT)
        return;
    write_hit(ctx);
    if (ctx->select.depth == 0) {
        rgl_error(ctx, GL_STACK_UNDERFLOW);
        return;
    }
    ctx->select.depth--;
}
