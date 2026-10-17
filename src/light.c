#include "pipeline.h"

#include <math.h>
#include <string.h>

#define RGL_PI 3.14159265358979323846

static void set4(GLfloat out[4], const GLfloat a, const GLfloat b, const GLfloat c, const GLfloat d) {
    out[0] = a;
    out[1] = b;
    out[2] = c;
    out[3] = d;
}

void rgl_init_lighting(rgl_context *ctx) {
    struct rgl_lighting *l = &ctx->lighting;

    l->shade_model = GL_SMOOTH;
    set4(l->model_ambient, 0.2f, 0.2f, 0.2f, 1.0f);
    l->local_viewer = GL_FALSE;
    l->two_side = GL_FALSE;
    l->color_material_face = GL_FRONT_AND_BACK;
    l->color_material_mode = GL_AMBIENT_AND_DIFFUSE;

    for (int i = 0; i < RGL_MAX_LIGHTS; i++) {
        struct rgl_light *light = &l->light[i];
        const GLfloat c = i == 0 ? 1.0f : 0.0f;
        set4(light->ambient, 0.0f, 0.0f, 0.0f, 1.0f);
        set4(light->diffuse, c, c, c, 1.0f);
        set4(light->specular, c, c, c, 1.0f);
        set4(light->position, 0.0f, 0.0f, 1.0f, 0.0f);
        light->spot_direction[0] = 0.0f;
        light->spot_direction[1] = 0.0f;
        light->spot_direction[2] = -1.0f;
        light->spot_exponent = 0.0f;
        light->spot_cutoff = 180.0f;
        light->attenuation[0] = 1.0f;
        light->attenuation[1] = 0.0f;
        light->attenuation[2] = 0.0f;
    }

    for (int i = 0; i < 2; i++) {
        struct rgl_material *m = &l->material[i];
        set4(m->ambient, 0.2f, 0.2f, 0.2f, 1.0f);
        set4(m->diffuse, 0.8f, 0.8f, 0.8f, 1.0f);
        set4(m->specular, 0.0f, 0.0f, 0.0f, 1.0f);
        set4(m->emission, 0.0f, 0.0f, 0.0f, 1.0f);
        m->shininess = 0.0f;
        m->color_indexes[0] = 0.0f;
        m->color_indexes[1] = 1.0f;
        m->color_indexes[2] = 1.0f;
    }
}

