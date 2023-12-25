// TODO: Copyright header

#include "macos_api.h"

#include "state_tracker/st_context.h"

#ifdef GALLIUM_ZINK
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_metal.h>
#include "kopper_interface.h"
#endif

static uint32_t macos_fb_ID = 0;

struct macos_st_framebuffer *
macos_st_framebuffer(struct pipe_frontend_drawable *drawable) {
   return (struct macos_st_framebuffer *)drawable;
}

void
macos_get_st_visual(struct st_visual *visual) {
   visual->buffer_mask |= ST_ATTACHMENT_FRONT_LEFT_MASK;
   visual->buffer_mask |= ST_ATTACHMENT_BACK_LEFT_MASK;
   visual->buffer_mask |= ST_ATTACHMENT_DEPTH_STENCIL_MASK;
   visual->color_format = PIPE_FORMAT_BGRA8888_UNORM;
   // Seems to be the only supported depth/stencil format:
   // MTLPixelFormatDepth24Unorm_Stencil8 aka depth24Unorm_stencil8
   // However, vulkancapsviewer shows that I only have D32_SFLOAT_S8_UINT
   // which has no mapping in enum pipe_format...
   visual->depth_stencil_format = PIPE_FORMAT_Z24_UNORM_S8_UINT;
   // visual->depth_stencil_format = PIPE_FORMAT_Z32_FLOAT;
   // visual->depth_stencil_format = PIPE_FORMAT_Z16_UNORM;
   visual->accum_format = PIPE_FORMAT_NONE;
   visual->samples = 1;
}

static bool
macos_st_framebuffer_display(struct pipe_frontend_drawable *drawable,
                             struct st_context *st,
                             enum st_attachment_type statt,
                             struct pipe_box *box) {
   struct macos_st_framebuffer *macfb = macos_st_framebuffer(drawable);
   // in my tests, this seems to select BACK_LEFT, although that is
   // not a front buffer, and only FRONT_RIGHT has a present semaphore...
   struct pipe_resource *ptex = macfb->textures[statt];
   // display surface, as defined by its zink swapchain
   struct pipe_resource *pres;
   struct pipe_context *pctx = st ? st->pipe : NULL;

   if (!ptex) {
      printf("framebuffer_display: requested to present NULL texture\n");
      return true;
   }

   // xlib has a separate "display resource" for presentation, presumably
   // in order to send it to the x server manually, but zink
   // should use its own swapchain that its textures reference
   pres = ptex;

   // xlib: /* (re)allocate the surface for the texture to be displayed */
   // no?

   // most of these arguments aren't even used by zink
   printf("to zink: flush frontbuffer!\n");
   // this currently fails because zink_kopper_present isn't called;
   // the "present" semaphore is not defined
   macfb->screen->flush_frontbuffer(macfb->screen, pctx, pres, 0, 0, macfb->native, NULL);
}

static bool
macos_st_framebuffer_flush_front(struct st_context *st,
      struct pipe_frontend_drawable *drawable, enum st_attachment_type statt) {
   struct macos_st_framebuffer *macfb = macos_st_framebuffer(drawable);
   bool ret;

   if (statt != ST_ATTACHMENT_FRONT_LEFT) {
      printf("macos flush_front: specified attachment is not FRONT-LEFT!\n");
      return false;
   }

   ret = macos_st_framebuffer_display(drawable, st, statt, NULL);

   // TODO: if ret, check if the buffer is resized?

   return ret;
}

#ifdef GALLIUM_ZINK
static void macos_st_fill_private_loader_data(
     struct macos_st_framebuffer *fb,
     struct kopper_loader_info *out) {
   VkMetalSurfaceCreateInfoEXT *metal =
     (VkMetalSurfaceCreateInfoEXT *)&out->bos;
   metal->sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
   metal->pNext = NULL;
   metal->flags = 0;
   metal->pLayer = fb->native;
 }
#endif

