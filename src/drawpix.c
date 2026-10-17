#include "pipeline.h"
#include "pixel.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static void span(const GLfloat a, const GLfloat b, GLint *first, GLint *last) {
    const GLfloat lo = a < b ? a : b;
    const GLfloat hi = a < b ? b : a;
    *first = (GLint) ceilf(lo - 0.5f);
    *last = (GLint) ceilf(hi - 0.5f);
}

static void draw_groups(rgl_context *ctx, const GLsizei width, const GLsizei height, const GLfloat *rgba,
                        const GLfloat *depth) {
    const struct rgl_raster_pos *r = &ctx->raster;
    const GLfloat zx = ctx->pixel.zoom_x;
    const GLfloat zy = ctx->pixel.zoom_y;
    struct rgl_fragment frag;
    rgl_init_raster_fragment(ctx, &frag);

    for (GLsizei j = 0; j < height; j++) {
        GLint y0;
        GLint y1;
        span(r->window[1] + zy * (GLfloat) j, r->window[1] + zy * (GLfloat) (j + 1), &y0, &y1);

        for (GLsizei i = 0; i < width; i++) {
            GLint x0;
            GLint x1;
            span(r->window[0] + zx * (GLfloat) i, r->window[0] + zx * (GLfloat) (i + 1), &x0, &x1);

            const size_t index = (size_t) j * (size_t) width + (size_t) i;
            if (rgba != NULL)
                memcpy(frag.color, rgba + index * 4, sizeof(frag.color));
            if (depth != NULL)
                frag.z = depth[index];

            for (frag.y = y0; frag.y < y1; frag.y++) {
                for (frag.x = x0; frag.x < x1; frag.x++)
                    rgl_write_fragment(ctx, &frag);
            }
        }
    }
}

static void draw_stencil(rgl_context *ctx, const GLsizei width, const GLsizei height, const GLuint *stencil) {
    const struct rgl_raster_pos *r = &ctx->raster;
    const GLfloat zx = ctx->pixel.zoom_x;
    const GLfloat zy = ctx->pixel.zoom_y;

    for (GLsizei j = 0; j < height; j++) {
        GLint y0;
        GLint y1;
        span(r->window[1] + zy * (GLfloat) j, r->window[1] + zy * (GLfloat) (j + 1), &y0, &y1);

        for (GLsizei i = 0; i < width; i++) {
            GLint x0;
            GLint x1;
            span(r->window[0] + zx * (GLfloat) i, r->window[0] + zx * (GLfloat) (i + 1), &x0, &x1);

            const GLuint value = stencil[(size_t) j * (size_t) width + (size_t) i];
            for (GLint y = y0; y < y1; y++) {
                for (GLint x = x0; x < x1; x++)
                    rgl_write_stencil(ctx, x, y, value);
            }
        }
    }
}

static GLboolean inside(const rgl_context *ctx, const GLint x, const GLint y) {
    return x >= 0 && y >= 0 && x < ctx->fb.width && y < ctx->fb.height;
}

static size_t pixel_index(const rgl_context *ctx, const GLint x, const GLint y) {
    return (size_t) y * (size_t) ctx->fb.width + (size_t) x;
}

static void read_rgba(const rgl_context *ctx, const GLint x, const GLint y, const GLsizei width,
                      const GLsizei height, GLfloat *out) {
    for (GLsizei j = 0; j < height; j++) {
        for (GLsizei i = 0; i < width; i++, out += 4) {
            if (!inside(ctx, x + i, y + j)) {
                memset(out, 0, 4 * sizeof(*out));
                continue;
            }
            const GLubyte *p = ctx->fb.color + pixel_index(ctx, x + i, y + j) * 4;
            for (int k = 0; k < 4; k++)
                out[k] = (GLfloat) p[k] / 255.0f;
        }
    }
}

static void read_depth(const rgl_context *ctx, const GLint x, const GLint y, const GLsizei width,
                       const GLsizei height, GLfloat *out) {
    for (GLsizei j = 0; j < height; j++) {
        for (GLsizei i = 0; i < width; i++, out++) {
            if (!inside(ctx, x + i, y + j)) {
                *out = 0.0f;
                continue;
            }
            *out = (GLfloat) ((GLdouble) ctx->fb.depth[pixel_index(ctx, x + i, y + j)] / RGL_DEPTH_MAX);
        }
    }
}

static void read_stencil(const rgl_context *ctx, const GLint x, const GLint y, const GLsizei width,
                         const GLsizei height, GLuint *out) {
    for (GLsizei j = 0; j < height; j++) {
        for (GLsizei i = 0; i < width; i++, out++) {
            if (!inside(ctx, x + i, y + j)) {
                *out = 0u;
                continue;
            }
            *out = rgl_transfer_stencil(ctx, ctx->fb.stencil[pixel_index(ctx, x + i, y + j)]);
        }
    }
}

static GLboolean valid_format(const GLenum format) {
    return format == GL_COLOR_INDEX || format == GL_STENCIL_INDEX || format == GL_DEPTH_COMPONENT ||
           rgl_is_color_format(format);
}

static GLboolean valid_type(const GLenum format, const GLenum type) {
    if (type == GL_BITMAP)
        return format == GL_COLOR_INDEX || format == GL_STENCIL_INDEX;
    return rgl_is_numeric_type(type);
}

