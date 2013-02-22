#!/usr/bin/env python

import pygtk
import gtk
import pypulse

def sink_changed(index):
    print "DEBUG sink_changed", index

pypulse.PULSE.connect("sink-changed", sink_changed)

def value_changed(widget):
    current_sink = pypulse.get_fallback_sink_index()                        
    if current_sink is None:                                                
        print "DEBUG current_sink is None"
        return                                                              
    volume_list = pypulse.PULSE.get_output_volume_by_index(current_sink)       
    channel_list = pypulse.PULSE.get_output_channels_by_index(current_sink) 
    if not volume_list or not channel_list:                                 
        print "DEBUG volume_list or channel_list is None"
        return                                                              
    balance = pypulse.get_volume_balance(channel_list['channels'], volume_list, channel_list['map'])
    volume = int(widget.value / 100.0 * pypulse.NORMAL_VOLUME_VALUE)
    print "DEBUG speaker volumel set:", balance, volume                           
    pypulse.PULSE.set_output_volume_with_balance(current_sink, volume, balance)

def destroy(*args):
    """ Callback function that is activated when the program is destoyed """
    pypulse.PULSE.delete()
    gtk.main_quit()

window = gtk.Window(gtk.WINDOW_TOPLEVEL)
window.set_size_request(300, 200)
window.connect("destroy", destroy)
window.set_border_width(10)

adjust = gtk.Adjustment(value=90, lower=0, upper=120, step_incr=1, page_incr=1)
adjust.connect("value-changed", value_changed)
hscale = gtk.HScale(adjust)
window.add(hscale)
hscale.show()

window.show_all()
gtk.main()
