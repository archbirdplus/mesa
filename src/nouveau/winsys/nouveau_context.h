#ifndef NOUVEAU_CONTEXT
#define NOUVEAU_CONTEXT 1

#include "nouveau_private.h"

struct nouveau_ws_device;

struct nouveau_ws_object {
   uint8_t __pad;
};

struct nouveau_ws_context {
   struct nouveau_ws_device *dev;

   int channel;

   struct nouveau_ws_object eng2d;
   struct nouveau_ws_object m2mf;
   struct nouveau_ws_object compute;
};

int nouveau_ws_context_create(struct nouveau_ws_device *, struct nouveau_ws_context **out);
void nouveau_ws_context_destroy(struct nouveau_ws_context *);

#endif