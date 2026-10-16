#include "pipeline.h"

#include <math.h>

GLint rgl_integer_width(const GLfloat width, const GLint max) {
    const GLint w = (GLint) floorf(width + 0.5f);
    return w < 1 ? 1 : w > max ? max : w;
}

void rgl_write_fragment(rgl_context *ctx, const struct rgl_fragment *frag) {
    const struct rgl_framebuffer *fb = &ctx->fb;
    if (frag->x < 0 || frag->y < 0 || frag->x >= fb->width || frag->y >= fb->height)
        return;

    GLubyte *p = fb->color + ((size_t) frag->y * (size_t) fb->width + (size_t) frag->x) * 4;
    for (int i = 0; i < 4; i++) {
        if (ctx->mask.color[i])
            p[i] = (GLubyte) (rgl_clampf(frag->color[i], 0.0f, 1.0f) * 255.0f + 0.5f);
    }
}
