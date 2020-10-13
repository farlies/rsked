#############################################################################
# Part of the rsked package
#
# Prior to using meson for builds, use the script:        ./setup.sh
# to produce a build directory for the desired build
# configuration.
#
#############################################################################


project('rsked','cpp',
        version: '1.0.3',
        license: 'Apache 2.0')

# Target machine  'x86_64' or 'armv7l' (RPi)
tmach = get_option('target_machine')
btype = get_option('buildtype')


# Create version.h with the shared version number
vers_data = configuration_data()
vers_data.set('version',meson.project_version() + '-'  + tmach + '-' + btype)
configure_file(input : 'util/version.h.in',
               output : 'version.h',
               configuration : vers_data)

# C++ Compiler options
if meson.get_compiler('cpp').get_id() == 'clang'
  my_cpp_args = ['-std=c++17','-Wall', '-Wextra', '-Wpedantic',
                 '-Wconversion', '-Wshadow', '-Weffc++']
else # gcc
  my_cpp_args = ['-std=c++17','-Wall', '-Wextra', '-Wpedantic',
                 '-Wconversion', '-Wduplicated-branches', '-Wshadow',
                 '-Weffc++', '-Wno-psabi']
endif

# COMMON DEPENDENCIES

boost_dep = dependency('boost',
   modules : ['system','log','log_setup','program_options',
              'thread','filesystem', 'date_time'])

if get_option('buildtype') == 'debug'
  boost_utest_dep = dependency('boost', modules : ['unit_test_framework'])
else
  boost_utest_dep = disabler()
endif

json_dep  = dependency('jsoncpp')
pulse_dep = dependency('libpulse')
usb_dep  =  dependency('libusb-1.0')
mpd_dep = dependency('libmpdclient')
libgpiod_dep = dependency('libgpiodcxx')

utils = ['util/jobutil.cc','util/logging.cc','util/childmgr.cc',
         'util/configutil.cc','util/config.cc']

# Include Directories
shared_incdirs = include_directories('util')
rsked_incdirs = include_directories('rsked')
cooling_incdirs = include_directories('cooling')

# RSKED application
#    TODO: configure to build with only certain players
rsked_srcs = ['rsked/main.cc', 'rsked/rsked.cc',
              'rsked/respath.cc',
              'rsked/source.cc',
              'rsked/schedule.cc',
              'rsked/baseplayer.cc',
              'rsked/playermgr.cc',
              'rsked/inetcheck.cc',
              'rsked/vurunner.cc',
              'rsked/oggplayer.cc',
              'rsked/mp3player.cc',
              'rsked/mpdclient.cc', 'rsked/mpdplayer.cc',
              'rsked/gqrxclient.cc', 'rsked/sdrplayer.cc', 'util/usbprobe.cc'
             ] + utils

# COOLING application
cooling_srcs = ['cooling/main.cc', 'cooling/cooling.cc' ] + utils

# VUMONITOR application
vu_srcs = ['vumonitor/vumonitor.cc', 'util/jobutil.cc',
           'util/configutil.cc',  'util/logging.cc']


# Unit tests
tsrc_srcs = ['test/tsrc.cc', 'rsked/respath.cc', 'rsked/source.cc']+utils

tsked_srcs = ['test/tsked.cc', 'rsked/source.cc', 
              'rsked/respath.cc', 'rsked/schedule.cc']+utils

tproc_srcs = ['test/tproc.cc','util/logging.cc',
              'util/childmgr.cc','util/configutil.cc']

tconfig_srcs = ['test/tconfig.cc','util/logging.cc',
                'util/configutil.cc','util/config.cc']

tncheck_srcs = ['test/tncheck.cc', 'rsked/inetcheck.cc']+utils

ttimeout_srcs = ['test/ttimeout.cc']

mpdtest_srcs = ['test/mpdtest.cc', 'rsked/mpdclient.cc']+utils

#------------------------------------------------------------------------------

