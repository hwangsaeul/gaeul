source_h = [
  'source-application.h',
] 

source_c = [
  'source-application.c',
]

# GSettings Schema
source_setting = gaeul_fqdn_prefix + '.Source.gschema.xml'

install_data([ source_setting ],
  install_dir: join_paths(get_option('datadir'), 'glib-2.0', 'schemas'))

source_schemas = gnome.compile_schemas(depend_files: source_setting)
source_schemas_dir = meson.current_build_dir()

# DBus
dbus_sources = gnome.gdbus_codegen(
  'source-generated',
  sources: gaeul_fqdn_prefix + '.Source.xml',
  interface_prefix: gaeul_fqdn_prefix,
  namespace: 'Gaeul2DBus',
)

libgaeul_source = library(
  'gaeul-source-@0@'.format(apiversion),
  source_c, source_schemas, dbus_sources,
  version: libversion,
  soversion: soversion,
  include_directories: gaeul_incs,
  dependencies: [ gaeguli_dep, libgaeul_dep ],
  c_args: [ '-DG_LOG_DOMAIN="G2SRC"' ],
  link_args: common_ldflags,
  install: true
)

libgaeul_source_dep = declare_dependency (link_with: libgaeul_source, 
  include_directories: [ gaeul_incs ],
  dependencies: [ gaeguli_dep ]
)
