#include "context.h"

#include <math.h>

enum kind {
    KIND_BOOL,
    KIND_INT,
    KIND_FLOAT,
    KIND_COLOR
};

struct values {
    enum kind kind;
    int count;
    GLdouble v[16];
};

static void boolean(struct values *o, const GLboolean v) {
    o->kind = KIND_BOOL;
    o->count = 1;
    o->v[0] = v ? 1.0 : 0.0;
}

static void booleans(struct values *o, const int n, const GLboolean *v) {
    o->kind = KIND_BOOL;
    o->count = n;
    for (int i = 0; i < n; i++)
        o->v[i] = v[i] ? 1.0 : 0.0;
}

static void integer(struct values *o, const GLdouble v) {
    o->kind = KIND_INT;
    o->count = 1;
    o->v[0] = v;
}

static void integers(struct values *o, const int n, const GLint *v) {
    o->kind = KIND_INT;
    o->count = n;
    for (int i = 0; i < n; i++)
        o->v[i] = v[i];
}

static void real(struct values *o, const GLdouble v) {
    o->kind = KIND_FLOAT;
    o->count = 1;
    o->v[0] = v;
}

static void reals(struct values *o, const int n, const GLfloat *v) {
    o->kind = KIND_FLOAT;
    o->count = n;
    for (int i = 0; i < n; i++)
        o->v[i] = v[i];
}

static void colors(struct values *o, const int n, const GLfloat *v) {
    reals(o, n, v);
    o->kind = KIND_COLOR;
}

