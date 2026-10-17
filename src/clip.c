#include "pipeline.h"

#include <stdlib.h>

#define FRUSTUM_PLANES 6
#define TOTAL_PLANES (FRUSTUM_PLANES + RGL_MAX_CLIP_PLANES)

static GLboolean plane_enabled(const rgl_context *ctx, const int plane) {
    return plane < FRUSTUM_PLANES || ctx->enable.clip_plane[plane - FRUSTUM_PLANES];
}

static GLfloat distance(const rgl_context *ctx, const struct rgl_vertex *v, const int plane) {
    const GLfloat *c = v->clip;

    switch (plane) {
        case 0:
            return c[3] + c[0];
        case 1:
            return c[3] - c[0];
        case 2:
            return c[3] + c[1];
        case 3:
            return c[3] - c[1];
        case 4:
            return c[3] + c[2];
        case 5:
            return c[3] - c[2];
        default: {
            const GLfloat *p = ctx->transform.clip_plane[plane - FRUSTUM_PLANES];
            return p[0] * v->eye[0] + p[1] * v->eye[1] + p[2] * v->eye[2] + p[3] * v->eye[3];
        }
    }
}

static void lerp(GLfloat *out, const GLfloat *a, const GLfloat *b, const GLfloat t, const int n) {
    for (int i = 0; i < n; i++)
        out[i] = a[i] + t * (b[i] - a[i]);
}

static void interpolate(struct rgl_vertex *out, const struct rgl_vertex *a, const struct rgl_vertex *b,
                        const GLfloat t) {
    lerp(out->eye, a->eye, b->eye, t, 4);
    lerp(out->clip, a->clip, b->clip, t, 4);
    lerp(out->color, a->color, b->color, t, 4);
    lerp(out->back_color, a->back_color, b->back_color, t, 4);
    lerp(out->tex_coord, a->tex_coord, b->tex_coord, t, 4);
    lerp(out->normal, a->normal, b->normal, t, 3);
    out->fog_z = a->fog_z + t * (b->fog_z - a->fog_z);
}

void rgl_to_window(const rgl_context *ctx, struct rgl_vertex *v) {
    const struct rgl_viewport *vp = &ctx->viewport;
    const GLfloat inv_w = 1.0f / v->clip[3];

    v->window[0] = (v->clip[0] * inv_w + 1.0f) * 0.5f * (GLfloat) vp->width + (GLfloat) vp->x;
    v->window[1] = (v->clip[1] * inv_w + 1.0f) * 0.5f * (GLfloat) vp->height + (GLfloat) vp->y;
    v->window[2] = (GLfloat) (v->clip[2] * inv_w * (vp->z_far - vp->z_near) * 0.5 + (vp->z_far + vp->z_near) * 0.5);
    v->inv_w = inv_w;
}

static GLboolean reserve(rgl_context *ctx, const size_t count) {
    struct rgl_primitive *p = &ctx->primitive;
    if (count <= p->clip_capacity)
        return GL_TRUE;

    for (int i = 0; i < 2; i++) {
        struct rgl_vertex *buffer = realloc(p->clip[i], count * sizeof(*buffer));
        if (buffer == NULL)
            return GL_FALSE;
        p->clip[i] = buffer;
    }
    p->clip_capacity = count;
    return GL_TRUE;
}

GLboolean rgl_point_visible(const rgl_context *ctx, const struct rgl_vertex *v) {
    for (int plane = 0; plane < TOTAL_PLANES; plane++) {
        if (plane_enabled(ctx, plane) && distance(ctx, v, plane) < 0.0f)
            return GL_FALSE;
    }
    return v->clip[3] > 0.0f;
}

void rgl_clip_point(rgl_context *ctx, const struct rgl_vertex *v) {
    if (!rgl_point_visible(ctx, v))
        return;

    struct rgl_vertex w = *v;
    rgl_to_window(ctx, &w);
    rgl_raster_point(ctx, &w, w.color);
}

