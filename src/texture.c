#include "pipeline.h"
#include "list.h"
#include "pixel.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static void init_texture(struct rgl_texture *tex) {
    tex->min_filter = GL_NEAREST_MIPMAP_LINEAR;
    tex->mag_filter = GL_LINEAR;
    tex->wrap_s = GL_REPEAT;
    tex->wrap_t = GL_REPEAT;
    tex->complete = GL_FALSE;
    tex->complete_dirty = GL_TRUE;
}

void rgl_init_texture(rgl_context *ctx) {
    struct rgl_texture_state *t = &ctx->texture;

    init_texture(&t->texture_1d);
    init_texture(&t->texture_2d);
    t->env_mode = GL_MODULATE;

    for (int i = 0; i < 4; i++) {
        struct rgl_texgen *g = &t->gen[i];
        g->mode = GL_EYE_LINEAR;
        if (i < 2) {
            g->object_plane[i] = 1.0f;
            g->eye_plane[i] = 1.0f;
        }
    }
}

void rgl_free_texture(rgl_context *ctx) {
    for (int i = 0; i < RGL_MAX_TEXTURE_LEVELS; i++) {
        free(ctx->texture.texture_1d.levels[i].data);
        free(ctx->texture.texture_2d.levels[i].data);
    }
}

static struct rgl_texture *texture_for(rgl_context *ctx, const GLenum target) {
    switch (target) {
        case GL_TEXTURE_1D:
            return &ctx->texture.texture_1d;
        case GL_TEXTURE_2D:
            return &ctx->texture.texture_2d;
        default:
            return NULL;
    }
}

static GLboolean is_power_of_two(const GLsizei n) {
    return n > 0 && (n & (n - 1)) == 0;
}

static int log2_floor(GLsizei n) {
    int l = 0;
    while (n > 1) {
        n >>= 1;
        l++;
    }
    return l;
}

static GLsizei level_size(const GLsizei size, const int level) {
    return size >> level > 0 ? size >> level : 1;
}

static GLboolean uses_mipmaps(const GLenum filter) {
    return filter != GL_NEAREST && filter != GL_LINEAR;
}

static GLboolean check_complete(const struct rgl_texture *tex, const GLboolean is_2d) {
    const struct rgl_texture_image *base = &tex->levels[0];
    if (base->data == NULL)
        return GL_FALSE;
    if (!uses_mipmaps(tex->min_filter))
        return GL_TRUE;

    const GLint b = base->border;
    const GLsizei w = base->width - 2 * b;
    const GLsizei h = is_2d ? base->height - 2 * b : 1;
    const int q = log2_floor(w > h ? w : h);

    for (int i = 1; i <= q; i++) {
        const struct rgl_texture_image *img = &tex->levels[i];
        if (img->data == NULL || img->border != b || img->components != base->components ||
            img->width - 2 * b != level_size(w, i) || (is_2d && img->height - 2 * b != level_size(h, i)))
            return GL_FALSE;
    }
    return GL_TRUE;
}

static struct rgl_texture *active_texture(rgl_context *ctx, GLboolean *is_2d) {
    struct rgl_texture *tex;
    if (ctx->enable.texture_2d) {
        tex = &ctx->texture.texture_2d;
        *is_2d = GL_TRUE;
    } else if (ctx->enable.texture_1d) {
        tex = &ctx->texture.texture_1d;
        *is_2d = GL_FALSE;
    } else {
        return NULL;
    }

    if (tex->complete_dirty) {
        tex->complete = check_complete(tex, *is_2d);
        tex->complete_dirty = GL_FALSE;
    }
    return tex->complete ? tex : NULL;
}

GLboolean rgl_texturing_enabled(const rgl_context *ctx) {
    return ctx->enable.texture_1d || ctx->enable.texture_2d;
}

static GLint repeat(const GLint i, const GLsizei size) {
    const GLint m = i % size;
    return m < 0 ? m + size : m;
}

static void texel(const struct rgl_texture *tex, const struct rgl_texture_image *img, const GLboolean is_2d,
                  GLint i, GLint j, GLfloat out[4]) {
    const GLint b = img->border;
    const GLsizei w = img->width - 2 * b;
    const GLsizei h = is_2d ? img->height - 2 * b : 1;

    if (tex->wrap_s == GL_REPEAT)
        i = repeat(i, w);
    if (is_2d && tex->wrap_t == GL_REPEAT)
        j = repeat(j, h);
    if (i < -b || i >= w + b || (is_2d && (j < -b || j >= h + b))) {
        memcpy(out, tex->border_color, 4 * sizeof(*out));
        return;
    }

    const size_t row = is_2d ? (size_t) (j + b) : 0;
    memcpy(out, img->data + (row * (size_t) img->width + (size_t) (i + b)) * 4, 4 * sizeof(*out));
}

