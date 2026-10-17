#include "pixel.h"
#include "list.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define MAP_I_TO_I 0
#define MAP_S_TO_S 1
#define MAP_I_TO_R 2
#define MAP_R_TO_R 6

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

static size_t row_length(const struct rgl_pixel_store *store, const GLsizei width) {
    return (size_t) (store->row_length > 0 ? store->row_length : width);
}

static size_t row_stride(const struct rgl_pixel_store *store, const GLsizei width, const int components,
                         const size_t size) {
    const size_t alignment = (size_t) store->alignment;
    const size_t bytes = size * (size_t) components * row_length(store, width);
    if (size >= alignment)
        return bytes;
    return alignment * ((bytes + alignment - 1) / alignment);
}

static size_t bitmap_stride(const struct rgl_pixel_store *store, const GLsizei width) {
    const size_t bits = 8 * (size_t) store->alignment;
    return (size_t) store->alignment * ((row_length(store, width) + bits - 1) / bits);
}

static void swap_bytes(GLubyte *b, const size_t size) {
    for (size_t i = 0; i < size / 2; i++) {
        const GLubyte t = b[i];
        b[i] = b[size - 1 - i];
        b[size - 1 - i] = t;
    }
}

static GLfloat read_value(const GLubyte *p, const GLenum type, const GLboolean swap, const GLboolean normalize) {
    GLubyte b[4];
    const size_t size = type_size(type);
    memcpy(b, p, size);
    if (swap)
        swap_bytes(b, size);

    switch (type) {
        case GL_UNSIGNED_BYTE:
            return normalize ? (GLfloat) b[0] / 255.0f : (GLfloat) b[0];
        case GL_BYTE: {
            const GLbyte v = (GLbyte) b[0];
            return normalize ? (2.0f * (GLfloat) v + 1.0f) / 255.0f : (GLfloat) v;
        }
        case GL_UNSIGNED_SHORT: {
            GLushort v;
            memcpy(&v, b, sizeof(v));
            return normalize ? (GLfloat) v / 65535.0f : (GLfloat) v;
        }
        case GL_SHORT: {
            GLshort v;
            memcpy(&v, b, sizeof(v));
            return normalize ? (2.0f * (GLfloat) v + 1.0f) / 65535.0f : (GLfloat) v;
        }
        case GL_UNSIGNED_INT: {
            GLuint v;
            memcpy(&v, b, sizeof(v));
            return normalize ? (GLfloat) ((GLdouble) v / 4294967295.0) : (GLfloat) v;
        }
        case GL_INT: {
            GLint v;
            memcpy(&v, b, sizeof(v));
            return normalize ? rgl_int_to_float(v) : (GLfloat) v;
        }
        default: {
            GLfloat v;
            memcpy(&v, b, sizeof(v));
            return v;
        }
    }
}

