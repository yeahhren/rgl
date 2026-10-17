#ifndef RGL_PIXEL_H
#define RGL_PIXEL_H

#include "context.h"

GLboolean rgl_is_color_format(GLenum format);
GLboolean rgl_is_numeric_type(GLenum type);

void rgl_unpack_rgba(const struct rgl_pixel_store *store, GLsizei width, GLsizei height, GLenum format,
                     GLenum type, const GLvoid *pixels, GLfloat *rgba);
void rgl_pack_rgba(const struct rgl_pixel_store *store, GLsizei width, GLsizei height, GLenum format, GLenum type,
                   const GLfloat *rgba, GLboolean luminance_from_red, GLvoid *pixels);

#endif
