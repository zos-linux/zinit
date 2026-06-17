initsrc = files(
    'main.cpp',
    'osdetector.cpp',
    'events.cpp',
    'parse-file.cpp',
    'sserver/socketstuff.cpp'
)

inc = include_directories('../include')

executable(
    'zinit',
    initsrc,
    include_directories: inc,
    install: true,
    install_dir: get_option('sbindir')
)

servicedsrc = files(
    'serviced.cpp',
    'serviced-file-management.cpp',
    'serviced-svu.cpp',
    'parse-file.cpp',
    'events.cpp',
    'sserver/socketstuff.cpp'
)

executable(
    'zinit-serviced',
    servicedsrc,
    include_directories: inc,
    install: true,
    install_dir: get_option('sbindir')
)

systemctlsrc = files(
    'control/systemctl.cpp',
    'control/systemctl-misc.cpp',
    'events.cpp',
    'sserver/socketstuff.cpp'
)

executable(
    'systemctl',
    systemctlsrc,
    include_directories: inc,
    install: true
)
