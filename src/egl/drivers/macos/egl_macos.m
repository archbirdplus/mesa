// TODO: Copyright header

#import <Cocoa/Cocoa.h>
#import <QuartzCore/Quartzcore.h>

#include "eglconfig.h"
#include "eglcontext.h"
#include "eglcurrent.h"
#include "egldevice.h"
#include "egldriver.h"
#include "eglimage.h"
#include "egllog.h"
#include "eglsurface.h"
#include "egltypedefs.h"

#include "mac_sw_winsys.h"
#include "macos_api.h"

#include "target-helpers/inline_sw_helper.h"
#include "state_tracker/st_context.h"

_EGL_DRIVER_STANDARD_TYPECASTS(macos_egl)

struct macos_egl_display {
   struct pipe_frontend_screen base;
   _EGLDisplay *parent;
   int ref_count;
   // struct pipe_screen *screen; // this exists in base->screen
};

struct macos_egl_config {
   _EGLConfig base;
};

struct macos_egl_context {
   _EGLContext base;
   struct st_context *st;
};

struct macos_egl_surface {
   _EGLSurface base;
   struct macos_st_framebuffer *fb;
   struct pipe_fence_handle *throttle_fence;
};


char *
DO_LATER() {
   return "This function will not be implemented yet because it is not used in GLFW.";
}

// Based on https://github.com/alco/EGL_mac_ios/blob/master/src/egl.m
// as well as src/egl/drivers/haiku and xlib

static EGLBoolean
macos_add_configs_for_visuals(_EGLDisplay *disp) {
   struct macos_egl_config *conf = CALLOC_STRUCT(macos_egl_config);
   assert(conf);

   _eglInitConfig(&conf->base, disp, 1);

   // Copy this from Haiku, maybe check the Apple window later
   conf->base.RedSize = 8;
   conf->base.BlueSize = 8;
   conf->base.GreenSize = 8;
   conf->base.LuminanceSize = 0;
   conf->base.AlphaSize = 8;
   conf->base.ColorBufferType = EGL_RGB_BUFFER;
   conf->base.BufferSize = conf->base.RedSize + conf->base.GreenSize +
                        conf->base.BlueSize + conf->base.AlphaSize;
   conf->base.ConfigCaveat = EGL_NONE;
   conf->base.ConfigID = 1;
   conf->base.BindToTextureRGB = EGL_FALSE;
   conf->base.BindToTextureRGBA = EGL_FALSE;
   conf->base.StencilSize = 0;
   conf->base.TransparentType = EGL_NONE;
   conf->base.NativeRenderable = EGL_TRUE; // Let's say yes
   conf->base.NativeVisualID = 0;        // No visual
   conf->base.NativeVisualType = EGL_NONE; // No visual
   conf->base.RenderableType = 0x8;
   conf->base.SampleBuffers = 0; // TODO: How to get the right value ?
   conf->base.Samples = conf->base.SampleBuffers == 0 ? 0 : 0;
   conf->base.DepthSize = 24; // TODO: How to get the right value ?
   conf->base.Level = 0;
   conf->base.SurfaceType = EGL_WINDOW_BIT;

   if (!_eglValidateConfig(&conf->base, EGL_FALSE)) {
      _eglLog(_EGL_WARNING, "MacOS: failed to validate config");
      goto cleanup;
   }

   _eglLinkConfig(&conf->base);
   if (!_eglGetArraySize(disp->Configs)) {
      _eglLog(_EGL_WARNING, "MacOS: failed to create any config");
      goto cleanup;
   }

   return EGL_TRUE;

cleanup:
   free(conf);
   return EGL_FALSE;
}

static void
macos_display_destroy(_EGLDisplay *disp) {
   struct macos_egl_display *macos_display = macos_egl_display(disp);

   assert(macos_display->ref_count > 0);
   if (!p_atomic_dec_zero(&macos_display->ref_count))
      return;

   struct pipe_screen *screen = macos_display->base.screen;
   screen->destroy(screen);
   st_screen_destroy(&macos_display->base);

   free(macos_display);
}

static int
macos_egl_st_get_param(struct pipe_frontend_screen *fscreen,
                       enum st_manager_param param) {
   // Opt-out of everything
   return 0;
}