static void write_value(GLubyte *p, const GLenum type, const GLfloat c, const GLboolean swap,
                        const GLboolean normalize) {
    GLubyte b[4];
    const size_t size = type_size(type);
    const GLuint raw = (GLuint) (GLint) floorf(c);

    switch (type) {
        case GL_UNSIGNED_BYTE:
            b[0] = normalize ? (GLubyte) (c * 255.0f + 0.5f) : (GLubyte) raw;
            break;
        case GL_BYTE:
            b[0] = normalize ? (GLubyte) (GLbyte) floorf((255.0f * c - 1.0f) / 2.0f + 0.5f) : (GLubyte) raw;
            break;
        case GL_UNSIGNED_SHORT: {
            const GLushort v = normalize ? (GLushort) (c * 65535.0f + 0.5f) : (GLushort) raw;
            memcpy(b, &v, sizeof(v));
            break;
        }
        case GL_SHORT: {
            const GLshort v = normalize ? (GLshort) floorf((65535.0f * c - 1.0f) / 2.0f + 0.5f) : (GLshort) raw;
            memcpy(b, &v, sizeof(v));
            break;
        }
        case GL_UNSIGNED_INT: {
            const GLuint v = normalize ? (GLuint) ((GLdouble) c * 4294967295.0 + 0.5) : raw;
            memcpy(b, &v, sizeof(v));
            break;
        }
        case GL_INT: {
            const GLint v = normalize ? rgl_float_to_int(c) : (GLint) raw;
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

static GLubyte read_bit(const struct rgl_pixel_store *store, const GLsizei width, const GLubyte *pixels,
                        const GLsizei x, const GLsizei y) {
    const size_t bit = (size_t) store->skip_pixels + (size_t) x;
    const size_t row = (size_t) store->skip_rows + (size_t) y;
    const GLubyte byte = pixels[row * bitmap_stride(store, width) + bit / 8];
    const unsigned shift = store->lsb_first ? (unsigned) (bit % 8) : 7u - (unsigned) (bit % 8);
    return (GLubyte) (byte >> shift & 1u);
}

static void write_bit(const struct rgl_pixel_store *store, const GLsizei width, GLubyte *pixels, const GLsizei x,
                      const GLsizei y, const GLboolean value) {
    const size_t bit = (size_t) store->skip_pixels + (size_t) x;
    const size_t row = (size_t) store->skip_rows + (size_t) y;
    GLubyte *byte = &pixels[row * bitmap_stride(store, width) + bit / 8];
    const unsigned shift = store->lsb_first ? (unsigned) (bit % 8) : 7u - (unsigned) (bit % 8);
    if (value)
        *byte = (GLubyte) (*byte | 1u << shift);
    else
        *byte = (GLubyte) (*byte & ~(1u << shift));
}

static void unpack_components(const struct rgl_pixel_store *store, const GLsizei width, const GLsizei height,
                              const int n, const GLenum type, const GLvoid *pixels, const GLboolean normalize,
                              GLfloat *out) {
    if (type == GL_BITMAP) {
        for (GLsizei y = 0; y < height; y++) {
            for (GLsizei x = 0; x < width; x++)
                *out++ = read_bit(store, width, pixels, x, y);
        }
        return;
    }

    const size_t size = type_size(type);
    const size_t stride = row_stride(store, width, n, size);
    const GLubyte *base = (const GLubyte *) pixels + (size_t) store->skip_rows * stride +
                          (size_t) store->skip_pixels * (size_t) n * size;
    for (GLsizei y = 0; y < height; y++) {
        const GLubyte *p = base + (size_t) y * stride;
        for (GLsizei x = 0; x < width * n; x++, p += size)
            *out++ = read_value(p, type, store->swap_bytes, normalize);
    }
}

static void pack_components(const struct rgl_pixel_store *store, const GLsizei width, const GLsizei height,
                            const int n, const GLenum type, const GLfloat *values, const GLboolean normalize,
                            GLvoid *pixels) {
    if (type == GL_BITMAP) {
        for (GLsizei y = 0; y < height; y++) {
            for (GLsizei x = 0; x < width; x++)
                write_bit(store, width, pixels, x, y, ((GLuint) (GLint) *values++ & 1u) != 0);
        }
        return;
    }

    const size_t size = type_size(type);
    const size_t stride = row_stride(store, width, n, size);
    GLubyte *base = (GLubyte *) pixels + (size_t) store->skip_rows * stride +
                    (size_t) store->skip_pixels * (size_t) n * size;
    for (GLsizei y = 0; y < height; y++) {
        GLubyte *p = base + (size_t) y * stride;
        for (GLsizei x = 0; x < width * n; x++, p += size)
            write_value(p, type, *values++, store->swap_bytes, normalize);
    }
}

GLvoid *rgl_copy_image(const struct rgl_pixel_store *store, const GLsizei width, const GLsizei height,
                       const GLenum format, const GLenum type, const GLvoid *pixels) {
    if (pixels == NULL || width <= 0 || height <= 0 || (type != GL_BITMAP && !rgl_is_numeric_type(type)))
        return NULL;

    if (type == GL_BITMAP) {
        const struct rgl_pixel_store tight = {.alignment = 1};
        GLubyte *out = calloc(((size_t) width + 7) / 8 * (size_t) height, 1);
        if (out == NULL)
            return NULL;
        for (GLsizei y = 0; y < height; y++) {
            for (GLsizei x = 0; x < width; x++)
                write_bit(&tight, width, out, x, y, read_bit(store, width, pixels, x, y));
        }
        return out;
    }

    const int n = format_components(format);
    const size_t size = type_size(type);
    const size_t stride = row_stride(store, width, n, size);
    const size_t row = (size_t) width * (size_t) n * size;
    const GLubyte *base = (const GLubyte *) pixels + (size_t) store->skip_rows * stride +
                          (size_t) store->skip_pixels * (size_t) n * size;

    GLubyte *out = malloc(row * (size_t) height);
    if (out == NULL)
        return NULL;
    for (GLsizei y = 0; y < height; y++) {
        GLubyte *dst = out + (size_t) y * row;
        memcpy(dst, base + (size_t) y * stride, row);
        if (store->swap_bytes) {
            for (size_t i = 0; i < row; i += size)
                swap_bytes(dst + i, size);
        }
    }
    return out;
}

static GLfloat shift_index(const struct rgl_pixel_transfer *t, const GLfloat index) {
    return ldexpf(index, t->index_shift) + (GLfloat) t->index_offset;
}

static GLfloat lookup_index(const struct rgl_pixel_map *map, const GLfloat index) {
    return map->values[(GLuint) (GLint) floorf(index) & (GLuint) (map->size - 1)];
}

static GLfloat lookup_color(const struct rgl_pixel_map *map, const GLfloat c) {
    return map->values[(GLint) floorf(rgl_clampf(c, 0.0f, 1.0f) * (GLfloat) (map->size - 1) + 0.5f)];
}

void rgl_transfer_rgba(const rgl_context *ctx, GLfloat *rgba, const size_t count) {
    const struct rgl_pixel_transfer *t = &ctx->pixel.transfer;
    for (size_t i = 0; i < count; i++) {
        GLfloat *c = rgba + i * 4;
        for (int k = 0; k < 4; k++) {
            c[k] = c[k] * t->scale[k] + t->bias[k];
            if (t->map_color)
                c[k] = lookup_color(&ctx->pixel.maps[MAP_R_TO_R + k], c[k]);
            c[k] = rgl_clampf(c[k], 0.0f, 1.0f);
        }
    }
}

void rgl_transfer_depth(const rgl_context *ctx, GLfloat *depth, const size_t count) {
    const struct rgl_pixel_transfer *t = &ctx->pixel.transfer;
    for (size_t i = 0; i < count; i++)
        depth[i] = rgl_clampf(depth[i] * t->depth_scale + t->depth_bias, 0.0f, 1.0f);
}

GLuint rgl_transfer_stencil(const rgl_context *ctx, const GLfloat index) {
    GLfloat s = shift_index(&ctx->pixel.transfer, index);
    if (ctx->pixel.transfer.map_stencil)
        s = lookup_index(&ctx->pixel.maps[MAP_S_TO_S], s);
    return (GLuint) (GLint) floorf(s);
}

void rgl_unpack_rgba(const rgl_context *ctx, const GLsizei width, const GLsizei height, const GLenum format,
                     const GLenum type, const GLvoid *pixels, GLfloat *rgba) {
    const size_t count = (size_t) width * (size_t) height;
    if (pixels == NULL) {
        memset(rgba, 0, count * 4 * sizeof(*rgba));
        return;
    }

    const int n = format_components(format);
    const GLboolean index = format == GL_COLOR_INDEX;
    GLfloat *values = malloc(count * (size_t) n * sizeof(*values));
    if (values == NULL) {
        memset(rgba, 0, count * 4 * sizeof(*rgba));
        return;
    }
    unpack_components(&ctx->pixel.unpack, width, height, n, type, pixels, !index, values);

    for (size_t i = 0; i < count; i++) {
        const GLfloat *c = values + i * (size_t) n;
        GLfloat *out = rgba + i * 4;

        if (index) {
            const GLfloat value = shift_index(&ctx->pixel.transfer, c[0]);
            for (int k = 0; k < 4; k++)
                out[k] = lookup_index(&ctx->pixel.maps[MAP_I_TO_R + k], value);
            continue;
        }

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
    }
    free(values);

    if (!index)
        rgl_transfer_rgba(ctx, rgba, count);
}

void rgl_unpack_depth(const rgl_context *ctx, const GLsizei width, const GLsizei height, const GLenum type,
                      const GLvoid *pixels, GLfloat *depth) {
    unpack_components(&ctx->pixel.unpack, width, height, 1, type, pixels, GL_TRUE, depth);
    rgl_transfer_depth(ctx, depth, (size_t) width * (size_t) height);
}

void rgl_unpack_stencil(const rgl_context *ctx, const GLsizei width, const GLsizei height, const GLenum type,
                        const GLvoid *pixels, GLuint *stencil) {
    const size_t count = (size_t) width * (size_t) height;
    GLfloat *values = malloc(count * sizeof(*values));
    if (values == NULL) {
        memset(stencil, 0, count * sizeof(*stencil));
        return;
    }

    unpack_components(&ctx->pixel.unpack, width, height, 1, type, pixels, GL_FALSE, values);
    for (size_t i = 0; i < count; i++)
        stencil[i] = rgl_transfer_stencil(ctx, values[i]);
    free(values);
}

void rgl_unpack_bitmap(const struct rgl_pixel_store *store, const GLsizei width, const GLsizei height,
                       const GLvoid *bitmap, GLubyte *bits) {
    for (GLsizei y = 0; y < height; y++) {
        for (GLsizei x = 0; x < width; x++)
            *bits++ = read_bit(store, width, bitmap, x, y);
    }
}

void rgl_pack_rgba(const struct rgl_pixel_store *store, const GLsizei width, const GLsizei height,
                   const GLenum format, const GLenum type, const GLfloat *rgba, const GLboolean luminance_from_red,
                   GLvoid *pixels) {
    const size_t count = (size_t) width * (size_t) height;
    const int n = format_components(format);
    GLfloat *values = malloc(count * (size_t) n * sizeof(*values));
    if (values == NULL)
        return;

    for (size_t i = 0; i < count; i++) {
        const GLfloat *in = rgba + i * 4;
        GLfloat *c = values + i * (size_t) n;
        const GLfloat luminance = luminance_from_red ? in[0] : rgl_clampf(in[0] + in[1] + in[2], 0.0f, 1.0f);
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
                memcpy(c, in, (size_t) n * sizeof(*c));
                break;
        }
        for (int k = 0; k < n; k++)
            c[k] = rgl_clampf(c[k], 0.0f, 1.0f);
    }

    pack_components(store, width, height, n, type, values, GL_TRUE, pixels);
    free(values);
}

void rgl_pack_depth(const struct rgl_pixel_store *store, const GLsizei width, const GLsizei height,
                    const GLenum type, const GLfloat *depth, GLvoid *pixels) {
    pack_components(store, width, height, 1, type, depth, GL_TRUE, pixels);
}

void rgl_pack_index(const struct rgl_pixel_store *store, const GLsizei width, const GLsizei height,
                    const GLenum type, const GLuint *index, GLvoid *pixels) {
    const size_t count = (size_t) width * (size_t) height;
    GLfloat *values = malloc(count * sizeof(*values));
    if (values == NULL)
        return;

    for (size_t i = 0; i < count; i++)
        values[i] = (GLfloat) index[i];
    pack_components(store, width, height, 1, type, values, GL_FALSE, pixels);
    free(values);
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

void glPixelTransferf(const GLenum pname, const GLfloat param) {
    GET_CONTEXT(ctx);
    if (SAVE(ctx, RGL_OP_PIXEL_TRANSFER, {.e = pname}, {.f = param}))
        return;
    OUTSIDE_BEGIN_END(ctx);

    struct rgl_pixel_transfer *t = &ctx->pixel.transfer;
    switch (pname) {
        case GL_MAP_COLOR:
            t->map_color = param != 0.0f;
            break;
        case GL_MAP_STENCIL:
            t->map_stencil = param != 0.0f;
            break;
        case GL_INDEX_SHIFT:
            t->index_shift = (GLint) floorf(param + 0.5f);
            break;
        case GL_INDEX_OFFSET:
            t->index_offset = (GLint) floorf(param + 0.5f);
            break;
        case GL_RED_SCALE:
            t->scale[0] = param;
            break;
        case GL_GREEN_SCALE:
            t->scale[1] = param;
            break;
        case GL_BLUE_SCALE:
            t->scale[2] = param;
            break;
        case GL_ALPHA_SCALE:
            t->scale[3] = param;
            break;
        case GL_RED_BIAS:
            t->bias[0] = param;
            break;
        case GL_GREEN_BIAS:
            t->bias[1] = param;
            break;
        case GL_BLUE_BIAS:
            t->bias[2] = param;
            break;
        case GL_ALPHA_BIAS:
            t->bias[3] = param;
            break;
        case GL_DEPTH_SCALE:
            t->depth_scale = param;
            break;
        case GL_DEPTH_BIAS:
            t->depth_bias = param;
            break;
        default:
            rgl_error(ctx, GL_INVALID_ENUM);
            break;
    }
}

void glPixelTransferi(const GLenum pname, const GLint param) {
    glPixelTransferf(pname, (GLfloat) param);
}

static GLboolean is_color_map(const int map) {
    return map != MAP_I_TO_I && map != MAP_S_TO_S;
}

static struct rgl_pixel_map *pixel_map(rgl_context *ctx, const GLenum map) {
    if (map < GL_PIXEL_MAP_I_TO_I || map > GL_PIXEL_MAP_A_TO_A) {
        rgl_error(ctx, GL_INVALID_ENUM);
        return NULL;
    }
    return &ctx->pixel.maps[map - GL_PIXEL_MAP_I_TO_I];
}

static void pixel_map_values(const GLenum map, const GLsizei mapsize, const GLfloat *values) {
    GET_CONTEXT(ctx);
    if (rgl_list_compiling(ctx)) {
        const GLsizei size = mapsize < 0 ? 0 : mapsize > RGL_MAX_PIXEL_MAP_TABLE ? RGL_MAX_PIXEL_MAP_TABLE : mapsize;
        GLfloat *copy = malloc(((size_t) size + 1) * sizeof(*copy));
        if (copy != NULL)
            memcpy(copy, values, (size_t) size * sizeof(*copy));
        const union rgl_list_arg args[2] = {{.e = map}, {.i = mapsize}};
        if (rgl_save(ctx, RGL_OP_PIXEL_MAP, args, 2, copy))
            return;
    }
    OUTSIDE_BEGIN_END(ctx);

    struct rgl_pixel_map *m = pixel_map(ctx, map);
    if (m == NULL)
        return;

    const int index = (int) (map - GL_PIXEL_MAP_I_TO_I);
    const GLboolean power_of_two = mapsize > 0 && (mapsize & (mapsize - 1)) == 0;
    if (mapsize < 1 || mapsize > RGL_MAX_PIXEL_MAP_TABLE || (map <= GL_PIXEL_MAP_I_TO_A && !power_of_two)) {
        rgl_error(ctx, GL_INVALID_VALUE);
        return;
    }

    m->size = mapsize;
    for (GLsizei i = 0; i < mapsize; i++)
        m->values[i] = is_color_map(index) ? rgl_clampf(values[i], 0.0f, 1.0f) : values[i];
}

void glPixelMapfv(const GLenum map, const GLsizei mapsize, const GLfloat *values) {
    pixel_map_values(map, mapsize, values);
}

void glPixelMapuiv(const GLenum map, const GLsizei mapsize, const GLuint *values) {
    GLfloat f[RGL_MAX_PIXEL_MAP_TABLE];
    const GLboolean color = map >= GL_PIXEL_MAP_I_TO_R && map <= GL_PIXEL_MAP_A_TO_A;
    for (GLsizei i = 0; i < mapsize && i < RGL_MAX_PIXEL_MAP_TABLE; i++)
        f[i] = color ? (GLfloat) ((GLdouble) values[i] / 4294967295.0) : (GLfloat) values[i];
    pixel_map_values(map, mapsize, f);
}

void glPixelMapusv(const GLenum map, const GLsizei mapsize, const GLushort *values) {
    GLfloat f[RGL_MAX_PIXEL_MAP_TABLE];
    const GLboolean color = map >= GL_PIXEL_MAP_I_TO_R && map <= GL_PIXEL_MAP_A_TO_A;
    for (GLsizei i = 0; i < mapsize && i < RGL_MAX_PIXEL_MAP_TABLE; i++)
        f[i] = color ? (GLfloat) values[i] / 65535.0f : (GLfloat) values[i];
    pixel_map_values(map, mapsize, f);
}

static const struct rgl_pixel_map *get_pixel_map(const GLenum map) {
    GET_CONTEXT_OR(ctx, NULL);
    OUTSIDE_BEGIN_END_OR(ctx, NULL);
    return pixel_map(ctx, map);
}

void glGetPixelMapfv(const GLenum map, GLfloat *values) {
    const struct rgl_pixel_map *m = get_pixel_map(map);
    if (m != NULL)
        memcpy(values, m->values, (size_t) m->size * sizeof(*values));
}

void glGetPixelMapuiv(const GLenum map, GLuint *values) {
    const struct rgl_pixel_map *m = get_pixel_map(map);
    if (m == NULL)
        return;

    const GLboolean color = map >= GL_PIXEL_MAP_I_TO_R;
    for (GLint i = 0; i < m->size; i++) {
        values[i] = color ? (GLuint) ((GLdouble) m->values[i] * 4294967295.0 + 0.5)
                          : (GLuint) (GLint) floorf(m->values[i] + 0.5f);
    }
}

void glGetPixelMapusv(const GLenum map, GLushort *values) {
    const struct rgl_pixel_map *m = get_pixel_map(map);
    if (m == NULL)
        return;

    const GLboolean color = map >= GL_PIXEL_MAP_I_TO_R;
    for (GLint i = 0; i < m->size; i++) {
        values[i] = color ? (GLushort) (m->values[i] * 65535.0f + 0.5f)
                          : (GLushort) (GLint) floorf(m->values[i] + 0.5f);
    }
}

void glPixelZoom(const GLfloat xfactor, const GLfloat yfactor) {
    GET_CONTEXT(ctx);
    if (SAVE(ctx, RGL_OP_PIXEL_ZOOM, {.f = xfactor}, {.f = yfactor}))
        return;
    OUTSIDE_BEGIN_END(ctx);

    ctx->pixel.zoom_x = xfactor;
    ctx->pixel.zoom_y = yfactor;
}
