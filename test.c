#include "png.h"

#include <rgl/rgl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 800
#define HEIGHT 600

static GLsizei next_power_of_two(const GLsizei n) {
    GLsizei p = 1;
    while (p < n)
        p *= 2;
    return p;
}

static int load_texture(const struct png_image *image, GLfloat *s_max, GLfloat *t_max) {
    const GLsizei width = next_power_of_two(image->width);
    const GLsizei height = next_power_of_two(image->height);
    GLubyte *texels = calloc((size_t) width * (size_t) height, 4);
    if (texels == NULL)
        return 0;

    for (int y = 0; y < image->height; y++)
        memcpy(texels + (size_t) y * (size_t) width * 4, image->rgba + (size_t) y * (size_t) image->width * 4,
               (size_t) image->width * 4);

    glTexImage2D(GL_TEXTURE_2D, 0, 3, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, texels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    free(texels);

    *s_max = (GLfloat) image->width / (GLfloat) width;
    *t_max = (GLfloat) image->height / (GLfloat) height;
    return 1;
}

static void draw(const struct png_image *image, const GLfloat s_max, const GLfloat t_max) {
    const GLfloat aspect = (GLfloat) WIDTH / (GLfloat) HEIGHT;
    const GLfloat x = 1.2f;
    const GLfloat y = x * (GLfloat) image->height / (GLfloat) image->width;

    glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-0.5 * aspect, 0.5 * aspect, -0.5, 0.5, 1.0, 10.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0f, 0.0f, -2.4f);
    glRotatef(-25.0f, 0.0f, 1.0f, 0.0f);

    glEnable(GL_TEXTURE_2D);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(-x, -y);
    glTexCoord2f(s_max, 0.0f);
    glVertex2f(x, -y);
    glTexCoord2f(s_max, t_max);
    glVertex2f(x, y);
    glTexCoord2f(0.0f, t_max);
    glVertex2f(-x, y);
    glEnd();
    glDisable(GL_TEXTURE_2D);
}

int main(const int argc, char **argv) {
    const char *input = argc > 1 ? argv[1] : RGL_ASSET_DIR "/cat.png";
    const char *output = argc > 2 ? argv[2] : "out.png";

    // load image
    struct png_image image;
    if (!png_read(input, &image)) {
        fprintf(stderr, "failed to read %s\n", input);
        return 1;
    }

    // create context
    rgl_context *ctx = rgl_create_context(WIDTH, HEIGHT);
    if (ctx == NULL) {
        fprintf(stderr, "failed to create context\n");
        png_free(&image);
        return 1;
    }
    rgl_make_current(ctx);

    // render
    GLfloat s_max;
    GLfloat t_max;
    int ok = load_texture(&image, &s_max, &t_max);
    if (ok)
        draw(&image, s_max, t_max);

    // check for errors
    const GLenum error = glGetError();
    if (error != GL_NO_ERROR)
        fprintf(stderr, "gl error 0x%04x\n", error);

    // write
    ok = ok && png_write(output, rgl_get_color_buffer(ctx), WIDTH, HEIGHT);
    if (ok)
        printf("wrote %s\n", output);
    else
        fprintf(stderr, "failed to write %s\n", output);

    // destroy
    rgl_destroy_context(ctx);
    png_free(&image);
    return ok && error == GL_NO_ERROR ? 0 : 1;
}
