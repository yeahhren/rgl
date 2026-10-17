#include "png.h"

#include <rgl/rgl.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RENDER_WIDTH 400
#define RENDER_HEIGHT 250
#define SCALE 2

#define WORLD_X 64
#define WORLD_Y 32
#define WORLD_Z 64
#define CHUNK 16
#define CHUNKS_X (WORLD_X / CHUNK)
#define CHUNKS_Z (WORLD_Z / CHUNK)

#define ATLAS 64
#define TILE 16
#define TILES_PER_ROW (ATLAS / TILE)

#define PI 3.14159265f
#define FOG_END 40.0f
#define REACH 6.0f

enum block {
    AIR,
    GRASS,
    DIRT,
    STONE,
    LOG,
    LEAVES,
    PLANKS,
    SAND,
    CAT,
    BLOCK_COUNT
};

enum tile {
    TILE_GRASS_TOP,
    TILE_GRASS_SIDE,
    TILE_DIRT,
    TILE_STONE,
    TILE_LOG_SIDE,
    TILE_LOG_TOP,
    TILE_LEAVES,
    TILE_PLANKS,
    TILE_SAND,
    TILE_CAT,
    TILE_COUNT
};

struct face {
    int dx;
    int dy;
    int dz;
    GLfloat shade;
    GLfloat corners[4][3];
    GLfloat uv[4][2];
};

static const struct face faces[6] = {
    {1, 0, 0, 0.8f, {{1, 0, 0}, {1, 1, 0}, {1, 1, 1}, {1, 0, 1}}, {{0, 0}, {0, 1}, {1, 1}, {1, 0}}},
    {-1, 0, 0, 0.8f, {{0, 0, 0}, {0, 0, 1}, {0, 1, 1}, {0, 1, 0}}, {{0, 0}, {1, 0}, {1, 1}, {0, 1}}},
    {0, 1, 0, 1.0f, {{0, 1, 0}, {0, 1, 1}, {1, 1, 1}, {1, 1, 0}}, {{0, 0}, {0, 1}, {1, 1}, {1, 0}}},
    {0, -1, 0, 0.5f, {{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}}, {{0, 0}, {1, 0}, {1, 1}, {0, 1}}},
    {0, 0, 1, 0.65f, {{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}}, {{0, 0}, {1, 0}, {1, 1}, {0, 1}}},
    {0, 0, -1, 0.65f, {{0, 0, 0}, {0, 1, 0}, {1, 1, 0}, {1, 0, 0}}, {{0, 0}, {0, 1}, {1, 1}, {1, 0}}},
};

static const GLfloat sky[4] = {0.55f, 0.75f, 1.0f, 1.0f};

struct player {
    GLfloat x;
    GLfloat y;
    GLfloat z;
    GLfloat vy;
    GLfloat yaw;
    GLfloat pitch;
    int on_ground;
    int selected;
};

struct target {
    int hit;
    int block[3];
    int previous[3];
};

struct input {
    int keys[256];
    int captured;
    int break_block;
    int place_block;
    int quit;
};

static unsigned char world[WORLD_X][WORLD_Y][WORLD_Z];
static GLuint chunk_lists;
static struct input input;
static GLuint frame[RENDER_WIDTH * RENDER_HEIGHT];

