gaeul_install_header_subdir = join_paths(gaeul_api_name, 'gaeul')

gaeul_fqdn_prefix = 'org.hwangsaeul.Gaeul2'

source_h = [
  'gaeul.h',
  'application.h',
  'tuple.h',
  'stream-authenticator.h',
]

source_c = [
  'gaeul.c',
  'application.c',
  'tuple.c',
  'stream-authenticator.c',
]

gstreamer_dep = dependency ('gstreamer-1.0', version: '>= 1.14.0')
gstreamer_pbutils_dep = dependency ('gstreamer-pbutils-1.0')

gaeul_enums = gnome.mkenums(
  'enumtypes.h',
  h_template: 'enumtypes.h.template',
  c_template: 'enumtypes.c.template',
  sources: source_h,
  install_dir: join_paths(get_option('includedir'), gaeul_install_header_subdir),
  install_header: true,
)

libgaeul = library(
  'gaeul-@0@'.format(apiversion),
  gaeul_enums, source_c,
  version: libversion,
  soversion: soversion,
  include_directories: gaeul_incs,
  dependencies: [
    gio_dep,
    gaeguli_dep,
    gstreamer_dep,
    gstreamer_pbutils_dep,
    hwangsae_dep,
  ],
  c_args: [ '-DG_LOG_DOMAIN="GAEUL2"' ],
  link_args: common_ldflags,
  install: true
)

libgaeul_dep = declare_dependency(link_with: libgaeul,
  include_directories: [ gaeul_incs ],
  dependencies: [ gaeguli_dep ]
)

subdir('mjpeg')
subdir('source')
subdir('relay')
