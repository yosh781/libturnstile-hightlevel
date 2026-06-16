project('libturnstile-highlevel', 'c',
    version: '0.7.0',
    license: 'BSD-2-Clause',
    default_options: [
        'c_std=c11',
        'buildtype=release',
        'default_library=both'
    ]
)

# Library sources
lib_sources = [
    'libturnstile/turnstile-highlevel.c'
]

lib_headers = [
    'include/turnstile-highlevel.h'
]

# Version script for symbol visibility
version_script = 'libturnstile/version-script.map'

# Build library with version script
libturnstile_highlevel = library('turnstile-highlevel',
    lib_sources,
    version: '0.7.0',
    soversion: '0',
    include_directories: include_directories('include'),
    link_args: ['-Wl,--version-script=' + meson.current_source_dir() + '/' + version_script],
    install: true,
    install_dir: [true, true, true]
)

# Install headers
install_headers(lib_headers, subdir: 'turnstile')

# Pkg-config file
pkg = import('pkgconfig')
pkg.generate(libturnstile_highlevel,
    name: 'libturnstile-highlevel',
    description: 'High-level Turnstile session management library',
)

# Test program
executable('turnstile-test',
    'test_highlevel.c',
    link_with: libturnstile_highlevel,
    include_directories: include_directories('include'),
    install: false
)

# turnstile-ctl utility
executable('turnstile-ctl',
    'src/turnstile-ctl.c',
    link_with: libturnstile_highlevel,
    include_directories: include_directories('include'),
    install: true,
    install_dir: 'bin'
)
