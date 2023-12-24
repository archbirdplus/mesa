// TODO: Copyright header

#include "pipe/p_context.h"
#include "frontend/sw_winsys.h"
#include "util/u_memory.h"

struct macos_sw_displaytarget {
    void *native_window; // ? maybe a CAMetalLayer for Vulkan to bind to
};

struct macos_sw_winsys {
    struct sw_winsys base;
    void *native_window;
};

static bool
macos_is_displaytarget_format_supported(struct sw_winsys *ws,
                                        unsigned tex_usage,
                                        enum pipe_format format) {
    // I don't even have the xlib winsys to copy from here
    return true;
}

static struct sw_displaytarget *
macos_displaytarget_create(struct sw_winsys *winsys,
                           unsigned tex_usage,
                           enum pipe_format format,
                           unsigned width, unsigned height,
                           unsigned alignment,
                           const void *front_private,
                           unsigned *stride) {
    struct macos_sw_displaytarget *macos_dt;

    struct macos_sw_winsys *macos_winsys = (struct macos_sw_winsys *)winsys;

    // screen resource create?
    macos_dt = CALLOC_STRUCT(macos_sw_displaytarget);
    macos_dt->native_window = macos_winsys->native_window;

    return (struct sw_dispaytarget *)macos_dt;
}

static void
macos_displaytarget_display(struct sw_winsys *winsys,
                            struct sw_displaytarget *dt,
                            void *context_private,
                            struct pipe_box *box) {
    printf("UNIMPLEMENTED display targeting\n");
}

static void
macos_sw_destroy(struct sw_winsys *winsys) {
    free(winsys);
}

struct sw_winsys *
macos_create_sw_winsys(void *native_window) {
   struct macos_sw_winsys *ws;

    ws = CALLOC_STRUCT(macos_sw_winsys);
    if (!ws)
        return NULL;

    ws->native_window = native_window;
    ws->base.destroy = macos_sw_destroy;

    //  probably the 3 important ones
    ws->base.is_displaytarget_format_supported = macos_is_displaytarget_format_supported;
    ws->base.displaytarget_create = macos_displaytarget_create;
    ws->base.displaytarget_display = macos_displaytarget_display;

    // ws->base.displaytarget_from_handle = macos_displaytarget_from_handle;
    // ws->base.displaytarget_get_handle = macos_displaytarget_get_handle;
    // ws->base.displaytarget_map = macos_displaytarget_map;
    // ws->base.displaytarget_unmap = macos_displaytarget_unmap;
    // ws->base.displaytarget_destroy = macos_displaytarget_destroy;


    return &ws->base;
}

