
project('lai', 'c')

include = include_directories('include')

library = static_library('lai',
        'src/eval.c',
        'src/exec.c',
        'src/exec2.c',
        'src/execns.c',
        'src/libc.c',
        'src/ns.c',
        'src/opregion.c',
        'src/os_methods.c',
        'src/pciroute.c',
        'src/resource.c',
        'src/sci.c',
        'src/sleep.c',
    include_directories: include)

dependency = declare_dependency(link_with: library,
    include_directories: include)

