#include "png.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_BITS 15

static const uint8_t signature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};

struct buffer {
    uint8_t *data;
    size_t size;
    size_t capacity;
};

static int buffer_reserve(struct buffer *b, const size_t extra) {
    if (b->size + extra <= b->capacity)
        return 1;

    size_t capacity = b->capacity ? b->capacity : 4096;
    while (capacity < b->size + extra)
        capacity *= 2;
    uint8_t *data = realloc(b->data, capacity);
    if (data == NULL)
        return 0;
    b->data = data;
    b->capacity = capacity;
    return 1;
}

static int buffer_append(struct buffer *b, const uint8_t *data, const size_t size) {
    if (!buffer_reserve(b, size))
        return 0;
    memcpy(b->data + b->size, data, size);
    b->size += size;
    return 1;
}

struct bit_reader {
    const uint8_t *data;
    size_t size;
    size_t pos;
    uint32_t bits;
    int count;
    int error;
};

static int get_bits(struct bit_reader *br, const int n) {
    uint32_t v = br->bits;
    while (br->count < n) {
        if (br->pos >= br->size) {
            br->error = 1;
            return 0;
        }
        v |= (uint32_t) br->data[br->pos++] << br->count;
        br->count += 8;
    }
    br->bits = v >> n;
    br->count -= n;
    return (int) (v & ((1u << n) - 1u));
}

struct huffman {
    short count[MAX_BITS + 1];
    short symbol[288];
};

static int build_huffman(struct huffman *h, const short *lengths, const int n) {
    short offsets[MAX_BITS + 1];

    memset(h->count, 0, sizeof(h->count));
    for (int s = 0; s < n; s++)
        h->count[lengths[s]]++;
    if (h->count[0] == n)
        return 0;

    int left = 1;
    for (int len = 1; len <= MAX_BITS; len++) {
        left <<= 1;
        left -= h->count[len];
        if (left < 0)
            return -1;
    }

    offsets[1] = 0;
    for (int len = 1; len < MAX_BITS; len++)
        offsets[len + 1] = (short) (offsets[len] + h->count[len]);
    for (int s = 0; s < n; s++) {
        if (lengths[s] != 0)
            h->symbol[offsets[lengths[s]]++] = (short) s;
    }
    return left;
}

static int decode(struct bit_reader *br, const struct huffman *h) {
    int code = 0;
    int first = 0;
    int index = 0;
    for (int len = 1; len <= MAX_BITS; len++) {
        code |= get_bits(br, 1);
        const int count = h->count[len];
        if (code - count < first)
            return h->symbol[index + (code - first)];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    return -1;
}

static const short length_base[29] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
};
static const short length_extra[29] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
};
static const short dist_base[30] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097,
    6145, 8193, 12289, 16385, 24577
};
static const short dist_extra[30] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
};

static int inflate_codes(struct bit_reader *br, struct buffer *out, const struct huffman *lencode,
                         const struct huffman *distcode) {
    for (;;) {
        int symbol = decode(br, lencode);
        if (symbol < 0 || br->error)
            return 0;
        if (symbol < 256) {
            const uint8_t byte = (uint8_t) symbol;
            if (!buffer_append(out, &byte, 1))
                return 0;
            continue;
        }
        if (symbol == 256)
            return 1;

        symbol -= 257;
        if (symbol >= 29)
            return 0;
        const size_t length = (size_t) (length_base[symbol] + get_bits(br, length_extra[symbol]));

        const int dsym = decode(br, distcode);
        if (dsym < 0 || dsym >= 30)
            return 0;
        const size_t dist = (size_t) (dist_base[dsym] + get_bits(br, dist_extra[dsym]));
        if (br->error || dist > out->size || !buffer_reserve(out, length))
            return 0;

        for (size_t i = 0; i < length; i++) {
            out->data[out->size] = out->data[out->size - dist];
            out->size++;
        }
    }
}

static int inflate_stored(struct bit_reader *br, struct buffer *out) {
    br->bits = 0;
    br->count = 0;
    if (br->pos + 4 > br->size)
        return 0;

    const size_t len = (size_t) br->data[br->pos] | (size_t) br->data[br->pos + 1] << 8;
    const size_t nlen = (size_t) br->data[br->pos + 2] | (size_t) br->data[br->pos + 3] << 8;
    br->pos += 4;
    if (len != (~nlen & 0xffffu) || br->pos + len > br->size)
        return 0;
    if (!buffer_append(out, br->data + br->pos, len))
        return 0;
    br->pos += len;
    return 1;
}