static unsigned hash(const int x, const int y, const int seed) {
    unsigned h = (unsigned) x * 374761393u + (unsigned) y * 668265263u + (unsigned) seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

static float random01(const int x, const int y, const int seed) {
    return (float) (hash(x, y, seed) & 0xffffu) / 65535.0f;
}

static float noise(const float x, const float y, const int seed) {
    const int ix = (int) floorf(x);
    const int iy = (int) floorf(y);
    float fx = x - (float) ix;
    float fy = y - (float) iy;
    fx = fx * fx * (3.0f - 2.0f * fx);
    fy = fy * fy * (3.0f - 2.0f * fy);

    const float a = random01(ix, iy, seed);
    const float b = random01(ix + 1, iy, seed);
    const float c = random01(ix, iy + 1, seed);
    const float d = random01(ix + 1, iy + 1, seed);
    return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy;
}

static int block_at(const int x, const int y, const int z) {
    if (y < 0)
        return STONE;
    if (x < 0 || z < 0 || x >= WORLD_X || y >= WORLD_Y || z >= WORLD_Z)
        return AIR;
    return world[x][y][z];
}

static void set_block(const int x, const int y, const int z, const int block) {
    if (x >= 0 && y >= 0 && z >= 0 && x < WORLD_X && y < WORLD_Y && z < WORLD_Z)
        world[x][y][z] = (unsigned char) block;
}

static int surface_height(const int x, const int z) {
    for (int y = WORLD_Y - 1; y >= 0; y--) {
        if (world[x][y][z] != AIR && world[x][y][z] != LEAVES)
            return y + 1;
    }
    return 0;
}

static void plant_tree(const int x, const int base, const int z) {
    const int height = 4 + (int) (random01(x, z, 11) * 2.0f);
    for (int dy = height - 2; dy <= height + 1; dy++) {
        const int radius = dy > height ? 1 : 2;
        for (int dx = -radius; dx <= radius; dx++) {
            for (int dz = -radius; dz <= radius; dz++) {
                if (abs(dx) == radius && abs(dz) == radius && random01(x + dx, z + dz, dy) < 0.6f)
                    continue;
                if (block_at(x + dx, base + dy, z + dz) == AIR)
                    set_block(x + dx, base + dy, z + dz, LEAVES);
            }
        }
    }
    for (int dy = 0; dy < height; dy++)
        set_block(x, base + dy, z, LOG);
}

static void generate_world(void) {
    for (int x = 0; x < WORLD_X; x++) {
        for (int z = 0; z < WORLD_Z; z++) {
            const float h = 9.0f + noise((float) x / 16.0f, (float) z / 16.0f, 1) * 9.0f +
                            noise((float) x / 5.0f, (float) z / 5.0f, 2) * 2.5f;
            const int height = (int) h;
            for (int y = 0; y < height && y < WORLD_Y; y++) {
                if (y < height - 4)
                    world[x][y][z] = STONE;
                else if (y < height - 1)
                    world[x][y][z] = DIRT;
                else
                    world[x][y][z] = height <= 11 ? SAND : GRASS;
            }
        }
    }

    for (int x = 3; x < WORLD_X - 3; x++) {
        for (int z = 3; z < WORLD_Z - 3; z++) {
            const int top = surface_height(x, z);
            if (top > 0 && world[x][top - 1][z] == GRASS && random01(x, z, 7) < 0.015f &&
                (abs(x - WORLD_X / 2) > 4 || abs(z - WORLD_Z / 2) > 4))
                plant_tree(x, top, z);
        }
    }

    const int mx = WORLD_X / 2 + 3;
    const int mz = WORLD_Z / 2 - 1;
    const int base = surface_height(mx, mz);
    for (int dx = 0; dx < 2; dx++) {
        for (int dz = 0; dz < 2; dz++) {
            for (int dy = 0; dy < 2; dy++)
                set_block(mx + dx, base + dy, mz + dz, CAT);
        }
    }
}

static void set_color(GLfloat c[3], const GLfloat r, const GLfloat g, const GLfloat b) {
    c[0] = r;
    c[1] = g;
    c[2] = b;
}

static void scale_color(GLfloat c[3], const GLfloat k) {
    for (int i = 0; i < 3; i++)
        c[i] *= k;
}

static void tile_pixel(const int tile, const int x, const int y, const struct png_image *cat, GLubyte out[4]) {
    const GLfloat n = random01(x + tile * 17, y, 3);
    const GLfloat v = 0.85f + 0.3f * n;
    GLfloat c[3];
    out[3] = 255;

    switch (tile) {
        case TILE_GRASS_TOP:
            set_color(c, 0.36f, 0.62f, 0.24f);
            break;
        case TILE_GRASS_SIDE:
            if (y >= 12 - (int) (random01(x, 0, 5) * 3.0f))
                set_color(c, 0.36f, 0.62f, 0.24f);
            else
                set_color(c, 0.52f, 0.36f, 0.24f);
            break;
        case TILE_DIRT:
            set_color(c, 0.52f, 0.36f, 0.24f);
            break;
        case TILE_STONE: {
            const GLfloat g = n < 0.15f ? 0.35f : 0.5f;
            set_color(c, g, g, g);
            break;
        }
        case TILE_LOG_SIDE:
            set_color(c, 0.4f, 0.3f, 0.18f);
            if (x % 4 == 0)
                scale_color(c, 0.7f);
            break;
        case TILE_LOG_TOP: {
            const GLfloat d = sqrtf((GLfloat) ((x - 8) * (x - 8) + (y - 8) * (y - 8)));
            set_color(c, 0.68f, 0.55f, 0.34f);
            if ((int) d % 3 == 0)
                scale_color(c, 0.8f);
            if (d > 7.0f)
                set_color(c, 0.4f, 0.3f, 0.18f);
            break;
        }
        case TILE_LEAVES:
            set_color(c, 0.2f, 0.5f, 0.15f);
            if (n < 0.25f)
                out[3] = 0;
            break;
        case TILE_PLANKS:
            set_color(c, 0.72f, 0.56f, 0.34f);
            if (y % 4 == 0 || x == (y / 4 % 2 ? 4 : 12))
                scale_color(c, 0.75f);
            break;
        case TILE_SAND:
            set_color(c, 0.86f, 0.82f, 0.6f);
            break;
        default:
            if (cat != NULL) {
                const int side = cat->width < cat->height ? cat->width : cat->height;
                const int sx = (cat->width - side) / 2 + (int) (((GLfloat) x + 0.5f) * (GLfloat) side / TILE);
                const int sy = (cat->height - side) / 2 + (int) (((GLfloat) y + 0.5f) * (GLfloat) side / TILE);
                memcpy(out, cat->rgba + ((size_t) sy * (size_t) cat->width + (size_t) sx) * 4, 4);
                return;
            }
            set_color(c, (x / 4 + y / 4) % 2 ? 1.0f : 0.2f, 0.1f, (x / 4 + y / 4) % 2 ? 1.0f : 0.2f);
            break;
    }

    for (int i = 0; i < 3; i++)
        out[i] = (GLubyte) fminf(c[i] * v * 255.0f, 255.0f);
}

static void load_atlas(void) {
    static GLubyte atlas[ATLAS * ATLAS * 4];
    struct png_image cat;
    const int have_cat = png_read(RGL_ASSET_DIR "/cat.png", &cat);

    for (int t = 0; t < TILE_COUNT; t++) {
        const int ox = t % TILES_PER_ROW * TILE;
        const int oy = t / TILES_PER_ROW * TILE;
        for (int y = 0; y < TILE; y++) {
            for (int x = 0; x < TILE; x++)
                tile_pixel(t, x, y, have_cat ? &cat : NULL,
                           atlas + ((size_t) (oy + y) * ATLAS + (size_t) (ox + x)) * 4);
        }
    }
    if (have_cat)
        png_free(&cat);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, 4, ATLAS, ATLAS, 0, GL_RGBA, GL_UNSIGNED_BYTE, atlas);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
}

