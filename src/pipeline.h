#ifndef RGL_PIPELINE_H
#define RGL_PIPELINE_H

#include "context.h"

struct rgl_fragment {
    GLint x;
    GLint y;
    GLfloat z;
    GLfloat color[4];
    GLfloat tex_coord[4];
    GLfloat tex_dx[2];
    GLfloat tex_dy[2];
    GLfloat fog_z;
};

void rgl_light_vertex(rgl_context *ctx, struct rgl_vertex *v);
void rgl_texgen(const rgl_context *ctx, const GLfloat object[4], const struct rgl_vertex *v, GLfloat out[4]);
GLboolean rgl_texgen_needs_normal(const rgl_context *ctx);
void rgl_transform_vertex(rgl_context *ctx, const GLfloat object[4], struct rgl_vertex *v);
void rgl_emit_vertex(rgl_context *ctx, const GLfloat object[4]);
void rgl_begin(rgl_context *ctx, GLenum mode);
void rgl_end(rgl_context *ctx);
void rgl_render_primitive(rgl_context *ctx);

GLboolean rgl_point_visible(const rgl_context *ctx, const struct rgl_vertex *v);
void rgl_to_window(const rgl_context *ctx, struct rgl_vertex *v);
void rgl_clip_point(rgl_context *ctx, const struct rgl_vertex *v);
void rgl_clip_line(rgl_context *ctx, const struct rgl_vertex *a, const struct rgl_vertex *b,
                   const GLfloat *flat_color);
void rgl_clip_polygon(rgl_context *ctx, const struct rgl_vertex *const *vertices, size_t count,
                      GLboolean edge_flags, const struct rgl_vertex *provoking);

void rgl_raster_point(rgl_context *ctx, const struct rgl_vertex *v, const GLfloat *color);
void rgl_raster_line(rgl_context *ctx, const struct rgl_vertex *a, const struct rgl_vertex *b,
                     const GLfloat *flat_color);
void rgl_raster_polygon(rgl_context *ctx, struct rgl_vertex *vertices, size_t count,
                        const struct rgl_vertex *provoking);

GLint rgl_integer_width(GLfloat width, GLint max);
GLboolean rgl_texturing_enabled(const rgl_context *ctx);
void rgl_texture_fragment(rgl_context *ctx, const struct rgl_fragment *frag, GLfloat color[4]);
void rgl_fog_fragment(const rgl_context *ctx, const struct rgl_fragment *frag, GLfloat color[4]);
void rgl_write_fragment(rgl_context *ctx, const struct rgl_fragment *frag);
void rgl_write_stencil(rgl_context *ctx, GLint x, GLint y, GLuint value);
void rgl_init_raster_fragment(const rgl_context *ctx, struct rgl_fragment *frag);

#endif
