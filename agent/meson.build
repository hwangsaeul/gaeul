agents = [
  [ 'GAEUL2-SA', 'source-agent', [ libgaeul_source_dep ] ],
  [ 'GAEUL2-MJ', 'mjpeg-agent', [ libgaeul_mjpeg_dep ] ],
  [ 'GAEUL2-RY', 'relay-agent', [ libgaeul_relay_dep ] ],
]

foreach agent: agents
  exe_name = 'gaeul@0@-@1@'.format(abiversion, agent[1])
  src_file = 'gaeul@0@-@1@.c'.format(abiversion, agent[1])
  log_c_arg = '-DG_LOG_DOMAIN="@0@"'.format(agent[0])

  executable (exe_name,
    src_file,
    install: true,
    include_directories: gaeul_incs,
    dependencies: [ agent[2], libgaeul_dep ],
    c_args: [ log_c_arg ]
  )
    
endforeach