static int block_tile(const int block, const int face) {
    switch (block) {
        case GRASS:
            return face == 2 ? TILE_GRASS_TOP : face == 3 ? TILE_DIRT : TILE_GRASS_SIDE;
        case DIRT:
            return TILE_DIRT;
        case STONE:
            return TILE_STONE;
        case LOG:
            return face == 2 || face == 3 ? TILE_LOG_TOP : TILE_LOG_SIDE;
        case LEAVES:
            return TILE_LEAVES;
        case PLANKS:
            return TILE_PLANKS;
        case SAND:
            return TILE_SAND;
        default:
            return TILE_CAT;
    }
}

static void tile_uv(const int tile, const GLfloat u, const GLfloat v, GLfloat *s, GLfloat *t) {
    const GLfloat inset = 0.001f;
    const GLfloat size = 1.0f / TILES_PER_ROW;
    const GLfloat u0 = (GLfloat) (tile % TILES_PER_ROW) * size;
    const GLfloat v0 = (GLfloat) (tile / TILES_PER_ROW) * size;
    *s = u0 + inset + u * (size - 2.0f * inset);
    *t = v0 + inset + v * (size - 2.0f * inset);
}

static void emit_face(const int x, const int y, const int z, const int block, const int f) {
    const struct face *face = &faces[f];
    const int tile = block_tile(block, f);

    glColor3f(face->shade, face->shade, face->shade);
    for (int k = 0; k < 4; k++) {
        GLfloat s;
        GLfloat t;
        tile_uv(tile, face->uv[k][0], face->uv[k][1], &s, &t);
        glTexCoord2f(s, t);
        glVertex3f((GLfloat) x + face->corners[k][0], (GLfloat) y + face->corners[k][1],
                   (GLfloat) z + face->corners[k][2]);
    }
}