# 1. This program manages audio players
executable('rsked',
           sources : rsked_srcs,
           cpp_args : my_cpp_args,
           link_args : '-pthread',
           install : true,
           include_directories : [shared_incdirs,rsked_incdirs],
           dependencies : [ boost_dep, json_dep, mpd_dep, usb_dep ])

# 2. This program manages rsked, and controls cooling fan
executable('cooling',
           sources : cooling_srcs,
           cpp_args : my_cpp_args,
           install : true,
           include_directories : [shared_incdirs, cooling_incdirs],
           dependencies : [ boost_dep, json_dep, libgpiod_dep ],
           link_args : '-pthread'
       )

# 3. This program monitors VU levels for prolonged silence.
executable('vumonitor',
           sources: vu_srcs,
           cpp_args : my_cpp_args,
           link_args : '-pthread',
           install : true,
           include_directories : [shared_incdirs],
           dependencies : [ boost_dep, pulse_dep ])

# 4. Tests for the rsked Source class.
executable('tsrc',
           sources: tsrc_srcs,
           cpp_args : my_cpp_args,
           include_directories : [shared_incdirs,rsked_incdirs],
           dependencies : [ boost_dep, boost_utest_dep, json_dep])

# 5. Tests for the rsked schedule class.
executable('tsked',
           sources: tsked_srcs,
           cpp_args : my_cpp_args,
           include_directories : [shared_incdirs,rsked_incdirs],
           dependencies : [ boost_dep, boost_utest_dep, json_dep])

# 6. Tests for Config
executable('tconfig',
           sources: tconfig_srcs,
           cpp_args : my_cpp_args,
           include_directories : [shared_incdirs,rsked_incdirs],
           dependencies : [ boost_dep, boost_utest_dep, json_dep ])

# 7. Tests for GPIO
# executable('gpiotest',
#            sources: [ 'test/gpiotest.cc' ],
#            cpp_args : my_cpp_args,
#            dependencies : [ libgpiod_dep ])


# 8. Test the inet_checker class
executable('tncheck',
           sources: tncheck_srcs,
           cpp_args : my_cpp_args,
           include_directories : [shared_incdirs,rsked_incdirs],
           dependencies : [ boost_dep, boost_utest_dep, json_dep])

# 9. Test reading socket with timeout
# executable('ttimeout',
#            sources: ttimeout_srcs,
#            cpp_args : my_cpp_args,
#            include_directories : [shared_incdirs],
#            dependencies : [ boost_dep ])

# 10. Tests for Child_mgr
# executable('tproc',
#             sources: tproc_srcs,
#             cpp_args : my_cpp_args,
#             include_directories : [shared_incdirs,rsked_incdirs],
#             dependencies : [ boost_dep ])


# 11. Tests for Mpd_client
executable('mpdtest',
            sources: mpdtest_srcs,
            cpp_args : my_cpp_args,
            include_directories : [shared_incdirs,rsked_incdirs],
            dependencies : [ boost_dep, mpd_dep, json_dep ])

##########

## INSTALL

# Scripts are installed in <bindir>, conventionally $HOME/bin

install_subdir('scripts',
               strip_directory : true,
               install_dir : get_option('bindir'))

# Configuration files are installed in subdirectories of <data_dir>.
# This is typically ~/.config

# RSKED
install_subdir('config' / tmach / 'rsked',
               strip_directory : true,
               install_dir : get_option('datadir') / 'rsked' )

# audio files for rsked
install_subdir('resource', install_dir : get_option('datadir') / 'rsked')
install_subdir('motd', install_dir : get_option('datadir') / 'rsked')


# MPD
install_subdir('config' / tmach / 'mpd',
               strip_directory : true,
               install_dir : get_option('datadir') / 'mpd')


# GQRX
install_subdir('config' / tmach / 'gqrx',
               strip_directory : true,
               install_dir : get_option('datadir') / 'gqrx')

