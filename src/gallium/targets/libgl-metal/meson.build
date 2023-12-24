# TODO: Copyright header

libgl = shared_library(
  'GL',
  files('metal.c'),
  include_directories : [
    inc_include, inc_src, inc_mapi, inc_mesa, inc_gallium, inc_gallium_aux, inc_gallium_drivers, include_directories('../../frontends/macos'),
  ],
  c_args : ['-DGALLIUM_SOFTPIPE'],
  link_depends: [
    files('libgl-metal.sym')
  ],
  link_args : [ld_args_bsymbolic, ld_args_gc_sections],
  link_whole : [libmacos, libglapi_static],
  link_with : [
    libgallium, libmesa, libglapi
  ],
  dependencies : [dep_thread, dep_clock, dep_unwind, driver_swrast, driver_zink],
  install : true,
  version : '1.2.0',
  darwin_versions : '1.2.0',
)