static int inflate_fixed(struct bit_reader *br, struct buffer *out) {
    short lengths[288];
    struct huffman lencode;
    struct huffman distcode;

    for (int s = 0; s < 288; s++)
        lengths[s] = (short) (s < 144 ? 8 : s < 256 ? 9 : s < 280 ? 7 : 8);
    build_huffman(&lencode, lengths, 288);
    for (int s = 0; s < 30; s++)
        lengths[s] = 5;
    build_huffman(&distcode, lengths, 30);
    return inflate_codes(br, out, &lencode, &distcode);
}

static int inflate_dynamic(struct bit_reader *br, struct buffer *out) {
    static const short order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
    short lengths[320];
    struct huffman lencode;
    struct huffman distcode;

    const int nlen = get_bits(br, 5) + 257;
    const int ndist = get_bits(br, 5) + 1;
    const int ncode = get_bits(br, 4) + 4;
    if (br->error || nlen > 286 || ndist > 30)
        return 0;

    memset(lengths, 0, sizeof(lengths));
    for (int i = 0; i < ncode; i++)
        lengths[order[i]] = (short) get_bits(br, 3);
    if (build_huffman(&lencode, lengths, 19) != 0)
        return 0;

    int index = 0;
    while (index < nlen + ndist) {
        const int symbol = decode(br, &lencode);
        if (symbol < 0 || br->error)
            return 0;
        if (symbol < 16) {
            lengths[index++] = (short) symbol;
            continue;
        }

        short value = 0;
        int repeat;
        if (symbol == 16) {
            if (index == 0)
                return 0;
            value = lengths[index - 1];
            repeat = 3 + get_bits(br, 2);
        } else if (symbol == 17) {
            repeat = 3 + get_bits(br, 3);
        } else {
            repeat = 11 + get_bits(br, 7);
        }
        if (index + repeat > nlen + ndist)
            return 0;
        while (repeat--)
            lengths[index++] = value;
    }

    const int lerr = build_huffman(&lencode, lengths, nlen);
    if (lerr < 0 || (lerr > 0 && nlen - lencode.count[0] != 1))
        return 0;
    const int derr = build_huffman(&distcode, lengths + nlen, ndist);
    if (derr < 0 || (derr > 0 && ndist - distcode.count[0] != 1))
        return 0;
    return inflate_codes(br, out, &lencode, &distcode);
}

static int zlib_inflate(const uint8_t *data, const size_t size, struct buffer *out) {
    if (size < 2 || (data[0] & 0x0f) != 8 || (data[0] << 8 | data[1]) % 31 != 0)
        return 0;

    struct bit_reader br = {.data = data, .size = size, .pos = 2};
    int last;
    do {
        last = get_bits(&br, 1);
        const int type = get_bits(&br, 2);
        int ok;
        switch (type) {
            case 0:
                ok = inflate_stored(&br, out);
                break;
            case 1:
                ok = inflate_fixed(&br, out);
                break;
            case 2:
                ok = inflate_dynamic(&br, out);
                break;
            default:
                ok = 0;
                break;
        }
        if (!ok || br.error)
            return 0;
    } while (!last);
    return 1;
}

static uint32_t get_u32(const uint8_t *p) {
    return (uint32_t) p[0] << 24 | (uint32_t) p[1] << 16 | (uint32_t) p[2] << 8 | p[3];
}

static uint8_t paeth(const int a, const int b, const int c) {
    const int p = a + b - c;
    const int pa = abs(p - a);
    const int pb = abs(p - b);
    const int pc = abs(p - c);
    if (pa <= pb && pa <= pc)
        return (uint8_t) a;
    return (uint8_t) (pb <= pc ? b : c);
}

static int unfilter(uint8_t *data, const size_t size, const int width, const int height, const int bpp) {
    const size_t stride = (size_t) width * (size_t) bpp;
    if (size < (stride + 1) * (size_t) height)
        return 0;

    uint8_t *prev = NULL;
    for (int y = 0; y < height; y++) {
        uint8_t *row = data + (size_t) y * (stride + 1);
        const int filter = row[0];
        uint8_t *cur = row + 1;
        for (size_t x = 0; x < stride; x++) {
            const int a = x >= (size_t) bpp ? cur[x - (size_t) bpp] : 0;
            const int b = prev ? prev[x] : 0;
            const int c = prev && x >= (size_t) bpp ? prev[x - (size_t) bpp] : 0;
            switch (filter) {
                case 0:
                    break;
                case 1:
                    cur[x] = (uint8_t) (cur[x] + a);
                    break;
                case 2:
                    cur[x] = (uint8_t) (cur[x] + b);
                    break;
                case 3:
                    cur[x] = (uint8_t) (cur[x] + (a + b) / 2);
                    break;
                case 4:
                    cur[x] = (uint8_t) (cur[x] + paeth(a, b, c));
                    break;
                default:
                    return 0;
            }
        }
        prev = cur;
    }
    return 1;
}