void rgl_clip_line(rgl_context *ctx, const struct rgl_vertex *a, const struct rgl_vertex *b,
                   const GLfloat *flat_color) {
    GLfloat t0 = 0.0f;
    GLfloat t1 = 1.0f;

    for (int plane = 0; plane < TOTAL_PLANES; plane++) {
        if (!plane_enabled(ctx, plane))
            continue;

        const GLfloat da = distance(ctx, a, plane);
        const GLfloat db = distance(ctx, b, plane);
        if (da < 0.0f && db < 0.0f)
            return;
        if (da < 0.0f) {
            const GLfloat t = da / (da - db);
            if (t > t0)
                t0 = t;
        } else if (db < 0.0f) {
            const GLfloat t = da / (da - db);
            if (t < t1)
                t1 = t;
        }
    }
    if (t0 > t1)
        return;

    struct rgl_vertex p = *a;
    struct rgl_vertex q = *b;
    if (t0 > 0.0f)
        interpolate(&p, a, b, t0);
    if (t1 < 1.0f)
        interpolate(&q, a, b, t1);
    if (p.clip[3] <= 0.0f || q.clip[3] <= 0.0f)
        return;

    rgl_to_window(ctx, &p);
    rgl_to_window(ctx, &q);
    rgl_raster_line(ctx, &p, &q, flat_color);
}

void rgl_clip_polygon(rgl_context *ctx, const struct rgl_vertex *const *vertices, const size_t count,
                      const GLboolean use_edge_flags, const struct rgl_vertex *provoking) {
    if (!reserve(ctx, count + TOTAL_PLANES)) {
        rgl_error(ctx, GL_OUT_OF_MEMORY);
        return;
    }

    struct rgl_vertex *in = ctx->primitive.clip[0];
    struct rgl_vertex *out = ctx->primitive.clip[1];
    size_t n = count;

    for (size_t i = 0; i < n; i++) {
        in[i] = *vertices[i];
        if (!use_edge_flags)
            in[i].edge_flag = GL_TRUE;
    }

    for (int plane = 0; plane < TOTAL_PLANES; plane++) {
        if (!plane_enabled(ctx, plane))
            continue;

        GLboolean all_inside = GL_TRUE;
        for (size_t i = 0; i < n && all_inside; i++)
            all_inside = distance(ctx, &in[i], plane) >= 0.0f;
        if (all_inside)
            continue;

        size_t m = 0;
        for (size_t i = 0; i < n; i++) {
            const struct rgl_vertex *a = &in[i];
            const struct rgl_vertex *b = &in[(i + 1) % n];
            const GLfloat da = distance(ctx, a, plane);
            const GLfloat db = distance(ctx, b, plane);

            if (da >= 0.0f)
                out[m++] = *a;
            if ((da >= 0.0f) != (db >= 0.0f)) {
                struct rgl_vertex *v = &out[m++];
                interpolate(v, a, b, da / (da - db));
                v->edge_flag = da >= 0.0f ? GL_FALSE : a->edge_flag;
            }
        }
        if (m < 3)
            return;

        struct rgl_vertex *t = in;
        in = out;
        out = t;
        n = m;
    }

    for (size_t i = 0; i < n; i++) {
        if (in[i].clip[3] <= 0.0f)
            return;
        rgl_to_window(ctx, &in[i]);
    }
    rgl_raster_polygon(ctx, in, n, provoking);
}

void glClipPlane(const GLenum plane, const GLdouble *equation) {
    GET_CONTEXT(ctx);
    OUTSIDE_BEGIN_END(ctx);

    if (plane < GL_CLIP_PLANE0 || plane >= GL_CLIP_PLANE0 + RGL_MAX_CLIP_PLANES) {
        rgl_error(ctx, GL_INVALID_ENUM);
        return;
    }

    GLfloat inv[16];
    if (!rgl_matrix_invert(rgl_matrix_top(&ctx->transform.modelview), inv))
        return;

    GLfloat *p = ctx->transform.clip_plane[plane - GL_CLIP_PLANE0];
    for (int j = 0; j < 4; j++) {
        p[j] = (GLfloat) (equation[0] * inv[j * 4] + equation[1] * inv[j * 4 + 1] + equation[2] * inv[j * 4 + 2] +
                          equation[3] * inv[j * 4 + 3]);
    }
}

void glGetClipPlane(const GLenum plane, GLdouble *equation) {
    GET_CONTEXT(ctx);
    OUTSIDE_BEGIN_END(ctx);

    if (plane < GL_CLIP_PLANE0 || plane >= GL_CLIP_PLANE0 + RGL_MAX_CLIP_PLANES) {
        rgl_error(ctx, GL_INVALID_ENUM);
        return;
    }

    const GLfloat *p = ctx->transform.clip_plane[plane - GL_CLIP_PLANE0];
    for (int i = 0; i < 4; i++)
        equation[i] = p[i];
}
