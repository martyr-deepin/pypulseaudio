#!/usr/bin/env python
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

import gtk
import pypulse_small as pypulse

adjust = gtk.Adjustment(value=60000, lower=0, upper=100000, step_incr=1, page_incr=1)

def value_changed(widget):
    print "DEBUG value_changed", widget.value
    index = pypulse.get_fallback_sink_index()
    if index is None:
        return
    value = widget.get_value()
    channel_num = pypulse.output_channels[index]['channels']
    balance = pypulse.get_volume_balance(channel_num, [int(value)], pypulse.output_channels[index]['map'])
    #pypulse.PULSE.set_output_volume(index, [value]*channel_num, channel_num)
    pypulse.PULSE.set_output_volume_with_balance(index, int(value), balance, channel_num, pypulse.output_channels[index]['map'])

def record_stream_read_cb(obj, value):
    print "record_stream_read_cb", value

def record_stream_suspended(obj):
    print "record_stream_suspended"

def sink_state_cb(obj, channel, port, volume, sink, idx):
    #if idx in pypulse.output_channels:
        #del pypulse.output_channels[idx]
    #if idx in pypulse.output_active_ports:
        #del pypulse.output_active_ports[idx]
    #if idx in pypulse.output_volumes:
        #del pypulse.output_volumes[idx]
    #if idx in pypulse.output_devices:
        #del pypulse.output_volumes[idx]
    pypulse.output_channels[idx] = channel
    pypulse.output_active_ports[idx] = port
    pypulse.output_volumes[idx] = volume
    pypulse.output_devices[idx] = sink
    if pypulse.get_fallback_sink_index() == idx:
        adjust.set_value(max(volume))
    print "sink state cb", idx

def source_state_cb(obj, channel, port, volume, sink, idx):
    #if idx in pypulse.output_channels:
        #del pypulse.output_channels[idx]
    #if idx in pypulse.output_active_ports:
        #del pypulse.output_active_ports[idx]
    #if idx in pypulse.output_volumes:
        #del pypulse.output_volumes[idx]
    #if idx in pypulse.output_devices:
        #del pypulse.output_volumes[idx]
    pypulse.input_channels[idx] = channel
    pypulse.input_active_ports[idx] = port
    pypulse.input_volumes[idx] = volume
    pypulse.input_devices[idx] = sink
    print "source state cb", idx

def server_state_cb(obj, dt):
    pypulse.server_info = dt
    index = pypulse.get_fallback_sink_index()
    if index in pypulse.output_volumes:
        adjust.set_value(max(pypulse.output_volumes[index]))
    obj.connect_record({"read": record_stream_read_cb, "suspended": record_stream_suspended})
    
def card_state_cb(obj, dt, idx):
    print 'card:', idx
    pypulse.card_devices[idx] = dt
    print "-"*20
    
def sinkinput_state_cb(obj, dt, idx):
    print "sinkinput", idx
    pypulse.playback_info[idx] = dt

def sourceoutput_state_cb(obj, dt, idx):
    print "sourceoutput", idx
    pypulse.record_info[idx] = dt

def destroy(*args):
    """ Callback function that is activated when the program is destoyed """
    gtk.main_quit()

def sinkinput_removed_cb(obj, idx):
    print "sinkinput removed", idx
    if idx in pypulse.playback_info:
        del pypulse.playback_info[idx]

def sourceoutput_removed_cb(obj, idx):
    print "sourceoutput removed", idx
    if idx in pypulse.record_info:
        del pypulse.record_info[idx]

window = gtk.Window(gtk.WINDOW_TOPLEVEL)
window.set_size_request(300, 200)
window.connect("destroy", destroy)
window.set_border_width(10)

state_cb_fun = {}
state_cb_fun["server"] = server_state_cb
state_cb_fun["card"] = card_state_cb
state_cb_fun["sink"] = sink_state_cb
state_cb_fun["source"] = source_state_cb
state_cb_fun["sinkinput"] = sinkinput_state_cb
state_cb_fun["sourceoutput"] = sourceoutput_state_cb
pypulse.PULSE.connect_to_pulse(state_cb_fun)
pypulse.PULSE.connect("sinkinput-removed", sinkinput_removed_cb)
pypulse.PULSE.connect("sourceoutput-removed", sourceoutput_removed_cb)

adjust.connect("value-changed", value_changed)

hscale = gtk.HScale(adjust)
window.add(hscale)
window.show_all()
gtk.main()
