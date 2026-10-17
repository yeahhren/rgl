#ifndef RGL_CONTEXT_H
#define RGL_CONTEXT_H

#include <rgl/rgl.h>

#include <stddef.h>

#define RGL_MAX_LIGHTS 8
#define RGL_MAX_CLIP_PLANES 6
#define RGL_MAX_EVAL_MAPS 9
#define RGL_MAX_VIEWPORT_DIM 4096
#define RGL_MAX_MODELVIEW_STACK_DEPTH 32
#define RGL_MAX_PROJECTION_STACK_DEPTH 2
#define RGL_MAX_TEXTURE_STACK_DEPTH 2

#define RGL_MAX_TEXTURE_SIZE 1024
#define RGL_MAX_TEXTURE_LEVELS 11

#define RGL_MAX_POINT_SIZE 64
#define RGL_MAX_LINE_WIDTH 64

#define RGL_SUBPIXEL_BITS 4
#define RGL_DEPTH_BITS 24
#define RGL_STENCIL_BITS 8
#define RGL_DEPTH_MAX ((1u << RGL_DEPTH_BITS) - 1u)
#define RGL_STENCIL_MAX ((1u << RGL_STENCIL_BITS) - 1u)

struct rgl_framebuffer {
    GLsizei width;
    GLsizei height;
    GLubyte *color;
    GLuint *depth;
    GLubyte *stencil;
};

struct rgl_enable {
    GLboolean alpha_test;
    GLboolean auto_normal;
    GLboolean blend;
    GLboolean clip_plane[RGL_MAX_CLIP_PLANES];
    GLboolean color_material;
    GLboolean cull_face;
    GLboolean depth_test;
    GLboolean dither;
    GLboolean fog;
    GLboolean light[RGL_MAX_LIGHTS];
    GLboolean lighting;
    GLboolean line_smooth;
    GLboolean line_stipple;
    GLboolean logic_op;
    GLboolean map1[RGL_MAX_EVAL_MAPS];
    GLboolean map2[RGL_MAX_EVAL_MAPS];
    GLboolean normalize;
    GLboolean point_smooth;
    GLboolean polygon_smooth;
    GLboolean polygon_stipple;
    GLboolean scissor_test;
    GLboolean stencil_test;
    GLboolean texture_1d;
    GLboolean texture_2d;
    GLboolean texture_gen[4];
};

struct rgl_clear {
    GLclampf color[4];
    GLfloat index;
    GLclampd depth;
    GLint stencil;
    GLfloat accum[4];
};

struct rgl_mask {
    GLboolean color[4];
    GLuint index;
    GLboolean depth;
    GLuint stencil;
};

struct rgl_viewport {
    GLint x;
    GLint y;
    GLsizei width;
    GLsizei height;
    GLclampd z_near;
    GLclampd z_far;
};

struct rgl_scissor {
    GLint x;
    GLint y;
    GLsizei width;
    GLsizei height;
};

struct rgl_matrix_stack {
    GLfloat items[RGL_MAX_MODELVIEW_STACK_DEPTH][16];
    GLint depth;
    GLint max_depth;
};

struct rgl_transform {
    GLenum matrix_mode;
    struct rgl_matrix_stack modelview;
    struct rgl_matrix_stack projection;
    struct rgl_matrix_stack texture;
    GLfloat clip_plane[RGL_MAX_CLIP_PLANES][4];
    GLfloat modelview_inverse[16];
    GLboolean modelview_inverse_dirty;
};

struct rgl_point {
    GLfloat size;
};

struct rgl_line {
    GLfloat width;
    GLint stipple_factor;
    GLushort stipple_pattern;
    GLuint stipple_counter;
};

struct rgl_polygon {
    GLenum front_mode;
    GLenum back_mode;
    GLenum cull_face;
    GLenum front_face;
    GLubyte stipple[128];
};

struct rgl_color_buffer {
    GLenum alpha_func;
    GLclampf alpha_ref;
    GLenum blend_src;
    GLenum blend_dst;
    GLenum logic_op;
    GLenum draw_buffer;
};

struct rgl_depth {
    GLenum func;
};

struct rgl_stencil {
    GLenum func;
    GLint ref;
    GLuint value_mask;
    GLenum fail;
    GLenum zfail;
    GLenum zpass;
};

struct rgl_pixel_store {
    GLboolean swap_bytes;
    GLboolean lsb_first;
    GLint row_length;
    GLint skip_rows;
    GLint skip_pixels;
    GLint alignment;
};

struct rgl_pixel {
    GLenum read_buffer;
    struct rgl_pixel_store pack;
    struct rgl_pixel_store unpack;
};

struct rgl_texture_image {
    GLsizei width;
    GLsizei height;
    GLint border;
    GLint components;
    GLfloat *data;
};

