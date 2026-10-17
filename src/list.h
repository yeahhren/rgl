#ifndef RGL_LIST_H
#define RGL_LIST_H

#include "context.h"

#define RGL_LIST_ARGS 16

enum rgl_op {
    RGL_OP_ACCUM,
    RGL_OP_ALPHA_FUNC,
    RGL_OP_BEGIN,
    RGL_OP_BITMAP,
    RGL_OP_BLEND_FUNC,
    RGL_OP_CALL_LIST,
    RGL_OP_CALL_LISTS,
    RGL_OP_CLEAR,
    RGL_OP_CLEAR_ACCUM,
    RGL_OP_CLEAR_COLOR,
    RGL_OP_CLEAR_DEPTH,
    RGL_OP_CLEAR_INDEX,
    RGL_OP_CLEAR_STENCIL,
    RGL_OP_CLIP_PLANE,
    RGL_OP_COLOR,
    RGL_OP_COLOR_MASK,
    RGL_OP_COLOR_MATERIAL,
    RGL_OP_COPY_PIXELS,
    RGL_OP_CULL_FACE,
    RGL_OP_DEPTH_FUNC,
    RGL_OP_DEPTH_MASK,
    RGL_OP_DEPTH_RANGE,
    RGL_OP_DISABLE,
    RGL_OP_DRAW_BUFFER,
    RGL_OP_DRAW_PIXELS,
    RGL_OP_EDGE_FLAG,
    RGL_OP_ENABLE,
    RGL_OP_END,
    RGL_OP_EVAL_COORD1,
    RGL_OP_EVAL_COORD2,
    RGL_OP_EVAL_MESH1,
    RGL_OP_EVAL_MESH2,
    RGL_OP_EVAL_POINT1,
    RGL_OP_EVAL_POINT2,
    RGL_OP_FOG,
    RGL_OP_FRONT_FACE,
    RGL_OP_FRUSTUM,
    RGL_OP_INDEX,
    RGL_OP_INDEX_MASK,
    RGL_OP_INIT_NAMES,
    RGL_OP_LIGHT,
    RGL_OP_LIGHT_MODEL,
    RGL_OP_LINE_STIPPLE,
    RGL_OP_LINE_WIDTH,
    RGL_OP_LIST_BASE,
    RGL_OP_LOAD_IDENTITY,
    RGL_OP_LOAD_MATRIX,
    RGL_OP_LOAD_NAME,
    RGL_OP_LOGIC_OP,
    RGL_OP_MAP1,
    RGL_OP_MAP2,
    RGL_OP_MAP_GRID1,
    RGL_OP_MAP_GRID2,
    RGL_OP_MATERIAL,
    RGL_OP_MATRIX_MODE,
    RGL_OP_MULT_MATRIX,
    RGL_OP_NORMAL,
    RGL_OP_ORTHO,
    RGL_OP_PASS_THROUGH,
    RGL_OP_PIXEL_MAP,
    RGL_OP_PIXEL_TRANSFER,
    RGL_OP_PIXEL_ZOOM,
    RGL_OP_POINT_SIZE,
    RGL_OP_POLYGON_MODE,
    RGL_OP_POLYGON_STIPPLE,
    RGL_OP_POP_MATRIX,
    RGL_OP_POP_NAME,
    RGL_OP_PUSH_MATRIX,
    RGL_OP_PUSH_NAME,
    RGL_OP_RASTER_POS,
    RGL_OP_READ_BUFFER,
    RGL_OP_SCISSOR,
    RGL_OP_SHADE_MODEL,
    RGL_OP_STENCIL_FUNC,
    RGL_OP_STENCIL_MASK,
    RGL_OP_STENCIL_OP,
    RGL_OP_TEX_COORD,
    RGL_OP_TEX_ENV,
    RGL_OP_TEX_GEN,
    RGL_OP_TEX_IMAGE_1D,
    RGL_OP_TEX_IMAGE_2D,
    RGL_OP_TEX_PARAMETER,
    RGL_OP_VERTEX,
    RGL_OP_VIEWPORT
};

union rgl_list_arg {
    GLint i;
    GLuint u;
    GLenum e;
    GLfloat f;
    GLdouble d;
};

struct rgl_list_node {
    enum rgl_op op;
    union rgl_list_arg args[RGL_LIST_ARGS];
    void *data;
};

GLboolean rgl_list_compiling(const rgl_context *ctx);
GLboolean rgl_save(rgl_context *ctx, enum rgl_op op, const union rgl_list_arg *args, int count, void *data);

#define SAVE(ctx, op, ...) \
    rgl_save((ctx), (op), (const union rgl_list_arg[]) {__VA_ARGS__}, \
             (int) (sizeof((const union rgl_list_arg[]) {__VA_ARGS__}) / sizeof(union rgl_list_arg)), NULL)

#define SAVE0(ctx, op) rgl_save((ctx), (op), NULL, 0, NULL)

#endif
