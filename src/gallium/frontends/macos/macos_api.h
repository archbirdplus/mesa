// TODO: Copyright header

// #include "state_tracker/st_context.h"
#include "util/u_thread.h"
#include "pipe/p_screen.h"
#include "frontend/api.h"

struct size {
    unsigned width;
    unsigned height;
};

struct macos_display {
    mtx_t mutex;

    struct pipe_frontend_screen *fscreen;
};

struct macos_st_framebuffer {
    struct pipe_frontend_drawable base;
    struct st_visual visual;

    unsigned width;
    unsigned height;
    unsigned texture_mask;

    struct pipe_screen *screen;
    void *native;
    struct size (*getSize)(void *);

    enum pipe_texture_target target;
    struct pipe_resource *textures[ST_ATTACHMENT_COUNT];
};

struct macos_st_framebuffer *
macos_st_framebuffer(struct pipe_frontend_drawable *drawable);

void
macos_get_st_visual(struct st_visual *visual);

// bool
// macos_st_framebuffer_flush_front(struct st_context *st,
    // struct pipe_frontend_drawable *drawable, enum st_attachment_type statt);

static bool
macos_st_framebuffer_validate(struct st_context *st,
    struct pipe_frontend_drawable *drawable,
    const enum st_attachment_type *stats,
    unsigned count, struct pipe_resource **out,
    struct pipe_resource **resolve);


struct macos_st_framebuffer *
macos_create_st_framebuffer(struct pipe_frontend_screen *fscreen, struct st_visual *visual, void *native_window, struct size (*getSize) (void *));

