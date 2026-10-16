#include "context.h"

#include <stdlib.h>
#include <string.h>

#define INITIAL_VERTEX_CAPACITY 64

static GLboolean grow(struct rgl_primitive *p) {
    const size_t capacity = p->capacity ? p->capacity * 2 : INITIAL_VERTEX_CAPACITY;
    struct rgl_vertex *vertices = realloc(p->vertices, capacity * sizeof(*vertices));
    if (vertices == NULL)
        return GL_FALSE;
    p->vertices = vertices;
    p->capacity = capacity;
    return GL_TRUE;
}

static void vertex(const GLfloat x, const GLfloat y, const GLfloat z, const GLfloat w) {
    GET_CONTEXT(ctx);
    if (!ctx->in_begin_end)
        return;

    struct rgl_primitive *p = &ctx->primitive;
    if (p->count == p->capacity && !grow(p)) {
        rgl_error(ctx, GL_OUT_OF_MEMORY);
        return;
    }

    struct rgl_vertex *v = &p->vertices[p->count++];
    const GLfloat object[4] = {x, y, z, w};
    rgl_matrix_transform(rgl_matrix_top(&ctx->transform.modelview), object, v->eye);
    rgl_matrix_transform(rgl_matrix_top(&ctx->transform.projection), v->eye, v->clip);
    rgl_matrix_transform(rgl_matrix_top(&ctx->transform.texture), ctx->current.tex_coord, v->tex_coord);
    memcpy(v->color, ctx->current.color, sizeof(v->color));
    memcpy(v->normal, ctx->current.normal, sizeof(v->normal));
    v->edge_flag = ctx->current.edge_flag;
}

void glBegin(const GLenum mode) {
    GET_CONTEXT(ctx);
    OUTSIDE_BEGIN_END(ctx);

    if (mode > GL_POLYGON) {
        rgl_error(ctx, GL_INVALID_ENUM);
        return;
    }
    ctx->in_begin_end = GL_TRUE;
    ctx->primitive.mode = mode;
    ctx->primitive.count = 0;
}

void glEnd(void) {
    GET_CONTEXT(ctx);

    if (!ctx->in_begin_end) {
        rgl_error(ctx, GL_INVALID_OPERATION);
        return;
    }
    ctx->in_begin_end = GL_FALSE;
    ctx->primitive.count = 0;
}

void glVertex2d(const GLdouble x, const GLdouble y) {
    vertex((GLfloat) x, (GLfloat) y, 0.0f, 1.0f);
}

void glVertex2dv(const GLdouble *v) {
    glVertex2d(v[0], v[1]);
}

void glVertex2f(const GLfloat x, const GLfloat y) {
    vertex(x, y, 0.0f, 1.0f);
}

void glVertex2fv(const GLfloat *v) {
    glVertex2f(v[0], v[1]);
}

void glVertex2i(const GLint x, const GLint y) {
    vertex((GLfloat) x, (GLfloat) y, 0.0f, 1.0f);
}

void glVertex2iv(const GLint *v) {
    glVertex2i(v[0], v[1]);
}

void glVertex2s(const GLshort x, const GLshort y) {
    vertex(x, y, 0.0f, 1.0f);
}

void glVertex2sv(const GLshort *v) {
    glVertex2s(v[0], v[1]);
}

void glVertex3d(const GLdouble x, const GLdouble y, const GLdouble z) {
    vertex((GLfloat) x, (GLfloat) y, (GLfloat) z, 1.0f);
}

void glVertex3dv(const GLdouble *v) {
    glVertex3d(v[0], v[1], v[2]);
}

void glVertex3f(const GLfloat x, const GLfloat y, const GLfloat z) {
    vertex(x, y, z, 1.0f);
}

void glVertex3fv(const GLfloat *v) {
    glVertex3f(v[0], v[1], v[2]);
}

void glVertex3i(const GLint x, const GLint y, const GLint z) {
    vertex((GLfloat) x, (GLfloat) y, (GLfloat) z, 1.0f);
}

void glVertex3iv(const GLint *v) {
    glVertex3i(v[0], v[1], v[2]);
}

void glVertex3s(const GLshort x, const GLshort y, const GLshort z) {
    vertex(x, y, z, 1.0f);
}

void glVertex3sv(const GLshort *v) {
    glVertex3s(v[0], v[1], v[2]);
}

void glVertex4d(const GLdouble x, const GLdouble y, const GLdouble z, const GLdouble w) {
    vertex((GLfloat) x, (GLfloat) y, (GLfloat) z, (GLfloat) w);
}

void glVertex4dv(const GLdouble *v) {
    glVertex4d(v[0], v[1], v[2], v[3]);
}

void glVertex4f(const GLfloat x, const GLfloat y, const GLfloat z, const GLfloat w) {
    vertex(x, y, z, w);
}

void glVertex4fv(const GLfloat *v) {
    glVertex4f(v[0], v[1], v[2], v[3]);
}

void glVertex4i(const GLint x, const GLint y, const GLint z, const GLint w) {
    vertex((GLfloat) x, (GLfloat) y, (GLfloat) z, (GLfloat) w);
}

void glVertex4iv(const GLint *v) {
    glVertex4i(v[0], v[1], v[2], v[3]);
}

void glVertex4s(const GLshort x, const GLshort y, const GLshort z, const GLshort w) {
    vertex(x, y, z, w);
}

void glVertex4sv(const GLshort *v) {
    glVertex4s(v[0], v[1], v[2], v[3]);
}

void glRectf(const GLfloat x1, const GLfloat y1, const GLfloat x2, const GLfloat y2) {
    GET_CONTEXT(ctx);
    OUTSIDE_BEGIN_END(ctx);

    glBegin(GL_POLYGON);
    glVertex2f(x1, y1);
    glVertex2f(x2, y1);
    glVertex2f(x2, y2);
    glVertex2f(x1, y2);
    glEnd();
}

void glRectfv(const GLfloat *v1, const GLfloat *v2) {
    glRectf(v1[0], v1[1], v2[0], v2[1]);
}

void glRectd(const GLdouble x1, const GLdouble y1, const GLdouble x2, const GLdouble y2) {
    glRectf((GLfloat) x1, (GLfloat) y1, (GLfloat) x2, (GLfloat) y2);
}

void glRectdv(const GLdouble *v1, const GLdouble *v2) {
    glRectd(v1[0], v1[1], v2[0], v2[1]);
}

void glRecti(const GLint x1, const GLint y1, const GLint x2, const GLint y2) {
    glRectf((GLfloat) x1, (GLfloat) y1, (GLfloat) x2, (GLfloat) y2);
}

void glRectiv(const GLint *v1, const GLint *v2) {
    glRecti(v1[0], v1[1], v2[0], v2[1]);
}

void glRects(const GLshort x1, const GLshort y1, const GLshort x2, const GLshort y2) {
    glRectf(x1, y1, x2, y2);
}

void glRectsv(const GLshort *v1, const GLshort *v2) {
    glRects(v1[0], v1[1], v2[0], v2[1]);
}
