#ifndef RGL_PIXEL_H
#define RGL_PIXEL_H

#include "context.h"

GLboolean rgl_is_color_format(GLenum format);
GLboolean rgl_is_numeric_type(GLenum type);

void rgl_unpack_rgba(const rgl_context *ctx, GLsizei width, GLsizei height, GLenum format, GLenum type,
                     const GLvoid *pixels, GLfloat *rgba);
void rgl_unpack_depth(const rgl_context *ctx, GLsizei width, GLsizei height, GLenum type, const GLvoid *pixels,
                      GLfloat *depth);
void rgl_unpack_stencil(const rgl_context *ctx, GLsizei width, GLsizei height, GLenum type, const GLvoid *pixels,
                        GLuint *stencil);
void rgl_unpack_bitmap(const struct rgl_pixel_store *store, GLsizei width, GLsizei height, const GLvoid *bitmap,
                       GLubyte *bits);

GLvoid *rgl_copy_image(const struct rgl_pixel_store *store, GLsizei width, GLsizei height, GLenum format,
                       GLenum type, const GLvoid *pixels);

void rgl_transfer_rgba(const rgl_context *ctx, GLfloat *rgba, size_t count);
void rgl_transfer_depth(const rgl_context *ctx, GLfloat *depth, size_t count);
GLuint rgl_transfer_stencil(const rgl_context *ctx, GLfloat index);

void rgl_pack_rgba(const struct rgl_pixel_store *store, GLsizei width, GLsizei height, GLenum format, GLenum type,
                   const GLfloat *rgba, GLboolean luminance_from_red, GLvoid *pixels);
void rgl_pack_depth(const struct rgl_pixel_store *store, GLsizei width, GLsizei height, GLenum type,
                    const GLfloat *depth, GLvoid *pixels);
void rgl_pack_index(const struct rgl_pixel_store *store, GLsizei width, GLsizei height, GLenum type,
                    const GLuint *index, GLvoid *pixels);

#endif
