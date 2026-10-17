#include "list.h"

#include <stdlib.h>
#include <string.h>

// wasn't fun to write
// works tho
// written in 20 minutes:tm:

GLboolean rgl_list_compiling(const rgl_context *ctx) {
    return ctx->list.compiling != 0 && ctx->list.depth == 0;
}

GLboolean rgl_save(rgl_context *ctx, const enum rgl_op op, const union rgl_list_arg *args, const int count,
                   void *data) {
    if (!rgl_list_compiling(ctx)) {
        free(data);
        return GL_FALSE;
    }

    struct rgl_list_state *l = &ctx->list;
    if (l->build_count == l->build_capacity) {
        const size_t capacity = l->build_capacity ? l->build_capacity * 2 : 64;
        struct rgl_list_node *nodes = realloc(l->build, capacity * sizeof(*nodes));
        if (nodes == NULL) {
            rgl_error(ctx, GL_OUT_OF_MEMORY);
            free(data);
            return l->mode == GL_COMPILE;
        }
        l->build = nodes;
        l->build_capacity = capacity;
    }

    struct rgl_list_node *node = &l->build[l->build_count++];
    memset(node, 0, sizeof(*node));
    node->op = op;
    if (count > 0)
        memcpy(node->args, args, (size_t) count * sizeof(*args));
    node->data = data;
    return l->mode == GL_COMPILE;
}

static void free_nodes(struct rgl_list_node *nodes, const size_t count) {
    for (size_t i = 0; i < count; i++)
        free(nodes[i].data);
    free(nodes);
}