static int see_through(const int block) {
    return block == AIR || block == LEAVES;
}

static void build_chunk(const int cx, const int cz) {
    glNewList(chunk_lists + (GLuint) (cx * CHUNKS_Z + cz), GL_COMPILE);
    glBegin(GL_QUADS);
    for (int x = cx * CHUNK; x < (cx + 1) * CHUNK; x++) {
        for (int z = cz * CHUNK; z < (cz + 1) * CHUNK; z++) {
            for (int y = 0; y < WORLD_Y; y++) {
                const int block = world[x][y][z];
                if (block == AIR)
                    continue;
                for (int f = 0; f < 6; f++) {
                    const int neighbor = block_at(x + faces[f].dx, y + faces[f].dy, z + faces[f].dz);
                    if (see_through(neighbor) && !(block == LEAVES && neighbor == LEAVES))
                        emit_face(x, y, z, block, f);
                }
            }
        }
    }
    glEnd();
    glEndList();
}

static void rebuild_around(const int x, const int z) {
    const int cx = x / CHUNK;
    const int cz = z / CHUNK;
    build_chunk(cx, cz);
    if (x % CHUNK == 0 && cx > 0)
        build_chunk(cx - 1, cz);
    if (x % CHUNK == CHUNK - 1 && cx < CHUNKS_X - 1)
        build_chunk(cx + 1, cz);
    if (z % CHUNK == 0 && cz > 0)
        build_chunk(cx, cz - 1);
    if (z % CHUNK == CHUNK - 1 && cz < CHUNKS_Z - 1)
        build_chunk(cx, cz + 1);
}

static int collides(const GLfloat x, const GLfloat y, const GLfloat z) {
    const GLfloat half = 0.3f;
    for (int bx = (int) floorf(x - half); bx <= (int) floorf(x + half); bx++) {
        for (int by = (int) floorf(y); by <= (int) floorf(y + 1.79f); by++) {
            for (int bz = (int) floorf(z - half); bz <= (int) floorf(z + half); bz++) {
                if (block_at(bx, by, bz) != AIR)
                    return 1;
            }
        }
    }
    return 0;
}

static void look_direction(const struct player *p, GLfloat dir[3]) {
    dir[0] = sinf(p->yaw) * cosf(p->pitch);
    dir[1] = sinf(p->pitch);
    dir[2] = -cosf(p->yaw) * cosf(p->pitch);
}

static void eye(const struct player *p, GLfloat out[3]) {
    out[0] = p->x;
    out[1] = p->y + 1.62f;
    out[2] = p->z;
}

static struct target raycast(const struct player *p) {
    struct target result = {0};
    GLfloat o[3];
    GLfloat d[3];
    eye(p, o);
    look_direction(p, d);

