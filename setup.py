#! /usr/bin/env python

from setuptools import setup, Extension

deepin_pulseaudio_mod = Extension('deepin_pulseaudio', 
                libraries = ['pulse'],
                sources = ['deepin_pulseaudio.c'])

deepin_pulseaudio_signal_mod = Extension('deepin_pulseaudio_signal', 
                libraries = ['pulse'], 
                sources = ['deepin_pulseaudio_signal.c'])

setup(name='pypulseaudio',
      version='0.1',
      ext_modules = [deepin_pulseaudio_mod, deepin_pulseaudio_signal_mod],
      description='PulseAudio Python binding.',
      long_description ="""PulseAudio Python binding.""",
      author='Linux Deepin Team',
      author_email='zhaixiang@linuxdeepin.com',
      license='GPL-3',
      url="https://github.com/linuxdeepin/pypulseaudio",
      download_url="git@github.com:linuxdeepin/pypulseaudio.git",
      platforms = ['Linux'], 
      )
