source_h = [
  'application.h',
  'source-application.h',
]

source_c = [
  'application.c',
  'source-application.c',
]

gsettings_schemas = [
  'org.hwangsaeul.Gaeul2.Source.gschema.xml',
  'org.hwangsaeul.Gaeul2.Relay.gschema.xml',
]

install_data(gsettings_schemas,
  install_dir: join_paths(get_option('datadir'), 'glib-2.0', 'schemas'))

libgaeul = library(
  'gaeul-@0@'.format(apiversion),
  source_c,
  version: libversion,
  soversion: soversion,
  include_directories: gaeul_incs,
  dependencies: [ gio_dep, gaeguli_dep ],
  c_args: [ '-DG_LOG_DOMAIN="GAEUL2"' ],
  link_args: common_ldflags,
  install: true
)

libgaeul_dep = declare_dependency(link_with: libgaeul,
  include_directories: [ gaeul_incs ],
  dependencies: [ gaeguli_dep ]
)

# Used only in the test suite
gaeul_schemas = gnome.compile_schemas(depend_files: gsettings_schemas)
gaeul_schemas_dir = meson.current_build_dir()

