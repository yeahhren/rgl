#include "context.h"

static GLfloat byte_to_float(const GLbyte c) {
    return (2.0f * (GLfloat) c + 1.0f) / 255.0f;
}

static GLfloat ubyte_to_float(const GLubyte c) {
    return (GLfloat) c / 255.0f;
}

static GLfloat short_to_float(const GLshort c) {
    return (2.0f * (GLfloat) c + 1.0f) / 65535.0f;
}

static GLfloat ushort_to_float(const GLushort c) {
    return (GLfloat) c / 65535.0f;
}

static GLfloat int_to_float(const GLint c) {
    return (GLfloat) ((2.0 * (GLdouble) c + 1.0) / 4294967295.0);
}

static GLfloat uint_to_float(const GLuint c) {
    return (GLfloat) ((GLdouble) c / 4294967295.0);
}

static void set_color(const GLfloat red, const GLfloat green, const GLfloat blue, const GLfloat alpha) {
    GET_CONTEXT(ctx);

    ctx->current.color[0] = red;
    ctx->current.color[1] = green;
    ctx->current.color[2] = blue;
    ctx->current.color[3] = alpha;
}

static void set_normal(const GLfloat nx, const GLfloat ny, const GLfloat nz) {
    GET_CONTEXT(ctx);

    ctx->current.normal[0] = nx;
    ctx->current.normal[1] = ny;
    ctx->current.normal[2] = nz;
}

static void set_index(const GLfloat c) {
    GET_CONTEXT(ctx);

    ctx->current.index = c;
}

static void set_tex_coord(const GLfloat s, const GLfloat t, const GLfloat r, const GLfloat q) {
    GET_CONTEXT(ctx);

    ctx->current.tex_coord[0] = s;
    ctx->current.tex_coord[1] = t;
    ctx->current.tex_coord[2] = r;
    ctx->current.tex_coord[3] = q;
}

void glColor3b(const GLbyte red, const GLbyte green, const GLbyte blue) {
    set_color(byte_to_float(red), byte_to_float(green), byte_to_float(blue), 1.0f);
}

void glColor3bv(const GLbyte *v) {
    glColor3b(v[0], v[1], v[2]);
}

void glColor3d(const GLdouble red, const GLdouble green, const GLdouble blue) {
    set_color((GLfloat) red, (GLfloat) green, (GLfloat) blue, 1.0f);
}

void glColor3dv(const GLdouble *v) {
    glColor3d(v[0], v[1], v[2]);
}

void glColor3f(const GLfloat red, const GLfloat green, const GLfloat blue) {
    set_color(red, green, blue, 1.0f);
}

void glColor3fv(const GLfloat *v) {
    glColor3f(v[0], v[1], v[2]);
}

void glColor3i(const GLint red, const GLint green, const GLint blue) {
    set_color(int_to_float(red), int_to_float(green), int_to_float(blue), 1.0f);
}

void glColor3iv(const GLint *v) {
    glColor3i(v[0], v[1], v[2]);
}

void glColor3s(const GLshort red, const GLshort green, const GLshort blue) {
    set_color(short_to_float(red), short_to_float(green), short_to_float(blue), 1.0f);
}

void glColor3sv(const GLshort *v) {
    glColor3s(v[0], v[1], v[2]);
}

void glColor3ub(const GLubyte red, const GLubyte green, const GLubyte blue) {
    set_color(ubyte_to_float(red), ubyte_to_float(green), ubyte_to_float(blue), 1.0f);
}

void glColor3ubv(const GLubyte *v) {
    glColor3ub(v[0], v[1], v[2]);
}

void glColor3ui(const GLuint red, const GLuint green, const GLuint blue) {
    set_color(uint_to_float(red), uint_to_float(green), uint_to_float(blue), 1.0f);
}

void glColor3uiv(const GLuint *v) {
    glColor3ui(v[0], v[1], v[2]);
}

void glColor3us(const GLushort red, const GLushort green, const GLushort blue) {
    set_color(ushort_to_float(red), ushort_to_float(green), ushort_to_float(blue), 1.0f);
}

void glColor3usv(const GLushort *v) {
    glColor3us(v[0], v[1], v[2]);
}

void glColor4b(const GLbyte red, const GLbyte green, const GLbyte blue, const GLbyte alpha) {
    set_color(byte_to_float(red), byte_to_float(green), byte_to_float(blue), byte_to_float(alpha));
}

void glColor4bv(const GLbyte *v) {
    glColor4b(v[0], v[1], v[2], v[3]);
}