    int cell[3];
    int step[3];
    GLfloat t_max[3];
    GLfloat t_delta[3];
    for (int i = 0; i < 3; i++) {
        cell[i] = (int) floorf(o[i]);
        step[i] = d[i] > 0.0f ? 1 : -1;
        t_delta[i] = d[i] != 0.0f ? fabsf(1.0f / d[i]) : INFINITY;
        const GLfloat boundary = d[i] > 0.0f ? (GLfloat) cell[i] + 1.0f : (GLfloat) cell[i];
        t_max[i] = d[i] != 0.0f ? (boundary - o[i]) / d[i] : INFINITY;
    }

    memcpy(result.previous, cell, sizeof(cell));
    GLfloat t = 0.0f;
    while (t <= REACH) {
        if (block_at(cell[0], cell[1], cell[2]) != AIR) {
            result.hit = 1;
            memcpy(result.block, cell, sizeof(cell));
            return result;
        }
        memcpy(result.previous, cell, sizeof(cell));

        const int axis = t_max[0] < t_max[1] ? (t_max[0] < t_max[2] ? 0 : 2) : (t_max[1] < t_max[2] ? 1 : 2);
        cell[axis] += step[axis];
        t = t_max[axis];
        t_max[axis] += t_delta[axis];
    }
    return result;
}

static void update_player(struct player *p, const GLfloat dt) {
    const GLfloat speed = 4.3f;
    GLfloat forward = 0.0f;
    GLfloat strafe = 0.0f;
    if (input.keys['W'])
        forward += 1.0f;
    if (input.keys['S'])
        forward -= 1.0f;
    if (input.keys['D'])
        strafe += 1.0f;
    if (input.keys['A'])
        strafe -= 1.0f;

    const GLfloat len = sqrtf(forward * forward + strafe * strafe);
    if (len > 0.0f) {
        forward /= len;
        strafe /= len;
    }
    const GLfloat vx = (sinf(p->yaw) * forward + cosf(p->yaw) * strafe) * speed;
    const GLfloat vz = (-cosf(p->yaw) * forward + sinf(p->yaw) * strafe) * speed;

    if (input.keys[VK_SPACE] && p->on_ground)
        p->vy = 7.5f;
    p->vy -= 22.0f * dt;
    if (p->vy < -40.0f)
        p->vy = -40.0f;

    if (!collides(p->x + vx * dt, p->y, p->z))
        p->x += vx * dt;
    if (!collides(p->x, p->y, p->z + vz * dt))
        p->z += vz * dt;

    p->on_ground = 0;
    if (!collides(p->x, p->y + p->vy * dt, p->z)) {
        p->y += p->vy * dt;
    } else {
        if (p->vy < 0.0f) {
            p->on_ground = 1;
            p->y = floorf(p->y + 0.01f);
        }
        p->vy = 0.0f;
    }
    if (p->y < -20.0f) {
        p->x = WORLD_X / 2 + 0.5f;
        p->z = WORLD_Z / 2 + 0.5f;
        p->y = (GLfloat) surface_height(WORLD_X / 2, WORLD_Z / 2);
        p->vy = 0.0f;
    }
}

static void edit_world(const struct player *p, const struct target *target) {
    if (!target->hit)
        return;

    if (input.break_block) {
        set_block(target->block[0], target->block[1], target->block[2], AIR);
        rebuild_around(target->block[0], target->block[2]);
    }
    if (input.place_block) {
        const int *c = target->previous;
        set_block(c[0], c[1], c[2], p->selected);
        if (block_at(c[0], c[1], c[2]) == AIR)
            return;
        if (collides(p->x, p->y, p->z))
            set_block(c[0], c[1], c[2], AIR);
        else
            rebuild_around(c[0], c[2]);
    }
}

