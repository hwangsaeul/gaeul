source_h = [
  'gaeul-agent.h',
]

source_c = [
  'gaeul-agent.c',
]

gaeul_agent_c_args = [
  '-DG_LOG_DOMAIN="GAEUL-AGENT"',
]


gaeul_agent = executable (
  'gaeul-agent',
  source_c,
  include_directories: gaeul_incs,
  c_args: gaeul_agent_c_args,
  dependencies: [ gobject_dep, gio_dep, libgaeul_dbus_dep ],
  install: true
)
