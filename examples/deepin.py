#! /usr/bin/env python
# -*- coding: utf-8 -*-

# Copyright (C) 2013 Deepin, Inc.
#               2013 Zhai Xiang
# 
# Author:     Zhai Xiang <zhaixiang@linuxdeepin.com>
# Maintainer: Zhai Xiang <zhaixiang@linuxdeepin.com>
# 
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

import deepin_pulseaudio

def test():
    deepin_pulseaudio_obj = deepin_pulseaudio.new()
    deepin_pulseaudio_obj.get_devices()
    print "========get_output_ports testcase========"
    output_ports = deepin_pulseaudio_obj.get_output_ports()
    print output_ports
    print "========get_input_ports testcase========"
    input_ports = deepin_pulseaudio_obj.get_input_ports()
    print input_ports
    print "========get_output_devices testcase========"
    output_devices = deepin_pulseaudio_obj.get_output_devices()
    print output_devices
    print "========get_input_devices testcase========"
    input_devices = deepin_pulseaudio_obj.get_input_devices()
    print input_devices
    print "========get_output_channels testcase========"
    print deepin_pulseaudio_obj.get_output_channels("")
    print "========get_input_channels testcase========"
    print deepin_pulseaudio_obj.get_input_channels("")
    print "========get_output_active_ports testcase========"
    print deepin_pulseaudio_obj.get_output_active_ports("")
    print "========get_input_active_ports testcase========" 
    print deepin_pulseaudio_obj.get_input_active_ports("")
    print "========get_output_mute testcase========"
    print deepin_pulseaudio_obj.get_output_mute(output_devices[0][1])
    print "========get_input_mute testcase========"                            
    print deepin_pulseaudio_obj.get_input_mute(input_devices[0][1])      
    print "========get_output_volume testcase========"
    print deepin_pulseaudio_obj.get_output_volume("")
    print "========get_input_volume testcase========"
    print deepin_pulseaudio_obj.get_input_volume("")
    print "========set_output_active_port testcase========"
    print deepin_pulseaudio_obj.set_output_active_port(1, output_ports[1][1])
    print "========set_input_active_port testcase========"                     
    print deepin_pulseaudio_obj.set_input_active_port(0, input_ports[0][1])   

test()
