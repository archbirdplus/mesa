#ifndef NOUVEAU_DEVICE
#define NOUVEAU_DEVICE 1

#include "nouveau_private.h"

#include <stddef.h>

enum nvk_debug {
   /* dumps all push buffers after submission */
   NVK_DEBUG_PUSH_DUMP = 1ull << 0,

   /* push buffer submissions wait on completion
    *
    * This is useful to find the submission killing the GPU context. For easier debugging it also
    * dumps the buffer leading to that.
    */
   NVK_DEBUG_PUSH_SYNC = 1ull << 1,
};

enum nouveau_ws_device_type {
   NOUVEAU_WS_DEVICE_TYPE_IGP = 0,
   NOUVEAU_WS_DEVICE_TYPE_DIS = 1,
   NOUVEAU_WS_DEVICE_TYPE_SOC = 2,
};

struct nouveau_ws_device {
   int fd;

   uint16_t vendor_id;
   uint16_t device_id;
   enum nouveau_ws_device_type device_type;
   uint32_t chipset;

   char *chipset_name;
   char *device_name;

   /* first byte of class id */
   uint8_t cls;
   /* maps to CUDAs Compute capability version */
   uint8_t cm;

   uint64_t vram_size;
   uint64_t gart_size;
   bool is_integrated;
   uint32_t local_mem_domain;

   uint8_t gpc_count;
   uint16_t mp_count;

   enum nvk_debug debug_flags;
};

struct nouveau_ws_device *nouveau_ws_device_new(int fd);
void nouveau_ws_device_destroy(struct nouveau_ws_device *);

#endif