static bool
macos_st_framebuffer_validate_textures(
     struct pipe_frontend_drawable *drawable,
     unsigned width, unsigned height, unsigned mask) {
   struct macos_st_framebuffer *macfb = macos_st_framebuffer(drawable);
   struct pipe_resource templ;
   enum st_attachment_type i;

   /* remove outdated textures */
   if (macfb->width != width || macfb->height != height) {
     for (i = 0; i < ST_ATTACHMENT_COUNT; i++)
       pipe_resource_reference(&macfb->textures[i], NULL);
   }

   printf("validating textures: %u x %u\n", width, height);

   memset(&templ, 0, sizeof(templ));
   templ.target = macfb->target;
   templ.width0 = width;
   templ.height0 = height;
   templ.depth0 = 1;
   templ.array_size = 1;
   templ.last_level = 0;
   // templ.nr_samples = macfb->visual.samples;
   // templ.nr_storage_samples = macfb->visual.samples;

   for (i = 0; i < ST_ATTACHMENT_COUNT; i++) {
     enum pipe_format format;
     unsigned bind;

     if (macfb->textures[i] || !(mask & (1 << i))) {
       if (macfb->textures[i])
         mask |= (1 << i);
       continue;
     }

     switch (i) {
     case ST_ATTACHMENT_FRONT_LEFT:
     case ST_ATTACHMENT_BACK_LEFT:
     case ST_ATTACHMENT_FRONT_RIGHT:
     case ST_ATTACHMENT_BACK_RIGHT:
       format = macfb->visual.color_format;
       bind = PIPE_BIND_DISPLAY_TARGET | PIPE_BIND_RENDER_TARGET;
#ifdef GALLIUM_ZINK
       // if (dev->zink) {
         // see stw_st.c for the rationale
         if (i == ST_ATTACHMENT_BACK_LEFT && macfb->textures[ST_ATTACHMENT_FRONT_LEFT])
            bind &= ~PIPE_BIND_DISPLAY_TARGET;
       // }
#endif
       break;
     case ST_ATTACHMENT_DEPTH_STENCIL:
       format = macfb->visual.depth_stencil_format;
       bind = PIPE_BIND_DEPTH_STENCIL;
#ifdef GALLIUM_ZINK
       // if (dev->zink)
         bind |= PIPE_BIND_DISPLAY_TARGET;
#endif
       break;
     default:
       format = PIPE_FORMAT_NONE;
       break;
     }

     if (format == PIPE_FORMAT_NONE)
       continue;
     templ.format = format;
     templ.bind = bind;

#ifdef GALLIUM_ZINK
     if (
         // dev->zink &&
         i < ST_ATTACHMENT_DEPTH_STENCIL &&
         macfb->screen->resource_create_drawable) {

       struct kopper_loader_info loader_info;
       void *data;

       if (bind & PIPE_BIND_DISPLAY_TARGET) {
         macos_st_fill_private_loader_data(macfb, &loader_info);
         data = &loader_info;
       } else {
         data = macfb->textures[ST_ATTACHMENT_FRONT_LEFT];
       }

       assert(data);

       printf("zink will resource_create_drawable!\n");
       macfb->textures[i] =
         macfb->screen->resource_create_drawable(macfb->screen, &templ, data);
     } else {
#endif
       printf("zink will simply resource_create\n");
       macfb->textures[i] =
         macfb->screen->resource_create(macfb->screen, &templ);
#ifdef GALLIUM_ZINK
     }
#endif

     if (!macfb->textures[i])
       return false;
   }

   macfb->width = width;
   macfb->height = height;
   macfb->texture_mask = mask;

   return true;
}

static bool
macos_st_framebuffer_validate(struct st_context *st,
      struct pipe_frontend_drawable *drawable,
      const enum st_attachment_type *statts,
      unsigned count, struct pipe_resource **out,
      struct pipe_resource **resolve) {
   printf("validate the framebuffer!\n");

   struct macos_st_framebuffer *macfb = macos_st_framebuffer(drawable);
   struct size drawableSize = macfb->getSize(macfb->native);
   unsigned statt_mask, new_mask, i;
   bool resized = false;
   bool ret;

   statt_mask = 0x0;
   for (i = 0; i < count; i++)
     statt_mask |= 1 << statts[i];

   new_mask = statt_mask & ~macfb->texture_mask;

   

   resized = (macfb->width != drawableSize.width ||
           macfb->height != drawableSize.height);

   printf("validate: resized: %s\n", resized ? "yes" : "no");

   if (resized || new_mask) {
      // MacOS does not do well with 0 pixel images
      ret = macos_st_framebuffer_validate_textures(&macfb->base,
            drawableSize.width || 1, drawableSize.height || 1, statt_mask);

      if (!ret)
         return ret;

      // potentially copy the old back to the new front
      /* if (!resized) {
         enum st_attachment_type back, front;

         back = ST_ATTACHMENT_BACK_LEFT;
         front = ST_ATTACHMENT_FRONT_LEFT;
      } */
   }

   for (i = 0; i < count; i++)
     pipe_resource_reference(&out[i], macfb->textures[statts[i]]);
   /* if (resolve && drawable->visual->samples > 1) {
     if (statt_mask & BITFIELD_BIT(ST_ATTACHMENT_FRONT_LEFT))
       pipe_resource_reference(resolve, macfb->display_resource);
     else if (statt_mask & BITFIELD_BIT(ST_ATTACHMENT_BACK_LEFT))
       pipe_resource_reference(resolve, macfb->display_resource);
   } */

   return true;
}

struct macos_st_framebuffer *
macos_create_st_framebuffer(struct pipe_frontend_screen *fscreen,
   struct st_visual *visual, void *native, struct size (*getSize) (void *)) {

   printf("create the framebuffer!\n");
   struct macos_st_framebuffer *fb;

   assert(visual);

   fb = CALLOC_STRUCT(macos_st_framebuffer);
   if(!fb) {
      return NULL;
   }

   fb->visual = *visual;
   fb->screen = fscreen->screen;
   fb->width = 0;
   fb->height = 0;
   fb->native = native;
   fb->getSize = getSize;

   fb->target = PIPE_TEXTURE_2D; // ?

   fb->base.flush_front = macos_st_framebuffer_flush_front;
   fb->base.validate = macos_st_framebuffer_validate;
   fb->base.visual = &fb->visual;

   p_atomic_set(&fb->base.stamp, 1);
   fb->base.ID = p_atomic_inc_return(&macos_fb_ID);
   fb->base.fscreen = fscreen;

   return fb;
}






