source_h = [
  'mjpeg-application.h',
  'mjpeg-request.h',
]

source_c = [
  'mjpeg-application.c',
  'mjpeg-request.c',
]

# GSettings Schema
mjpeg_setting = gaeul_fqdn_prefix + '.MJPEG.gschema.xml'

install_data([ mjpeg_setting ],
  install_dir: join_paths(get_option('datadir'), 'glib-2.0', 'schemas'))

mjpeg_schemas = gnome.compile_schemas(depend_files: mjpeg_setting)
mjpeg_schemas_dir = meson.current_build_dir()

# DBus
dbus_sources = gnome.gdbus_codegen(
  'mjpeg-generated',
  sources: gaeul_fqdn_prefix + '.MJPEG.Service.xml',
  interface_prefix: gaeul_fqdn_prefix,
  namespace: 'Gaeul2DBus',
)

libgaeul_mjpeg = library(
  'gaeul-mjpeg-@0@'.format(apiversion),
  source_c, mjpeg_schemas, dbus_sources,
  version: libversion,
  soversion: soversion,
  include_directories: gaeul_incs,
  dependencies: [ soup_dep, libgaeul_dep ],
  c_args: [ '-DG_LOG_DOMAIN="G2MJPG"' ],
  link_args: common_ldflags,
  install: true
)

libgaeul_mjpeg_dep = declare_dependency (link_with: libgaeul_mjpeg, 
  include_directories: [ gaeul_incs ],
  dependencies: [ gaeguli_dep ]
)