static void sample_level(const struct rgl_texture *tex, const struct rgl_texture_image *img, const GLboolean is_2d,
                         const GLenum filter, GLfloat s, GLfloat t, GLfloat out[4]) {
    const GLsizei w = img->width - 2 * img->border;
    const GLsizei h = is_2d ? img->height - 2 * img->border : 1;

    if (tex->wrap_s == GL_CLAMP)
        s = rgl_clampf(s, 0.0f, 1.0f);
    else
        s -= floorf(s);
    if (tex->wrap_t == GL_CLAMP)
        t = rgl_clampf(t, 0.0f, 1.0f);
    else
        t -= floorf(t);

    const GLfloat u = s * (GLfloat) w;
    const GLfloat v = t * (GLfloat) h;

    if (filter == GL_NEAREST) {
        GLint i = (GLint) floorf(u);
        GLint j = is_2d ? (GLint) floorf(v) : 0;
        if (i >= w)
            i = w - 1;
        if (j >= h)
            j = h - 1;
        texel(tex, img, is_2d, i, j, out);
        return;
    }

    const GLint i0 = (GLint) floorf(u - 0.5f);
    const GLint j0 = is_2d ? (GLint) floorf(v - 0.5f) : 0;
    const GLfloat alpha = u - 0.5f - (GLfloat) i0;
    const GLfloat beta = is_2d ? v - 0.5f - (GLfloat) j0 : 0.0f;

    GLfloat t00[4];
    GLfloat t10[4];
    texel(tex, img, is_2d, i0, j0, t00);
    texel(tex, img, is_2d, i0 + 1, j0, t10);
    if (!is_2d) {
        for (int k = 0; k < 4; k++)
            out[k] = (1.0f - alpha) * t00[k] + alpha * t10[k];
        return;
    }

    GLfloat t01[4];
    GLfloat t11[4];
    texel(tex, img, is_2d, i0, j0 + 1, t01);
    texel(tex, img, is_2d, i0 + 1, j0 + 1, t11);
    for (int k = 0; k < 4; k++) {
        out[k] = (1.0f - alpha) * (1.0f - beta) * t00[k] + alpha * (1.0f - beta) * t10[k] +
                 (1.0f - alpha) * beta * t01[k] + alpha * beta * t11[k];
    }
}

static GLfloat level_of_detail(const struct rgl_fragment *frag, const GLsizei w, const GLsizei h,
                               const GLboolean is_2d) {
    const GLfloat dudx = frag->tex_dx[0] * (GLfloat) w;
    const GLfloat dudy = frag->tex_dy[0] * (GLfloat) w;
    const GLfloat dvdx = is_2d ? frag->tex_dx[1] * (GLfloat) h : 0.0f;
    const GLfloat dvdy = is_2d ? frag->tex_dy[1] * (GLfloat) h : 0.0f;
    const GLfloat rx = sqrtf(dudx * dudx + dvdx * dvdx);
    const GLfloat ry = sqrtf(dudy * dudy + dvdy * dvdy);
    const GLfloat rho = rx > ry ? rx : ry;
    return rho > 0.0f ? log2f(rho) : -1000.0f;
}

static void apply_env(const rgl_context *ctx, const GLint components, const GLfloat t[4], GLfloat c[4]) {
    const GLfloat *env = ctx->texture.env_color;

    switch (ctx->texture.env_mode) {
        case GL_MODULATE:
            for (int k = 0; k < 3; k++)
                c[k] *= t[k];
            if (components == 2 || components == 4)
                c[3] *= t[3];
            break;
        case GL_DECAL:
            if (components == 3) {
                memcpy(c, t, 3 * sizeof(*c));
            } else if (components == 4) {
                for (int k = 0; k < 3; k++)
                    c[k] = c[k] * (1.0f - t[3]) + t[k] * t[3];
            }
            break;
        case GL_BLEND:
            if (components == 1 || components == 2) {
                for (int k = 0; k < 3; k++)
                    c[k] = c[k] * (1.0f - t[k]) + env[k] * t[k];
                if (components == 2)
                    c[3] *= t[3];
            }
            break;
        default:
            break;
    }
}

