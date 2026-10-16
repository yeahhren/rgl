#include <rgl/rgl.h>

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
            failures++; \
        } \
    } while (0)

static const GLubyte *pixel(const rgl_context *ctx, int width, int x, int y) {
    return rgl_get_color_buffer(ctx) + ((size_t) y * (size_t) width + (size_t) x) * 4;
}

int main(void) {
    const int width = 64;
    const int height = 32;

    rgl_context *ctx = rgl_create_context(width, height);
    CHECK(ctx != NULL);
    if (ctx == NULL)
        return 1;
    rgl_make_current(ctx);

    CHECK(strcmp((const char *) glGetString(GL_VERSION), "1.0 rgl") == 0);
    CHECK(glIsEnabled(GL_DITHER) == GL_TRUE);
    CHECK(glIsEnabled(GL_DEPTH_TEST) == GL_FALSE);

    glClearColor(1.0f, 0.0f, 0.5f, 2.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    const GLubyte *p = pixel(ctx, width, 10, 10);
    CHECK(p[0] == 255 && p[1] == 0 && p[2] == 128 && p[3] == 255);

    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 0, 8, 8);
    glColorMask(GL_FALSE, GL_TRUE, GL_FALSE, GL_FALSE);
    glClearColor(0.0f, 1.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    p = pixel(ctx, width, 4, 4);
    CHECK(p[0] == 255 && p[1] == 255 && p[2] == 128 && p[3] == 255);
    p = pixel(ctx, width, 8, 8);
    CHECK(p[0] == 255 && p[1] == 0 && p[2] == 128 && p[3] == 255);

    CHECK(glGetError() == GL_NO_ERROR);
    glClear(0x1);
    glViewport(0, 0, -1, 1);
    glEnable(0xffff);
    CHECK(glGetError() == GL_INVALID_VALUE);
    CHECK(glGetError() == GL_NO_ERROR);

    rgl_destroy_context(ctx);
    CHECK(rgl_get_current() == NULL);

    if (failures == 0)
        printf("ok\n");
    return failures == 0 ? 0 : 1;
}
