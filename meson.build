project('gaeul', 'c',
  version: '0.10.0',
  license: 'Apache-2.0',
  meson_version: '>= 0.40',
  default_options: [ 'warning_level=1',
                     'buildtype=debugoptimized' ])

project_version = meson.project_version()
version_arr = project_version.split('.')
project_version_major = version_arr[0].to_int()
project_version_minor = version_arr[1].to_int()
project_version_micro = version_arr[2].to_int()

apiversion = '1.0'
soversion = 0
curversion = project_version_minor * 100 + project_version_micro
libversion = '@0@.@1@.0'.format(soversion, curversion)
gaeul_api_name = '@0@-@1@'.format(meson.project_name(), apiversion)

prefix = get_option('prefix')

cc = meson.get_compiler('c')

cdata = configuration_data()
cdata.set_quoted('PACKAGE_STRING', meson.project_name())
cdata.set_quoted('PACKAGE_NAME', meson.project_name())
cdata.set_quoted('PACKAGE', meson.project_name())
cdata.set_quoted('VERSION', meson.project_version())

# TODO: Check host_system
cdata.set('LIBDIR', join_paths(get_option('prefix'), get_option('libdir')))

configure_file(output : 'config.h', configuration : cdata)

# Dependencies
glib_req_version = '>= 2.44.0'

glib_dep = dependency('glib-2.0', version: glib_req_version,
    fallback: ['glib', 'libglib_dep'])
gio_dep = [dependency('gio-2.0', version: glib_req_version,
                  fallback: ['glib', 'libgio_dep']),
           dependency('gio-unix-2.0', version: glib_req_version,
                  fallback: ['glib', 'libgio_dep'])]
gobject_dep = dependency('gobject-2.0', version: glib_req_version,
    fallback: ['glib', 'libgobject_dep'])

json_glib_dep = dependency ('json-glib-1.0', version: '>= 1.1.2')

hwangsaeul_req_version = '>= 0.10.0'

gaeguli_dep = dependency('gaeguli-1.0', version: hwangsaeul_req_version,
    fallback: ['gaeguli', 'libgaeguli_dep'])
chamge_dep = dependency('chamge-1.0', version: hwangsaeul_req_version,
    fallback: ['chamge', 'libchamge_dep'])

gnome = import('gnome')

gaeul_incs = include_directories('.', 'dbus')

common_ldflags = [ ]

gaeul_c_args = [
  '-DG_LOG_DOMAIN="GAEUL"',
]

subdir('dbus')
subdir('agent')

if meson.version().version_compare('>= 0.46.0')
  python3 = import('python').find_installation()
  run_command(python3, '-c',
    'import shutil; shutil.copy("hooks/pre-commit.hook", ".git/hooks/pre-commit")')
endif
