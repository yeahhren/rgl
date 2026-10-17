#include "context.h"
#include "list.h"

static GLboolean is_missing_buffer(const GLenum mode) {
    switch (mode) {
        case GL_FRONT_RIGHT:
        case GL_BACK_LEFT:
        case GL_BACK_RIGHT:
        case GL_BACK:
        case GL_RIGHT:
        case GL_AUX0:
        case GL_AUX1:
        case GL_AUX2:
        case GL_AUX3:
            return GL_TRUE;
        default:
            return GL_FALSE;
    }
}

void glDrawBuffer(const GLenum mode) {
    GET_CONTEXT(ctx);
    if (SAVE(ctx, RGL_OP_DRAW_BUFFER, {.e = mode}))
        return;
    OUTSIDE_BEGIN_END(ctx);

    switch (mode) {
        case GL_NONE:
        case GL_FRONT:
        case GL_FRONT_LEFT:
        case GL_LEFT:
        case GL_FRONT_AND_BACK:
            ctx->color_buffer.draw_buffer = mode;
            break;
        default:
            rgl_error(ctx, is_missing_buffer(mode) ? GL_INVALID_OPERATION : GL_INVALID_ENUM);
            break;
    }
}

void glReadBuffer(const GLenum mode) {
    GET_CONTEXT(ctx);
    if (SAVE(ctx, RGL_OP_READ_BUFFER, {.e = mode}))
        return;
    OUTSIDE_BEGIN_END(ctx);

    switch (mode) {
        case GL_FRONT:
        case GL_FRONT_LEFT:
        case GL_LEFT:
            ctx->pixel.read_buffer = mode;
            break;
        default:
            rgl_error(ctx, is_missing_buffer(mode) ? GL_INVALID_OPERATION : GL_INVALID_ENUM);
            break;
    }
}