void rgl_texture_fragment(rgl_context *ctx, const struct rgl_fragment *frag, GLfloat color[4]) {
    GLboolean is_2d = GL_FALSE;
    const struct rgl_texture *tex = active_texture(ctx, &is_2d);
    if (tex == NULL || frag->tex_coord[3] == 0.0f)
        return;

    const GLfloat s = frag->tex_coord[0] / frag->tex_coord[3];
    const GLfloat t = frag->tex_coord[1] / frag->tex_coord[3];
    const struct rgl_texture_image *base = &tex->levels[0];
    const GLsizei w = base->width - 2 * base->border;
    const GLsizei h = is_2d ? base->height - 2 * base->border : 1;
    const GLfloat lambda = level_of_detail(frag, w, h, is_2d);
    const GLenum min = tex->min_filter;
    const GLfloat c = tex->mag_filter == GL_LINEAR &&
                      (min == GL_NEAREST_MIPMAP_NEAREST || min == GL_LINEAR_MIPMAP_NEAREST)
                          ? 0.5f
                          : 0.0f;

    GLfloat result[4];
    if (lambda <= c || !uses_mipmaps(min)) {
        sample_level(tex, base, is_2d, lambda <= c ? tex->mag_filter : min, s, t, result);
    } else {
        const int q = log2_floor(w > h ? w : h);
        const GLfloat l = lambda < (GLfloat) q ? lambda : (GLfloat) q;
        const GLenum filter = min == GL_NEAREST_MIPMAP_NEAREST || min == GL_NEAREST_MIPMAP_LINEAR
                                  ? GL_NEAREST
                                  : GL_LINEAR;

        if (min == GL_NEAREST_MIPMAP_NEAREST || min == GL_LINEAR_MIPMAP_NEAREST) {
            const int d = l <= 0.5f ? 0 : (int) ceilf(l + 0.5f) - 1;
            sample_level(tex, &tex->levels[d < q ? d : q], is_2d, filter, s, t, result);
        } else {
            const int d1 = (int) floorf(l);
            const int d2 = d1 < q ? d1 + 1 : q;
            const GLfloat f = l - (GLfloat) d1;
            GLfloat t1[4];
            GLfloat t2[4];
            sample_level(tex, &tex->levels[d1], is_2d, filter, s, t, t1);
            sample_level(tex, &tex->levels[d2], is_2d, filter, s, t, t2);
            for (int k = 0; k < 4; k++)
                result[k] = (1.0f - f) * t1[k] + f * t2[k];
        }
    }

    apply_env(ctx, base->components, result, color);
}

static void tex_image(rgl_context *ctx, const GLenum target, const GLint level, const GLint components,
                      const GLsizei width, const GLsizei height, const GLint border, const GLenum format,
                      const GLenum type, const GLvoid *pixels) {
    const GLboolean is_2d = target == GL_TEXTURE_2D;
    struct rgl_texture *tex = texture_for(ctx, target);

    if (tex == NULL || (format != GL_COLOR_INDEX && !rgl_is_color_format(format)) ||
        (type != GL_BITMAP && !rgl_is_numeric_type(type)) || (type == GL_BITMAP && format != GL_COLOR_INDEX)) {
        rgl_error(ctx, GL_INVALID_ENUM);
        return;
    }
    if (level < 0 || level >= RGL_MAX_TEXTURE_LEVELS || components < 1 || components > 4 ||
        (border != 0 && border != 1) || !is_power_of_two(width - 2 * border) ||
        width - 2 * border > RGL_MAX_TEXTURE_SIZE ||
        (is_2d && (!is_power_of_two(height - 2 * border) || height - 2 * border > RGL_MAX_TEXTURE_SIZE))) {
        rgl_error(ctx, GL_INVALID_VALUE);
        return;
    }

    const size_t count = (size_t) width * (size_t) height;
    GLfloat *data = malloc(count * 4 * sizeof(*data));
    if (data == NULL) {
        rgl_error(ctx, GL_OUT_OF_MEMORY);
        return;
    }

    rgl_unpack_rgba(ctx, width, height, format, type, pixels, data);
    for (size_t i = 0; i < count; i++) {
        GLfloat *p = data + i * 4;
        if (components <= 2)
            p[1] = p[2] = p[0];
        if (components == 1 || components == 3)
            p[3] = 1.0f;
    }

    struct rgl_texture_image *img = &tex->levels[level];
    free(img->data);
    img->width = width;
    img->height = height;
    img->border = border;
    img->components = components;
    img->data = data;
    tex->complete_dirty = GL_TRUE;
}

