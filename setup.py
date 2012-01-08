from distutils.core import setup, Extension

module1 = Extension('libre',
                    define_macros = [('HAVE_INET6', '1')],
                    include_dirs = ['/usr/local/include/re'],
                    libraries = ['re'],
                    library_dirs = ['/usr/local/lib'],
                    sources = ['src/init.c',
                               'src/main.c',
                               'src/sip.c'])

setup (name = 'libre',
       version = '0.1',
       description = 'Python wrapper for Libre',
       ext_modules = [module1])
