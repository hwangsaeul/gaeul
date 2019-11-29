sources = [
]

dbus_ifaces = [
  [ 'Manager', 'dbus-manager-generated' ]
]

dbus_headers = []

foreach iface: dbus_ifaces
  prefix = 'org.hwangsaeul.Gaeul.'
  name = prefix + iface[0] 

  if meson.version().version_compare('>= 0.46.0')
    sources += gnome.gdbus_codegen(
      iface[1],
      sources: name + '.xml',
      interface_prefix: prefix,
      namespace: 'GaeulDBus',
    )
  else
    sources += custom_target(
      iface[1],
      input: name + '.xml',
      output: [iface[1] + '.h', iface[1] + '.c'],
      command: [
        find_program('codegen.py'), iface[0], iface[1],
        meson.current_build_dir(), '@INPUT@'
      ]
    )
  endif

endforeach

libgaeul_dbus = static_library(
  'gaeul-dbus-@0@'.format(apiversion),
  sources,
  version: libversion,
  soversion: soversion,
  include_directories: gaeul_incs,
  dependencies: [ gio_dep ],
  c_args: gaeul_c_args,
  link_args: common_ldflags,
  install: false
)

libgaeul_dbus_dep = declare_dependency(link_with: libgaeul_dbus,
  include_directories: [ gaeul_incs ],
  dependencies: [ gio_dep ]
)

install_data(
  'org.hwangsaeul.Gaeul.conf',
  install_dir : join_paths(get_option('datadir'), 'dbus-1', 'system.d')
)