void glTexImage1D(const GLenum target, const GLint level, const GLint components, const GLsizei width,
                  const GLint border, const GLenum format, const GLenum type, const GLvoid *pixels) {
    GET_CONTEXT(ctx);
    if (rgl_list_compiling(ctx)) {
        const union rgl_list_arg args[7] = {{.e = target}, {.i = level}, {.i = components}, {.i = width},
                                            {.i = border}, {.e = format}, {.e = type}};
        if (rgl_save(ctx, RGL_OP_TEX_IMAGE_1D, args, 7,
                     rgl_copy_image(&ctx->pixel.unpack, width, 1, format, type, pixels)))
            return;
    }
    OUTSIDE_BEGIN_END(ctx);

    if (target != GL_TEXTURE_1D) {
        rgl_error(ctx, GL_INVALID_ENUM);
        return;
    }
    tex_image(ctx, target, level, components, width, 1, border, format, type, pixels);
}

void glTexImage2D(const GLenum target, const GLint level, const GLint components, const GLsizei width,
                  const GLsizei height, const GLint border, const GLenum format, const GLenum type,
                  const GLvoid *pixels) {
    GET_CONTEXT(ctx);
    if (rgl_list_compiling(ctx)) {
        const union rgl_list_arg args[8] = {{.e = target}, {.i = level}, {.i = components}, {.i = width},
                                            {.i = height}, {.i = border}, {.e = format}, {.e = type}};
        if (rgl_save(ctx, RGL_OP_TEX_IMAGE_2D, args, 8,
                     rgl_copy_image(&ctx->pixel.unpack, width, height, format, type, pixels)))
            return;
    }
    OUTSIDE_BEGIN_END(ctx);

    if (target != GL_TEXTURE_2D) {
        rgl_error(ctx, GL_INVALID_ENUM);
        return;
    }
    tex_image(ctx, target, level, components, width, height, border, format, type, pixels);
}

void glTexParameterfv(const GLenum target, const GLenum pname, const GLfloat *params) {
    GET_CONTEXT(ctx);
    union rgl_list_arg args[6] = {{.e = target}, {.e = pname}};
    for (int k = 0; k < (pname == GL_TEXTURE_BORDER_COLOR ? 4 : 1); k++)
        args[2 + k].f = params[k];
    if (rgl_save(ctx, RGL_OP_TEX_PARAMETER, args, 6, NULL))
        return;
    OUTSIDE_BEGIN_END(ctx);

    struct rgl_texture *tex = texture_for(ctx, target);
    if (tex == NULL) {
        rgl_error(ctx, GL_INVALID_ENUM);
        return;
    }

    const GLenum value = (GLenum) (GLint) params[0];
    switch (pname) {
        case GL_TEXTURE_MIN_FILTER:
            if (value != GL_NEAREST && value != GL_LINEAR && (value < GL_NEAREST_MIPMAP_NEAREST ||
                                                              value > GL_LINEAR_MIPMAP_LINEAR))
                break;
            tex->min_filter = value;
            tex->complete_dirty = GL_TRUE;
            return;
        case GL_TEXTURE_MAG_FILTER:
            if (value != GL_NEAREST && value != GL_LINEAR)
                break;
            tex->mag_filter = value;
            return;
        case GL_TEXTURE_WRAP_S:
        case GL_TEXTURE_WRAP_T:
            if (value != GL_CLAMP && value != GL_REPEAT)
                break;
            if (pname == GL_TEXTURE_WRAP_S)
                tex->wrap_s = value;
            else
                tex->wrap_t = value;
            return;
        case GL_TEXTURE_BORDER_COLOR:
            for (int k = 0; k < 4; k++)
                tex->border_color[k] = rgl_clampf(params[k], 0.0f, 1.0f);
            return;
        default:
            break;
    }
    rgl_error(ctx, GL_INVALID_ENUM);
}

void glTexParameterf(const GLenum target, const GLenum pname, const GLfloat param) {
    if (pname == GL_TEXTURE_BORDER_COLOR) {
        GET_CONTEXT(ctx);
        rgl_error(ctx, GL_INVALID_ENUM);
        return;
    }
    glTexParameterfv(target, pname, &param);
}

void glTexParameteriv(const GLenum target, const GLenum pname, const GLint *params) {
    GLfloat f[4] = {0};
    if (pname == GL_TEXTURE_BORDER_COLOR) {
        for (int k = 0; k < 4; k++)
            f[k] = rgl_int_to_float(params[k]);
    } else {
        f[0] = (GLfloat) params[0];
    }
    glTexParameterfv(target, pname, f);
}

