sources = [
]

dbus_ifaces = [
  [ 'Manager', 'dbus-manager-generated' ]
]

dbus_headers = []

foreach iface: dbus_ifaces
  prefix = 'org.hwangsaeul.Gaeul.'
  name = prefix + iface[0] 
  dbus_sources = gnome.gdbus_codegen(
    iface[1],
    sources: name + '.xml',
    interface_prefix: prefix,
    namespace: 'GaeulDBus',
  )
  dbus_headers = dbus_sources[0]
  sources += dbus_sources[1]

endforeach

libgaeul_dbus = static_library(
  'gaeul-dbus-@0@'.format(apiversion),
  sources, dbus_headers,
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
