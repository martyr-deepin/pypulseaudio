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
import deepin_pulseaudio_signal
import gobject

deepin_pulseaudio_obj = deepin_pulseaudio.new()
deepin_pulseaudio_obj.get_devices()

deepin_pulseaudio_signal_obj = deepin_pulseaudio_signal.new()
deepin_pulseaudio_signal_obj.connect_to_pulse()

def m_sink_new_cb(index):
    print "DEBUG sink new callback", index

def m_sink_changed_cb(handle, index):
    print "DEBUG sink changed callback", handle, index

def m_sink_removed_cb(index):
    print "DEBUG sink remove callback", index
    
def m_source_new_cb(index):
    print "DEBUG source new callback", index
    
def m_source_changed_cb(index):
    print "DEBUG source changed callback", index
    
def m_source_removed_cb(index):
    print "DEBUG source remove callback", index
    
def m_card_new_cb(index):
    print "DEBUG card new callback", index
    
def m_card_changed_cb(index):
    print "DEBUG card changed callback", index
    
def m_card_removed_cb(index):
    print "DEBUG card remove callback", index
    
def m_server_new_cb():
    print "DEBUG server new callback"
    
def m_server_changed_cb():
    print "DEBUG server changed callback"
    
def m_server_removed_cb(index):
    print "DEBUG server remove callback", index
    
def m_sink_input_new_cb(index):
    print "DEBUG sink_input new callback", index
    
def m_sink_input_changed_cb(index):
    print "DEBUG sink_input changed callback", index
    
def m_sink_input_removed_cb(index):
    print "DEBUG sink_input remove callback", index
    
def m_source_output_new_cb(index):
    print "DEBUG source_output new callback", index
    
def m_source_output_changed_cb(index):
    print "DEBUG source_output changed callback", index

def m_source_output_removed_cb(index):
    print "DEBUG source_output remove callback", index

def test():
    deepin_pulseaudio_signal_obj.connect("sink-new", m_sink_new_cb)
    deepin_pulseaudio_signal_obj.connect("sink-changed", m_sink_changed_cb)
    deepin_pulseaudio_signal_obj.connect("sink-removed", m_sink_removed_cb)

    deepin_pulseaudio_signal_obj.connect("source-new", m_source_new_cb)
    deepin_pulseaudio_signal_obj.connect("source-changed", m_source_changed_cb)
    deepin_pulseaudio_signal_obj.connect("source-removed", m_source_removed_cb)

    deepin_pulseaudio_signal_obj.connect("card-new", m_card_new_cb)
    deepin_pulseaudio_signal_obj.connect("card-changed", m_card_changed_cb)
    deepin_pulseaudio_signal_obj.connect("card-removed", m_card_removed_cb)

    deepin_pulseaudio_signal_obj.connect("sink-input-new", m_sink_input_new_cb)
    deepin_pulseaudio_signal_obj.connect("sink-input-changed", m_sink_input_changed_cb)
    deepin_pulseaudio_signal_obj.connect("sink-input-removed", m_sink_input_removed_cb)

    deepin_pulseaudio_signal_obj.connect("source_output-new", m_source_output_new_cb)
    deepin_pulseaudio_signal_obj.connect("source_output-changed", m_source_output_changed_cb)
    deepin_pulseaudio_signal_obj.connect("source_output-removed", m_source_output_removed_cb)

    deepin_pulseaudio_signal_obj.connect("server-new", m_server_new_cb)
    deepin_pulseaudio_signal_obj.connect("server-changed", m_server_changed_cb)
    deepin_pulseaudio_signal_obj.connect("server-removed", m_server_removed_cb)

    print deepin_pulseaudio_obj.get_cards()

    mainloop = gobject.MainLoop()
    mainloop.run()

test()
