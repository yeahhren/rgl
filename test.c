#include <rgl/rgl.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 256
#define HEIGHT 256

static uint32_t crc_table[256];

static void init_crc_table(void) {
    for (uint32_t n = 0; n < 256; n++) {
        uint32_t c = n;
        for (int k = 0; k < 8; k++)
            c = c & 1u ? 0xedb88320u ^ (c >> 1) : c >> 1;
        crc_table[n] = c;
    }
}

static uint32_t crc32(uint32_t crc, const uint8_t *data, const size_t size) {
    for (size_t i = 0; i < size; i++)
        crc = crc_table[(crc ^ data[i]) & 0xffu] ^ (crc >> 8);
    return crc;
}

static uint32_t adler32(const uint8_t *data, const size_t size) {
    uint32_t a = 1;
    uint32_t b = 0;
    for (size_t i = 0; i < size; i++) {
        a = (a + data[i]) % 65521u;
        b = (b + a) % 65521u;
    }
    return b << 16 | a;
}

static void put_u32(uint8_t *p, const uint32_t v) {
    p[0] = (uint8_t) (v >> 24);
    p[1] = (uint8_t) (v >> 16);
    p[2] = (uint8_t) (v >> 8);
    p[3] = (uint8_t) v;
}

static void write_chunk(FILE *f, const char *type, const uint8_t *data, const size_t size) {
    uint8_t header[8];
    put_u32(header, (uint32_t) size);
    memcpy(header + 4, type, 4);
    fwrite(header, 1, 8, f);
    fwrite(data, 1, size, f);

    uint8_t crc[4];
    put_u32(crc, crc32(crc32(0xffffffffu, header + 4, 4), data, size) ^ 0xffffffffu);
    fwrite(crc, 1, 4, f);
}

// we can discuss my decisions later
static int write_png(const char *path, const GLubyte *rgba, const int width, const int height) {
    const size_t stride = (size_t) width * 4;
    const size_t raw_size = (stride + 1) * (size_t) height;
    const size_t blocks = raw_size / 65535 + 1;
    const size_t zlib_size = 2 + raw_size + blocks * 5 + 4;

    uint8_t *raw = malloc(raw_size);
    uint8_t *zlib = malloc(zlib_size);
    if (raw == NULL || zlib == NULL) {
        free(raw);
        free(zlib);
        return 0;
    }

    for (int y = 0; y < height; y++) {
        uint8_t *row = raw + (size_t) y * (stride + 1);
        row[0] = 0;
        memcpy(row + 1, rgba + (size_t) (height - 1 - y) * stride, stride);
    }

    uint8_t *p = zlib;
    *p++ = 0x78;
    *p++ = 0x01;
    for (size_t offset = 0; offset < raw_size;) {
        const size_t len = raw_size - offset < 65535 ? raw_size - offset : 65535;
        *p++ = offset + len == raw_size ? 1 : 0;
        *p++ = (uint8_t) len;
        *p++ = (uint8_t) (len >> 8);
        *p++ = (uint8_t) ~len;
        *p++ = (uint8_t) (~len >> 8);
        memcpy(p, raw + offset, len);
        p += len;
        offset += len;
    }
    put_u32(p, adler32(raw, raw_size));
    p += 4;

    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        free(raw);
        free(zlib);
        return 0;
    }

    static const uint8_t signature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
    fwrite(signature, 1, sizeof(signature), f);

    uint8_t ihdr[13];
    put_u32(ihdr, (uint32_t) width);
    put_u32(ihdr + 4, (uint32_t) height);
    ihdr[8] = 8;
    ihdr[9] = 6;
    ihdr[10] = 0;
    ihdr[11] = 0;
    ihdr[12] = 0;

    init_crc_table();
    write_chunk(f, "IHDR", ihdr, sizeof(ihdr));
    write_chunk(f, "IDAT", zlib, (size_t) (p - zlib));
    write_chunk(f, "IEND", NULL, 0);

    const int ok = ferror(f) == 0;
    fclose(f);
    free(raw);
    free(zlib);
    return ok;
}

static void fill(const GLint x, const GLint y, const GLsizei width, const GLsizei height,
                 const GLclampf r, const GLclampf g, const GLclampf b) {
    glScissor(x, y, width, height);
    glClearColor(r, g, b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

static void draw(void) {
    glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    glEnable(GL_SCISSOR_TEST);
    for (int i = 0; i < 8; i++) {
        const GLclampf t = (GLclampf) i / 7.0f;
        fill(16 + i * 28, 16, 24, 24, t, 0.2f, 1.0f - t);
    }
    fill(16, 56, 224, 184, 0.9f, 0.9f, 0.9f);

    glColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_FALSE);
    fill(48, 88, 160, 120, 0.0f, 0.0f, 0.0f);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDisable(GL_SCISSOR_TEST);
}

int main(const int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "out.png";

    // create context
    rgl_context *ctx = rgl_create_context(WIDTH, HEIGHT);
    if (ctx == NULL) {
        fprintf(stderr, "failed to create context\n");
        return 1;
    }
    rgl_make_current(ctx);

    // render
    draw();

    // check for errors
    const GLenum error = glGetError();
    if (error != GL_NO_ERROR)
        fprintf(stderr, "gl error 0x%04x\n", error);

    // write
    const int ok = write_png(path, rgl_get_color_buffer(ctx), WIDTH, HEIGHT);
    if (ok)
        printf("wrote %s\n", path);
    else
        fprintf(stderr, "failed to write %s\n", path);

    // destroy
    rgl_destroy_context(ctx);
    return ok && error == GL_NO_ERROR ? 0 : 1;
}