void glColor4d(const GLdouble red, const GLdouble green, const GLdouble blue, const GLdouble alpha) {
    set_color((GLfloat) red, (GLfloat) green, (GLfloat) blue, (GLfloat) alpha);
}

void glColor4dv(const GLdouble *v) {
    glColor4d(v[0], v[1], v[2], v[3]);
}

void glColor4f(const GLfloat red, const GLfloat green, const GLfloat blue, const GLfloat alpha) {
    set_color(red, green, blue, alpha);
}

void glColor4fv(const GLfloat *v) {
    glColor4f(v[0], v[1], v[2], v[3]);
}

void glColor4i(const GLint red, const GLint green, const GLint blue, const GLint alpha) {
    set_color(int_to_float(red), int_to_float(green), int_to_float(blue), int_to_float(alpha));
}

void glColor4iv(const GLint *v) {
    glColor4i(v[0], v[1], v[2], v[3]);
}

void glColor4s(const GLshort red, const GLshort green, const GLshort blue, const GLshort alpha) {
    set_color(short_to_float(red), short_to_float(green), short_to_float(blue), short_to_float(alpha));
}

void glColor4sv(const GLshort *v) {
    glColor4s(v[0], v[1], v[2], v[3]);
}

void glColor4ub(const GLubyte red, const GLubyte green, const GLubyte blue, const GLubyte alpha) {
    set_color(ubyte_to_float(red), ubyte_to_float(green), ubyte_to_float(blue), ubyte_to_float(alpha));
}

void glColor4ubv(const GLubyte *v) {
    glColor4ub(v[0], v[1], v[2], v[3]);
}

void glColor4ui(const GLuint red, const GLuint green, const GLuint blue, const GLuint alpha) {
    set_color(uint_to_float(red), uint_to_float(green), uint_to_float(blue), uint_to_float(alpha));
}

void glColor4uiv(const GLuint *v) {
    glColor4ui(v[0], v[1], v[2], v[3]);
}

void glColor4us(const GLushort red, const GLushort green, const GLushort blue, const GLushort alpha) {
    set_color(ushort_to_float(red), ushort_to_float(green), ushort_to_float(blue), ushort_to_float(alpha));
}

void glColor4usv(const GLushort *v) {
    glColor4us(v[0], v[1], v[2], v[3]);
}

void glNormal3b(const GLbyte nx, const GLbyte ny, const GLbyte nz) {
    set_normal(byte_to_float(nx), byte_to_float(ny), byte_to_float(nz));
}

void glNormal3bv(const GLbyte *v) {
    glNormal3b(v[0], v[1], v[2]);
}

void glNormal3d(const GLdouble nx, const GLdouble ny, const GLdouble nz) {
    set_normal((GLfloat) nx, (GLfloat) ny, (GLfloat) nz);
}

void glNormal3dv(const GLdouble *v) {
    glNormal3d(v[0], v[1], v[2]);
}

void glNormal3f(const GLfloat nx, const GLfloat ny, const GLfloat nz) {
    set_normal(nx, ny, nz);
}

void glNormal3fv(const GLfloat *v) {
    glNormal3f(v[0], v[1], v[2]);
}

void glNormal3i(const GLint nx, const GLint ny, const GLint nz) {
    set_normal(int_to_float(nx), int_to_float(ny), int_to_float(nz));
}

void glNormal3iv(const GLint *v) {
    glNormal3i(v[0], v[1], v[2]);
}

void glNormal3s(const GLshort nx, const GLshort ny, const GLshort nz) {
    set_normal(short_to_float(nx), short_to_float(ny), short_to_float(nz));
}

void glNormal3sv(const GLshort *v) {
    glNormal3s(v[0], v[1], v[2]);
}

void glIndexd(const GLdouble c) {
    set_index((GLfloat) c);
}

void glIndexdv(const GLdouble *c) {
    glIndexd(*c);
}

void glIndexf(const GLfloat c) {
    set_index(c);
}

void glIndexfv(const GLfloat *c) {
    glIndexf(*c);
}

void glIndexi(const GLint c) {
    set_index((GLfloat) c);
}

void glIndexiv(const GLint *c) {
    glIndexi(*c);
}

void glIndexs(const GLshort c) {
    set_index(c);
}

void glIndexsv(const GLshort *c) {
    glIndexs(*c);
}

void glTexCoord1d(const GLdouble s) {
    set_tex_coord((GLfloat) s, 0.0f, 0.0f, 1.0f);
}