static size_t lower_bound(const struct rgl_list_state *l, const GLuint name) {
    size_t lo = 0;
    size_t hi = l->count;
    while (lo < hi) {
        const size_t mid = lo + (hi - lo) / 2;
        if (l->lists[mid].name < name)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo;
}

static struct rgl_display_list *find_list(const struct rgl_list_state *l, const GLuint name) {
    const size_t i = lower_bound(l, name);
    return i < l->count && l->lists[i].name == name ? &l->lists[i] : NULL;
}

static GLboolean store_list(struct rgl_list_state *l, const GLuint name, struct rgl_list_node *nodes,
                            const size_t count) {
    const size_t i = lower_bound(l, name);
    if (i < l->count && l->lists[i].name == name) {
        free_nodes(l->lists[i].nodes, l->lists[i].count);
        l->lists[i].nodes = nodes;
        l->lists[i].count = count;
        return GL_TRUE;
    }

    if (l->count == l->capacity) {
        const size_t capacity = l->capacity ? l->capacity * 2 : 16;
        struct rgl_display_list *lists = realloc(l->lists, capacity * sizeof(*lists));
        if (lists == NULL)
            return GL_FALSE;
        l->lists = lists;
        l->capacity = capacity;
    }

    memmove(&l->lists[i + 1], &l->lists[i], (l->count - i) * sizeof(*l->lists));
    l->lists[i] = (struct rgl_display_list) {.name = name, .nodes = nodes, .count = count};
    l->count++;
    return GL_TRUE;
}

void rgl_free_lists(rgl_context *ctx) {
    struct rgl_list_state *l = &ctx->list;
    for (size_t i = 0; i < l->count; i++)
        free_nodes(l->lists[i].nodes, l->lists[i].count);
    free(l->lists);
    free_nodes(l->build, l->build_count);
}

static void floats(const union rgl_list_arg *a, GLfloat *out, const int n) {
    for (int i = 0; i < n; i++)
        out[i] = a[i].f;
}

static void doubles(const union rgl_list_arg *a, GLdouble *out, const int n) {
    for (int i = 0; i < n; i++)
        out[i] = a[i].d;
}

static void call_list(rgl_context *ctx, GLuint name);

static void execute_unpacked(rgl_context *ctx, const struct rgl_list_node *n) {
    const union rgl_list_arg *a = n->args;
    const struct rgl_pixel_store saved = ctx->pixel.unpack;
    ctx->pixel.unpack = (struct rgl_pixel_store) {.alignment = 1};

    switch (n->op) {
        case RGL_OP_BITMAP:
            glBitmap(a[0].i, a[1].i, a[2].f, a[3].f, a[4].f, a[5].f, n->data);
            break;
        case RGL_OP_DRAW_PIXELS:
            glDrawPixels(a[0].i, a[1].i, a[2].e, a[3].e, n->data);
            break;
        case RGL_OP_POLYGON_STIPPLE:
            glPolygonStipple(n->data);
            break;
        case RGL_OP_TEX_IMAGE_1D:
            glTexImage1D(a[0].e, a[1].i, a[2].i, a[3].i, a[4].i, a[5].e, a[6].e, n->data);
            break;
        default:
            glTexImage2D(a[0].e, a[1].i, a[2].i, a[3].i, a[4].i, a[5].i, a[6].e, a[7].e, n->data);
            break;
    }

    ctx->pixel.unpack = saved;
}

static void execute_node(rgl_context *ctx, const struct rgl_list_node *n) {
    const union rgl_list_arg *a = n->args;
    GLfloat f[16];
    GLdouble d[4];

    switch (n->op) {
        case RGL_OP_ACCUM:
            glAccum(a[0].e, a[1].f);
            break;
        case RGL_OP_ALPHA_FUNC:
            glAlphaFunc(a[0].e, a[1].f);
            break;
        case RGL_OP_BEGIN:
            glBegin(a[0].e);
            break;
        case RGL_OP_BITMAP:
        case RGL_OP_DRAW_PIXELS:
        case RGL_OP_POLYGON_STIPPLE:
        case RGL_OP_TEX_IMAGE_1D:
        case RGL_OP_TEX_IMAGE_2D:
            execute_unpacked(ctx, n);
            break;
        case RGL_OP_BLEND_FUNC:
            glBlendFunc(a[0].e, a[1].e);
            break;
        case RGL_OP_CALL_LIST:
            call_list(ctx, a[0].u);
            break;
        case RGL_OP_CALL_LISTS: {
            const GLuint *names = n->data;
            for (GLint i = 0; i < a[0].i; i++)
                call_list(ctx, ctx->list.base + names[i]);
            break;
        }
        case RGL_OP_CLEAR:
            glClear(a[0].u);
            break;
        case RGL_OP_CLEAR_ACCUM:
            glClearAccum(a[0].f, a[1].f, a[2].f, a[3].f);
            break;
        case RGL_OP_CLEAR_COLOR:
            glClearColor(a[0].f, a[1].f, a[2].f, a[3].f);
            break;
        case RGL_OP_CLEAR_DEPTH:
            glClearDepth(a[0].d);
            break;
        case RGL_OP_CLEAR_INDEX:
            glClearIndex(a[0].f);
            break;
        case RGL_OP_CLEAR_STENCIL:
            glClearStencil(a[0].i);
            break;
        case RGL_OP_CLIP_PLANE:
            doubles(a + 1, d, 4);
            glClipPlane(a[0].e, d);
            break;
        case RGL_OP_COLOR:
            glColor4f(a[0].f, a[1].f, a[2].f, a[3].f);
            break;
        case RGL_OP_COLOR_MASK:
            glColorMask((GLboolean) a[0].i, (GLboolean) a[1].i, (GLboolean) a[2].i, (GLboolean) a[3].i);
            break;
        case RGL_OP_COLOR_MATERIAL:
            glColorMaterial(a[0].e, a[1].e);
            break;
        case RGL_OP_COPY_PIXELS:
            glCopyPixels(a[0].i, a[1].i, a[2].i, a[3].i, a[4].e);
            break;
        case RGL_OP_CULL_FACE:
            glCullFace(a[0].e);
            break;
        case RGL_OP_DEPTH_FUNC:
            glDepthFunc(a[0].e);
            break;
        case RGL_OP_DEPTH_MASK:
            glDepthMask((GLboolean) a[0].i);
            break;
        case RGL_OP_DEPTH_RANGE:
            glDepthRange(a[0].d, a[1].d);
            break;
        case RGL_OP_DISABLE:
            glDisable(a[0].e);
            break;
        case RGL_OP_DRAW_BUFFER:
            glDrawBuffer(a[0].e);
            break;
        case RGL_OP_EDGE_FLAG:
            glEdgeFlag((GLboolean) a[0].i);
            break;
        case RGL_OP_ENABLE:
            glEnable(a[0].e);
            break;
        case RGL_OP_END:
            glEnd();
            break;
        case RGL_OP_EVAL_COORD1:
            glEvalCoord1f(a[0].f);
            break;
        case RGL_OP_EVAL_COORD2:
            glEvalCoord2f(a[0].f, a[1].f);
            break;
        case RGL_OP_EVAL_MESH1:
            glEvalMesh1(a[0].e, a[1].i, a[2].i);
            break;
        case RGL_OP_EVAL_MESH2:
            glEvalMesh2(a[0].e, a[1].i, a[2].i, a[3].i, a[4].i);
            break;
        case RGL_OP_EVAL_POINT1:
            glEvalPoint1(a[0].i);
            break;
        case RGL_OP_EVAL_POINT2:
            glEvalPoint2(a[0].i, a[1].i);
            break;
        case RGL_OP_FOG:
            floats(a + 1, f, 4);
            glFogfv(a[0].e, f);
            break;
        case RGL_OP_FRONT_FACE:
            glFrontFace(a[0].e);
            break;
        case RGL_OP_FRUSTUM:
            glFrustum(a[0].d, a[1].d, a[2].d, a[3].d, a[4].d, a[5].d);
            break;
        case RGL_OP_HINT:
            glHint(a[0].e, a[1].e);
            break;
        case RGL_OP_INDEX:
            glIndexf(a[0].f);
            break;
        case RGL_OP_INDEX_MASK:
            glIndexMask(a[0].u);
            break;
        case RGL_OP_INIT_NAMES:
            glInitNames();
            break;
        case RGL_OP_LIGHT:
            floats(a + 2, f, 4);
            glLightfv(a[0].e, a[1].e, f);
            break;
        case RGL_OP_LIGHT_MODEL:
            floats(a + 1, f, 4);
            glLightModelfv(a[0].e, f);
            break;
        case RGL_OP_LINE_STIPPLE:
            glLineStipple(a[0].i, (GLushort) a[1].u);
            break;
        case RGL_OP_LINE_WIDTH:
            glLineWidth(a[0].f);
            break;
        case RGL_OP_LIST_BASE:
            glListBase(a[0].u);
            break;
        case RGL_OP_LOAD_IDENTITY:
            glLoadIdentity();
            break;
        case RGL_OP_LOAD_MATRIX:
            floats(a, f, 16);
            glLoadMatrixf(f);
            break;
        case RGL_OP_LOAD_NAME:
            glLoadName(a[0].u);
            break;
        case RGL_OP_LOGIC_OP:
            glLogicOp(a[0].e);
            break;
        case RGL_OP_MAP1:
            glMap1f(a[0].e, a[1].f, a[2].f, a[3].i, a[4].i, n->data);
            break;
        case RGL_OP_MAP2:
            glMap2f(a[0].e, a[1].f, a[2].f, a[3].i, a[4].i, a[5].f, a[6].f, a[7].i, a[8].i, n->data);
            break;
        case RGL_OP_MAP_GRID1:
            glMapGrid1f(a[0].i, a[1].f, a[2].f);
            break;
        case RGL_OP_MAP_GRID2:
            glMapGrid2f(a[0].i, a[1].f, a[2].f, a[3].i, a[4].f, a[5].f);
            break;
        case RGL_OP_MATERIAL:
            floats(a + 2, f, 4);
            glMaterialfv(a[0].e, a[1].e, f);
            break;
        case RGL_OP_MATRIX_MODE:
            glMatrixMode(a[0].e);
            break;
        case RGL_OP_MULT_MATRIX:
            floats(a, f, 16);
            glMultMatrixf(f);
            break;
        case RGL_OP_NORMAL:
            glNormal3f(a[0].f, a[1].f, a[2].f);
            break;
        case RGL_OP_ORTHO:
            glOrtho(a[0].d, a[1].d, a[2].d, a[3].d, a[4].d, a[5].d);
            break;
        case RGL_OP_PASS_THROUGH:
            glPassThrough(a[0].f);
            break;
        case RGL_OP_PIXEL_MAP:
            glPixelMapfv(a[0].e, a[1].i, n->data);
            break;
        case RGL_OP_PIXEL_TRANSFER:
            glPixelTransferf(a[0].e, a[1].f);
            break;
        case RGL_OP_PIXEL_ZOOM:
            glPixelZoom(a[0].f, a[1].f);
            break;
        case RGL_OP_POINT_SIZE:
            glPointSize(a[0].f);
            break;
        case RGL_OP_POLYGON_MODE:
            glPolygonMode(a[0].e, a[1].e);
            break;
        case RGL_OP_POP_ATTRIB:
            glPopAttrib();
            break;
        case RGL_OP_POP_MATRIX:
            glPopMatrix();
            break;
        case RGL_OP_POP_NAME:
            glPopName();
            break;
        case RGL_OP_PUSH_ATTRIB:
            glPushAttrib(a[0].u);
            break;
        case RGL_OP_PUSH_MATRIX:
            glPushMatrix();
            break;
        case RGL_OP_PUSH_NAME:
            glPushName(a[0].u);
            break;
        case RGL_OP_RASTER_POS:
            glRasterPos4f(a[0].f, a[1].f, a[2].f, a[3].f);
            break;
        case RGL_OP_READ_BUFFER:
            glReadBuffer(a[0].e);
            break;
        case RGL_OP_SCISSOR:
            glScissor(a[0].i, a[1].i, a[2].i, a[3].i);
            break;
        case RGL_OP_SHADE_MODEL:
            glShadeModel(a[0].e);
            break;
        case RGL_OP_STENCIL_FUNC:
            glStencilFunc(a[0].e, a[1].i, a[2].u);
            break;
        case RGL_OP_STENCIL_MASK:
            glStencilMask(a[0].u);
            break;
        case RGL_OP_STENCIL_OP:
            glStencilOp(a[0].e, a[1].e, a[2].e);
            break;
        case RGL_OP_TEX_COORD:
            glTexCoord4f(a[0].f, a[1].f, a[2].f, a[3].f);
            break;
        case RGL_OP_TEX_ENV:
            floats(a + 2, f, 4);
            glTexEnvfv(a[0].e, a[1].e, f);
            break;
        case RGL_OP_TEX_GEN:
            doubles(a + 2, d, 4);
            glTexGendv(a[0].e, a[1].e, d);
            break;
        case RGL_OP_TEX_PARAMETER:
            floats(a + 2, f, 4);
            glTexParameterfv(a[0].e, a[1].e, f);
            break;
        case RGL_OP_VERTEX:
            glVertex4f(a[0].f, a[1].f, a[2].f, a[3].f);
            break;
        case RGL_OP_VIEWPORT:
            glViewport(a[0].i, a[1].i, a[2].i, a[3].i);
            break;
    }
}

static void call_list(rgl_context *ctx, const GLuint name) {
    if (ctx->list.depth >= RGL_MAX_LIST_NESTING)
        return;

    const struct rgl_display_list *list = find_list(&ctx->list, name);
    if (list == NULL)
        return;

    ctx->list.depth++;
    for (size_t i = 0; i < list->count; i++)
        execute_node(ctx, &list->nodes[i]);
    ctx->list.depth--;
}

void glNewList(const GLuint list, const GLenum mode) {
    GET_CONTEXT(ctx);
    OUTSIDE_BEGIN_END(ctx);

    if (list == 0) {
        rgl_error(ctx, GL_INVALID_VALUE);
        return;
    }
    if (mode != GL_COMPILE && mode != GL_COMPILE_AND_EXECUTE) {
        rgl_error(ctx, GL_INVALID_ENUM);
        return;
    }
    if (ctx->list.compiling != 0) {
        rgl_error(ctx, GL_INVALID_OPERATION);
        return;
    }

    ctx->list.compiling = list;
    ctx->list.mode = mode;
    ctx->list.build_count = 0;
}

void glEndList(void) {
    GET_CONTEXT(ctx);
    OUTSIDE_BEGIN_END(ctx);

    struct rgl_list_state *l = &ctx->list;
    if (l->compiling == 0) {
        rgl_error(ctx, GL_INVALID_OPERATION);
        return;
    }

    if (!store_list(l, l->compiling, l->build, l->build_count)) {
        rgl_error(ctx, GL_OUT_OF_MEMORY);
        free_nodes(l->build, l->build_count);
    }
    l->build = NULL;
    l->build_count = 0;
    l->build_capacity = 0;
    l->compiling = 0;
}

void glCallList(const GLuint list) {
    GET_CONTEXT(ctx);
    if (SAVE(ctx, RGL_OP_CALL_LIST, {.u = list}))
        return;

    call_list(ctx, list);
}

static GLboolean list_name(const GLenum type, const GLvoid *lists, const GLsizei i, GLuint *name) {
    const GLubyte *b = lists;
    switch (type) {
        case GL_BYTE:
            *name = (GLuint) ((const GLbyte *) lists)[i];
            return GL_TRUE;
        case GL_UNSIGNED_BYTE:
            *name = b[i];
            return GL_TRUE;
        case GL_SHORT:
            *name = (GLuint) ((const GLshort *) lists)[i];
            return GL_TRUE;
        case GL_UNSIGNED_SHORT:
            *name = ((const GLushort *) lists)[i];
            return GL_TRUE;
        case GL_INT:
            *name = (GLuint) ((const GLint *) lists)[i];
            return GL_TRUE;
        case GL_UNSIGNED_INT:
            *name = ((const GLuint *) lists)[i];
            return GL_TRUE;
        case GL_FLOAT:
            *name = (GLuint) (GLint) ((const GLfloat *) lists)[i];
            return GL_TRUE;
        case GL_2_BYTES:
            *name = (GLuint) b[i * 2] << 8 | b[i * 2 + 1];
            return GL_TRUE;
        case GL_3_BYTES:
            *name = (GLuint) b[i * 3] << 16 | (GLuint) b[i * 3 + 1] << 8 | b[i * 3 + 2];
            return GL_TRUE;
        case GL_4_BYTES:
            *name = (GLuint) b[i * 4] << 24 | (GLuint) b[i * 4 + 1] << 16 | (GLuint) b[i * 4 + 2] << 8 |
                    b[i * 4 + 3];
            return GL_TRUE;
        default:
            return GL_FALSE;
    }
}

void glCallLists(const GLsizei n, const GLenum type, const GLvoid *lists) {
    GET_CONTEXT(ctx);

    if (n < 0) {
        rgl_error(ctx, GL_INVALID_VALUE);
        return;
    }

    GLuint *names = malloc(((size_t) n + 1) * sizeof(*names));
    if (names == NULL) {
        rgl_error(ctx, GL_OUT_OF_MEMORY);
        return;
    }
    for (GLsizei i = 0; i < n; i++) {
        if (!list_name(type, lists, i, &names[i])) {
            free(names);
            rgl_error(ctx, GL_INVALID_ENUM);
            return;
        }
    }

    if (rgl_list_compiling(ctx)) {
        GLuint *copy = malloc(((size_t) n + 1) * sizeof(*copy));
        if (copy != NULL)
            memcpy(copy, names, (size_t) n * sizeof(*copy));
        const union rgl_list_arg args[1] = {{.i = copy != NULL ? n : 0}};
        if (rgl_save(ctx, RGL_OP_CALL_LISTS, args, 1, copy)) {
            free(names);
            return;
        }
    }

    for (GLsizei i = 0; i < n; i++)
        call_list(ctx, ctx->list.base + names[i]);
    free(names);
}

void glListBase(const GLuint base) {
    GET_CONTEXT(ctx);
    if (SAVE(ctx, RGL_OP_LIST_BASE, {.u = base}))
        return;
    OUTSIDE_BEGIN_END(ctx);

    ctx->list.base = base;
}

GLuint glGenLists(const GLsizei range) {
    GET_CONTEXT_OR(ctx, 0);
    OUTSIDE_BEGIN_END_OR(ctx, 0);

    if (range < 0) {
        rgl_error(ctx, GL_INVALID_VALUE);
        return 0;
    }
    if (range == 0)
        return 0;

    struct rgl_list_state *l = &ctx->list;
    const GLuint first = l->count > 0 ? l->lists[l->count - 1].name + 1 : 1;
    if (first == 0 || first + (GLuint) range - 1 < first)
        return 0;

    for (GLsizei i = 0; i < range; i++) {
        if (!store_list(l, first + (GLuint) i, NULL, 0)) {
            rgl_error(ctx, GL_OUT_OF_MEMORY);
            return 0;
        }
    }
    return first;
}

void glDeleteLists(const GLuint list, const GLsizei range) {
    GET_CONTEXT(ctx);
    OUTSIDE_BEGIN_END(ctx);

    if (range < 0) {
        rgl_error(ctx, GL_INVALID_VALUE);
        return;
    }
    if (range == 0)
        return;

    struct rgl_list_state *l = &ctx->list;
    const GLuint last = list + (GLuint) range - 1 < list ? 0xffffffffu : list + (GLuint) range - 1;
    const size_t begin = lower_bound(l, list);
    size_t end = begin;
    while (end < l->count && l->lists[end].name <= last) {
        free_nodes(l->lists[end].nodes, l->lists[end].count);
        end++;
    }
    memmove(&l->lists[begin], &l->lists[end], (l->count - end) * sizeof(*l->lists));
    l->count -= end - begin;
}

GLboolean glIsList(const GLuint list) {
    GET_CONTEXT_OR(ctx, GL_FALSE);
    OUTSIDE_BEGIN_END_OR(ctx, GL_FALSE);

    return find_list(&ctx->list, list) != NULL;
}
