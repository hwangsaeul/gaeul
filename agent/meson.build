agents = [
  [ 'GAEUL2-SA', 'source-agent'],
]

foreach agent: agents
  exe_name = 'gaeul@0@-@1@'.format(abiversion, agent[1])
  src_file = 'gaeul@0@-@1@.c'.format(abiversion, agent[1])
  log_c_arg = '-DG_LOG_DOMAIN="@0@"'.format(agent[0])

  executable (exe_name,
    src_file,
    install: true,
    include_directories: gaeul_incs,
    dependencies: [ libgaeul_dep ],
    c_args: [ log_c_arg ]
  )
    
endforeach