void glTexCoord1dv(const GLdouble *v) {
    glTexCoord1d(v[0]);
}

void glTexCoord1f(const GLfloat s) {
    set_tex_coord(s, 0.0f, 0.0f, 1.0f);
}

void glTexCoord1fv(const GLfloat *v) {
    glTexCoord1f(v[0]);
}

void glTexCoord1i(const GLint s) {
    set_tex_coord((GLfloat) s, 0.0f, 0.0f, 1.0f);
}

void glTexCoord1iv(const GLint *v) {
    glTexCoord1i(v[0]);
}

void glTexCoord1s(const GLshort s) {
    set_tex_coord(s, 0.0f, 0.0f, 1.0f);
}

void glTexCoord1sv(const GLshort *v) {
    glTexCoord1s(v[0]);
}

void glTexCoord2d(const GLdouble s, const GLdouble t) {
    set_tex_coord((GLfloat) s, (GLfloat) t, 0.0f, 1.0f);
}

void glTexCoord2dv(const GLdouble *v) {
    glTexCoord2d(v[0], v[1]);
}

void glTexCoord2f(const GLfloat s, const GLfloat t) {
    set_tex_coord(s, t, 0.0f, 1.0f);
}

void glTexCoord2fv(const GLfloat *v) {
    glTexCoord2f(v[0], v[1]);
}

void glTexCoord2i(const GLint s, const GLint t) {
    set_tex_coord((GLfloat) s, (GLfloat) t, 0.0f, 1.0f);
}

void glTexCoord2iv(const GLint *v) {
    glTexCoord2i(v[0], v[1]);
}

void glTexCoord2s(const GLshort s, const GLshort t) {
    set_tex_coord(s, t, 0.0f, 1.0f);
}

void glTexCoord2sv(const GLshort *v) {
    glTexCoord2s(v[0], v[1]);
}

void glTexCoord3d(const GLdouble s, const GLdouble t, const GLdouble r) {
    set_tex_coord((GLfloat) s, (GLfloat) t, (GLfloat) r, 1.0f);
}

void glTexCoord3dv(const GLdouble *v) {
    glTexCoord3d(v[0], v[1], v[2]);
}

void glTexCoord3f(const GLfloat s, const GLfloat t, const GLfloat r) {
    set_tex_coord(s, t, r, 1.0f);
}

void glTexCoord3fv(const GLfloat *v) {
    glTexCoord3f(v[0], v[1], v[2]);
}

void glTexCoord3i(const GLint s, const GLint t, const GLint r) {
    set_tex_coord((GLfloat) s, (GLfloat) t, (GLfloat) r, 1.0f);
}

void glTexCoord3iv(const GLint *v) {
    glTexCoord3i(v[0], v[1], v[2]);
}

void glTexCoord3s(const GLshort s, const GLshort t, const GLshort r) {
    set_tex_coord(s, t, r, 1.0f);
}

void glTexCoord3sv(const GLshort *v) {
    glTexCoord3s(v[0], v[1], v[2]);
}

void glTexCoord4d(const GLdouble s, const GLdouble t, const GLdouble r, const GLdouble q) {
    set_tex_coord((GLfloat) s, (GLfloat) t, (GLfloat) r, (GLfloat) q);
}

void glTexCoord4dv(const GLdouble *v) {
    glTexCoord4d(v[0], v[1], v[2], v[3]);
}

void glTexCoord4f(const GLfloat s, const GLfloat t, const GLfloat r, const GLfloat q) {
    set_tex_coord(s, t, r, q);
}

void glTexCoord4fv(const GLfloat *v) {
    glTexCoord4f(v[0], v[1], v[2], v[3]);
}

void glTexCoord4i(const GLint s, const GLint t, const GLint r, const GLint q) {
    set_tex_coord((GLfloat) s, (GLfloat) t, (GLfloat) r, (GLfloat) q);
}

void glTexCoord4iv(const GLint *v) {
    glTexCoord4i(v[0], v[1], v[2], v[3]);
}

void glTexCoord4s(const GLshort s, const GLshort t, const GLshort r, const GLshort q) {
    set_tex_coord(s, t, r, q);
}

void glTexCoord4sv(const GLshort *v) {
    glTexCoord4s(v[0], v[1], v[2], v[3]);
}

void glEdgeFlag(const GLboolean flag) {
    GET_CONTEXT(ctx);

    ctx->current.edge_flag = flag ? GL_TRUE : GL_FALSE;
}

void glEdgeFlagv(const GLboolean *flag) {
    glEdgeFlag(*flag);
}
