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
import pypulse
from deepin_utils.process import run_command

def sink_changed(handle, index):
    print "DEBUG sink_changed", handle, index

def value_changed(widget):
    print "DEBUG value_changed", widget.value
    # TODO: 直接调用命令行
    # 改变输出音量
    run_command("pacmd set-sink-volume %d %d" % (1, widget.value * 1000))

def destroy(*args):
    """ Callback function that is activated when the program is destoyed """
    pypulse.PULSE_SIGNAL.delete()
    gtk.main_quit()

window = gtk.Window(gtk.WINDOW_TOPLEVEL)
window.set_size_request(300, 200)
window.connect("destroy", destroy)
window.set_border_width(10)

pypulse.PULSE_SIGNAL.connect("sink-changed", sink_changed)

adjust = gtk.Adjustment(value=90, lower=0, upper=120, step_incr=1, page_incr=1)
adjust.connect("value-changed", value_changed)
hscale = gtk.HScale(adjust)
window.add(hscale)
hscale.show()

window.show_all()
gtk.main()