static int chunk_visible(const struct player *p, const int cx, const int cz) {
    GLfloat e[3];
    GLfloat d[3];
    eye(p, e);
    look_direction(p, d);

    const GLfloat x = (GLfloat) (cx * CHUNK) + CHUNK / 2.0f - e[0];
    const GLfloat z = (GLfloat) (cz * CHUNK) + CHUNK / 2.0f - e[2];
    const GLfloat radius = CHUNK * 0.75f + WORLD_Y * 0.5f;
    const GLfloat flat = sqrtf(d[0] * d[0] + d[2] * d[2]);
    const GLfloat ahead = flat > 0.0f ? (x * d[0] + z * d[2]) / flat : 0.0f;
    return sqrtf(x * x + z * z) < FOG_END + radius && ahead > -radius;
}

static void draw_box_outline(const int x, const int y, const int z) {
    const GLfloat e = 0.005f;
    const GLfloat lo[3] = {(GLfloat) x - e, (GLfloat) y - e, (GLfloat) z - e};
    const GLfloat hi[3] = {(GLfloat) x + 1.0f + e, (GLfloat) y + 1.0f + e, (GLfloat) z + 1.0f + e};

    glBegin(GL_LINES);
    for (int i = 0; i < 4; i++) {
        const GLfloat a = i & 1 ? hi[0] : lo[0];
        const GLfloat b = i & 2 ? hi[2] : lo[2];
        glVertex3f(a, lo[1], b);
        glVertex3f(a, hi[1], b);
        const GLfloat c = i & 1 ? hi[1] : lo[1];
        glVertex3f(lo[0], c, b);
        glVertex3f(hi[0], c, b);
        const GLfloat h = i & 2 ? hi[1] : lo[1];
        glVertex3f(a, h, lo[2]);
        glVertex3f(a, h, hi[2]);
    }
    glEnd();
}

static void draw_hud(const struct player *p) {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, RENDER_WIDTH, 0.0, RENDER_HEIGHT, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    const GLfloat cx = RENDER_WIDTH / 2.0f;
    const GLfloat cy = RENDER_HEIGHT / 2.0f;
    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_LINES);
    glVertex2f(cx - 5.0f, cy);
    glVertex2f(cx + 6.0f, cy);
    glVertex2f(cx, cy - 5.0f);
    glVertex2f(cx, cy + 6.0f);
    glEnd();

    const GLfloat size = 24.0f;
    const GLfloat x0 = RENDER_WIDTH - size - 10.0f;
    const GLfloat y0 = 10.0f;
    glColor3f(0.1f, 0.1f, 0.1f);
    glRectf(x0 - 2.0f, y0 - 2.0f, x0 + size + 2.0f, y0 + size + 2.0f);

    glEnable(GL_TEXTURE_2D);
    glColor3f(1.0f, 1.0f, 1.0f);
    const int tile = block_tile(p->selected, 4);
    GLfloat s[2];
    GLfloat t[2];
    tile_uv(tile, 0.0f, 0.0f, &s[0], &t[0]);
    tile_uv(tile, 1.0f, 1.0f, &s[1], &t[1]);
    glBegin(GL_QUADS);
    glTexCoord2f(s[0], t[0]);
    glVertex2f(x0, y0);
    glTexCoord2f(s[1], t[0]);
    glVertex2f(x0 + size, y0);
    glTexCoord2f(s[1], t[1]);
    glVertex2f(x0 + size, y0 + size);
    glTexCoord2f(s[0], t[1]);
    glVertex2f(x0, y0 + size);
    glEnd();
    glDisable(GL_TEXTURE_2D);
}