static GLboolean query(rgl_context *ctx, const GLenum pname, struct values *o) {
    const struct rgl_raster_pos *r = &ctx->raster;
    const struct rgl_pixel_store *unpack = &ctx->pixel.unpack;
    const struct rgl_pixel_store *pack = &ctx->pixel.pack;
    const struct rgl_pixel_transfer *t = &ctx->pixel.transfer;

    if (pname >= GL_PIXEL_MAP_I_TO_I_SIZE && pname <= GL_PIXEL_MAP_A_TO_A_SIZE) {
        integer(o, ctx->pixel.maps[pname - GL_PIXEL_MAP_I_TO_I_SIZE].size);
        return GL_TRUE;
    }

    switch (pname) {
        case GL_CURRENT_COLOR:
            colors(o, 4, ctx->current.color);
            return GL_TRUE;
        case GL_CURRENT_INDEX:
            real(o, ctx->current.index);
            return GL_TRUE;
        case GL_CURRENT_NORMAL:
            colors(o, 3, ctx->current.normal);
            return GL_TRUE;
        case GL_CURRENT_TEXTURE_COORDS:
            reals(o, 4, ctx->current.tex_coord);
            return GL_TRUE;
        case GL_CURRENT_RASTER_COLOR:
            colors(o, 4, r->color);
            return GL_TRUE;
        case GL_CURRENT_RASTER_INDEX:
            real(o, r->index);
            return GL_TRUE;
        case GL_CURRENT_RASTER_TEXTURE_COORDS:
            reals(o, 4, r->tex_coord);
            return GL_TRUE;
        case GL_CURRENT_RASTER_POSITION: {
            const GLfloat position[4] = {r->window[0], r->window[1], r->window[2], r->clip_w};
            reals(o, 4, position);
            return GL_TRUE;
        }
        case GL_CURRENT_RASTER_POSITION_VALID:
            boolean(o, r->valid);
            return GL_TRUE;
        case GL_CURRENT_RASTER_DISTANCE:
            real(o, r->distance);
            return GL_TRUE;
        case GL_POINT_SIZE:
            real(o, ctx->point.size);
            return GL_TRUE;
        case GL_POINT_SIZE_RANGE:
        case GL_LINE_WIDTH_RANGE: {
            const GLfloat range[2] = {1.0f, pname == GL_POINT_SIZE_RANGE ? RGL_MAX_POINT_SIZE : RGL_MAX_LINE_WIDTH};
            reals(o, 2, range);
            return GL_TRUE;
        }
        case GL_POINT_SIZE_GRANULARITY:
        case GL_LINE_WIDTH_GRANULARITY:
            real(o, 1.0);
            return GL_TRUE;
        case GL_LINE_WIDTH:
            real(o, ctx->line.width);
            return GL_TRUE;
        case GL_LINE_STIPPLE_PATTERN:
            integer(o, ctx->line.stipple_pattern);
            return GL_TRUE;
        case GL_LINE_STIPPLE_REPEAT:
            integer(o, ctx->line.stipple_factor);
            return GL_TRUE;
        case GL_LIST_MODE:
            integer(o, ctx->list.compiling != 0 ? ctx->list.mode : 0);
            return GL_TRUE;
        case GL_MAX_LIST_NESTING:
            integer(o, RGL_MAX_LIST_NESTING);
            return GL_TRUE;
        case GL_LIST_BASE:
            integer(o, ctx->list.base);
            return GL_TRUE;
        case GL_LIST_INDEX:
            integer(o, ctx->list.compiling);
            return GL_TRUE;
        case GL_POLYGON_MODE: {
            const GLint modes[2] = {(GLint) ctx->polygon.front_mode, (GLint) ctx->polygon.back_mode};
            integers(o, 2, modes);
            return GL_TRUE;
        }
        case GL_EDGE_FLAG:
            boolean(o, ctx->current.edge_flag);
            return GL_TRUE;
        case GL_CULL_FACE_MODE:
            integer(o, ctx->polygon.cull_face);
            return GL_TRUE;
        case GL_FRONT_FACE:
            integer(o, ctx->polygon.front_face);
            return GL_TRUE;
        case GL_LIGHT_MODEL_LOCAL_VIEWER:
            boolean(o, ctx->lighting.local_viewer);
            return GL_TRUE;
        case GL_LIGHT_MODEL_TWO_SIDE:
            boolean(o, ctx->lighting.two_side);
            return GL_TRUE;
        case GL_LIGHT_MODEL_AMBIENT:
            colors(o, 4, ctx->lighting.model_ambient);
            return GL_TRUE;
        case GL_SHADE_MODEL:
            integer(o, ctx->lighting.shade_model);
            return GL_TRUE;
        case GL_COLOR_MATERIAL_FACE:
            integer(o, ctx->lighting.color_material_face);
            return GL_TRUE;
        case GL_COLOR_MATERIAL_PARAMETER:
            integer(o, ctx->lighting.color_material_mode);
            return GL_TRUE;
        case GL_FOG_INDEX:
            real(o, ctx->fog.index);
            return GL_TRUE;
        case GL_FOG_DENSITY:
            real(o, ctx->fog.density);
            return GL_TRUE;
        case GL_FOG_START:
            real(o, ctx->fog.start);
            return GL_TRUE;
        case GL_FOG_END:
            real(o, ctx->fog.end);
            return GL_TRUE;
        case GL_FOG_MODE:
            integer(o, ctx->fog.mode);
            return GL_TRUE;
        case GL_FOG_COLOR:
            colors(o, 4, ctx->fog.color);
            return GL_TRUE;
        case GL_DEPTH_RANGE: {
            const GLfloat range[2] = {(GLfloat) ctx->viewport.z_near, (GLfloat) ctx->viewport.z_far};
            colors(o, 2, range);
            return GL_TRUE;
        }
        case GL_DEPTH_WRITEMASK:
            boolean(o, ctx->mask.depth);
            return GL_TRUE;
        case GL_DEPTH_CLEAR_VALUE: {
            const GLfloat depth = (GLfloat) ctx->clear.depth;
            colors(o, 1, &depth);
            return GL_TRUE;
        }
        case GL_DEPTH_FUNC:
            integer(o, ctx->depth.func);
            return GL_TRUE;
        case GL_ACCUM_CLEAR_VALUE:
            colors(o, 4, ctx->clear.accum);
            return GL_TRUE;
        case GL_STENCIL_CLEAR_VALUE:
            integer(o, ctx->clear.stencil);
            return GL_TRUE;
        case GL_STENCIL_FUNC:
            integer(o, ctx->stencil.func);
            return GL_TRUE;
        case GL_STENCIL_VALUE_MASK:
            integer(o, ctx->stencil.value_mask);
            return GL_TRUE;
        case GL_STENCIL_FAIL:
            integer(o, ctx->stencil.fail);
            return GL_TRUE;
        case GL_STENCIL_PASS_DEPTH_FAIL:
            integer(o, ctx->stencil.zfail);
            return GL_TRUE;
        case GL_STENCIL_PASS_DEPTH_PASS:
            integer(o, ctx->stencil.zpass);
            return GL_TRUE;
        case GL_STENCIL_REF:
            integer(o, ctx->stencil.ref);
            return GL_TRUE;
        case GL_STENCIL_WRITEMASK:
            integer(o, ctx->mask.stencil);
            return GL_TRUE;
        case GL_MATRIX_MODE:
            integer(o, ctx->transform.matrix_mode);
            return GL_TRUE;
        case GL_VIEWPORT: {
            const GLint box[4] = {ctx->viewport.x, ctx->viewport.y, ctx->viewport.width, ctx->viewport.height};
            integers(o, 4, box);
            return GL_TRUE;
        }
        case GL_MODELVIEW_STACK_DEPTH:
            integer(o, ctx->transform.modelview.depth);
            return GL_TRUE;
        case GL_PROJECTION_STACK_DEPTH:
            integer(o, ctx->transform.projection.depth);
            return GL_TRUE;
        case GL_TEXTURE_STACK_DEPTH:
            integer(o, ctx->transform.texture.depth);
            return GL_TRUE;
        case GL_MODELVIEW_MATRIX:
            reals(o, 16, rgl_matrix_top(&ctx->transform.modelview));
            return GL_TRUE;
        case GL_PROJECTION_MATRIX:
            reals(o, 16, rgl_matrix_top(&ctx->transform.projection));
            return GL_TRUE;
        case GL_TEXTURE_MATRIX:
            reals(o, 16, rgl_matrix_top(&ctx->transform.texture));
            return GL_TRUE;
        case GL_ATTRIB_STACK_DEPTH:
            integer(o, ctx->attrib_depth);
            return GL_TRUE;
        case GL_ALPHA_TEST_FUNC:
            integer(o, ctx->color_buffer.alpha_func);
            return GL_TRUE;
        case GL_ALPHA_TEST_REF:
            colors(o, 1, &ctx->color_buffer.alpha_ref);
            return GL_TRUE;
        case GL_BLEND_DST:
            integer(o, ctx->color_buffer.blend_dst);
            return GL_TRUE;
        case GL_BLEND_SRC:
            integer(o, ctx->color_buffer.blend_src);
            return GL_TRUE;
        case GL_LOGIC_OP_MODE:
            integer(o, ctx->color_buffer.logic_op);
            return GL_TRUE;
        case GL_AUX_BUFFERS:
            integer(o, 0);
            return GL_TRUE;
        case GL_DRAW_BUFFER:
            integer(o, ctx->color_buffer.draw_buffer);
            return GL_TRUE;
        case GL_READ_BUFFER:
            integer(o, ctx->pixel.read_buffer);
            return GL_TRUE;
        case GL_SCISSOR_BOX: {
            const GLint box[4] = {ctx->scissor.x, ctx->scissor.y, ctx->scissor.width, ctx->scissor.height};
            integers(o, 4, box);
            return GL_TRUE;
        }
        case GL_INDEX_CLEAR_VALUE:
            real(o, ctx->clear.index);
            return GL_TRUE;
        case GL_INDEX_WRITEMASK:
            integer(o, ctx->mask.index);
            return GL_TRUE;
        case GL_COLOR_CLEAR_VALUE:
            colors(o, 4, ctx->clear.color);
            return GL_TRUE;
        case GL_COLOR_WRITEMASK:
            booleans(o, 4, ctx->mask.color);
            return GL_TRUE;
        case GL_INDEX_MODE:
        case GL_DOUBLEBUFFER:
        case GL_STEREO:
            boolean(o, GL_FALSE);
            return GL_TRUE;
        case GL_RGBA_MODE:
            boolean(o, GL_TRUE);
            return GL_TRUE;
        case GL_RENDER_MODE:
            integer(o, ctx->render_mode);
            return GL_TRUE;
        case GL_PERSPECTIVE_CORRECTION_HINT:
            integer(o, ctx->hints.perspective_correction);
            return GL_TRUE;
        case GL_POINT_SMOOTH_HINT:
            integer(o, ctx->hints.point_smooth);
            return GL_TRUE;
        case GL_LINE_SMOOTH_HINT:
            integer(o, ctx->hints.line_smooth);
            return GL_TRUE;
        case GL_POLYGON_SMOOTH_HINT:
            integer(o, ctx->hints.polygon_smooth);
            return GL_TRUE;
        case GL_FOG_HINT:
            integer(o, ctx->hints.fog);
            return GL_TRUE;
        case GL_UNPACK_SWAP_BYTES:
            boolean(o, unpack->swap_bytes);
            return GL_TRUE;
        case GL_UNPACK_LSB_FIRST:
            boolean(o, unpack->lsb_first);
            return GL_TRUE;
        case GL_UNPACK_ROW_LENGTH:
            integer(o, unpack->row_length);
            return GL_TRUE;
        case GL_UNPACK_SKIP_ROWS:
            integer(o, unpack->skip_rows);
            return GL_TRUE;
        case GL_UNPACK_SKIP_PIXELS:
            integer(o, unpack->skip_pixels);
            return GL_TRUE;
        case GL_UNPACK_ALIGNMENT:
            integer(o, unpack->alignment);
            return GL_TRUE;
        case GL_PACK_SWAP_BYTES:
            boolean(o, pack->swap_bytes);
            return GL_TRUE;
        case GL_PACK_LSB_FIRST:
            boolean(o, pack->lsb_first);
            return GL_TRUE;
        case GL_PACK_ROW_LENGTH:
            integer(o, pack->row_length);
            return GL_TRUE;
        case GL_PACK_SKIP_ROWS:
            integer(o, pack->skip_rows);
            return GL_TRUE;
        case GL_PACK_SKIP_PIXELS:
            integer(o, pack->skip_pixels);
            return GL_TRUE;
        case GL_PACK_ALIGNMENT:
            integer(o, pack->alignment);
            return GL_TRUE;
        case GL_MAP_COLOR:
            boolean(o, t->map_color);
            return GL_TRUE;
        case GL_MAP_STENCIL:
            boolean(o, t->map_stencil);
            return GL_TRUE;
        case GL_INDEX_SHIFT:
            integer(o, t->index_shift);
            return GL_TRUE;
        case GL_INDEX_OFFSET:
            integer(o, t->index_offset);
            return GL_TRUE;
        case GL_RED_SCALE:
            real(o, t->scale[0]);
            return GL_TRUE;
        case GL_GREEN_SCALE:
            real(o, t->scale[1]);
            return GL_TRUE;
        case GL_BLUE_SCALE:
            real(o, t->scale[2]);
            return GL_TRUE;
        case GL_ALPHA_SCALE:
            real(o, t->scale[3]);
            return GL_TRUE;
        case GL_RED_BIAS:
            real(o, t->bias[0]);
            return GL_TRUE;
        case GL_GREEN_BIAS:
            real(o, t->bias[1]);
            return GL_TRUE;
        case GL_BLUE_BIAS:
            real(o, t->bias[2]);
            return GL_TRUE;
        case GL_ALPHA_BIAS:
            real(o, t->bias[3]);
            return GL_TRUE;
        case GL_DEPTH_SCALE:
            real(o, t->depth_scale);
            return GL_TRUE;
        case GL_DEPTH_BIAS:
            real(o, t->depth_bias);
            return GL_TRUE;
        case GL_ZOOM_X:
            real(o, ctx->pixel.zoom_x);
            return GL_TRUE;
        case GL_ZOOM_Y:
            real(o, ctx->pixel.zoom_y);
            return GL_TRUE;
        case GL_MAX_EVAL_ORDER:
            integer(o, RGL_MAX_EVAL_ORDER);
            return GL_TRUE;
        case GL_MAX_LIGHTS:
            integer(o, RGL_MAX_LIGHTS);
            return GL_TRUE;
        case GL_MAX_CLIP_PLANES:
            integer(o, RGL_MAX_CLIP_PLANES);
            return GL_TRUE;
        case GL_MAX_TEXTURE_SIZE:
            integer(o, RGL_MAX_TEXTURE_SIZE);
            return GL_TRUE;
        case GL_MAX_PIXEL_MAP_TABLE:
            integer(o, RGL_MAX_PIXEL_MAP_TABLE);
            return GL_TRUE;
        case GL_MAX_ATTRIB_STACK_DEPTH:
            integer(o, RGL_MAX_ATTRIB_STACK_DEPTH);
            return GL_TRUE;
        case GL_MAX_MODELVIEW_STACK_DEPTH:
            integer(o, RGL_MAX_MODELVIEW_STACK_DEPTH);
            return GL_TRUE;
        case GL_MAX_NAME_STACK_DEPTH:
            integer(o, RGL_MAX_NAME_STACK_DEPTH);
            return GL_TRUE;
        case GL_MAX_PROJECTION_STACK_DEPTH:
            integer(o, RGL_MAX_PROJECTION_STACK_DEPTH);
            return GL_TRUE;
        case GL_MAX_TEXTURE_STACK_DEPTH:
            integer(o, RGL_MAX_TEXTURE_STACK_DEPTH);
            return GL_TRUE;
        case GL_MAX_VIEWPORT_DIMS: {
            const GLint dims[2] = {RGL_MAX_VIEWPORT_DIM, RGL_MAX_VIEWPORT_DIM};
            integers(o, 2, dims);
            return GL_TRUE;
        }
        case GL_SUBPIXEL_BITS:
            integer(o, RGL_SUBPIXEL_BITS);
            return GL_TRUE;
        case GL_INDEX_BITS:
            integer(o, 0);
            return GL_TRUE;
        case GL_RED_BITS:
        case GL_GREEN_BITS:
        case GL_BLUE_BITS:
        case GL_ALPHA_BITS:
            integer(o, 8);
            return GL_TRUE;
        case GL_DEPTH_BITS:
            integer(o, RGL_DEPTH_BITS);
            return GL_TRUE;
        case GL_STENCIL_BITS:
            integer(o, RGL_STENCIL_BITS);
            return GL_TRUE;
        case GL_ACCUM_RED_BITS:
        case GL_ACCUM_GREEN_BITS:
        case GL_ACCUM_BLUE_BITS:
        case GL_ACCUM_ALPHA_BITS:
            integer(o, RGL_ACCUM_BITS);
            return GL_TRUE;
        case GL_NAME_STACK_DEPTH:
            integer(o, ctx->select.depth);
            return GL_TRUE;
        case GL_MAP1_GRID_DOMAIN: {
            const GLfloat domain[2] = {ctx->eval.grid1_u1, ctx->eval.grid1_u2};
            reals(o, 2, domain);
            return GL_TRUE;
        }
        case GL_MAP1_GRID_SEGMENTS:
            integer(o, ctx->eval.grid1_un);
            return GL_TRUE;
        case GL_MAP2_GRID_DOMAIN: {
            const GLfloat domain[4] = {
                ctx->eval.grid2_u1, ctx->eval.grid2_u2, ctx->eval.grid2_v1, ctx->eval.grid2_v2
            };
            reals(o, 4, domain);
            return GL_TRUE;
        }
        case GL_MAP2_GRID_SEGMENTS: {
            const GLint segments[2] = {ctx->eval.grid2_un, ctx->eval.grid2_vn};
            integers(o, 2, segments);
            return GL_TRUE;
        }
        default: {
            const GLboolean *flag = rgl_cap_flag(ctx, pname);
            if (flag == NULL)
                return GL_FALSE;
            boolean(o, *flag);
            return GL_TRUE;
        }
    }
}

