#include <rgl/rgl.h>

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 512
#define HEIGHT 512

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

static void cell(const GLint x, const GLint y) {
    glViewport(x, y, WIDTH / 2, HEIGHT / 2);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

static void draw_shading(void) {
    cell(0, 0);

    glBegin(GL_TRIANGLES);
    glColor3f(1.0f, 0.0f, 0.0f);
    glVertex2f(-0.9f, -0.9f);
    glColor3f(0.0f, 1.0f, 0.0f);
    glVertex2f(0.1f, -0.9f);
    glColor3f(0.0f, 0.0f, 1.0f);
    glVertex2f(-0.4f, 0.0f);
    glEnd();

    glShadeModel(GL_FLAT);
    glBegin(GL_TRIANGLE_FAN);
    glColor3f(1.0f, 1.0f, 1.0f);
    glVertex2f(0.5f, 0.5f);
    for (int i = 0; i <= 6; i++) {
        const float a = (float) i * 6.2831853f / 6.0f;
        glColor3f(0.5f + 0.5f * cosf(a), 0.5f + 0.5f * sinf(a), 0.5f);
        glVertex2f(0.5f + 0.4f * cosf(a), 0.5f + 0.4f * sinf(a));
    }
    glEnd();
    glShadeModel(GL_SMOOTH);

    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= 8; i++) {
        const float x = -0.9f + (float) i * 0.1f;
        glColor3f((float) i / 8.0f, 0.3f, 1.0f - (float) i / 8.0f);
        glVertex2f(x, 0.2f);
        glVertex2f(x, 0.8f);
    }
    glEnd();
}

static void draw_lines(void) {
    cell(WIDTH / 2, 0);

    glBegin(GL_LINE_STRIP);
    for (int i = 0; i <= 32; i++) {
        const float x = -0.9f + (float) i * 1.8f / 32.0f;
        glColor3f(1.0f, (float) i / 32.0f, 0.2f);
        glVertex2f(x, -0.6f + 0.2f * sinf(x * 8.0f));
    }
    glEnd();

    glLineWidth(4.0f);
    glBegin(GL_LINES);
    glColor3f(0.2f, 0.8f, 1.0f);
    glVertex2f(-0.9f, -0.2f);
    glVertex2f(0.9f, 0.0f);
    glEnd();
    glLineWidth(1.0f);

    glEnable(GL_LINE_STIPPLE);
    glLineStipple(2, 0x0f0f);
    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(-0.8f, 0.2f);
    glVertex2f(0.8f, 0.2f);
    glVertex2f(0.0f, 0.9f);
    glEnd();
    glDisable(GL_LINE_STIPPLE);

    for (int i = 1; i <= 8; i++) {
        glPointSize((float) i);
        glBegin(GL_POINTS);
        glColor3f(1.0f, 1.0f, 0.0f);
        glVertex2f(-0.9f + (float) i * 0.2f, -0.85f);
        glEnd();
    }
    glPointSize(1.0f);
}

static void draw_cube(void) {
    static const float v[8][3] = {
        {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
        {-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1},
    };
    static const int faces[6][4] = {
        {0, 3, 2, 1}, {4, 5, 6, 7}, {0, 1, 5, 4},
        {2, 3, 7, 6}, {0, 4, 7, 3}, {1, 2, 6, 5},
    };

    glViewport(0, HEIGHT / 2, WIDTH / 2, HEIGHT / 2);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-1.0, 1.0, -1.0, 1.0, 1.5, 20.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0f, 0.0f, -4.0f);
    glRotatef(30.0f, 1.0f, 0.0f, 0.0f);
    glRotatef(-40.0f, 0.0f, 1.0f, 0.0f);

    glEnable(GL_CULL_FACE);
    glBegin(GL_QUADS);
    for (int f = 0; f < 6; f++) {
        for (int k = 0; k < 4; k++) {
            const float *p = v[faces[f][k]];
            glColor3f(0.5f + 0.5f * p[0], 0.5f + 0.5f * p[1], 0.5f + 0.5f * p[2]);
            glVertex3fv(p);
        }
    }
    glEnd();
    glDisable(GL_CULL_FACE);

    glLoadIdentity();
    glTranslatef(0.0f, 0.0f, -2.0f);
    glRotatef(45.0f, 0.0f, 1.0f, 0.0f);
    glScalef(0.8f, 0.8f, 0.8f);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);
    for (int f = 0; f < 6; f++) {
        for (int k = 0; k < 4; k++)
            glVertex3fv(v[faces[f][k]]);
    }
    glEnd();
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

static void draw_polygons(void) {
    static GLubyte stipple[128];
    for (int i = 0; i < 128; i++)
        stipple[i] = (i / 4 / 4) % 2 ? 0xf0 : 0x0f;

    cell(WIDTH / 2, HEIGHT / 2);

    glEnable(GL_POLYGON_STIPPLE);
    glPolygonStipple(stipple);
    glColor3f(0.9f, 0.6f, 0.2f);
    glRectf(-0.9f, 0.1f, -0.1f, 0.9f);
    glDisable(GL_POLYGON_STIPPLE);

    glPolygonMode(GL_FRONT, GL_LINE);
    glBegin(GL_POLYGON);
    for (int i = 0; i < 5; i++) {
        const float a = 1.5707963f + (float) i * 6.2831853f / 5.0f;
        glColor3f(0.3f + 0.7f * (float) (i % 2), 1.0f, 0.5f);
        glEdgeFlag(i != 2);
        glVertex2f(0.5f + 0.4f * cosf(a), 0.5f + 0.4f * sinf(a));
    }
    glEnd();
    glEdgeFlag(GL_TRUE);

    glPolygonMode(GL_FRONT, GL_POINT);
    glPointSize(5.0f);
    glColor3f(1.0f, 0.3f, 0.6f);
    glRectf(-0.8f, -0.8f, -0.2f, -0.2f);
    glPointSize(1.0f);
    glPolygonMode(GL_FRONT, GL_FILL);

    static const GLdouble plane[4] = {1.0, 1.0, 0.0, 0.0};
    glClipPlane(GL_CLIP_PLANE0, plane);
    glEnable(GL_CLIP_PLANE0);
    glBegin(GL_TRIANGLES);
    glColor3f(0.2f, 0.4f, 1.0f);
    glVertex2f(0.0f, -1.5f);
    glColor3f(0.2f, 1.0f, 0.8f);
    glVertex2f(1.5f, -1.0f);
    glColor3f(1.0f, 1.0f, 1.0f);
    glVertex2f(0.3f, 0.0f);
    glEnd();
    glDisable(GL_CLIP_PLANE0);
}

static void draw(void) {
    glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    draw_shading();
    draw_lines();
    draw_cube();
    draw_polygons();
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