void glTexParameteri(const GLenum target, const GLenum pname, const GLint param) {
    glTexParameterf(target, pname, (GLfloat) param);
}

static GLboolean get_tex_parameter(rgl_context *ctx, const GLenum target, const GLenum pname, GLfloat out[4]) {
    const struct rgl_texture *tex = texture_for(ctx, target);
    if (tex != NULL) {
        switch (pname) {
            case GL_TEXTURE_MIN_FILTER:
                out[0] = (GLfloat) tex->min_filter;
                return GL_TRUE;
            case GL_TEXTURE_MAG_FILTER:
                out[0] = (GLfloat) tex->mag_filter;
                return GL_TRUE;
            case GL_TEXTURE_WRAP_S:
                out[0] = (GLfloat) tex->wrap_s;
                return GL_TRUE;
            case GL_TEXTURE_WRAP_T:
                out[0] = (GLfloat) tex->wrap_t;
                return GL_TRUE;
            case GL_TEXTURE_BORDER_COLOR:
                memcpy(out, tex->border_color, sizeof(tex->border_color));
                return GL_TRUE;
            default:
                break;
        }
    }
    rgl_error(ctx, GL_INVALID_ENUM);
    return GL_FALSE;
}

void glGetTexParameterfv(const GLenum target, const GLenum pname, GLfloat *params) {
    GET_CONTEXT(ctx);
    OUTSIDE_BEGIN_END(ctx);

    GLfloat values[4];
    if (get_tex_parameter(ctx, target, pname, values))
        memcpy(params, values, (pname == GL_TEXTURE_BORDER_COLOR ? 4 : 1) * sizeof(*params));
}

void glGetTexParameteriv(const GLenum target, const GLenum pname, GLint *params) {
    GET_CONTEXT(ctx);
    OUTSIDE_BEGIN_END(ctx);

    GLfloat values[4];
    if (!get_tex_parameter(ctx, target, pname, values))
        return;
    if (pname == GL_TEXTURE_BORDER_COLOR) {
        for (int k = 0; k < 4; k++)
            params[k] = rgl_float_to_int(values[k]);
    } else {
        params[0] = (GLint) values[0];
    }
}

void glTexEnvfv(const GLenum target, const GLenum pname, const GLfloat *params) {
    GET_CONTEXT(ctx);
    union rgl_list_arg args[6] = {{.e = target}, {.e = pname}};
    for (int k = 0; k < (pname == GL_TEXTURE_ENV_COLOR ? 4 : 1); k++)
        args[2 + k].f = params[k];
    if (rgl_save(ctx, RGL_OP_TEX_ENV, args, 6, NULL))
        return;
    OUTSIDE_BEGIN_END(ctx);

    if (target == GL_TEXTURE_ENV) {
        const GLenum mode = (GLenum) (GLint) params[0];
        if (pname == GL_TEXTURE_ENV_MODE && (mode == GL_MODULATE || mode == GL_DECAL || mode == GL_BLEND)) {
            ctx->texture.env_mode = mode;
            return;
        }
        if (pname == GL_TEXTURE_ENV_COLOR) {
            for (int k = 0; k < 4; k++)
                ctx->texture.env_color[k] = rgl_clampf(params[k], 0.0f, 1.0f);
            return;
        }
    }
    rgl_error(ctx, GL_INVALID_ENUM);
}

void glTexEnvf(const GLenum target, const GLenum pname, const GLfloat param) {
    if (pname == GL_TEXTURE_ENV_COLOR) {
        GET_CONTEXT(ctx);
        rgl_error(ctx, GL_INVALID_ENUM);
        return;
    }
    glTexEnvfv(target, pname, &param);
}

void glTexEnviv(const GLenum target, const GLenum pname, const GLint *params) {
    GLfloat f[4] = {0};
    if (pname == GL_TEXTURE_ENV_COLOR) {
        for (int k = 0; k < 4; k++)
            f[k] = rgl_int_to_float(params[k]);
    } else {
        f[0] = (GLfloat) params[0];
    }
    glTexEnvfv(target, pname, f);
}

void glTexEnvi(const GLenum target, const GLenum pname, const GLint param) {
    glTexEnvf(target, pname, (GLfloat) param);
}

