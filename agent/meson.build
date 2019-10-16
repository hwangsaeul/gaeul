source_h = [
  'gaeul-agent.h',
]

source_c = [
  'gaeul-agent.c',
]

gaeul_agent_c_args = [
  '-DG_LOG_DOMAIN="GAEUL-AGENT"',
]

schema_xmls = [
  'org.hwangsaeul.Gaeul.gschema.xml',
]

schema = gnome.compile_schemas (
  depend_files: files(schema_xmls),
)

if meson.version().version_compare('>= 0.45.1')

  gio_schemasdir = dependency('gio-2.0').get_pkgconfig_variable(
    'schemasdir',
    define_variable: [ 'datadir', join_paths(get_option('prefix'), get_option('datadir')) ],
    default: join_paths(get_option('prefix'), get_option('datadir'), 'glib-2.0/schemas'),
  )

else

  # only for ubuntu 16.04
  gio_schemasdir = join_paths(get_option('prefix'), get_option('datadir'), 'glib-2.0/schemas')

endif

foreach xml: schema_xmls
  install_data(
    xml,
    install_dir: gio_schemasdir,
  )
endforeach

gaeul_agent = executable (
  'gaeul-agent',
  sources: [ source_c, schema ],
  include_directories: gaeul_incs,
  c_args: gaeul_agent_c_args,
  dependencies: [ gobject_dep, libgaeul_dbus_dep, chamge_dep, gaeguli_dep, json_glib_dep ],
  install: true
)
