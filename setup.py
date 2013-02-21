#! /usr/bin/env python

from setuptools import setup, Extension
import commands

def pkg_config_cflags(pkgs):                                                    
    '''List all include paths that output by `pkg-config --cflags pkgs`'''         
    return map(lambda path: path[2::], commands.getoutput('pkg-config --cflags-only-I %s' % (' '.join(pkgs))).split())

deepin_pulseaudio_mod = Extension('deepin_pulseaudio', 
                include_dirs = pkg_config_cflags(['glib-2.0']),
                libraries = ['pulse', 'pulse-mainloop-glib'],
                #libraries = ['pulse'],
                sources = ['deepin_pulseaudio.c'])

setup(name='deepin_pulseaudio',
      version='0.1',
      ext_modules = [deepin_pulseaudio_mod],
      description='PulseAudio Python binding.',
      long_description ="""PulseAudio Python binding.""",
      author='Linux Deepin Team',
      author_email='zhaixiang@linuxdeepin.com',
      license='GPL-3',
      url="https://github.com/xiangzhai/pypulseaudio",
      download_url="git@github.com:xiangzhai/pypulseaudio.git",
      platforms = ['Linux'],
      )
