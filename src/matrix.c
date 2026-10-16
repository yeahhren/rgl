#include "context.h"

#include <math.h>
#include <string.h>

#define RGL_PI 3.14159265358979323846

void rgl_matrix_identity(GLfloat m[16]) {
    memset(m, 0, 16 * sizeof(*m));
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

void rgl_matrix_transform(const GLfloat m[16], const GLfloat in[4], GLfloat out[4]) {
    for (int r = 0; r < 4; r++)
        out[r] = m[r] * in[0] + m[4 + r] * in[1] + m[8 + r] * in[2] + m[12 + r] * in[3];
}

GLfloat *rgl_matrix_top(struct rgl_matrix_stack *stack) {
    return stack->items[stack->depth - 1];
}

static void multiply(GLfloat m[16], const GLfloat n[16]) {
    GLfloat r[16];
    for (int c = 0; c < 4; c++) {
        for (int row = 0; row < 4; row++) {
            r[c * 4 + row] = m[row] * n[c * 4] + m[4 + row] * n[c * 4 + 1] + m[8 + row] * n[c * 4 + 2] +
                             m[12 + row] * n[c * 4 + 3];
        }
    }
    memcpy(m, r, sizeof(r));
}

static struct rgl_matrix_stack *current_stack(rgl_context *ctx) {
    switch (ctx->transform.matrix_mode) {
        case GL_PROJECTION:
            return &ctx->transform.projection;
        case GL_TEXTURE:
            return &ctx->transform.texture;
        default:
            return &ctx->transform.modelview;
    }
}

static void mult_current(const GLfloat n[16]) {
    GET_CONTEXT(ctx);
    OUTSIDE_BEGIN_END(ctx);

    multiply(rgl_matrix_top(current_stack(ctx)), n);
}

static void to_float(const GLdouble *in, GLfloat out[16]) {
    for (int i = 0; i < 16; i++)
        out[i] = (GLfloat) in[i];
}

void glMatrixMode(const GLenum mode) {
    GET_CONTEXT(ctx);
    OUTSIDE_BEGIN_END(ctx);

    if (mode != GL_MODELVIEW && mode != GL_PROJECTION && mode != GL_TEXTURE) {
        rgl_error(ctx, GL_INVALID_ENUM);
        return;
    }
    ctx->transform.matrix_mode = mode;
}

void glLoadIdentity(void) {
    GET_CONTEXT(ctx);
    OUTSIDE_BEGIN_END(ctx);

    rgl_matrix_identity(rgl_matrix_top(current_stack(ctx)));
}

void glLoadMatrixf(const GLfloat *m) {
    GET_CONTEXT(ctx);
    OUTSIDE_BEGIN_END(ctx);

    memcpy(rgl_matrix_top(current_stack(ctx)), m, 16 * sizeof(*m));
}

void glLoadMatrixd(const GLdouble *m) {
    GLfloat f[16];
    to_float(m, f);
    glLoadMatrixf(f);
}

void glMultMatrixf(const GLfloat *m) {
    mult_current(m);
}

void glMultMatrixd(const GLdouble *m) {
    GLfloat f[16];
    to_float(m, f);
    mult_current(f);
}

void glRotated(const GLdouble angle, GLdouble x, GLdouble y, GLdouble z) {
    GLfloat n[16];
    rgl_matrix_identity(n);

    const GLdouble len = sqrt(x * x + y * y + z * z);
    if (len > 0.0) {
        x /= len;
        y /= len;
        z /= len;
        const GLdouble rad = angle * RGL_PI / 180.0;
        const GLdouble c = cos(rad);
        const GLdouble s = sin(rad);
        const GLdouble t = 1.0 - c;

        n[0] = (GLfloat) (x * x * t + c);
        n[1] = (GLfloat) (y * x * t + z * s);
        n[2] = (GLfloat) (x * z * t - y * s);
        n[4] = (GLfloat) (x * y * t - z * s);
        n[5] = (GLfloat) (y * y * t + c);
        n[6] = (GLfloat) (y * z * t + x * s);
        n[8] = (GLfloat) (x * z * t + y * s);
        n[9] = (GLfloat) (y * z * t - x * s);
        n[10] = (GLfloat) (z * z * t + c);
    }
    mult_current(n);
}

void glRotatef(const GLfloat angle, const GLfloat x, const GLfloat y, const GLfloat z) {
    glRotated(angle, x, y, z);
}

void glTranslatef(const GLfloat x, const GLfloat y, const GLfloat z) {
    GLfloat n[16];
    rgl_matrix_identity(n);
    n[12] = x;
    n[13] = y;
    n[14] = z;
    mult_current(n);
}

void glTranslated(const GLdouble x, const GLdouble y, const GLdouble z) {
    glTranslatef((GLfloat) x, (GLfloat) y, (GLfloat) z);
}

void glScalef(const GLfloat x, const GLfloat y, const GLfloat z) {
    GLfloat n[16];
    rgl_matrix_identity(n);
    n[0] = x;
    n[5] = y;
    n[10] = z;
    mult_current(n);
}

void glScaled(const GLdouble x, const GLdouble y, const GLdouble z) {
    glScalef((GLfloat) x, (GLfloat) y, (GLfloat) z);
}

void glFrustum(const GLdouble left, const GLdouble right, const GLdouble bottom, const GLdouble top,
               const GLdouble zNear, const GLdouble zFar) {
    GET_CONTEXT(ctx);
    OUTSIDE_BEGIN_END(ctx);

    if (zNear <= 0.0 || zFar <= 0.0 || left == right || bottom == top || zNear == zFar) {
        rgl_error(ctx, GL_INVALID_VALUE);
        return;
    }

    GLfloat n[16] = {0};
    n[0] = (GLfloat) (2.0 * zNear / (right - left));
    n[5] = (GLfloat) (2.0 * zNear / (top - bottom));
    n[8] = (GLfloat) ((right + left) / (right - left));
    n[9] = (GLfloat) ((top + bottom) / (top - bottom));
    n[10] = (GLfloat) (-(zFar + zNear) / (zFar - zNear));
    n[11] = -1.0f;
    n[14] = (GLfloat) (-2.0 * zFar * zNear / (zFar - zNear));
    multiply(rgl_matrix_top(current_stack(ctx)), n);
}

void glOrtho(const GLdouble left, const GLdouble right, const GLdouble bottom, const GLdouble top,
             const GLdouble zNear, const GLdouble zFar) {
    GET_CONTEXT(ctx);
    OUTSIDE_BEGIN_END(ctx);

    if (left == right || bottom == top || zNear == zFar) {
        rgl_error(ctx, GL_INVALID_VALUE);
        return;
    }

    GLfloat n[16] = {0};
    n[0] = (GLfloat) (2.0 / (right - left));
    n[5] = (GLfloat) (2.0 / (top - bottom));
    n[10] = (GLfloat) (-2.0 / (zFar - zNear));
    n[12] = (GLfloat) (-(right + left) / (right - left));
    n[13] = (GLfloat) (-(top + bottom) / (top - bottom));
    n[14] = (GLfloat) (-(zFar + zNear) / (zFar - zNear));
    n[15] = 1.0f;
    multiply(rgl_matrix_top(current_stack(ctx)), n);
}

void glPushMatrix(void) {
    GET_CONTEXT(ctx);
    OUTSIDE_BEGIN_END(ctx);

    struct rgl_matrix_stack *stack = current_stack(ctx);
    if (stack->depth == stack->max_depth) {
        rgl_error(ctx, GL_STACK_OVERFLOW);
        return;
    }
    memcpy(stack->items[stack->depth], stack->items[stack->depth - 1], sizeof(stack->items[0]));
    stack->depth++;
}

void glPopMatrix(void) {
    GET_CONTEXT(ctx);
    OUTSIDE_BEGIN_END(ctx);

    struct rgl_matrix_stack *stack = current_stack(ctx);
    if (stack->depth == 1) {
        rgl_error(ctx, GL_STACK_UNDERFLOW);
        return;
    }
    stack->depth--;
}
