#include "pipeline.h"
#include "list.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define MAP_COLOR_4 0
#define MAP_INDEX 1
#define MAP_NORMAL 2
#define MAP_TEXTURE_COORD_1 3
#define MAP_TEXTURE_COORD_4 6
#define MAP_VERTEX_3 7
#define MAP_VERTEX_4 8

static const int map_size[RGL_MAX_EVAL_MAPS] = {4, 1, 3, 1, 2, 3, 4, 3, 4};

static const GLfloat map_default[RGL_MAX_EVAL_MAPS][4] = {
    {1.0f, 1.0f, 1.0f, 1.0f},
    {1.0f, 0.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 1.0f, 0.0f},
    {0.0f, 0.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 0.0f, 1.0f},
    {0.0f, 0.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 0.0f, 1.0f},
};

void rgl_init_eval(rgl_context *ctx) {
    struct rgl_eval *e = &ctx->eval;

    for (int i = 0; i < RGL_MAX_EVAL_MAPS; i++) {
        e->map1[i] = (struct rgl_map1) {.order = 1, .u1 = 0.0f, .u2 = 1.0f};
        e->map2[i] = (struct rgl_map2) {.uorder = 1, .vorder = 1, .u1 = 0.0f, .u2 = 1.0f, .v1 = 0.0f, .v2 = 1.0f};
    }
    e->grid1_un = 1;
    e->grid1_u1 = 0.0f;
    e->grid1_u2 = 1.0f;
    e->grid2_un = 1;
    e->grid2_vn = 1;
    e->grid2_u1 = 0.0f;
    e->grid2_u2 = 1.0f;
    e->grid2_v1 = 0.0f;
    e->grid2_v2 = 1.0f;
}

void rgl_free_eval(rgl_context *ctx) {
    for (int i = 0; i < RGL_MAX_EVAL_MAPS; i++) {
        free(ctx->eval.map1[i].points);
        free(ctx->eval.map2[i].points);
    }
}

static int map_index(const GLenum target, const GLenum first) {
    return target >= first && target < first + RGL_MAX_EVAL_MAPS ? (int) (target - first) : -1;
}

static const GLfloat *map1_points(const struct rgl_map1 *m, const int index) {
    return m->points != NULL ? m->points : map_default[index];
}

static const GLfloat *map2_points(const struct rgl_map2 *m, const int index) {
    return m->points != NULL ? m->points : map_default[index];
}

static void bezier(const GLfloat *points, const int order, const int stride, const int k, const GLfloat t,
                   GLfloat *out, GLfloat *deriv) {
    GLfloat tmp[RGL_MAX_EVAL_ORDER][4];
    for (int i = 0; i < order; i++) {
        for (int c = 0; c < k; c++)
            tmp[i][c] = points[i * stride + c];
    }
    if (deriv != NULL) {
        for (int c = 0; c < k; c++)
            deriv[c] = 0.0f;
    }

    for (int level = order - 1; level > 0; level--) {
        if (level == 1 && deriv != NULL) {
            for (int c = 0; c < k; c++)
                deriv[c] = (GLfloat) (order - 1) * (tmp[1][c] - tmp[0][c]);
        }
        for (int i = 0; i < level; i++) {
            for (int c = 0; c < k; c++)
                tmp[i][c] = (1.0f - t) * tmp[i][c] + t * tmp[i + 1][c];
        }
    }
    for (int c = 0; c < k; c++)
        out[c] = tmp[0][c];
}

static void eval1(const rgl_context *ctx, const int index, const GLfloat u, GLfloat out[4]) {
    const struct rgl_map1 *m = &ctx->eval.map1[index];
    const GLfloat t = (u - m->u1) / (m->u2 - m->u1);
    bezier(map1_points(m, index), m->order, map_size[index], map_size[index], t, out, NULL);
}

