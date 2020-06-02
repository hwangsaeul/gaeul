source_h = [
]

source_c = [
  'gaeul2-agent.c',
]

gaeul_agent_c_args = [
  '-DG_LOG_DOMAIN="GAEUL2-AGENT"',
]

schema_xmls = [
  'org.hwangsaeul.Gaeul2.gschema.xml',
]

schema = gnome.compile_schemas (
  depend_files: files(schema_xmls),
)

gio_schemasdir = dependency('gio-2.0').get_pkgconfig_variable(
  'schemasdir',
  define_variable: [ 'datadir', join_paths(get_option('prefix'), get_option('datadir')) ],
  default: join_paths(get_option('prefix'), get_option('datadir'), 'glib-2.0/schemas'),
)

foreach xml: schema_xmls
  install_data(
    xml,
    install_dir: gio_schemasdir,
  )
endforeach

gaeul_agent = executable (
  'gaeul2-agent',
  sources: [ source_c, schema ],
  include_directories: gaeul_incs,
  c_args: gaeul_agent_c_args,
  dependencies: [ gobject_dep, gaeguli_dep, json_glib_dep ],
  install: true
)
