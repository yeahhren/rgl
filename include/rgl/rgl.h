#ifndef RGL_RGL_H
#define RGL_RGL_H

#include "gl.h"

typedef struct rgl_context rgl_context;

rgl_context *rgl_create_context(GLsizei width, GLsizei height);
void rgl_destroy_context(rgl_context *ctx);
void rgl_make_current(rgl_context *ctx);
rgl_context *rgl_get_current(void);

/* rgba8, rows bottom to top */
const GLubyte *rgl_get_color_buffer(const rgl_context *ctx);

#endif
