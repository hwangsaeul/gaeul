source_h = [
  'relay-application.h',
]

source_c = [
  'relay-application.c',
]

# GSettings Schema
relay_setting = gaeul_fqdn_prefix + '.Relay.gschema.xml'

install_data([ relay_setting ],
  install_dir: join_paths(get_option('datadir'), 'glib-2.0', 'schemas'))

relay_schemas = gnome.compile_schemas(depend_files: relay_setting)
relay_schemas_dir = meson.current_build_dir()

libgaeul_relay = library(
  'gaeul-relay-@0@'.format(apiversion),
  source_c, relay_schemas,
  version: libversion,
  soversion: soversion,
  include_directories: gaeul_incs,
  dependencies: [ hwangsae_dep, libgaeul_dep ],
  c_args: [ '-DG_LOG_DOMAIN="G2RLY"' ],
  link_args: common_ldflags,
  install: true
)

libgaeul_relay_dep = declare_dependency (link_with: libgaeul_relay, 
  include_directories: [ gaeul_incs ],
  dependencies: [ gaeguli_dep ]
)