static void draw_frame(const struct player *p, const struct target *target) {
    const GLdouble aspect = (GLdouble) RENDER_WIDTH / RENDER_HEIGHT;
    const GLdouble top = 0.1 * tan(70.0 * 3.14159265 / 360.0);
    GLfloat e[3];
    eye(p, e);

    glClearColor(sky[0], sky[1], sky[2], sky[3]);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-top * aspect, top * aspect, -top, top, 0.1, 100.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glRotatef(-p->pitch * 180.0f / PI, 1.0f, 0.0f, 0.0f);
    glRotatef(p->yaw * 180.0f / PI, 0.0f, 1.0f, 0.0f);
    glTranslatef(-e[0], -e[1], -e[2]);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_ALPHA_TEST);
    glEnable(GL_FOG);
    for (int cx = 0; cx < CHUNKS_X; cx++) {
        for (int cz = 0; cz < CHUNKS_Z; cz++) {
            if (chunk_visible(p, cx, cz))
                glCallList(chunk_lists + (GLuint) (cx * CHUNKS_Z + cz));
        }
    }
    glDisable(GL_FOG);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);

    if (target->hit) {
        glDepthFunc(GL_LEQUAL);
        glColor3f(0.0f, 0.0f, 0.0f);
        draw_box_outline(target->block[0], target->block[1], target->block[2]);
        glDepthFunc(GL_LESS);
    }
    glDisable(GL_DEPTH_TEST);

    draw_hud(p);
}

static void init_gl(void) {
    glAlphaFunc(GL_GREATER, 0.5f);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogf(GL_FOG_START, FOG_END * 0.5f);
    glFogf(GL_FOG_END, FOG_END);
    glFogfv(GL_FOG_COLOR, sky);

    load_atlas();
    chunk_lists = glGenLists(CHUNKS_X * CHUNKS_Z);
    for (int cx = 0; cx < CHUNKS_X; cx++) {
        for (int cz = 0; cz < CHUNKS_Z; cz++)
            build_chunk(cx, cz);
    }
}

static void spawn(struct player *p) {
    memset(p, 0, sizeof(*p));
    p->x = WORLD_X / 2 + 0.5f;
    p->z = WORLD_Z / 2 + 0.5f;
    p->y = (GLfloat) surface_height(WORLD_X / 2, WORLD_Z / 2);
    p->yaw = PI * 0.5f;
    p->pitch = -0.15f;
    p->selected = PLANKS;
}

static void present(HWND hwnd, const rgl_context *ctx) {
    const GLubyte *src = rgl_get_color_buffer(ctx);
    for (int i = 0; i < RENDER_WIDTH * RENDER_HEIGHT; i++, src += 4)
        frame[i] = (GLuint) src[0] << 16 | (GLuint) src[1] << 8 | src[2];

    BITMAPINFO info = {0};
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = RENDER_WIDTH;
    info.bmiHeader.biHeight = RENDER_HEIGHT;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    RECT client;
    GetClientRect(hwnd, &client);
    HDC dc = GetDC(hwnd);
    StretchDIBits(dc, 0, 0, client.right, client.bottom, 0, 0, RENDER_WIDTH, RENDER_HEIGHT, frame, &info,
                  DIB_RGB_COLORS, SRCCOPY);
    ReleaseDC(hwnd, dc);
}

static void set_captured(HWND hwnd, const int captured) {
    if (input.captured == captured)
        return;
    input.captured = captured;
    ShowCursor(captured ? FALSE : TRUE);
    if (captured) {
        RECT r;
        GetClientRect(hwnd, &r);
        MapWindowPoints(hwnd, NULL, (POINT *) &r, 2);
        ClipCursor(&r);
    } else {
        ClipCursor(NULL);
    }
}

static LRESULT CALLBACK window_proc(HWND hwnd, const UINT message, const WPARAM wparam, const LPARAM lparam) {
    switch (message) {
        case WM_KEYDOWN:
            if (wparam == VK_ESCAPE) {
                if (input.captured)
                    set_captured(hwnd, 0);
                else
                    input.quit = 1;
            }
            input.keys[wparam & 0xff] = 1;
            return 0;
        case WM_KEYUP:
            input.keys[wparam & 0xff] = 0;
            return 0;
        case WM_LBUTTONDOWN:
            if (!input.captured)
                set_captured(hwnd, 1);
            else
                input.break_block = 1;
            return 0;
        case WM_RBUTTONDOWN:
            if (input.captured)
                input.place_block = 1;
            return 0;
        case WM_KILLFOCUS:
            set_captured(hwnd, 0);
            memset(input.keys, 0, sizeof(input.keys));
            return 0;
        case WM_DESTROY:
            input.quit = 1;
            return 0;
        default:
            return DefWindowProcA(hwnd, message, wparam, lparam);
    }
}

