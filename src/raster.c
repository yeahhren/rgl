#include "pipeline.h"
#include "list.h"
#include "pixel.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void rgl_init_raster_fragment(const rgl_context *ctx, struct rgl_fragment *frag) {
    const struct rgl_raster_pos *r = &ctx->raster;

    frag->x = 0;
    frag->y = 0;
    frag->z = r->window[2];
    memcpy(frag->color, r->color, sizeof(frag->color));
    memcpy(frag->tex_coord, r->tex_coord, sizeof(frag->tex_coord));
    memset(frag->tex_dx, 0, sizeof(frag->tex_dx));
    memset(frag->tex_dy, 0, sizeof(frag->tex_dy));
    frag->fog_z = r->distance;
}

static void raster_pos(const GLfloat x, const GLfloat y, const GLfloat z, const GLfloat w) {
    GET_CONTEXT(ctx);
    if (SAVE(ctx, RGL_OP_RASTER_POS, {.f = x}, {.f = y}, {.f = z}, {.f = w}))
        return;
    OUTSIDE_BEGIN_END(ctx);

    struct rgl_vertex v;
    const GLfloat object[4] = {x, y, z, w};
    rgl_transform_vertex(ctx, object, &v);

    struct rgl_raster_pos *r = &ctx->raster;
    r->valid = rgl_point_visible(ctx, &v);
    if (!r->valid)
        return;

    rgl_to_window(ctx, &v);
    memcpy(r->window, v.window, sizeof(r->window));
    r->clip_w = v.clip[3];
    r->distance = v.fog_z;
    memcpy(r->color, v.color, sizeof(r->color));
    r->index = ctx->current.index;
    memcpy(r->tex_coord, v.tex_coord, sizeof(r->tex_coord));
    if (ctx->render_mode == GL_SELECT)
        rgl_select_hit(ctx, r->window[2]);
}

void glBitmap(const GLsizei width, const GLsizei height, const GLfloat xorig, const GLfloat yorig,
              const GLfloat xmove, const GLfloat ymove, const GLubyte *bitmap) {
    GET_CONTEXT(ctx);
    if (rgl_list_compiling(ctx)) {
        const union rgl_list_arg args[6] = {{.i = width}, {.i = height}, {.f = xorig}, {.f = yorig}, {.f = xmove},
                                            {.f = ymove}};
        if (rgl_save(ctx, RGL_OP_BITMAP, args, 6,
                     rgl_copy_image(&ctx->pixel.unpack, width, height, GL_COLOR_INDEX, GL_BITMAP, bitmap)))
            return;
    }
    OUTSIDE_BEGIN_END(ctx);

    if (width < 0 || height < 0) {
        rgl_error(ctx, GL_INVALID_VALUE);
        return;
    }

    struct rgl_raster_pos *r = &ctx->raster;
    if (!r->valid)
        return;

    if (!rgl_feedback_raster(ctx, GL_BITMAP_TOKEN) && width > 0 && height > 0 && bitmap != NULL) {
        GLubyte *bits = malloc((size_t) width * (size_t) height);
        if (bits == NULL) {
            rgl_error(ctx, GL_OUT_OF_MEMORY);
            return;
        }
        rgl_unpack_bitmap(&ctx->pixel.unpack, width, height, bitmap, bits);

        struct rgl_fragment frag;
        rgl_init_raster_fragment(ctx, &frag);
        const GLint x0 = (GLint) floorf(r->window[0] - xorig);
        const GLint y0 = (GLint) floorf(r->window[1] - yorig);
        for (GLsizei j = 0; j < height; j++) {
            for (GLsizei i = 0; i < width; i++) {
                if (!bits[(size_t) j * (size_t) width + (size_t) i])
                    continue;
                frag.x = x0 + i;
                frag.y = y0 + j;
                rgl_write_fragment(ctx, &frag);
            }
        }
        free(bits);
    }

    r->window[0] += xmove;
    r->window[1] += ymove;
}

void glRasterPos2d(const GLdouble x, const GLdouble y) {
    raster_pos((GLfloat) x, (GLfloat) y, 0.0f, 1.0f);
}

void glRasterPos2dv(const GLdouble *v) {
    glRasterPos2d(v[0], v[1]);
}

void glRasterPos2f(const GLfloat x, const GLfloat y) {
    raster_pos(x, y, 0.0f, 1.0f);
}

void glRasterPos2fv(const GLfloat *v) {
    glRasterPos2f(v[0], v[1]);
}

void glRasterPos2i(const GLint x, const GLint y) {
    raster_pos((GLfloat) x, (GLfloat) y, 0.0f, 1.0f);
}

void glRasterPos2iv(const GLint *v) {
    glRasterPos2i(v[0], v[1]);
}

void glRasterPos2s(const GLshort x, const GLshort y) {
    raster_pos(x, y, 0.0f, 1.0f);
}

void glRasterPos2sv(const GLshort *v) {
    glRasterPos2s(v[0], v[1]);
}

void glRasterPos3d(const GLdouble x, const GLdouble y, const GLdouble z) {
    raster_pos((GLfloat) x, (GLfloat) y, (GLfloat) z, 1.0f);
}

void glRasterPos3dv(const GLdouble *v) {
    glRasterPos3d(v[0], v[1], v[2]);
}

void glRasterPos3f(const GLfloat x, const GLfloat y, const GLfloat z) {
    raster_pos(x, y, z, 1.0f);
}

void glRasterPos3fv(const GLfloat *v) {
    glRasterPos3f(v[0], v[1], v[2]);
}

void glRasterPos3i(const GLint x, const GLint y, const GLint z) {
    raster_pos((GLfloat) x, (GLfloat) y, (GLfloat) z, 1.0f);
}

void glRasterPos3iv(const GLint *v) {
    glRasterPos3i(v[0], v[1], v[2]);
}

void glRasterPos3s(const GLshort x, const GLshort y, const GLshort z) {
    raster_pos(x, y, z, 1.0f);
}

void glRasterPos3sv(const GLshort *v) {
    glRasterPos3s(v[0], v[1], v[2]);
}

void glRasterPos4d(const GLdouble x, const GLdouble y, const GLdouble z, const GLdouble w) {
    raster_pos((GLfloat) x, (GLfloat) y, (GLfloat) z, (GLfloat) w);
}

void glRasterPos4dv(const GLdouble *v) {
    glRasterPos4d(v[0], v[1], v[2], v[3]);
}

void glRasterPos4f(const GLfloat x, const GLfloat y, const GLfloat z, const GLfloat w) {
    raster_pos(x, y, z, w);
}

void glRasterPos4fv(const GLfloat *v) {
    glRasterPos4f(v[0], v[1], v[2], v[3]);
}

void glRasterPos4i(const GLint x, const GLint y, const GLint z, const GLint w) {
    raster_pos((GLfloat) x, (GLfloat) y, (GLfloat) z, (GLfloat) w);
}

void glRasterPos4iv(const GLint *v) {
    glRasterPos4i(v[0], v[1], v[2], v[3]);
}

void glRasterPos4s(const GLshort x, const GLshort y, const GLshort z, const GLshort w) {
    raster_pos(x, y, z, w);
}

void glRasterPos4sv(const GLshort *v) {
    glRasterPos4s(v[0], v[1], v[2], v[3]);
}