struct rgl_texture {
    struct rgl_texture_image levels[RGL_MAX_TEXTURE_LEVELS];
    GLenum min_filter;
    GLenum mag_filter;
    GLenum wrap_s;
    GLenum wrap_t;
    GLfloat border_color[4];
    GLboolean complete;
    GLboolean complete_dirty;
};

struct rgl_texgen {
    GLenum mode;
    GLfloat object_plane[4];
    GLfloat eye_plane[4];
};

struct rgl_texture_state {
    struct rgl_texture texture_1d;
    struct rgl_texture texture_2d;
    GLenum env_mode;
    GLfloat env_color[4];
    struct rgl_texgen gen[4];
};

struct rgl_light {
    GLfloat ambient[4];
    GLfloat diffuse[4];
    GLfloat specular[4];
    GLfloat position[4];
    GLfloat spot_direction[3];
    GLfloat spot_exponent;
    GLfloat spot_cutoff;
    GLfloat attenuation[3];
};

struct rgl_material {
    GLfloat ambient[4];
    GLfloat diffuse[4];
    GLfloat specular[4];
    GLfloat emission[4];
    GLfloat shininess;
    GLfloat color_indexes[3];
};

struct rgl_lighting {
    GLenum shade_model;
    struct rgl_light light[RGL_MAX_LIGHTS];
    struct rgl_material material[2];
    GLfloat model_ambient[4];
    GLboolean local_viewer;
    GLboolean two_side;
    GLenum color_material_face;
    GLenum color_material_mode;
};

struct rgl_current_values {
    GLfloat color[4];
    GLfloat index;
    GLfloat normal[3];
    GLfloat tex_coord[4];
    GLboolean edge_flag;
};

struct rgl_vertex {
    GLfloat eye[4];
    GLfloat clip[4];
    GLfloat color[4];
    GLfloat back_color[4];
    GLfloat tex_coord[4];
    GLfloat normal[3];
    GLboolean edge_flag;
    GLfloat window[3];
    GLfloat inv_w;
};

struct rgl_primitive {
    GLenum mode;
    struct rgl_vertex *vertices;
    size_t count;
    size_t capacity;
    struct rgl_vertex *clip[2];
    size_t clip_capacity;
};

struct rgl_context {
    GLenum error;
    GLboolean in_begin_end;
    struct rgl_framebuffer fb;
    struct rgl_enable enable;
    struct rgl_clear clear;
    struct rgl_mask mask;
    struct rgl_viewport viewport;
    struct rgl_scissor scissor;
    struct rgl_color_buffer color_buffer;
    struct rgl_depth depth;
    struct rgl_stencil stencil;
    struct rgl_pixel pixel;
    struct rgl_texture_state texture;
    struct rgl_transform transform;
    struct rgl_point point;
    struct rgl_line line;
    struct rgl_polygon polygon;
    struct rgl_lighting lighting;
    struct rgl_current_values current;
    struct rgl_primitive primitive;
};

extern rgl_context *rgl_current;

void rgl_error(rgl_context *ctx, GLenum error);

GLfloat rgl_clampf(GLfloat x, GLfloat lo, GLfloat hi);
GLdouble rgl_clampd(GLdouble x, GLdouble lo, GLdouble hi);
GLfloat rgl_int_to_float(GLint c);
GLint rgl_float_to_int(GLfloat c);

void rgl_matrix_identity(GLfloat m[16]);
void rgl_matrix_transform(const GLfloat m[16], const GLfloat in[4], GLfloat out[4]);
GLfloat *rgl_matrix_top(struct rgl_matrix_stack *stack);
GLboolean rgl_matrix_invert(const GLfloat m[16], GLfloat out[16]);
const GLfloat *rgl_modelview_inverse(rgl_context *ctx);

void rgl_init_lighting(rgl_context *ctx);
void rgl_init_texture(rgl_context *ctx);
void rgl_free_texture(rgl_context *ctx);
void rgl_update_color_material(rgl_context *ctx);

#define GET_CONTEXT(ctx) \
    struct rgl_context *ctx = rgl_current; \
    if (ctx == NULL) \
        return

#define GET_CONTEXT_OR(ctx, ret) \
    struct rgl_context *ctx = rgl_current; \
    if (ctx == NULL) \
        return (ret)

#define OUTSIDE_BEGIN_END(ctx) \
    do { \
        if ((ctx)->in_begin_end) { \
            rgl_error((ctx), GL_INVALID_OPERATION); \
            return; \
        } \
    } while (0)

#define OUTSIDE_BEGIN_END_OR(ctx, ret) \
    do { \
        if ((ctx)->in_begin_end) { \
            rgl_error((ctx), GL_INVALID_OPERATION); \
            return (ret); \
        } \
    } while (0)

#endif