void glDrawPixels(const GLsizei width, const GLsizei height, const GLenum format, const GLenum type,
                  const GLvoid *pixels) {
    GET_CONTEXT(ctx);
    OUTSIDE_BEGIN_END(ctx);

    if (!valid_format(format) || !valid_type(format, type)) {
        rgl_error(ctx, GL_INVALID_ENUM);
        return;
    }
    if (width < 0 || height < 0) {
        rgl_error(ctx, GL_INVALID_VALUE);
        return;
    }
    if (!ctx->raster.valid || width == 0 || height == 0 || pixels == NULL)
        return;

    const size_t count = (size_t) width * (size_t) height;
    if (format == GL_STENCIL_INDEX) {
        GLuint *stencil = malloc(count * sizeof(*stencil));
        if (stencil == NULL) {
            rgl_error(ctx, GL_OUT_OF_MEMORY);
            return;
        }
        rgl_unpack_stencil(ctx, width, height, type, pixels, stencil);
        draw_stencil(ctx, width, height, stencil);
        free(stencil);
    } else if (format == GL_DEPTH_COMPONENT) {
        GLfloat *depth = malloc(count * sizeof(*depth));
        if (depth == NULL) {
            rgl_error(ctx, GL_OUT_OF_MEMORY);
            return;
        }
        rgl_unpack_depth(ctx, width, height, type, pixels, depth);
        draw_groups(ctx, width, height, NULL, depth);
        free(depth);
    } else {
        GLfloat *rgba = malloc(count * 4 * sizeof(*rgba));
        if (rgba == NULL) {
            rgl_error(ctx, GL_OUT_OF_MEMORY);
            return;
        }
        rgl_unpack_rgba(ctx, width, height, format, type, pixels, rgba);
        draw_groups(ctx, width, height, rgba, NULL);
        free(rgba);
    }
}

void glReadPixels(const GLint x, const GLint y, const GLsizei width, const GLsizei height, const GLenum format,
                  const GLenum type, GLvoid *pixels) {
    GET_CONTEXT(ctx);
    OUTSIDE_BEGIN_END(ctx);

    if (!valid_format(format) || !valid_type(format, type)) {
        rgl_error(ctx, GL_INVALID_ENUM);
        return;
    }
    if (width < 0 || height < 0) {
        rgl_error(ctx, GL_INVALID_VALUE);
        return;
    }
    if (format == GL_COLOR_INDEX) {
        rgl_error(ctx, GL_INVALID_OPERATION);
        return;
    }
    if (width == 0 || height == 0)
        return;

    const size_t count = (size_t) width * (size_t) height;
    if (format == GL_STENCIL_INDEX) {
        GLuint *stencil = malloc(count * sizeof(*stencil));
        if (stencil == NULL) {
            rgl_error(ctx, GL_OUT_OF_MEMORY);
            return;
        }
        read_stencil(ctx, x, y, width, height, stencil);
        rgl_pack_index(&ctx->pixel.pack, width, height, type, stencil, pixels);
        free(stencil);
    } else if (format == GL_DEPTH_COMPONENT) {
        GLfloat *depth = malloc(count * sizeof(*depth));
        if (depth == NULL) {
            rgl_error(ctx, GL_OUT_OF_MEMORY);
            return;
        }
        read_depth(ctx, x, y, width, height, depth);
        rgl_transfer_depth(ctx, depth, count);
        rgl_pack_depth(&ctx->pixel.pack, width, height, type, depth, pixels);
        free(depth);
    } else {
        GLfloat *rgba = malloc(count * 4 * sizeof(*rgba));
        if (rgba == NULL) {
            rgl_error(ctx, GL_OUT_OF_MEMORY);
            return;
        }
        read_rgba(ctx, x, y, width, height, rgba);
        rgl_transfer_rgba(ctx, rgba, count);
        rgl_pack_rgba(&ctx->pixel.pack, width, height, format, type, rgba, GL_FALSE, pixels);
        free(rgba);
    }
}

void glCopyPixels(const GLint x, const GLint y, const GLsizei width, const GLsizei height, const GLenum type) {
    GET_CONTEXT(ctx);
    OUTSIDE_BEGIN_END(ctx);

    if (type != GL_COLOR && type != GL_DEPTH && type != GL_STENCIL) {
        rgl_error(ctx, GL_INVALID_ENUM);
        return;
    }
    if (width < 0 || height < 0) {
        rgl_error(ctx, GL_INVALID_VALUE);
        return;
    }
    if (!ctx->raster.valid || width == 0 || height == 0)
        return;

    const size_t count = (size_t) width * (size_t) height;
    if (type == GL_STENCIL) {
        GLuint *stencil = malloc(count * sizeof(*stencil));
        if (stencil == NULL) {
            rgl_error(ctx, GL_OUT_OF_MEMORY);
            return;
        }
        read_stencil(ctx, x, y, width, height, stencil);
        draw_stencil(ctx, width, height, stencil);
        free(stencil);
    } else if (type == GL_DEPTH) {
        GLfloat *depth = malloc(count * sizeof(*depth));
        if (depth == NULL) {
            rgl_error(ctx, GL_OUT_OF_MEMORY);
            return;
        }
        read_depth(ctx, x, y, width, height, depth);
        rgl_transfer_depth(ctx, depth, count);
        draw_groups(ctx, width, height, NULL, depth);
        free(depth);
    } else {
        GLfloat *rgba = malloc(count * 4 * sizeof(*rgba));
        if (rgba == NULL) {
            rgl_error(ctx, GL_OUT_OF_MEMORY);
            return;
        }
        read_rgba(ctx, x, y, width, height, rgba);
        rgl_transfer_rgba(ctx, rgba, count);
        draw_groups(ctx, width, height, rgba, NULL);
        free(rgba);
    }
}
