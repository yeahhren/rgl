#ifndef RGL_PIPELINE_H
#define RGL_PIPELINE_H

#include "context.h"

struct rgl_fragment {
    GLint x;
    GLint y;
    GLfloat z;
    GLfloat color[4];
    GLfloat tex_coord[4];
};

void rgl_light_vertex(rgl_context *ctx, struct rgl_vertex *v);
void rgl_render_primitive(rgl_context *ctx);

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
void rgl_write_fragment(rgl_context *ctx, const struct rgl_fragment *frag);

#endif