static EGLBoolean
macos_initialize_impl(_EGLDisplay *disp) {
   struct macos_egl_display *macos_display = CALLOC_STRUCT(macos_egl_display);

   macos_display->ref_count = 1;
   disp->DriverData = (void *)macos_display;

   struct sw_winsys *winsys = macos_create_sw_winsys(NULL); // add native_window coming from disp probably
   struct pipe_screen *screen = sw_screen_create(winsys);
   printf("created pipe_screen, setting to md->base.screen\n");
   macos_display->base.screen = screen; // idk about the other methods
   macos_display->base.get_param = macos_egl_st_get_param;
   // st_screen managed by st: yes

   disp->ClientAPIs = 0;
   if (_eglIsApiValid(EGL_OPENGL_API))
      disp->ClientAPIs |= EGL_OPENGL_BIT;
   if (_eglIsApiValid(EGL_OPENGL_ES_API))
      disp->ClientAPIs |=
         EGL_OPENGL_ES_BIT | EGL_OPENGL_ES2_BIT | EGL_OPENGL_ES3_BIT_KHR;

   disp->Extensions.KHR_no_config_context = EGL_TRUE;
   disp->Extensions.KHR_surfaceless_context = EGL_TRUE;
   disp->Extensions.MESA_query_driver = EGL_TRUE;

   if (macos_display->base.screen->is_format_supported(
         macos_display->base.screen, PIPE_FORMAT_B8G8R8A8_SRGB, PIPE_TEXTURE_2D,
         0, 0, PIPE_BIND_RENDER_TARGET))
      disp->Extensions.KHR_gl_colorspace = EGL_TRUE;

   disp->Extensions.KHR_create_context = EGL_TRUE;
   disp->Extensions.KHR_reusable_sync = EGL_TRUE;

   if (macos_add_configs_for_visuals(disp))
      return EGL_TRUE;

   macos_display_destroy(disp);
   return _eglError(EGL_NOT_INITIALIZED, "bruh");
}

static EGLBoolean
macos_initialize(_EGLDisplay *disp) {
   macos_initialize_impl(disp);
   if (macos_add_configs_for_visuals(disp))
      return EGL_TRUE;
   return EGL_FALSE;
}

static EGLBoolean
macos_terminate(_EGLDisplay *disp) {
   printf("bruh no terminate\n");
   return _eglError(EGL_NOT_INITIALIZED, "bruh no terminate");
}
static _EGLContext *
macos_createContext(_EGLDisplay *disp, _EGLConfig *conf, _EGLContext *share_list, const EGLint *attrib_list) {
   struct macos_egl_display *dpy = macos_egl_display(disp);

   struct macos_egl_context *context = CALLOC_STRUCT(macos_egl_context);
   assert(context);

   if (!_eglInitContext(&context->base, disp, conf, share_list,
                   attrib_list)) {
      free(context);
      return NULL;
   }

   struct st_visual visual;
   macos_get_st_visual(&visual);

   struct st_context_attribs attribs;
   memset(&attribs, 0, sizeof(attribs));
   attribs.options.force_glsl_extensions_warn = false;
   attribs.profile = API_OPENGL_COMPAT;
   attribs.visual = visual;
   attribs.major = 1;
   attribs.minor = 0;

   enum st_context_error result;
   context->st = st_api_create_context(
      &dpy->base, &attribs, &result,
      share_list == NULL ? NULL : macos_egl_context(share_list)->st);
   if (context->st == NULL) {
      free(context);
      return NULL;
   }

   // context->st->frontend_context = macos stuff

   return &context->base;
}
static EGLBoolean
macos_destroyContext(_EGLDisplay *disp, _EGLContext *ctx) {
   return _eglError(EGL_NOT_INITIALIZED, "bruh no destroy context");
}
static EGLBoolean
macos_destroySurface(_EGLDisplay *disp, _EGLSurface *surf) {
   return _eglError(EGL_NOT_INITIALIZED, "bruh no destroy surface");
}
static EGLBoolean
macos_makeCurrent(_EGLDisplay *disp, _EGLSurface *drawsurf,
                  _EGLSurface *readsurf, _EGLContext *ctx) {
   struct macos_egl_context *macos_ctx = macos_egl_context(ctx);
   struct macos_egl_surface *macos_drawsurf = macos_egl_surface(drawsurf);
   struct macos_egl_surface *macos_readsurf = macos_egl_surface(readsurf);

   _EGLContext *old_ctx;
   _EGLSurface *old_drawsurf, *old_readsurf;

   if (!_eglBindContext(ctx, drawsurf, readsurf, &old_ctx, &old_drawsurf,
                        &old_readsurf))
      return EGL_FALSE;

   if (old_ctx == ctx &&
       old_drawsurf == drawsurf &&
       old_readsurf == readsurf) {
      // decrement reference counter
      _eglPutSurface(old_drawsurf);
      _eglPutSurface(old_readsurf);
      _eglPutContext(old_ctx);
   }

   if (ctx == NULL) {
      st_api_make_current(NULL, NULL, NULL);
   } else {
      if (drawsurf != NULL && drawsurf != old_drawsurf)
         // update_size(macos_drawsurf->fb);

      st_api_make_current(macos_ctx->st,
          macos_drawsurf == NULL ? NULL : &macos_drawsurf->fb->base,
          macos_readsurf == NULL ? NULL : &macos_readsurf->fb->base);
   }

   if (old_readsurf != NULL)
       macos_destroySurface(disp, old_readsurf);
   if (old_drawsurf != NULL)
       macos_destroySurface(disp, old_drawsurf);
   if (old_ctx != NULL)
       macos_destroyContext(disp, old_ctx);

   return EGL_TRUE;
}