void glGetTexEnvfv(const GLenum target, const GLenum pname, GLfloat *params) {
    GET_CONTEXT(ctx);
    OUTSIDE_BEGIN_END(ctx);

    if (target == GL_TEXTURE_ENV && pname == GL_TEXTURE_ENV_MODE)
        params[0] = (GLfloat) ctx->texture.env_mode;
    else if (target == GL_TEXTURE_ENV && pname == GL_TEXTURE_ENV_COLOR)
        memcpy(params, ctx->texture.env_color, sizeof(ctx->texture.env_color));
    else
        rgl_error(ctx, GL_INVALID_ENUM);
}

void glGetTexEnviv(const GLenum target, const GLenum pname, GLint *params) {
    GET_CONTEXT(ctx);
    OUTSIDE_BEGIN_END(ctx);

    if (target == GL_TEXTURE_ENV && pname == GL_TEXTURE_ENV_MODE) {
        params[0] = (GLint) ctx->texture.env_mode;
    } else if (target == GL_TEXTURE_ENV && pname == GL_TEXTURE_ENV_COLOR) {
        for (int k = 0; k < 4; k++)
            params[k] = rgl_float_to_int(ctx->texture.env_color[k]);
    } else {
        rgl_error(ctx, GL_INVALID_ENUM);
    }
}

static const struct rgl_texture_image *get_level(rgl_context *ctx, const GLenum target, const GLint level) {
    const struct rgl_texture *tex = texture_for(ctx, target);
    if (tex == NULL) {
        rgl_error(ctx, GL_INVALID_ENUM);
        return NULL;
    }
    if (level < 0 || level >= RGL_MAX_TEXTURE_LEVELS) {
        rgl_error(ctx, GL_INVALID_VALUE);
        return NULL;
    }
    return &tex->levels[level];
}

static GLboolean get_level_parameter(rgl_context *ctx, const GLenum target, const GLint level, const GLenum pname,
                                     GLint *out) {
    const struct rgl_texture_image *img = get_level(ctx, target, level);
    if (img == NULL)
        return GL_FALSE;

    const GLboolean defined = img->data != NULL;
    switch (pname) {
        case GL_TEXTURE_WIDTH:
            *out = defined ? img->width : 0;
            return GL_TRUE;
        case GL_TEXTURE_HEIGHT:
            *out = defined ? img->height : 0;
            return GL_TRUE;
        case GL_TEXTURE_COMPONENTS:
            *out = defined ? img->components : 1;
            return GL_TRUE;
        case GL_TEXTURE_BORDER:
            *out = defined ? img->border : 0;
            return GL_TRUE;
        default:
            rgl_error(ctx, GL_INVALID_ENUM);
            return GL_FALSE;
    }
}

void glGetTexLevelParameteriv(const GLenum target, const GLint level, const GLenum pname, GLint *params) {
    GET_CONTEXT(ctx);
    OUTSIDE_BEGIN_END(ctx);

    get_level_parameter(ctx, target, level, pname, params);
}

void glGetTexLevelParameterfv(const GLenum target, const GLint level, const GLenum pname, GLfloat *params) {
    GET_CONTEXT(ctx);
    OUTSIDE_BEGIN_END(ctx);

    GLint value;
    if (get_level_parameter(ctx, target, level, pname, &value))
        params[0] = (GLfloat) value;
}

void glGetTexImage(const GLenum target, const GLint level, const GLenum format, const GLenum type,
                   GLvoid *pixels) {
    GET_CONTEXT(ctx);
    OUTSIDE_BEGIN_END(ctx);

    const struct rgl_texture_image *img = get_level(ctx, target, level);
    if (img == NULL)
        return;
    if (!rgl_is_color_format(format) || !rgl_is_numeric_type(type)) {
        rgl_error(ctx, GL_INVALID_ENUM);
        return;
    }
    if (img->data == NULL)
        return;

    const size_t count = (size_t) img->width * (size_t) img->height;
    GLfloat *rgba = malloc(count * 4 * sizeof(*rgba));
    if (rgba == NULL) {
        rgl_error(ctx, GL_OUT_OF_MEMORY);
        return;
    }

    memcpy(rgba, img->data, count * 4 * sizeof(*rgba));
    if (img->components <= 2) {
        for (size_t i = 0; i < count; i++)
            rgba[i * 4 + 1] = rgba[i * 4 + 2] = 0.0f;
    }
    rgl_pack_rgba(&ctx->pixel.pack, img->width, img->height, format, type, rgba, GL_TRUE, pixels);
    free(rgba);
}