static int screenshot(rgl_context *ctx, const char *path) {
    struct player p;
    spawn(&p);
    const struct target target = raycast(&p);

    LARGE_INTEGER frequency;
    LARGE_INTEGER start;
    LARGE_INTEGER end;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&start);
    const int frames = 10;
    for (int i = 0; i < frames; i++)
        draw_frame(&p, &target);
    QueryPerformanceCounter(&end);

    const double ms = (double) (end.QuadPart - start.QuadPart) * 1000.0 / (double) frequency.QuadPart / frames;
    printf("%.1f ms per frame, gl error 0x%04x\n", ms, glGetError());
    return png_write(path, rgl_get_color_buffer(ctx), RENDER_WIDTH, RENDER_HEIGHT) ? 0 : 1;
}

static int run(rgl_context *ctx) {
    WNDCLASSA wc = {0};
    wc.lpfnWndProc = window_proc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "rgl";
    RegisterClassA(&wc);

    RECT r = {0, 0, RENDER_WIDTH * SCALE, RENDER_HEIGHT * SCALE};
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hwnd = CreateWindowA("rgl", "rgl craft", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
                              r.right - r.left, r.bottom - r.top, NULL, NULL, wc.hInstance, NULL);
    if (hwnd == NULL)
        return 1;

    struct player p;
    spawn(&p);

    LARGE_INTEGER frequency;
    LARGE_INTEGER last;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&last);
    double title_timer = 0.0;
    int frames = 0;

    while (!input.quit) {
        MSG msg;
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        const double elapsed = (double) (now.QuadPart - last.QuadPart) / (double) frequency.QuadPart;
        last = now;
        const GLfloat dt = (GLfloat) (elapsed < 0.05 ? elapsed : 0.05);

        if (input.captured) {
            RECT client;
            GetClientRect(hwnd, &client);
            POINT center = {client.right / 2, client.bottom / 2};
            ClientToScreen(hwnd, &center);
            POINT cursor;
            GetCursorPos(&cursor);
            p.yaw += (GLfloat) (cursor.x - center.x) * 0.003f;
            p.pitch -= (GLfloat) (cursor.y - center.y) * 0.003f;
            p.pitch = fmaxf(-1.55f, fminf(1.55f, p.pitch));
            SetCursorPos(center.x, center.y);
        }
        for (int i = 0; i < BLOCK_COUNT - 1; i++) {
            if (input.keys['1' + i])
                p.selected = GRASS + i;
        }

        update_player(&p, dt);
        const struct target target = raycast(&p);
        edit_world(&p, &target);
        input.break_block = 0;
        input.place_block = 0;

        draw_frame(&p, &target);
        present(hwnd, ctx);

        if (input.keys[VK_F2]) {
            png_write("screenshot.png", rgl_get_color_buffer(ctx), RENDER_WIDTH, RENDER_HEIGHT);
            input.keys[VK_F2] = 0;
        }

        frames++;
        title_timer += elapsed;
        if (title_timer >= 1.0) {
            char title[64];
            snprintf(title, sizeof(title), "rgl craft - %d fps", (int) ((double) frames / title_timer));
            SetWindowTextA(hwnd, title);
            frames = 0;
            title_timer = 0.0;
        }
    }

    set_captured(hwnd, 0);
    DestroyWindow(hwnd);
    return 0;
}

int main(const int argc, char **argv) {
    rgl_context *ctx = rgl_create_context(RENDER_WIDTH, RENDER_HEIGHT);
    if (ctx == NULL) {
        fprintf(stderr, "failed to create context\n");
        return 1;
    }
    rgl_make_current(ctx);

    generate_world();
    init_gl();

    const int status = argc > 2 && strcmp(argv[1], "--screenshot") == 0 ? screenshot(ctx, argv[2]) : run(ctx);
    rgl_destroy_context(ctx);
    return status;
}