static void eval2(const rgl_context *ctx, const int index, const GLfloat u, const GLfloat v, GLfloat out[4],
                  GLfloat du[4], GLfloat dv[4]) {
    const struct rgl_map2 *m = &ctx->eval.map2[index];
    const int k = map_size[index];
    const GLfloat *points = map2_points(m, index);
    const GLfloat tu = (u - m->u1) / (m->u2 - m->u1);
    const GLfloat tv = (v - m->v1) / (m->v2 - m->v1);

    GLfloat c[RGL_MAX_EVAL_ORDER][4] = {{0.0f}};
    GLfloat dc[RGL_MAX_EVAL_ORDER][4] = {{0.0f}};
    for (int i = 0; i < m->uorder; i++)
        bezier(points + i * m->vorder * k, m->vorder, k, k, tv, c[i], dv != NULL ? dc[i] : NULL);

    bezier(c[0], m->uorder, 4, k, tu, out, du);
    if (dv != NULL)
        bezier(dc[0], m->uorder, 4, k, tu, dv, NULL);

    for (int i = 0; i < k; i++) {
        if (du != NULL)
            du[i] /= m->u2 - m->u1;
        if (dv != NULL)
            dv[i] /= m->v2 - m->v1;
    }
}

static void set_tex_coord(rgl_context *ctx, const int index, const GLfloat value[4]) {
    const int n = map_size[index];
    ctx->current.tex_coord[0] = value[0];
    ctx->current.tex_coord[1] = n > 1 ? value[1] : 0.0f;
    ctx->current.tex_coord[2] = n > 2 ? value[2] : 0.0f;
    ctx->current.tex_coord[3] = n > 3 ? value[3] : 1.0f;
}

