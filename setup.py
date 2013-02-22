#! /usr/bin/env python

from setuptools import setup, Extension

deepin_pulseaudio_mod = Extension('deepin_pulseaudio', 
                libraries = ['pulse'],
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
