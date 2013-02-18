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
import time

def m_sink_new_cb(obj, index):
    print "DEBUG sink new callback", index
    sink_dev = obj.get_output_devices()
    print sink_dev[index]['mute'], obj.get_output_active_ports_by_index(index)[1], obj.get_output_volume_by_index(index)
    
def m_sink_changed_cb(obj, index):
    print "DEBUG sink changed callback", index
    sink_dev = obj.get_output_devices()
    print sink_dev[index]['mute'], obj.get_output_active_ports_by_index(index)[1], obj.get_output_volume_by_index(index)
    
def m_sink_removed_cb(obj, index):
    print "DEBUG sink remove callback", index

    
def m_source_new_cb(obj, index):
    print "DEBUG source new callback", index
    source_dev = obj.get_input_devices()
    print source_dev[index]['mute'], obj.get_input_active_ports_by_index(index)[1], obj.get_input_volume_by_index(index)
    
def m_source_changed_cb(obj, index):
    print "DEBUG source changed callback", index
    source_dev = obj.get_input_devices()
    print source_dev[index]['mute'], obj.get_input_active_ports_by_index(index)[1], obj.get_input_volume_by_index(index)
    
def m_source_removed_cb(obj, index):
    print "DEBUG source remove callback", index

    
def m_card_new_cb(obj, index):
    print "DEBUG card new callback", index
    card_dev = obj.get_cards()
    print card_dev[index]
    
def m_card_changed_cb(obj, index):
    print "DEBUG card changed callback", index
    card_dev = obj.get_cards()
    print card_dev[index]
    
def m_card_removed_cb(obj, index):
    print "DEBUG card remove callback", index

    
def m_server_new_cb(obj):
    print "DEBUG server new callback"
    print obj.get_server_info()
    
def m_server_changed_cb(obj):
    print "DEBUG server changed callback"
    print obj.get_server_info()
    
def m_server_removed_cb(obj, index):
    print "DEBUG server remove callback", index

    
def m_sink_input_new_cb(obj, index):
    print "DEBUG sink_input new callback", index
    print obj.get_playback_streams()[index]
    
def m_sink_input_changed_cb(obj, index):
    print "DEBUG sink_input changed callback", index
    print obj.get_playback_streams()[index]
    
def m_sink_input_removed_cb(obj, index):
    print "DEBUG sink_input remove callback", index

    
def m_source_output_new_cb(obj, index):
    print "DEBUG source_output new callback", index
    print obj.get_record_streams()[index]
    
def m_source_output_changed_cb(obj, index):
    print "DEBUG source_output changed callback", index
    print obj.get_record_streams()[index]
    
def m_source_output_removed_cb(obj, index):
    print "DEBUG source_output remove callback", index
    

def test():
    deepin_pulseaudio_obj = deepin_pulseaudio.new()
    deepin_pulseaudio_obj.connect("sink-new", m_sink_new_cb)
    deepin_pulseaudio_obj.connect("sink-changed", m_sink_changed_cb)
    deepin_pulseaudio_obj.connect("sink-removed", m_sink_removed_cb)

    deepin_pulseaudio_obj.connect("source-new", m_source_new_cb)
    deepin_pulseaudio_obj.connect("source-changed", m_source_changed_cb)
    deepin_pulseaudio_obj.connect("source-removed", m_source_removed_cb)

    deepin_pulseaudio_obj.connect("card-new", m_card_new_cb)
    deepin_pulseaudio_obj.connect("card-changed", m_card_changed_cb)
    deepin_pulseaudio_obj.connect("card-removed", m_card_removed_cb)

    deepin_pulseaudio_obj.connect("sink-input-new", m_sink_input_new_cb)
    deepin_pulseaudio_obj.connect("sink-input-changed", m_sink_input_changed_cb)
    deepin_pulseaudio_obj.connect("sink-input-removed", m_sink_input_removed_cb)

    deepin_pulseaudio_obj.connect("source_output-new", m_source_output_new_cb)
    deepin_pulseaudio_obj.connect("source_output-changed", m_source_output_changed_cb)
    deepin_pulseaudio_obj.connect("source_output-removed", m_source_output_removed_cb)

    deepin_pulseaudio_obj.connect("server-new", m_server_new_cb)
    deepin_pulseaudio_obj.connect("server-changed", m_server_changed_cb)
    deepin_pulseaudio_obj.connect("server-removed", m_server_removed_cb)
    deepin_pulseaudio_obj.get_devices()
    #print "========get_output_ports testcase========"
    #output_ports = deepin_pulseaudio_obj.get_output_ports()
    #print output_ports
    #print "========get_input_ports testcase========"
    #input_ports = deepin_pulseaudio_obj.get_input_ports()
    #print input_ports
    #print "========get_output_devices testcase========"
    #output_devices = deepin_pulseaudio_obj.get_output_devices()
    #print output_devices
    #print "========get_input_devices testcase========"
    #input_devices = deepin_pulseaudio_obj.get_input_devices()
    #print input_devices
    #print "========get_output_channels testcase========"
    #print deepin_pulseaudio_obj.get_output_channels("")
    #print "========get_input_channels testcase========"
    #print deepin_pulseaudio_obj.get_input_channels("")
    #print "========get_output_active_ports testcase========"
    #print deepin_pulseaudio_obj.get_output_active_ports("")
    #print "========get_input_active_ports testcase========" 
    #print deepin_pulseaudio_obj.get_input_active_ports("")
    #print "========get_output_mute testcase========"
    #print deepin_pulseaudio_obj.get_output_mute("")
    #print "========get_input_mute testcase========"                            
    #print deepin_pulseaudio_obj.get_input_mute("")      
    #print "========get_output_volume testcase========"
    #print deepin_pulseaudio_obj.get_output_volume("")
    #print "========get_input_volume testcase========"
    #print deepin_pulseaudio_obj.get_input_volume("")
    #print "========set_output_active_port testcase========"
    #'''
    #@param output device index
           #you can print get_output_devices to see the indexes
    #'''
    #print deepin_pulseaudio_obj.set_output_active_port(1, output_ports[0][1])
    #print "========set_input_active_port testcase========"                     
    #print deepin_pulseaudio_obj.set_input_active_port(0, input_ports[0][1])   
    #print "========set_output_mute testcase========"                     
    #print deepin_pulseaudio_obj.set_output_mute(1, False)   
    #print "========set_input_mute testcase========"                      
    #'''
    #@param input device index
           #you can print get_input_devices to see the indexes
    #@param mute or not
    #'''
    #print deepin_pulseaudio_obj.set_input_mute(2, False) 
    #print "========set_output_volume testcase========"                     
    #print deepin_pulseaudio_obj.set_output_volume(1, (60000, 60000))   
    #print "========set_input_volume testcase========"                      
    #print deepin_pulseaudio_obj.set_input_volume(2, (80000, 80000)) 
    while True:
        time.sleep(100)

    deepin_pulseaudio_obj.delete()

'''
i = 0
while i < 1024:
    test()
    i += 1
'''

test()