static void auto_normal(rgl_context *ctx, const int index, const GLfloat p[4], const GLfloat du[4],
                        const GLfloat dv[4]) {
    GLfloat a[3];
    GLfloat b[3];
    for (int i = 0; i < 3; i++) {
        if (index == MAP_VERTEX_4) {
            a[i] = (du[i] * p[3] - p[i] * du[3]) / (p[3] * p[3]);
            b[i] = (dv[i] * p[3] - p[i] * dv[3]) / (p[3] * p[3]);
        } else {
            a[i] = du[i];
            b[i] = dv[i];
        }
    }

    GLfloat *n = ctx->current.normal;
    n[0] = a[1] * b[2] - a[2] * b[1];
    n[1] = a[2] * b[0] - a[0] * b[2];
    n[2] = a[0] * b[1] - a[1] * b[0];
    const GLfloat len = sqrtf(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    if (len > 0.0f) {
        for (int i = 0; i < 3; i++)
            n[i] /= len;
    }
}

static void eval_coord1(rgl_context *ctx, const GLfloat u) {
    const struct rgl_current_values saved = ctx->current;
    const GLboolean *on = ctx->enable.map1;
    GLfloat value[4];

    if (on[MAP_COLOR_4]) {
        eval1(ctx, MAP_COLOR_4, u, value);
        memcpy(ctx->current.color, value, sizeof(ctx->current.color));
    }
    if (on[MAP_INDEX]) {
        eval1(ctx, MAP_INDEX, u, value);
        ctx->current.index = value[0];
    }
    if (on[MAP_NORMAL]) {
        eval1(ctx, MAP_NORMAL, u, value);
        memcpy(ctx->current.normal, value, sizeof(ctx->current.normal));
    }
    for (int i = MAP_TEXTURE_COORD_4; i >= MAP_TEXTURE_COORD_1; i--) {
        if (on[i]) {
            eval1(ctx, i, u, value);
            set_tex_coord(ctx, i, value);
            break;
        }
    }

    if (on[MAP_VERTEX_4] || on[MAP_VERTEX_3]) {
        const int index = on[MAP_VERTEX_4] ? MAP_VERTEX_4 : MAP_VERTEX_3;
        value[3] = 1.0f;
        eval1(ctx, index, u, value);
        rgl_emit_vertex(ctx, value);
    }
    ctx->current = saved;
}

static void eval_coord2(rgl_context *ctx, const GLfloat u, const GLfloat v) {
    const struct rgl_current_values saved = ctx->current;
    const GLboolean *on = ctx->enable.map2;
    GLfloat value[4];

    if (on[MAP_COLOR_4]) {
        eval2(ctx, MAP_COLOR_4, u, v, value, NULL, NULL);
        memcpy(ctx->current.color, value, sizeof(ctx->current.color));
    }
    if (on[MAP_INDEX]) {
        eval2(ctx, MAP_INDEX, u, v, value, NULL, NULL);
        ctx->current.index = value[0];
    }
    if (on[MAP_NORMAL] && !ctx->enable.auto_normal) {
        eval2(ctx, MAP_NORMAL, u, v, value, NULL, NULL);
        memcpy(ctx->current.normal, value, sizeof(ctx->current.normal));
    }
    for (int i = MAP_TEXTURE_COORD_4; i >= MAP_TEXTURE_COORD_1; i--) {
        if (on[i]) {
            eval2(ctx, i, u, v, value, NULL, NULL);
            set_tex_coord(ctx, i, value);
            break;
        }
    }

    if (on[MAP_VERTEX_4] || on[MAP_VERTEX_3]) {
        const int index = on[MAP_VERTEX_4] ? MAP_VERTEX_4 : MAP_VERTEX_3;
        GLfloat du[4];
        GLfloat dv[4];
        value[3] = 1.0f;
        if (ctx->enable.auto_normal) {
            du[3] = dv[3] = 0.0f;
            eval2(ctx, index, u, v, value, du, dv);
            auto_normal(ctx, index, value, du, dv);
        } else {
            eval2(ctx, index, u, v, value, NULL, NULL);
        }
        rgl_emit_vertex(ctx, value);
    }
    ctx->current = saved;
}

static GLfloat grid(const GLint i, const GLint n, const GLfloat a, const GLfloat b) {
    return i == n ? b : a + (GLfloat) i * (b - a) / (GLfloat) n;
}

static GLfloat grid1_u(const rgl_context *ctx, const GLint i) {
    return grid(i, ctx->eval.grid1_un, ctx->eval.grid1_u1, ctx->eval.grid1_u2);
}

static GLfloat grid2_u(const rgl_context *ctx, const GLint i) {
    return grid(i, ctx->eval.grid2_un, ctx->eval.grid2_u1, ctx->eval.grid2_u2);
}

static GLfloat grid2_v(const rgl_context *ctx, const GLint j) {
    return grid(j, ctx->eval.grid2_vn, ctx->eval.grid2_v1, ctx->eval.grid2_v2);
}

static GLfloat *gather(const GLfloat *f, const GLdouble *d, const GLint uorder, const GLint ustride,
                       const GLint vorder, const GLint vstride, const int k) {
    GLfloat *out = malloc((size_t) uorder * (size_t) vorder * (size_t) k * sizeof(*out));
    if (out == NULL)
        return NULL;

    GLfloat *p = out;
    for (GLint i = 0; i < uorder; i++) {
        for (GLint j = 0; j < vorder; j++) {
            for (int c = 0; c < k; c++) {
                const size_t at = (size_t) i * (size_t) ustride + (size_t) j * (size_t) vstride + (size_t) c;
                *p++ = f != NULL ? f[at] : (GLfloat) d[at];
            }
        }
    }
    return out;
}

static void map1(const GLenum target, const GLfloat u1, const GLfloat u2, const GLint stride, const GLint order,
                 const GLfloat *f, const GLdouble *d) {
    GET_CONTEXT(ctx);
    const int index = map_index(target, GL_MAP1_COLOR_4);
    const GLboolean valid = index >= 0 && stride >= map_size[index] && order >= 1 && order <= RGL_MAX_EVAL_ORDER;
    if (rgl_list_compiling(ctx)) {
        const int k = index >= 0 ? map_size[index] : 0;
        const union rgl_list_arg args[5] = {{.e = target}, {.f = u1}, {.f = u2}, {.i = k}, {.i = order}};
        if (rgl_save(ctx, RGL_OP_MAP1, args, 5, valid && (f || d) ? gather(f, d, order, stride, 1, 0, k) : NULL))
            return;
    }
    OUTSIDE_BEGIN_END(ctx);

    if (index < 0) {
        rgl_error(ctx, GL_INVALID_ENUM);
        return;
    }
    if (u1 == u2 || !valid) {
        rgl_error(ctx, GL_INVALID_VALUE);
        return;
    }
    if (f == NULL && d == NULL)
        return;

    GLfloat *points = gather(f, d, order, stride, 1, 0, map_size[index]);
    if (points == NULL) {
        rgl_error(ctx, GL_OUT_OF_MEMORY);
        return;
    }

    struct rgl_map1 *m = &ctx->eval.map1[index];
    free(m->points);
    *m = (struct rgl_map1) {.order = order, .u1 = u1, .u2 = u2, .points = points};
}

void glMap1f(const GLenum target, const GLfloat u1, const GLfloat u2, const GLint stride, const GLint order,
             const GLfloat *points) {
    map1(target, u1, u2, stride, order, points, NULL);
}

void glMap1d(const GLenum target, const GLdouble u1, const GLdouble u2, const GLint stride, const GLint order,
             const GLdouble *points) {
    map1(target, (GLfloat) u1, (GLfloat) u2, stride, order, NULL, points);
}

static void map2(const GLenum target, const GLfloat u1, const GLfloat u2, const GLint ustride, const GLint uorder,
                 const GLfloat v1, const GLfloat v2, const GLint vstride, const GLint vorder, const GLfloat *f,
                 const GLdouble *d) {
    GET_CONTEXT(ctx);
    const int index = map_index(target, GL_MAP2_COLOR_4);
    const int k = index >= 0 ? map_size[index] : 0;
    const GLboolean valid = index >= 0 && ustride >= k && vstride >= k && uorder >= 1 &&
                            uorder <= RGL_MAX_EVAL_ORDER && vorder >= 1 && vorder <= RGL_MAX_EVAL_ORDER;
    if (rgl_list_compiling(ctx)) {
        const union rgl_list_arg args[9] = {{.e = target}, {.f = u1}, {.f = u2}, {.i = vorder * k}, {.i = uorder},
                                            {.f = v1}, {.f = v2}, {.i = k}, {.i = vorder}};
        if (rgl_save(ctx, RGL_OP_MAP2, args, 9,
                     valid && (f || d) ? gather(f, d, uorder, ustride, vorder, vstride, k) : NULL))
            return;
    }
    OUTSIDE_BEGIN_END(ctx);

    if (index < 0) {
        rgl_error(ctx, GL_INVALID_ENUM);
        return;
    }
    if (u1 == u2 || v1 == v2 || !valid) {
        rgl_error(ctx, GL_INVALID_VALUE);
        return;
    }
    if (f == NULL && d == NULL)
        return;

    GLfloat *points = gather(f, d, uorder, ustride, vorder, vstride, k);
    if (points == NULL) {
        rgl_error(ctx, GL_OUT_OF_MEMORY);
        return;
    }

    struct rgl_map2 *m = &ctx->eval.map2[index];
    free(m->points);
    *m = (struct rgl_map2) {
        .uorder = uorder, .vorder = vorder, .u1 = u1, .u2 = u2, .v1 = v1, .v2 = v2, .points = points
    };
}

void glMap2f(const GLenum target, const GLfloat u1, const GLfloat u2, const GLint ustride, const GLint uorder,
             const GLfloat v1, const GLfloat v2, const GLint vstride, const GLint vorder, const GLfloat *points) {
    map2(target, u1, u2, ustride, uorder, v1, v2, vstride, vorder, points, NULL);
}

void glMap2d(const GLenum target, const GLdouble u1, const GLdouble u2, const GLint ustride, const GLint uorder,
             const GLdouble v1, const GLdouble v2, const GLint vstride, const GLint vorder, const GLdouble *points) {
    map2(target, (GLfloat) u1, (GLfloat) u2, ustride, uorder, (GLfloat) v1, (GLfloat) v2, vstride, vorder, NULL,
         points);
}

void glMapGrid1f(const GLint un, const GLfloat u1, const GLfloat u2) {
    GET_CONTEXT(ctx);
    if (SAVE(ctx, RGL_OP_MAP_GRID1, {.i = un}, {.f = u1}, {.f = u2}))
        return;
    OUTSIDE_BEGIN_END(ctx);

    if (un <= 0) {
        rgl_error(ctx, GL_INVALID_VALUE);
        return;
    }
    ctx->eval.grid1_un = un;
    ctx->eval.grid1_u1 = u1;
    ctx->eval.grid1_u2 = u2;
}

void glMapGrid1d(const GLint un, const GLdouble u1, const GLdouble u2) {
    glMapGrid1f(un, (GLfloat) u1, (GLfloat) u2);
}

void glMapGrid2f(const GLint un, const GLfloat u1, const GLfloat u2, const GLint vn, const GLfloat v1,
                 const GLfloat v2) {
    GET_CONTEXT(ctx);
    if (SAVE(ctx, RGL_OP_MAP_GRID2, {.i = un}, {.f = u1}, {.f = u2}, {.i = vn}, {.f = v1}, {.f = v2}))
        return;
    OUTSIDE_BEGIN_END(ctx);

    if (un <= 0 || vn <= 0) {
        rgl_error(ctx, GL_INVALID_VALUE);
        return;
    }
    ctx->eval.grid2_un = un;
    ctx->eval.grid2_u1 = u1;
    ctx->eval.grid2_u2 = u2;
    ctx->eval.grid2_vn = vn;
    ctx->eval.grid2_v1 = v1;
    ctx->eval.grid2_v2 = v2;
}

void glMapGrid2d(const GLint un, const GLdouble u1, const GLdouble u2, const GLint vn, const GLdouble v1,
                 const GLdouble v2) {
    glMapGrid2f(un, (GLfloat) u1, (GLfloat) u2, vn, (GLfloat) v1, (GLfloat) v2);
}

void glEvalCoord1f(const GLfloat u) {
    GET_CONTEXT(ctx);
    if (SAVE(ctx, RGL_OP_EVAL_COORD1, {.f = u}))
        return;

    eval_coord1(ctx, u);
}

void glEvalCoord1fv(const GLfloat *u) {
    glEvalCoord1f(u[0]);
}

void glEvalCoord1d(const GLdouble u) {
    glEvalCoord1f((GLfloat) u);
}

void glEvalCoord1dv(const GLdouble *u) {
    glEvalCoord1f((GLfloat) u[0]);
}

void glEvalCoord2f(const GLfloat u, const GLfloat v) {
    GET_CONTEXT(ctx);
    if (SAVE(ctx, RGL_OP_EVAL_COORD2, {.f = u}, {.f = v}))
        return;

    eval_coord2(ctx, u, v);
}

void glEvalCoord2fv(const GLfloat *u) {
    glEvalCoord2f(u[0], u[1]);
}

void glEvalCoord2d(const GLdouble u, const GLdouble v) {
    glEvalCoord2f((GLfloat) u, (GLfloat) v);
}

void glEvalCoord2dv(const GLdouble *u) {
    glEvalCoord2f((GLfloat) u[0], (GLfloat) u[1]);
}

void glEvalPoint1(const GLint i) {
    GET_CONTEXT(ctx);
    if (SAVE(ctx, RGL_OP_EVAL_POINT1, {.i = i}))
        return;

    eval_coord1(ctx, grid1_u(ctx, i));
}

void glEvalPoint2(const GLint i, const GLint j) {
    GET_CONTEXT(ctx);
    if (SAVE(ctx, RGL_OP_EVAL_POINT2, {.i = i}, {.i = j}))
        return;

    eval_coord2(ctx, grid2_u(ctx, i), grid2_v(ctx, j));
}

void glEvalMesh1(const GLenum mode, const GLint i1, const GLint i2) {
    GET_CONTEXT(ctx);
    if (SAVE(ctx, RGL_OP_EVAL_MESH1, {.e = mode}, {.i = i1}, {.i = i2}))
        return;
    OUTSIDE_BEGIN_END(ctx);

    if (mode != GL_POINT && mode != GL_LINE) {
        rgl_error(ctx, GL_INVALID_ENUM);
        return;
    }

    rgl_begin(ctx, mode == GL_POINT ? GL_POINTS : GL_LINE_STRIP);
    for (GLint i = i1; i <= i2; i++)
        eval_coord1(ctx, grid1_u(ctx, i));
    rgl_end(ctx);
}

void glEvalMesh2(const GLenum mode, const GLint i1, const GLint i2, const GLint j1, const GLint j2) {
    GET_CONTEXT(ctx);
    if (SAVE(ctx, RGL_OP_EVAL_MESH2, {.e = mode}, {.i = i1}, {.i = i2}, {.i = j1}, {.i = j2}))
        return;
    OUTSIDE_BEGIN_END(ctx);

    switch (mode) {
        case GL_FILL:
            for (GLint j = j1; j < j2; j++) {
                rgl_begin(ctx, GL_QUAD_STRIP);
                for (GLint i = i1; i <= i2; i++) {
                    eval_coord2(ctx, grid2_u(ctx, i), grid2_v(ctx, j));
                    eval_coord2(ctx, grid2_u(ctx, i), grid2_v(ctx, j + 1));
                }
                rgl_end(ctx);
            }
            break;
        case GL_LINE:
            for (GLint j = j1; j <= j2; j++) {
                rgl_begin(ctx, GL_LINE_STRIP);
                for (GLint i = i1; i <= i2; i++)
                    eval_coord2(ctx, grid2_u(ctx, i), grid2_v(ctx, j));
                rgl_end(ctx);
            }
            for (GLint i = i1; i <= i2; i++) {
                rgl_begin(ctx, GL_LINE_STRIP);
                for (GLint j = j1; j <= j2; j++)
                    eval_coord2(ctx, grid2_u(ctx, i), grid2_v(ctx, j));
                rgl_end(ctx);
            }
            break;
        case GL_POINT:
            rgl_begin(ctx, GL_POINTS);
            for (GLint j = j1; j <= j2; j++) {
                for (GLint i = i1; i <= i2; i++)
                    eval_coord2(ctx, grid2_u(ctx, i), grid2_v(ctx, j));
            }
            rgl_end(ctx);
            break;
        default:
            rgl_error(ctx, GL_INVALID_ENUM);
            break;
    }
}

enum value_kind {
    AS_FLOAT,
    AS_DOUBLE,
    AS_INT
};

static void put(void *v, const enum value_kind kind, const size_t i, const GLfloat value) {
    switch (kind) {
        case AS_FLOAT:
            ((GLfloat *) v)[i] = value;
            break;
        case AS_DOUBLE:
            ((GLdouble *) v)[i] = value;
            break;
        default:
            ((GLint *) v)[i] = (GLint) floorf(value + 0.5f);
            break;
    }
}

static void get_map(const GLenum target, const GLenum query, void *v, const enum value_kind kind) {
    GET_CONTEXT(ctx);
    OUTSIDE_BEGIN_END(ctx);

    const int index1 = map_index(target, GL_MAP1_COLOR_4);
    const int index2 = map_index(target, GL_MAP2_COLOR_4);
    if ((index1 < 0 && index2 < 0) || (query != GL_COEFF && query != GL_ORDER && query != GL_DOMAIN)) {
        rgl_error(ctx, GL_INVALID_ENUM);
        return;
    }

    if (index1 >= 0) {
        const struct rgl_map1 *m = &ctx->eval.map1[index1];
        if (query == GL_ORDER) {
            put(v, kind, 0, (GLfloat) m->order);
        } else if (query == GL_DOMAIN) {
            put(v, kind, 0, m->u1);
            put(v, kind, 1, m->u2);
        } else {
            const GLfloat *points = map1_points(m, index1);
            for (size_t i = 0; i < (size_t) m->order * (size_t) map_size[index1]; i++)
                put(v, kind, i, points[i]);
        }
        return;
    }

    const struct rgl_map2 *m = &ctx->eval.map2[index2];
    if (query == GL_ORDER) {
        put(v, kind, 0, (GLfloat) m->uorder);
        put(v, kind, 1, (GLfloat) m->vorder);
    } else if (query == GL_DOMAIN) {
        put(v, kind, 0, m->u1);
        put(v, kind, 1, m->u2);
        put(v, kind, 2, m->v1);
        put(v, kind, 3, m->v2);
    } else {
        const GLfloat *points = map2_points(m, index2);
        const size_t count = (size_t) m->uorder * (size_t) m->vorder * (size_t) map_size[index2];
        for (size_t i = 0; i < count; i++)
            put(v, kind, i, points[i]);
    }
}

void glGetMapfv(const GLenum target, const GLenum query, GLfloat *v) {
    get_map(target, query, v, AS_FLOAT);
}

void glGetMapdv(const GLenum target, const GLenum query, GLdouble *v) {
    get_map(target, query, v, AS_DOUBLE);
}

void glGetMapiv(const GLenum target, const GLenum query, GLint *v) {
    get_map(target, query, v, AS_INT);
}