static struct size getSize(void *native) {
   CAMetalLayer *layer = (CAMetalLayer *)native;
   struct size drawableSize;
   CGSize cgSize = layer.drawableSize;
   drawableSize.width = (unsigned) cgSize.width;
   drawableSize.height = (unsigned) cgSize.height;
   return drawableSize;
}

static _EGLSurface *
macos_createWindowSurface(_EGLDisplay *disp, _EGLConfig *conf,
                     void *native, const EGLint *attrib_list) {
   CAMetalLayer *layer = (CAMetalLayer *) native;

   // then create zink framebuffer
   struct macos_egl_surface *macos_surf =
      (struct macos_egl_surface *)calloc(1, sizeof(struct macos_egl_surface));
   if(!macos_surf)
      return NULL;

   if(!_eglInitSurface(&macos_surf->base, disp, EGL_WINDOW_BIT, conf,
                  attrib_list, NULL))
      return NULL;

   struct st_visual visual;
   macos_get_st_visual(&visual);

   struct macos_egl_display *mac_disp = macos_egl_display(disp);

   macos_surf->fb =
      macos_create_st_framebuffer(&mac_disp->base, &visual, layer, getSize);

   if (!macos_surf->fb) {
      free(macos_surf);
      return NULL;
   }

   return &macos_surf->base;
}

static _EGLSurface *
macos_createPbufferSurface(_EGLDisplay *disp, _EGLConfig *conf,
                          const EGLint *attrib_list) {
   printf("macos_createPbufferSurface: nope!\n");
   _eglError(EGL_NOT_INITIALIZED, "no pbuffer for macos");
   return NULL;
}

static _EGLSurface *
macos_createPixmapSurface(_EGLDisplay *disp, _EGLConfig *conf,
                          void *native_pixmap, const EGLint *attrib_list) {
   printf("macos_createPixmapSurface: nope!\n");
   _eglError(EGL_NOT_INITIALIZED, "no pixmap for macos");
   return NULL;
}

// static void
// macos_swapInterval() {
   // return;
// }
static EGLBoolean
macos_swapBuffers(_EGLDisplay *disp, _EGLSurface *surf) {
   return _eglError(EGL_NOT_INITIALIZED, "bruh no destroy surface");
}

// I am only implementing WGL functions that GLFW uses. I hope main/egl*
// have implemented the rest that GLFW needs.
struct _egl_driver _eglDriver = {
   .Initialize = macos_initialize,
   .Terminate = macos_terminate,
   .CreateContext = macos_createContext,
   .DestroyContext = macos_destroyContext,
   .MakeCurrent = macos_makeCurrent,
   // this is a family of 3 very similar surface-creating functions
   .CreateWindowSurface = macos_createWindowSurface,
   .CreatePixmapSurface = macos_createPixmapSurface,
   .CreatePbufferSurface = macos_createPbufferSurface,
   .DestroySurface = macos_destroySurface,
   // .QuerySurface = DO_LATER,
   // .BindTexImage = DO_LATER,
   // .ReleaseTexImage = DO_LATER,
   // .SwapInterval = macos_swapInterval, // ?
   .SwapBuffers = macos_swapBuffers,
   // .WaitClient = DO_LATER,
   // .WaitNative = DO_LATER,
   // .CreateImageKHR = DO_LATER,
   // .DestroyImageKHR = DO_LATER,
   // .CreateSyncKHR = DO_LATER,
   // .DestroySyncKHR = DO_LATER,
   // .ClientWaitSyncKHR = DO_LATER,
   // .WaitSyncKHR = DO_LATER,
   // .SignalSyncKHR = DO_LATER,
   // .QueryDriverName = DO_LATER,
   // .QueryDriverConfig = DO_LATER,
   // .GLInteropQueryDeviceInfo = DO_LATER,
   // .GLInteropExportObject = DO_LATER,
   // .GLInteropFlushObjects = DO_LATER,
};