static uint8_t *read_file(const char *path, size_t *size) {
    FILE *f = fopen(path, "rb");
    if (f == NULL)
        return NULL;

    struct buffer b = {0};
    uint8_t chunk[65536];
    size_t n;
    while ((n = fread(chunk, 1, sizeof(chunk), f)) > 0) {
        if (!buffer_append(&b, chunk, n)) {
            free(b.data);
            fclose(f);
            return NULL;
        }
    }
    fclose(f);
    *size = b.size;
    return b.data;
}

int png_read(const char *path, struct png_image *image) {
    size_t size = 0;
    uint8_t *file = read_file(path, &size);
    if (file == NULL)
        return 0;

    int ok = 0;
    int width = 0;
    int height = 0;
    int channels = 0;
    struct buffer idat = {0};
    struct buffer raw = {0};

    if (size < 8 || memcmp(file, signature, 8) != 0)
        goto done;

    for (size_t pos = 8; pos + 12 <= size;) {
        const uint32_t length = get_u32(file + pos);
        const uint8_t *type = file + pos + 4;
        const uint8_t *data = file + pos + 8;
        if (pos + 12 + length > size)
            goto done;

        if (memcmp(type, "IHDR", 4) == 0) {
            if (length < 13 || data[8] != 8 || data[10] != 0 || data[11] != 0 || data[12] != 0)
                goto done;
            width = (int) get_u32(data);
            height = (int) get_u32(data + 4);
            switch (data[9]) {
                case 0:
                    channels = 1;
                    break;
                case 2:
                    channels = 3;
                    break;
                case 4:
                    channels = 2;
                    break;
                case 6:
                    channels = 4;
                    break;
                default:
                    goto done;
            }
        } else if (memcmp(type, "IDAT", 4) == 0) {
            if (!buffer_append(&idat, data, length))
                goto done;
        } else if (memcmp(type, "IEND", 4) == 0) {
            break;
        }
        pos += 12 + length;
    }

    if (width <= 0 || height <= 0 || channels == 0 || !zlib_inflate(idat.data, idat.size, &raw) ||
        !unfilter(raw.data, raw.size, width, height, channels))
        goto done;

    image->rgba = malloc((size_t) width * (size_t) height * 4);
    if (image->rgba == NULL)
        goto done;

    for (int y = 0; y < height; y++) {
        const uint8_t *src = raw.data + (size_t) y * ((size_t) width * (size_t) channels + 1) + 1;
        uint8_t *dst = image->rgba + (size_t) (height - 1 - y) * (size_t) width * 4;
        for (int x = 0; x < width; x++, src += channels, dst += 4) {
            const int gray = channels <= 2;
            dst[0] = src[0];
            dst[1] = gray ? src[0] : src[1];
            dst[2] = gray ? src[0] : src[2];
            dst[3] = channels == 2 ? src[1] : channels == 4 ? src[3] : 255;
        }
    }
    image->width = width;
    image->height = height;
    ok = 1;

done:
    free(file);
    free(idat.data);
    free(raw.data);
    return ok;
}

void png_free(struct png_image *image) {
    free(image->rgba);
    image->rgba = NULL;
}

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
    if (size > 0)
        fwrite(data, 1, size, f);

    uint8_t crc[4];
    put_u32(crc, crc32(crc32(0xffffffffu, header + 4, 4), data, size) ^ 0xffffffffu);
    fwrite(crc, 1, 4, f);
}

/* zlib stream of stored deflate blocks */
int png_write(const char *path, const uint8_t *rgba, const int width, const int height) {
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

    uint8_t ihdr[13];
    put_u32(ihdr, (uint32_t) width);
    put_u32(ihdr + 4, (uint32_t) height);
    ihdr[8] = 8;
    ihdr[9] = 6;
    ihdr[10] = 0;
    ihdr[11] = 0;
    ihdr[12] = 0;

    init_crc_table();
    fwrite(signature, 1, sizeof(signature), f);
    write_chunk(f, "IHDR", ihdr, sizeof(ihdr));
    write_chunk(f, "IDAT", zlib, (size_t) (p - zlib));
    write_chunk(f, "IEND", NULL, 0);

    const int ok = ferror(f) == 0;
    fclose(f);
    free(raw);
    free(zlib);
    return ok;
}
