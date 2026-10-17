#include "pixel.h"

#include <math.h>
#include <string.h>

GLboolean rgl_is_color_format(const GLenum format) {
    return format >= GL_RED && format <= GL_LUMINANCE_ALPHA;
}

GLboolean rgl_is_numeric_type(const GLenum type) {
    return type >= GL_BYTE && type <= GL_FLOAT;
}

static int format_components(const GLenum format) {
    switch (format) {
        case GL_LUMINANCE_ALPHA:
            return 2;
        case GL_RGB:
            return 3;
        case GL_RGBA:
            return 4;
        default:
            return 1;
    }
}

static size_t type_size(const GLenum type) {
    switch (type) {
        case GL_BYTE:
        case GL_UNSIGNED_BYTE:
            return 1;
        case GL_SHORT:
        case GL_UNSIGNED_SHORT:
            return 2;
        default:
            return 4;
    }
}

static size_t row_stride(const struct rgl_pixel_store *store, const GLsizei width, const int components,
                         const size_t size) {
    const size_t length = (size_t) (store->row_length > 0 ? store->row_length : width);
    const size_t alignment = (size_t) store->alignment;
    const size_t bytes = size * (size_t) components * length;
    if (size >= alignment)
        return bytes;
    return alignment * ((bytes + alignment - 1) / alignment);
}

static void swap_bytes(GLubyte *b, const size_t size) {
    for (size_t i = 0; i < size / 2; i++) {
        const GLubyte t = b[i];
        b[i] = b[size - 1 - i];
        b[size - 1 - i] = t;
    }
}

static GLfloat read_component(const GLubyte *p, const GLenum type, const GLboolean swap) {
    GLubyte b[4];
    const size_t size = type_size(type);
    memcpy(b, p, size);
    if (swap)
        swap_bytes(b, size);

    switch (type) {
        case GL_UNSIGNED_BYTE:
            return (GLfloat) b[0] / 255.0f;
        case GL_BYTE:
            return (2.0f * (GLfloat) (GLbyte) b[0] + 1.0f) / 255.0f;
        case GL_UNSIGNED_SHORT: {
            GLushort v;
            memcpy(&v, b, sizeof(v));
            return (GLfloat) v / 65535.0f;
        }
        case GL_SHORT: {
            GLshort v;
            memcpy(&v, b, sizeof(v));
            return (2.0f * (GLfloat) v + 1.0f) / 65535.0f;
        }
        case GL_UNSIGNED_INT: {
            GLuint v;
            memcpy(&v, b, sizeof(v));
            return (GLfloat) ((GLdouble) v / 4294967295.0);
        }
        case GL_INT: {
            GLint v;
            memcpy(&v, b, sizeof(v));
            return rgl_int_to_float(v);
        }
        default: {
            GLfloat v;
            memcpy(&v, b, sizeof(v));
            return v;
        }
    }
}

static void write_component(GLubyte *p, const GLenum type, const GLfloat c, const GLboolean swap) {
    GLubyte b[4];
    const size_t size = type_size(type);

    switch (type) {
        case GL_UNSIGNED_BYTE:
            b[0] = (GLubyte) (c * 255.0f + 0.5f);
            break;
        case GL_BYTE:
            b[0] = (GLubyte) (GLbyte) floorf((255.0f * c - 1.0f) / 2.0f + 0.5f);
            break;
        case GL_UNSIGNED_SHORT: {
            const GLushort v = (GLushort) (c * 65535.0f + 0.5f);
            memcpy(b, &v, sizeof(v));
            break;
        }
        case GL_SHORT: {
            const GLshort v = (GLshort) floorf((65535.0f * c - 1.0f) / 2.0f + 0.5f);
            memcpy(b, &v, sizeof(v));
            break;
        }
        case GL_UNSIGNED_INT: {
            const GLuint v = (GLuint) ((GLdouble) c * 4294967295.0 + 0.5);
            memcpy(b, &v, sizeof(v));
            break;
        }
        case GL_INT: {
            const GLint v = rgl_float_to_int(c);
            memcpy(b, &v, sizeof(v));
            break;
        }
        default:
            memcpy(b, &c, sizeof(c));
            break;
    }

    if (swap)
        swap_bytes(b, size);
    memcpy(p, b, size);
}

