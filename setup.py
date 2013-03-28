#! /usr/bin/env python

from setuptools import setup, Extension
import commands

def pkg_config_cflags(pkgs):                                                    
    '''List all include paths that output by `pkg-config --cflags pkgs`'''         
    return map(lambda path: path[2::], commands.getoutput('pkg-config --cflags-only-I %s' % (' '.join(pkgs))).split())

deepin_pulseaudio_mod = Extension('deepin_pulseaudio', 
                include_dirs = pkg_config_cflags(['glib-2.0']), 
                libraries = ['pulse', 'pulse-mainloop-glib'], 
                sources = ['deepin_pulseaudio.c'])

deepin_pulseaudio_small_mod = Extension('deepin_pulseaudio_small',                          
                include_dirs = pkg_config_cflags(['glib-2.0']),                 
                libraries = ['pulse', 'pulse-mainloop-glib'],                   
                sources = ['deepin_pulseaudio_small.c'])

deepin_pulseaudio_lite_mod = Extension('deepin_pulseaudio_lite',                          
                include_dirs = pkg_config_cflags(['glib-2.0']),                 
                libraries = ['pulse', 'pulse-mainloop-glib'],                   
                sources = ['deepin_pulseaudio_lite.c'])

setup(name='pypulseaudio',
      version='0.1',
      ext_modules = [deepin_pulseaudio_mod, deepin_pulseaudio_lite_mod, deepin_pulseaudio_small_mod],
      description='PulseAudio Python binding.',
      long_description ="""PulseAudio Python binding.""",
      author='Linux Deepin Team',
      author_email='zhaixiang@linuxdeepin.com',
      license='GPL-3',
      url="https://github.com/linuxdeepin/pypulseaudio",
      download_url="git@github.com:linuxdeepin/pypulseaudio.git",
      platforms = ['Linux'], 
      )