static GLfloat dot3(const GLfloat *a, const GLfloat *b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static GLfloat normalize3(GLfloat *v) {
    const GLfloat len = sqrtf(dot3(v, v));
    if (len > 0.0f) {
        v[0] /= len;
        v[1] /= len;
        v[2] /= len;
    }
    return len;
}

static void shade(const rgl_context *ctx, const struct rgl_material *m, const GLfloat *position,
                  const GLfloat *normal, GLfloat out[4]) {
    const struct rgl_lighting *lighting = &ctx->lighting;
    GLfloat c[3];
    for (int k = 0; k < 3; k++)
        c[k] = m->emission[k] + m->ambient[k] * lighting->model_ambient[k];

    for (int i = 0; i < RGL_MAX_LIGHTS; i++) {
        if (!ctx->enable.light[i])
            continue;

        const struct rgl_light *l = &lighting->light[i];
        GLfloat vp[3];
        GLfloat attenuation = 1.0f;
        if (l->position[3] != 0.0f) {
            for (int k = 0; k < 3; k++)
                vp[k] = l->position[k] / l->position[3] - position[k];
            const GLfloat d = normalize3(vp);
            attenuation = 1.0f / (l->attenuation[0] + l->attenuation[1] * d + l->attenuation[2] * d * d);
        } else {
            for (int k = 0; k < 3; k++)
                vp[k] = l->position[k];
            normalize3(vp);
        }

        GLfloat spot = 1.0f;
        if (l->spot_cutoff != 180.0f) {
            GLfloat direction[3] = {l->spot_direction[0], l->spot_direction[1], l->spot_direction[2]};
            normalize3(direction);
            const GLfloat cos_angle = -dot3(vp, direction);
            const GLfloat cos_cutoff = (GLfloat) cos(l->spot_cutoff * RGL_PI / 180.0);
            spot = cos_angle >= cos_cutoff ? powf(cos_angle, l->spot_exponent) : 0.0f;
        }

        const GLfloat factor = attenuation * spot;
        if (factor == 0.0f)
            continue;

        for (int k = 0; k < 3; k++)
            c[k] += factor * m->ambient[k] * l->ambient[k];

        const GLfloat n_dot_vp = dot3(normal, vp);
        if (n_dot_vp <= 0.0f)
            continue;

        GLfloat h[3] = {vp[0], vp[1], vp[2] + 1.0f};
        if (lighting->local_viewer) {
            GLfloat to_eye[3] = {-position[0], -position[1], -position[2]};
            normalize3(to_eye);
            for (int k = 0; k < 3; k++)
                h[k] = vp[k] + to_eye[k];
        }
        normalize3(h);
        const GLfloat n_dot_h = dot3(normal, h);
        const GLfloat specular = powf(n_dot_h > 0.0f ? n_dot_h : 0.0f, m->shininess);

        for (int k = 0; k < 3; k++) {
            c[k] += factor * (n_dot_vp * m->diffuse[k] * l->diffuse[k] +
                              specular * m->specular[k] * l->specular[k]);
        }
    }

    for (int k = 0; k < 3; k++)
        out[k] = rgl_clampf(c[k], 0.0f, 1.0f);
    out[3] = rgl_clampf(m->diffuse[3], 0.0f, 1.0f);
}

void rgl_light_vertex(rgl_context *ctx, struct rgl_vertex *v) {
    if (!ctx->enable.lighting) {
        for (int k = 0; k < 4; k++)
            v->color[k] = rgl_clampf(ctx->current.color[k], 0.0f, 1.0f);
        memcpy(v->back_color, v->color, sizeof(v->back_color));
        memcpy(v->normal, ctx->current.normal, sizeof(v->normal));
        return;
    }

    const GLfloat *inv = rgl_modelview_inverse(ctx);
    const GLfloat *n = ctx->current.normal;
    for (int j = 0; j < 3; j++)
        v->normal[j] = n[0] * inv[j * 4] + n[1] * inv[j * 4 + 1] + n[2] * inv[j * 4 + 2];
    if (ctx->enable.normalize)
        normalize3(v->normal);

    GLfloat position[3];
    const GLfloat w = v->eye[3] != 0.0f ? v->eye[3] : 1.0f;
    for (int k = 0; k < 3; k++)
        position[k] = v->eye[k] / w;

    shade(ctx, &ctx->lighting.material[0], position, v->normal, v->color);
    if (ctx->lighting.two_side) {
        const GLfloat back[3] = {-v->normal[0], -v->normal[1], -v->normal[2]};
        shade(ctx, &ctx->lighting.material[1], position, back, v->back_color);
    } else {
        memcpy(v->back_color, v->color, sizeof(v->back_color));
    }
}

static GLboolean is_face(const GLenum face) {
    return face == GL_FRONT || face == GL_BACK || face == GL_FRONT_AND_BACK;
}

static void set_material(rgl_context *ctx, const GLenum face, const GLenum pname, const GLfloat *params) {
    for (int i = 0; i < 2; i++) {
        if ((i == 0 && face == GL_BACK) || (i == 1 && face == GL_FRONT))
            continue;

        struct rgl_material *m = &ctx->lighting.material[i];
        switch (pname) {
            case GL_AMBIENT:
                memcpy(m->ambient, params, sizeof(m->ambient));
                break;
            case GL_DIFFUSE:
                memcpy(m->diffuse, params, sizeof(m->diffuse));
                break;
            case GL_SPECULAR:
                memcpy(m->specular, params, sizeof(m->specular));
                break;
            case GL_EMISSION:
                memcpy(m->emission, params, sizeof(m->emission));
                break;
            case GL_SHININESS:
                m->shininess = params[0];
                break;
            case GL_AMBIENT_AND_DIFFUSE:
                memcpy(m->ambient, params, sizeof(m->ambient));
                memcpy(m->diffuse, params, sizeof(m->diffuse));
                break;
            case GL_COLOR_INDEXES:
                memcpy(m->color_indexes, params, sizeof(m->color_indexes));
                break;
            default:
                break;
        }
    }
}

void rgl_update_color_material(rgl_context *ctx) {
    if (!ctx->enable.color_material)
        return;
    set_material(ctx, ctx->lighting.color_material_face, ctx->lighting.color_material_mode, ctx->current.color);
}

static struct rgl_light *get_light(rgl_context *ctx, const GLenum light) {
    if (light < GL_LIGHT0 || light >= GL_LIGHT0 + RGL_MAX_LIGHTS)
        return NULL;
    return &ctx->lighting.light[light - GL_LIGHT0];
}

static GLboolean is_light_scalar(const GLenum pname) {
    return pname == GL_SPOT_EXPONENT || pname == GL_SPOT_CUTOFF || pname == GL_CONSTANT_ATTENUATION ||
           pname == GL_LINEAR_ATTENUATION || pname == GL_QUADRATIC_ATTENUATION;
}

static GLboolean is_light_color(const GLenum pname) {
    return pname == GL_AMBIENT || pname == GL_DIFFUSE || pname == GL_SPECULAR;
}

static int light_size(const GLenum pname) {
    if (is_light_color(pname) || pname == GL_POSITION)
        return 4;
    return pname == GL_SPOT_DIRECTION ? 3 : 1;
}

void glLightfv(const GLenum light, const GLenum pname, const GLfloat *params) {
    GET_CONTEXT(ctx);
    OUTSIDE_BEGIN_END(ctx);

    struct rgl_light *l = get_light(ctx, light);
    if (l == NULL) {
        rgl_error(ctx, GL_INVALID_ENUM);
        return;
    }

    const GLfloat *m = rgl_matrix_top(&ctx->transform.modelview);
    switch (pname) {
        case GL_AMBIENT:
            memcpy(l->ambient, params, sizeof(l->ambient));
            break;
        case GL_DIFFUSE:
            memcpy(l->diffuse, params, sizeof(l->diffuse));
            break;
        case GL_SPECULAR:
            memcpy(l->specular, params, sizeof(l->specular));
            break;
        case GL_POSITION:
            rgl_matrix_transform(m, params, l->position);
            break;
        case GL_SPOT_DIRECTION:
            for (int j = 0; j < 3; j++)
                l->spot_direction[j] = m[j] * params[0] + m[4 + j] * params[1] + m[8 + j] * params[2];
            break;
        case GL_SPOT_EXPONENT:
            if (params[0] < 0.0f || params[0] > 128.0f) {
                rgl_error(ctx, GL_INVALID_VALUE);
                return;
            }
            l->spot_exponent = params[0];
            break;
        case GL_SPOT_CUTOFF:
            if ((params[0] < 0.0f || params[0] > 90.0f) && params[0] != 180.0f) {
                rgl_error(ctx, GL_INVALID_VALUE);
                return;
            }
            l->spot_cutoff = params[0];
            break;
        case GL_CONSTANT_ATTENUATION:
        case GL_LINEAR_ATTENUATION:
        case GL_QUADRATIC_ATTENUATION:
            if (params[0] < 0.0f) {
                rgl_error(ctx, GL_INVALID_VALUE);
                return;
            }
            l->attenuation[pname - GL_CONSTANT_ATTENUATION] = params[0];
            break;
        default:
            rgl_error(ctx, GL_INVALID_ENUM);
            break;
    }
}

void glLightf(const GLenum light, const GLenum pname, const GLfloat param) {
    if (!is_light_scalar(pname)) {
        GET_CONTEXT(ctx);
        rgl_error(ctx, GL_INVALID_ENUM);
        return;
    }
    glLightfv(light, pname, &param);
}

void glLightiv(const GLenum light, const GLenum pname, const GLint *params) {
    GLfloat f[4] = {0};
    const int n = light_size(pname);
    for (int i = 0; i < n; i++)
        f[i] = is_light_color(pname) ? rgl_int_to_float(params[i]) : (GLfloat) params[i];
    glLightfv(light, pname, f);
}

void glLighti(const GLenum light, const GLenum pname, const GLint param) {
    glLightf(light, pname, (GLfloat) param);
}

void glLightModelfv(const GLenum pname, const GLfloat *params) {
    GET_CONTEXT(ctx);
    OUTSIDE_BEGIN_END(ctx);

    switch (pname) {
        case GL_LIGHT_MODEL_AMBIENT:
            memcpy(ctx->lighting.model_ambient, params, sizeof(ctx->lighting.model_ambient));
            break;
        case GL_LIGHT_MODEL_LOCAL_VIEWER:
            ctx->lighting.local_viewer = params[0] != 0.0f;
            break;
        case GL_LIGHT_MODEL_TWO_SIDE:
            ctx->lighting.two_side = params[0] != 0.0f;
            break;
        default:
            rgl_error(ctx, GL_INVALID_ENUM);
            break;
    }
}

void glLightModelf(const GLenum pname, const GLfloat param) {
    if (pname == GL_LIGHT_MODEL_AMBIENT) {
        GET_CONTEXT(ctx);
        rgl_error(ctx, GL_INVALID_ENUM);
        return;
    }
    glLightModelfv(pname, &param);
}

void glLightModeliv(const GLenum pname, const GLint *params) {
    GLfloat f[4] = {0};
    if (pname == GL_LIGHT_MODEL_AMBIENT) {
        for (int i = 0; i < 4; i++)
            f[i] = rgl_int_to_float(params[i]);
    } else {
        f[0] = (GLfloat) params[0];
    }
    glLightModelfv(pname, f);
}

void glLightModeli(const GLenum pname, const GLint param) {
    glLightModelf(pname, (GLfloat) param);
}

static GLboolean is_material_color(const GLenum pname) {
    return pname == GL_AMBIENT || pname == GL_DIFFUSE || pname == GL_SPECULAR || pname == GL_EMISSION ||
           pname == GL_AMBIENT_AND_DIFFUSE;
}

void glMaterialfv(const GLenum face, const GLenum pname, const GLfloat *params) {
    GET_CONTEXT(ctx);

    if (!is_face(face) || (!is_material_color(pname) && pname != GL_SHININESS && pname != GL_COLOR_INDEXES)) {
        rgl_error(ctx, GL_INVALID_ENUM);
        return;
    }
    if (pname == GL_SHININESS && (params[0] < 0.0f || params[0] > 128.0f)) {
        rgl_error(ctx, GL_INVALID_VALUE);
        return;
    }
    set_material(ctx, face, pname, params);
}

void glMaterialf(const GLenum face, const GLenum pname, const GLfloat param) {
    if (pname != GL_SHININESS) {
        GET_CONTEXT(ctx);
        rgl_error(ctx, GL_INVALID_ENUM);
        return;
    }
    glMaterialfv(face, pname, &param);
}

void glMaterialiv(const GLenum face, const GLenum pname, const GLint *params) {
    GLfloat f[4] = {0};
    const int n = is_material_color(pname) ? 4 : pname == GL_COLOR_INDEXES ? 3 : 1;
    for (int i = 0; i < n; i++)
        f[i] = is_material_color(pname) ? rgl_int_to_float(params[i]) : (GLfloat) params[i];
    glMaterialfv(face, pname, f);
}

void glMateriali(const GLenum face, const GLenum pname, const GLint param) {
    glMaterialf(face, pname, (GLfloat) param);
}

void glColorMaterial(const GLenum face, const GLenum mode) {
    GET_CONTEXT(ctx);
    OUTSIDE_BEGIN_END(ctx);

    if (!is_face(face) || (mode != GL_EMISSION && mode != GL_AMBIENT && mode != GL_DIFFUSE &&
                           mode != GL_SPECULAR && mode != GL_AMBIENT_AND_DIFFUSE)) {
        rgl_error(ctx, GL_INVALID_ENUM);
        return;
    }
    ctx->lighting.color_material_face = face;
    ctx->lighting.color_material_mode = mode;
    rgl_update_color_material(ctx);
}

static GLint color_to_int(const GLfloat c) {
    return (GLint) floor((4294967295.0 * c - 1.0) / 2.0 + 0.5);
}

static GLint round_to_int(const GLfloat v) {
    return (GLint) floorf(v + 0.5f);
}

static GLboolean get_light_values(rgl_context *ctx, const GLenum light, const GLenum pname, GLfloat out[4]) {
    struct rgl_light *l = get_light(ctx, light);
    if (l == NULL) {
        rgl_error(ctx, GL_INVALID_ENUM);
        return GL_FALSE;
    }

    switch (pname) {
        case GL_AMBIENT:
            memcpy(out, l->ambient, sizeof(l->ambient));
            return GL_TRUE;
        case GL_DIFFUSE:
            memcpy(out, l->diffuse, sizeof(l->diffuse));
            return GL_TRUE;
        case GL_SPECULAR:
            memcpy(out, l->specular, sizeof(l->specular));
            return GL_TRUE;
        case GL_POSITION:
            memcpy(out, l->position, sizeof(l->position));
            return GL_TRUE;
        case GL_SPOT_DIRECTION:
            memcpy(out, l->spot_direction, sizeof(l->spot_direction));
            return GL_TRUE;
        case GL_SPOT_EXPONENT:
            out[0] = l->spot_exponent;
            return GL_TRUE;
        case GL_SPOT_CUTOFF:
            out[0] = l->spot_cutoff;
            return GL_TRUE;
        case GL_CONSTANT_ATTENUATION:
        case GL_LINEAR_ATTENUATION:
        case GL_QUADRATIC_ATTENUATION:
            out[0] = l->attenuation[pname - GL_CONSTANT_ATTENUATION];
            return GL_TRUE;
        default:
            rgl_error(ctx, GL_INVALID_ENUM);
            return GL_FALSE;
    }
}

void glGetLightfv(const GLenum light, const GLenum pname, GLfloat *params) {
    GET_CONTEXT(ctx);
    OUTSIDE_BEGIN_END(ctx);

    GLfloat values[4];
    if (!get_light_values(ctx, light, pname, values))
        return;
    memcpy(params, values, (size_t) light_size(pname) * sizeof(*params));
}

void glGetLightiv(const GLenum light, const GLenum pname, GLint *params) {
    GET_CONTEXT(ctx);
    OUTSIDE_BEGIN_END(ctx);

    GLfloat values[4];
    if (!get_light_values(ctx, light, pname, values))
        return;
    for (int i = 0; i < light_size(pname); i++)
        params[i] = is_light_color(pname) ? color_to_int(values[i]) : round_to_int(values[i]);
}

static int get_material_values(rgl_context *ctx, const GLenum face, const GLenum pname, GLfloat out[4]) {
    if (face != GL_FRONT && face != GL_BACK) {
        rgl_error(ctx, GL_INVALID_ENUM);
        return 0;
    }

    const struct rgl_material *m = &ctx->lighting.material[face == GL_BACK];
    switch (pname) {
        case GL_AMBIENT:
            memcpy(out, m->ambient, sizeof(m->ambient));
            return 4;
        case GL_DIFFUSE:
            memcpy(out, m->diffuse, sizeof(m->diffuse));
            return 4;
        case GL_SPECULAR:
            memcpy(out, m->specular, sizeof(m->specular));
            return 4;
        case GL_EMISSION:
            memcpy(out, m->emission, sizeof(m->emission));
            return 4;
        case GL_SHININESS:
            out[0] = m->shininess;
            return 1;
        case GL_COLOR_INDEXES:
            memcpy(out, m->color_indexes, sizeof(m->color_indexes));
            return 3;
        default:
            rgl_error(ctx, GL_INVALID_ENUM);
            return 0;
    }
}

void glGetMaterialfv(const GLenum face, const GLenum pname, GLfloat *params) {
    GET_CONTEXT(ctx);
    OUTSIDE_BEGIN_END(ctx);

    GLfloat values[4];
    const int n = get_material_values(ctx, face, pname, values);
    memcpy(params, values, (size_t) n * sizeof(*params));
}

void glGetMaterialiv(const GLenum face, const GLenum pname, GLint *params) {
    GET_CONTEXT(ctx);
    OUTSIDE_BEGIN_END(ctx);

    GLfloat values[4];
    const int n = get_material_values(ctx, face, pname, values);
    for (int i = 0; i < n; i++)
        params[i] = is_material_color(pname) ? color_to_int(values[i]) : round_to_int(values[i]);
}
