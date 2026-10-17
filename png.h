#ifndef RGL_PNG_H
#define RGL_PNG_H

#include <stdint.h>

/* rgba8, rows bottom to top like the gl framebuffer */
struct png_image {
    int width;
    int height;
    uint8_t *rgba;
};

int png_read(const char *path, struct png_image *image);
int png_write(const char *path, const uint8_t *rgba, int width, int height);
void png_free(struct png_image *image);

#endif