static GLboolean get(const GLenum pname, struct values *o) {
    GET_CONTEXT_OR(ctx, GL_FALSE);
    OUTSIDE_BEGIN_END_OR(ctx, GL_FALSE);

    if (!query(ctx, pname, o)) {
        rgl_error(ctx, GL_INVALID_ENUM);
        return GL_FALSE;
    }
    return GL_TRUE;
}

void glGetBooleanv(const GLenum pname, GLboolean *params) {
    struct values o;
    if (!get(pname, &o))
        return;
    for (int i = 0; i < o.count; i++)
        params[i] = o.v[i] != 0.0 ? GL_TRUE : GL_FALSE;
}

void glGetIntegerv(const GLenum pname, GLint *params) {
    struct values o;
    if (!get(pname, &o))
        return;
    for (int i = 0; i < o.count; i++) {
        switch (o.kind) {
            case KIND_COLOR:
                params[i] = rgl_float_to_int((GLfloat) o.v[i]);
                break;
            case KIND_FLOAT:
                params[i] = (GLint) floor(o.v[i] + 0.5);
                break;
            default:
                params[i] = o.v[i] > 2147483647.0 ? (GLint) (GLuint) o.v[i] : (GLint) o.v[i];
                break;
        }
    }
}

void glGetFloatv(const GLenum pname, GLfloat *params) {
    struct values o;
    if (!get(pname, &o))
        return;
    for (int i = 0; i < o.count; i++)
        params[i] = (GLfloat) o.v[i];
}

void glGetDoublev(const GLenum pname, GLdouble *params) {
    struct values o;
    if (!get(pname, &o))
        return;
    for (int i = 0; i < o.count; i++)
        params[i] = o.v[i];
}