/* color index pixels go through the index maps, which default to a single zero entry */
void rgl_unpack_rgba(const struct rgl_pixel_store *store, const GLsizei width, const GLsizei height,
                     const GLenum format, const GLenum type, const GLvoid *pixels, GLfloat *rgba) {
    if (pixels == NULL || format == GL_COLOR_INDEX) {
        memset(rgba, 0, (size_t) width * (size_t) height * 4 * sizeof(*rgba));
        return;
    }

    const int n = format_components(format);
    const size_t size = type_size(type);
    const size_t stride = row_stride(store, width, n, size);
    const GLubyte *base = (const GLubyte *) pixels + (size_t) store->skip_rows * stride +
                          (size_t) store->skip_pixels * (size_t) n * size;

    for (GLsizei y = 0; y < height; y++) {
        for (GLsizei x = 0; x < width; x++) {
            const GLubyte *p = base + (size_t) y * stride + (size_t) x * (size_t) n * size;
            GLfloat c[4];
            for (int k = 0; k < n; k++)
                c[k] = read_component(p + (size_t) k * size, type, store->swap_bytes);

            GLfloat *out = rgba + ((size_t) y * (size_t) width + (size_t) x) * 4;
            out[0] = 0.0f;
            out[1] = 0.0f;
            out[2] = 0.0f;
            out[3] = 1.0f;
            switch (format) {
                case GL_RED:
                    out[0] = c[0];
                    break;
                case GL_GREEN:
                    out[1] = c[0];
                    break;
                case GL_BLUE:
                    out[2] = c[0];
                    break;
                case GL_ALPHA:
                    out[3] = c[0];
                    break;
                case GL_LUMINANCE:
                    out[0] = out[1] = out[2] = c[0];
                    break;
                case GL_LUMINANCE_ALPHA:
                    out[0] = out[1] = out[2] = c[0];
                    out[3] = c[1];
                    break;
                case GL_RGB:
                    memcpy(out, c, 3 * sizeof(*c));
                    break;
                default:
                    memcpy(out, c, 4 * sizeof(*c));
                    break;
            }
            for (int k = 0; k < 4; k++)
                out[k] = rgl_clampf(out[k], 0.0f, 1.0f);
        }
    }
}

void rgl_pack_rgba(const struct rgl_pixel_store *store, const GLsizei width, const GLsizei height,
                   const GLenum format, const GLenum type, const GLfloat *rgba, const GLboolean luminance_from_red,
                   GLvoid *pixels) {
    const int n = format_components(format);
    const size_t size = type_size(type);
    const size_t stride = row_stride(store, width, n, size);
    GLubyte *base = (GLubyte *) pixels + (size_t) store->skip_rows * stride +
                    (size_t) store->skip_pixels * (size_t) n * size;

    for (GLsizei y = 0; y < height; y++) {
        for (GLsizei x = 0; x < width; x++) {
            const GLfloat *in = rgba + ((size_t) y * (size_t) width + (size_t) x) * 4;
            const GLfloat luminance = luminance_from_red ? in[0] : rgl_clampf(in[0] + in[1] + in[2], 0.0f, 1.0f);
            GLfloat c[4];
            switch (format) {
                case GL_RED:
                    c[0] = in[0];
                    break;
                case GL_GREEN:
                    c[0] = in[1];
                    break;
                case GL_BLUE:
                    c[0] = in[2];
                    break;
                case GL_ALPHA:
                    c[0] = in[3];
                    break;
                case GL_LUMINANCE:
                    c[0] = luminance;
                    break;
                case GL_LUMINANCE_ALPHA:
                    c[0] = luminance;
                    c[1] = in[3];
                    break;
                default:
                    memcpy(c, in, 4 * sizeof(*c));
                    break;
            }

            GLubyte *p = base + (size_t) y * stride + (size_t) x * (size_t) n * size;
            for (int k = 0; k < n; k++)
                write_component(p + (size_t) k * size, type, rgl_clampf(c[k], 0.0f, 1.0f), store->swap_bytes);
        }
    }
}

static struct rgl_pixel_store *store_for(rgl_context *ctx, const GLenum pname) {
    return pname >= GL_PACK_SWAP_BYTES && pname <= GL_PACK_ALIGNMENT ? &ctx->pixel.pack : &ctx->pixel.unpack;
}

void glPixelStorei(const GLenum pname, const GLint param) {
    GET_CONTEXT(ctx);
    OUTSIDE_BEGIN_END(ctx);

    if ((pname < GL_UNPACK_SWAP_BYTES || pname > GL_UNPACK_ALIGNMENT) &&
        (pname < GL_PACK_SWAP_BYTES || pname > GL_PACK_ALIGNMENT)) {
        rgl_error(ctx, GL_INVALID_ENUM);
        return;
    }

    struct rgl_pixel_store *store = store_for(ctx, pname);
    switch (pname) {
        case GL_UNPACK_SWAP_BYTES:
        case GL_PACK_SWAP_BYTES:
            store->swap_bytes = param != 0;
            return;
        case GL_UNPACK_LSB_FIRST:
        case GL_PACK_LSB_FIRST:
            store->lsb_first = param != 0;
            return;
        case GL_UNPACK_ALIGNMENT:
        case GL_PACK_ALIGNMENT:
            if (param != 1 && param != 2 && param != 4 && param != 8) {
                rgl_error(ctx, GL_INVALID_VALUE);
                return;
            }
            store->alignment = param;
            return;
        default:
            break;
    }

    if (param < 0) {
        rgl_error(ctx, GL_INVALID_VALUE);
        return;
    }
    switch (pname) {
        case GL_UNPACK_ROW_LENGTH:
        case GL_PACK_ROW_LENGTH:
            store->row_length = param;
            break;
        case GL_UNPACK_SKIP_ROWS:
        case GL_PACK_SKIP_ROWS:
            store->skip_rows = param;
            break;
        default:
            store->skip_pixels = param;
            break;
    }
}

void glPixelStoref(const GLenum pname, const GLfloat param) {
    const GLboolean boolean = pname == GL_UNPACK_SWAP_BYTES || pname == GL_PACK_SWAP_BYTES ||
                              pname == GL_UNPACK_LSB_FIRST || pname == GL_PACK_LSB_FIRST;
    glPixelStorei(pname, boolean ? param != 0.0f : (GLint) floorf(param + 0.5f));
}
