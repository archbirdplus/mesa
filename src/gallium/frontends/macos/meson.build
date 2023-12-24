# TODO: Copyright header

libmacos = static_library(
    'macos',
    files('macos_api.c'),
    include_directories : [
        inc_include, inc_src, inc_gallium, inc_gallium_aux, inc_mapi, inc_mesa
    ],
    dependencies : [
        driver_zink
    ],
)

