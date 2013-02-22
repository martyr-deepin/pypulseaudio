/* 
 * Copyright (C) 2013 Deepin, Inc.
 *               2013 Zhai Xiang
 *
 * Author:     Zhai Xiang <zhaixiang@linuxdeepin.com>
 * Maintainer: Zhai Xiang <zhaixiang@linuxdeepin.com>
 *             Long Changjin <admin@longchangjin.cn>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <Python.h>
#include <pulse/pulseaudio.h>

#define PACKAGE "Deepin PulseAudio Python Binding"

#define INT(v) PyInt_FromLong(v)
#define STRING(v) PyString_FromString(v)
#define ERROR(v) PyErr_SetString(PyExc_TypeError, v)
#define RETURN_TRUE Py_INCREF(Py_True); return Py_True
#define RETURN_FALSE Py_INCREF(Py_False); return Py_False

/* Safe XDECREF for object states that handles nested deallocations */
#define ZAP(v) do {\
    PyObject *tmp = (PyObject *)(v); \
    (v) = NULL; \
    Py_XDECREF(tmp); \
} while (0)

typedef struct {
    PyObject_HEAD
    PyObject *dict; /* Python attributes dictionary */
    PyObject *server_info;  /* data */
    PyObject *card_devices;
    PyObject *input_devices;
    PyObject *output_devices;
    PyObject *input_ports;
    PyObject *output_ports;
    PyObject *input_channels;
    PyObject *output_channels;
    PyObject *input_active_ports;
    PyObject *output_active_ports;
    PyObject *input_mute;
    PyObject *output_mute;
    PyObject *input_volume;
    PyObject *output_volume;
    PyObject *playback_streams;
    PyObject *record_stream;
} DeepinPulseAudioObject;

static PyObject *m_deepin_pulseaudio_object_constants = NULL;
static PyTypeObject *m_DeepinPulseAudio_Type = NULL;

static DeepinPulseAudioObject *m_init_deepin_pulseaudio_object();
static DeepinPulseAudioObject *m_new(PyObject *self, PyObject *args);
static PyObject *m_pa_volume_get_balance(PyObject *self, PyObject *args);

static PyMethodDef deepin_pulseaudio_methods[] = 
{
    {"new", m_new, METH_NOARGS, "Deepin PulseAudio Construction"}, 
    {"volume_get_balance", m_pa_volume_get_balance, METH_VARARGS, "Get volume balance"},
    {NULL, NULL, 0, NULL}
};

static PyObject *m_delete(DeepinPulseAudioObject *self);
static void m_pa_server_info_cb(pa_context *c,
                                const pa_server_info *i,
                                void *userdate);
static void m_pa_cardlist_cb(pa_context *c,
                             const pa_card_info *i,
                             int eol,
                             void *userdata);
static void m_pa_sinklist_cb(pa_context *c, 
                             const pa_sink_info *l, 
                             int eol, 
                             void *userdata);
static void m_pa_sourcelist_cb(pa_context *c, 
                               const pa_source_info *l, 
                               int eol, 
                               void *userdata);
static void m_pa_sinkinputlist_info_cb(pa_context *c,
                                  const pa_sink_input_info *l,
                                  int eol,
                                  void *userdata);
static void m_pa_sourceoutputlist_info_cb(pa_context *c,
                                     const pa_source_output_info *l,
                                     int eol,
                                     void *userdata);

static PyObject *m_get_server_info(DeepinPulseAudioObject *self);
static PyObject *m_get_cards(DeepinPulseAudioObject *self);
static PyObject *m_get_devices(DeepinPulseAudioObject *self);
static PyObject *m_get_output_devices(DeepinPulseAudioObject *self);
static PyObject *m_get_input_devices(DeepinPulseAudioObject *self);
static PyObject *m_get_playback_streams(DeepinPulseAudioObject *self);
static PyObject *m_get_record_streams(DeepinPulseAudioObject *self);

static PyObject *m_get_output_ports(DeepinPulseAudioObject *self);
static PyObject *m_get_output_ports_by_index(DeepinPulseAudioObject *self, PyObject *args);
static PyObject *m_get_input_ports(DeepinPulseAudioObject *self);
static PyObject *m_get_input_ports_by_index(DeepinPulseAudioObject *self, PyObject *args);

static PyObject *m_get_output_channels(DeepinPulseAudioObject *self);
static PyObject *m_get_output_channels_by_index(DeepinPulseAudioObject *self, PyObject *args);
static PyObject *m_get_input_channels(DeepinPulseAudioObject *self);
static PyObject *m_get_input_channels_by_index(DeepinPulseAudioObject *self, PyObject *args);   

static PyObject *m_get_output_active_ports(DeepinPulseAudioObject *self);
static PyObject *m_get_output_active_ports_by_index(DeepinPulseAudioObject *self, PyObject *args);               
static PyObject *m_get_input_active_ports(DeepinPulseAudioObject *self);
static PyObject *m_get_input_active_ports_by_index(DeepinPulseAudioObject *self, PyObject *args);     

static PyObject *m_get_output_mute(DeepinPulseAudioObject *self);
static PyObject *m_get_output_mute_by_index(DeepinPulseAudioObject *self, PyObject *args);     
static PyObject *m_get_input_mute(DeepinPulseAudioObject *self);
static PyObject *m_get_input_mute_by_index(DeepinPulseAudioObject *self, PyObject *args);

static PyObject *m_get_fallback_sink(DeepinPulseAudioObject *self);
static PyObject *m_get_fallback_source(DeepinPulseAudioObject *self);

static PyObject *m_get_output_volume(DeepinPulseAudioObject *self);
static PyObject *m_get_output_volume_by_index(DeepinPulseAudioObject *self, PyObject *args);
static PyObject *m_get_input_volume(DeepinPulseAudioObject *self);
static PyObject *m_get_input_volume_by_index(DeepinPulseAudioObject *self, PyObject *args);

static PyObject *m_set_output_active_port(DeepinPulseAudioObject *self, PyObject *args);
static PyObject *m_set_input_active_port(DeepinPulseAudioObject *self, PyObject *args);

static PyObject *m_set_output_mute(DeepinPulseAudioObject *self, PyObject *args);
static PyObject *m_set_input_mute(DeepinPulseAudioObject *self, PyObject *args);

static PyObject *m_set_output_volume(DeepinPulseAudioObject *self, PyObject *args);
static PyObject *m_set_output_volume_with_balance(DeepinPulseAudioObject *self, PyObject *args);
static PyObject *m_set_input_volume(DeepinPulseAudioObject *self, PyObject *args);
static PyObject *m_set_input_volume_with_balance(DeepinPulseAudioObject *self, PyObject *args);

static PyObject *m_set_fallback_sink(DeepinPulseAudioObject *self, PyObject *args);
static PyObject *m_set_fallback_source(DeepinPulseAudioObject *self, PyObject *args);

static PyMethodDef deepin_pulseaudio_object_methods[] = 
{
    {"delete", m_delete, METH_NOARGS, "Deepin PulseAudio destruction"}, 
    {"get_server_info", m_get_server_info, METH_NOARGS, "Get server info"},
    {"get_cards", m_get_cards, METH_NOARGS, "Get card list"}, 
    {"get_devices", m_get_devices, METH_NOARGS, "Get device list"}, 

    {"get_output_devices", m_get_output_devices, METH_NOARGS, "Get output device list"},  
    {"get_input_devices", m_get_input_devices, METH_NOARGS, "Get input device list"},      
    {"get_playback_streams", m_get_playback_streams, METH_NOARGS, "Get playback stream list"},
    {"get_record_streams", m_get_record_streams, METH_NOARGS, "Get record stream list"},

    {"get_output_ports", m_get_output_ports, METH_NOARGS, "Get output port list"}, 
    {"get_output_ports_by_index", m_get_output_ports_by_index, METH_VARARGS, "Get output port list"}, 
    {"get_input_ports", m_get_input_ports, METH_NOARGS, "Get input port list"},    
    {"get_input_ports_by_index", m_get_input_ports_by_index, METH_VARARGS, "Get input port list"},    

    {"get_output_channels", m_get_output_channels, METH_VARARGS, "Get output channels"}, 
    {"get_output_channels_by_index", m_get_output_channels_by_index, METH_VARARGS, "Get output channels"}, 
    {"get_input_channels", m_get_input_channels, METH_VARARGS, "Get input channels"},   
    {"get_input_channels_by_index", m_get_input_channels_by_index, METH_VARARGS, "Get input channels"},   

    {"get_output_active_ports", m_get_output_active_ports, METH_VARARGS, "Get output active ports"},
    {"get_output_active_ports_by_index", m_get_output_active_ports_by_index, METH_VARARGS, "Get output active ports"},
    {"get_input_active_ports", m_get_input_active_ports, METH_VARARGS, "Get input active ports"},    
    {"get_input_active_ports_by_index", m_get_input_active_ports_by_index, METH_VARARGS, "Get input active ports"},    

    {"get_output_mute", m_get_output_mute, METH_VARARGS, "Get output mute"}, 
    {"get_output_mute_by_index", m_get_output_mute_by_index, METH_VARARGS, "Get output mute"}, 
    {"get_input_mute", m_get_input_mute, METH_VARARGS, "Get input mute"},
    {"get_input_mute_by_index", m_get_input_mute_by_index, METH_VARARGS, "Get input mute"},

    {"get_output_volume", m_get_output_volume, METH_VARARGS, "Get output volume"}, 
    {"get_output_volume_by_index", m_get_output_volume_by_index, METH_VARARGS, "Get output volume"}, 
    {"get_input_volume", m_get_input_volume, METH_VARARGS, "Get input volume"},  
    {"get_input_volume_by_index", m_get_input_volume_by_index, METH_VARARGS, "Get input volume"},  
    
    {"get_fallback_sink", m_get_fallback_sink, METH_NOARGS, "Get fallback sink"},
    {"get_fallback_source", m_get_fallback_source, METH_NOARGS, "Get fallback source"},

    {"set_output_active_port", m_set_output_active_port, METH_VARARGS, "Set output active port"}, 
    {"set_input_active_port", m_set_input_active_port, METH_VARARGS, "Set input active port"}, 

    {"set_output_mute", m_set_output_mute, METH_VARARGS, "Set output mute"}, 
    {"set_input_mute", m_set_input_mute, METH_VARARGS, "Set input mute"}, 

    {"set_output_volume", m_set_output_volume, METH_VARARGS, "Set output volume"}, 
    {"set_output_volume_with_balance", m_set_output_volume_with_balance, METH_VARARGS, "Set output volume"}, 
    {"set_input_volume", m_set_input_volume, METH_VARARGS, "Set input volume"}, 
    {"set_input_volume_with_balance", m_set_input_volume_with_balance, METH_VARARGS, "Set input volume"}, 
    
    {"set_fallback_sink", m_set_fallback_sink, METH_VARARGS, "Set fallback sink"},
    {"set_fallback_source", m_set_fallback_source, METH_VARARGS, "Set fallback source"},
    {NULL, NULL, 0, NULL}
};

static void m_deepin_pulseaudio_dealloc(DeepinPulseAudioObject *self) 
{
    PyObject_GC_UnTrack(self);
    Py_TRASHCAN_SAFE_BEGIN(self)

    ZAP(self->dict);
    m_delete(self);

    PyObject_GC_Del(self);
    Py_TRASHCAN_SAFE_END(self)
}

static PyObject *m_getattr(PyObject *co, 
                           char *name, 
                           PyObject *dict1, 
                           PyObject *dict2, 
                           PyMethodDef *m)
{
    PyObject *v = NULL;
    
    if (!v && dict1)
        v = PyDict_GetItemString(dict1, name);
    if (!v && dict2)
        v = PyDict_GetItemString(dict2, name);
    if (v) {
        Py_INCREF(v);
        return v;
    }
    
    return Py_FindMethod(m, co, name);
}

static int m_setattr(PyObject **dict, char *name, PyObject *v)
{
    if (!v) {
        int rv = -1;
        if (*dict)
            rv = PyDict_DelItemString(*dict, name);
        if (rv < 0) {
            PyErr_SetString(PyExc_AttributeError, 
                            "delete non-existing attribute");
            return rv;
        }
    }
    if (!*dict) {
        *dict = PyDict_New();
        if (!*dict)
            return -1;
    }
    return PyDict_SetItemString(*dict, name, v);
}

static PyObject *m_deepin_pulseaudio_getattr(DeepinPulseAudioObject *dpo, 
                                             char *name) 
{
    return m_getattr((PyObject *)dpo, 
                     name, 
                     dpo->dict, 
                     m_deepin_pulseaudio_object_constants, 
                     deepin_pulseaudio_object_methods);
}

static PyObject *m_deepin_pulseaudio_setattr(DeepinPulseAudioObject *dpo, 
                                             char *name, 
                                             PyObject *v) 
{
    return m_setattr(&dpo->dict, name, v);
}

static PyObject *m_deepin_pulseaudio_traverse(DeepinPulseAudioObject *self, 
                                              visitproc visit, 
                                              void *args) 
{
    int err;
#undef VISIT
#define VISIT(v) if ((v) != NULL && ((err = visit(v, args)) != 0)) return err

    VISIT(self->dict);

    return 0;
#undef VISIT
}

static PyObject *m_deepin_pulseaudio_clear(DeepinPulseAudioObject *self) 
{
    ZAP(self->dict);
    return 0;
}

static PyTypeObject DeepinPulseAudio_Type = {
    PyObject_HEAD_INIT(NULL)
    0, 
    "deepin_pulseaudio.new", 
    sizeof(DeepinPulseAudioObject), 
    0, 
    (destructor)m_deepin_pulseaudio_dealloc,
    0, 
    (getattrfunc)m_deepin_pulseaudio_getattr, 
    (setattrfunc)m_deepin_pulseaudio_setattr, 
    0, 
    0, 
    0,  
    0,  
    0,  
    0,  
    0,  
    0,  
    0,  
    0,  
    Py_TPFLAGS_HAVE_GC,
    0,  
    (traverseproc)m_deepin_pulseaudio_traverse, 
    (inquiry)m_deepin_pulseaudio_clear
};

PyMODINIT_FUNC initdeepin_pulseaudio() 
{
    PyObject *m = NULL;
             
    m_DeepinPulseAudio_Type = &DeepinPulseAudio_Type;
    DeepinPulseAudio_Type.ob_type = &PyType_Type;

    m = Py_InitModule("deepin_pulseaudio", deepin_pulseaudio_methods);
    if (!m)
        return;
    // pulseaudio module constants
    if (PyModule_AddObject(m, "CVOLUME_SNPRINT_MAX",
                           PyLong_FromLong(PA_CVOLUME_SNPRINT_MAX)) < 0) {
        return;
    }
    if (PyModule_AddObject(m, "DECIBEL_MININFTY",
                           PyFloat_FromDouble(PA_DECIBEL_MININFTY)) < 0) {
        return;
    }
    if (PyModule_AddObject(m, "SW_CVOLUME_SNPRINT_DB_MAX",
                           PyLong_FromLong(PA_SW_CVOLUME_SNPRINT_DB_MAX)) < 0) {
        return;
    }
    if (PyModule_AddObject(m, "SW_VOLUME_SNPRINT_DB_MAX",
                           PyLong_FromLong(PA_SW_VOLUME_SNPRINT_DB_MAX)) < 0) {
        return;
    }
    if (PyModule_AddObject(m, "VOLUME_INVALID",
                           PyLong_FromUnsignedLong(PA_VOLUME_INVALID)) < 0) {
        return;
    }
    if (PyModule_AddObject(m, "VOLUME_MAX",
                           PyLong_FromUnsignedLong(PA_VOLUME_MAX)) < 0) {
        return;
    }
    if (PyModule_AddObject(m, "VOLUME_MUTED",
                           PyLong_FromUnsignedLong(PA_VOLUME_MUTED)) < 0) {
        return;
    }
    if (PyModule_AddObject(m, "VOLUME_NORM",
                           PyLong_FromUnsignedLong(PA_VOLUME_NORM)) < 0) {
        return;
    }
    if (PyModule_AddObject(m, "VOLUME_SNPRINT_MAX",
                           PyLong_FromLong(PA_VOLUME_SNPRINT_MAX)) < 0) {
        return;
    }
    if (PyModule_AddObject(m, "VOLUME_UI_MAX",
                           PyLong_FromLong(PA_VOLUME_UI_MAX)) < 0) {
        return;
    }
    if (PyModule_AddObject(m, "API_VERSION",
                           PyLong_FromLong(PA_API_VERSION)) < 0) {
        return;
    }
    if (PyModule_AddObject(m, "MAJOR",
                           PyLong_FromLong(PA_MAJOR)) < 0) {
        return;
    }
    if (PyModule_AddObject(m, "MICRO",
                           PyLong_FromLong(PA_MICRO)) < 0) {
        return;
    }
    if (PyModule_AddObject(m, "MINOR",
                           PyLong_FromLong(PA_MINOR)) < 0) {
        return;
    }
    if (PyModule_AddObject(m, "PROTOCOL_VERSION",
                           PyLong_FromLong(PA_PROTOCOL_VERSION)) < 0) {
        return;
    }
    m_deepin_pulseaudio_object_constants = PyDict_New();
}

static DeepinPulseAudioObject *m_init_deepin_pulseaudio_object() 
{
    DeepinPulseAudioObject *self = NULL;

    self = (DeepinPulseAudioObject *) PyObject_GC_New(DeepinPulseAudioObject, 
                                                      m_DeepinPulseAudio_Type);
    if (!self)
        return NULL;
    
    PyObject_GC_Track(self);

    self->dict = NULL;
    
    self->server_info = NULL;
    self->card_devices = NULL;
    self->input_devices = NULL;
    self->output_devices = NULL;
    self->input_ports = NULL;
    self->output_ports = NULL;
    self->input_channels = NULL;
    self->output_channels = NULL;
    self->input_active_ports = NULL;
    self->output_active_ports = NULL;
    self->input_mute = NULL;
    self->output_mute = NULL;
    self->input_volume = NULL;
    self->output_volume = NULL;
    self->playback_streams = NULL;
    self->record_stream = NULL;

    return self;
}

static void m_pa_state_cb(pa_context *c, void *userdata) {                               
    pa_context_state_t state;                                               
    int *pa_ready = userdata;                                               
                                                                                
    state = pa_context_get_state(c);                                        
    switch (state) {                                                       
        // There are just here for reference                            
        case PA_CONTEXT_UNCONNECTED:                                    
        case PA_CONTEXT_CONNECTING:                                     
        case PA_CONTEXT_AUTHORIZING:                                    
        case PA_CONTEXT_SETTING_NAME:                                   
        default:                                                        
            break;                                                  
        case PA_CONTEXT_FAILED:                                         
        case PA_CONTEXT_TERMINATED:                                     
            *pa_ready = 2;                                          
            break;                                                  
        case PA_CONTEXT_READY:                                          
            *pa_ready = 1;                                          
            break;                                                  
    }
}

static DeepinPulseAudioObject *m_new(PyObject *dummy, PyObject *args) 
{
    DeepinPulseAudioObject *self = NULL;

    self = m_init_deepin_pulseaudio_object();
    if (!self)
        return NULL;

    self->server_info = PyDict_New();
    if (!self->server_info) {
        ERROR("PyDict_New error");
        m_delete(self);
        return NULL;
    }

    self->card_devices = PyDict_New();
    if (!self->card_devices) {
        ERROR("PyDict_New error");
        m_delete(self);
        return NULL;
    }

    self->input_devices = PyDict_New();
    if (!self->input_devices) {
        ERROR("PyDict_New error");
        m_delete(self);
        return NULL;
    }

    self->output_devices = PyDict_New();
    if (!self->output_devices) {
        ERROR("PyDict_New error");
        m_delete(self);
        return NULL;
    }

    self->input_ports = PyDict_New();
    if (!self->input_ports) {
        printf("PyDict_New error");
        return NULL;
    }

    self->output_ports = PyDict_New();
    if (!self->output_ports) {
        printf("PyDict_New error");
        return NULL;
    }

    self->input_channels = PyDict_New();
    if (!self->input_channels) {
        ERROR("PyDict_New error");
        m_delete(self);
        return NULL;
    }

    self->output_channels = PyDict_New();
    if (!self->output_channels) {
        ERROR("PyDict_New error");
        m_delete(self);
        return NULL;
    }

    self->input_active_ports = PyDict_New();
    if (!self->input_active_ports) {
        ERROR("PyDict_New error");
        m_delete(self);
        return NULL;
    }

    self->output_active_ports = PyDict_New();
    if (!self->output_active_ports) {
        ERROR("PyDict_New error");
        m_delete(self);
        return NULL;
    }

    self->input_mute = PyDict_New();
    if (!self->input_mute) {
        ERROR("PyDict_New error");
        m_delete(self);
        return NULL;
    }

    self->output_mute = PyDict_New();                                            
    if (!self->output_mute) {                                                    
        ERROR("PyDict_New error");                                              
        m_delete(self);                                                         
        return NULL;                                                            
    } 

    self->input_volume = PyDict_New();                                         
    if (!self->input_volume) {                                                 
        ERROR("PyDict_New error");                                              
        m_delete(self);                                                         
        return NULL;                                                            
    }

    self->output_volume = PyDict_New();
    if (!self->output_volume) {
        ERROR("PyDict_New error");
        m_delete(self);
        return NULL;
    }

    self->playback_streams = PyDict_New();
    if (!self->playback_streams) {
        ERROR("PyDict_New error");
        m_delete(self);
        return NULL;
    }

    self->record_stream = PyDict_New();
    if (!self->record_stream) {
        ERROR("PyDict_New error");
        m_delete(self);
        return NULL;
    }

    return self;
}

static PyObject *m_pa_volume_get_balance(PyObject *self, PyObject *args)
{
    pa_cvolume cvolume;
    pa_channel_map channel_map;
    PyObject *volume = NULL;
    PyObject *channel = NULL;
    PyObject *balance = NULL;
    int channel_num = 0;
    int i;
    Py_ssize_t size_volume, size_channel;

    if (!PyArg_ParseTuple(args, "iOO", &channel_num, &volume, &channel)) {
        ERROR("invalid arguments to connect");                                  
        return NULL;                                                            
    }
    if (PyList_Check(volume)) {
        volume = PyList_AsTuple(volume);
    }
    if (!PyTuple_Check(volume)) {
        Py_RETURN_NONE;
    }

    if (PyList_Check(channel)) {
        channel = PyList_AsTuple(channel);
    }
    if (!PyTuple_Check(channel)) {
        Py_RETURN_NONE;
    }

    memset(&cvolume, 0, sizeof(pa_cvolume));
    memset(&channel_map, 0, sizeof(pa_channel_map));
    channel_map.channels = channel_num;
    cvolume.channels = channel_num;

    size_volume = PyTuple_Size(volume);
    size_channel = PyTuple_Size(channel);
    if (channel_num < size_volume) {
        channel_num = size_volume;
    }
    if (channel_num < size_channel) {
        channel_num = size_channel;
    }
    for (i = 0; i < channel_num; i++) {
        cvolume.values[i] = PyInt_AsUnsignedLongMask(PyTuple_GetItem(volume, i));
        channel_map.map[i] = PyInt_AsUnsignedLongMask(PyTuple_GetItem(channel, i));
    }
    balance = PyFloat_FromDouble(pa_cvolume_get_balance(&cvolume, &channel_map));
    Py_INCREF(balance);
    return balance;
}

/* FIXME: fuzzy ... more object wait for destruction */
static PyObject *m_delete(DeepinPulseAudioObject *self) 
{
    if (self->output_devices) {
        Py_XDECREF(self->output_devices);
        self->output_devices = NULL;
    }

    if (self->input_devices) {
        Py_XDECREF(self->input_devices);
        self->input_devices = NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *m_get_server_info(DeepinPulseAudioObject *self)
{
    if (self->server_info) {
        Py_INCREF(self->server_info);
        return self->server_info;
    } else {
        Py_INCREF(Py_None);
        return Py_None;
    }
}

static PyObject *m_get_cards(DeepinPulseAudioObject *self) 
{
    if (self->card_devices) {
        Py_INCREF(self->card_devices);
        return self->card_devices;
    } else {
        Py_INCREF(Py_None);
        return Py_None;
    }
}

/* http://freedesktop.org/software/pulseaudio/doxygen/introspect.html#sinksrc_subsec */
static PyObject *m_get_devices(DeepinPulseAudioObject *self) 
{
    // Define our pulse audio loop and connection variables                     
    pa_mainloop *pa_ml;                                                         
    pa_mainloop_api *pa_mlapi;                                                  
    pa_context *pa_ctx;
    pa_operation *pa_op;                                                        
                                                                                
    // We'll need these state variables to keep track of our requests           
    int state = 0;                                                              
    int pa_ready = 0;                                                           

    PyDict_Clear(self->server_info);
    PyDict_Clear(self->card_devices);

    PyDict_Clear(self->input_devices);
    PyDict_Clear(self->output_devices);
    PyDict_Clear(self->input_ports);
    PyDict_Clear(self->output_ports);
    PyDict_Clear(self->input_channels);
    PyDict_Clear(self->output_channels);
    PyDict_Clear(self->input_active_ports);
    PyDict_Clear(self->output_active_ports);
    PyDict_Clear(self->input_mute);
    PyDict_Clear(self->output_mute);
    PyDict_Clear(self->input_volume);
    PyDict_Clear(self->output_volume);
    PyDict_Clear(self->playback_streams);
    PyDict_Clear(self->record_stream);

    // Create a mainloop API and connection to the default server               
    pa_ml = pa_mainloop_new();                                                  
    pa_mlapi = pa_mainloop_get_api(pa_ml);                                      
    pa_ctx = pa_context_new(pa_mlapi, PACKAGE);       
                                                                                
    // This function connects to the pulse server                               
    pa_context_connect(pa_ctx, NULL, 0, NULL);

    // This function defines a callback so the server will tell us it's state.  
    // Our callback will wait for the state to be ready.  The callback will     
    // modify the variable to 1 so we know when we have a connection and it's   
    // ready.                                                                   
    // If there's an error, the callback will set pa_ready to 2                 
    pa_context_set_state_callback(pa_ctx, m_pa_state_cb, &pa_ready);              
                                                                                
    // Now we'll enter into an infinite loop until we get the data we receive   
    // or if there's an error                                                   
    for (;;) {                                                                  
        // We can't do anything until PA is ready, so just iterate the mainloop 
        // and continue                                                         
        if (pa_ready == 0) {                                                    
            pa_mainloop_iterate(pa_ml, 1, NULL);                                
            continue;                                                           
        }                                                                       
        // We couldn't get a connection to the server, so exit out              
        if (pa_ready == 2) {                                                    
            pa_context_disconnect(pa_ctx);                                      
            pa_context_unref(pa_ctx);
            pa_mainloop_free(pa_ml);                                            
            Py_INCREF(Py_False);                                                    
            return Py_False;     
        }                                                                       
        // At this point, we're connected to the server and ready to make
        // requests                                                             
        switch (state) {                                                        
            // State 0: we haven't done anything yet                            
            case 0:                                                             
                // This sends an operation to the server.  pa_sinklist_info is  
                // our callback function and a pointer to our devicelist will   
                // be passed to the callback The operation ID is stored in the  
                // pa_op variable                                               
                pa_op = pa_context_get_sink_info_list(pa_ctx,                   
                        m_pa_sinklist_cb,                                         
                        self);                                                      
                                                                                
                // Update state for next iteration through the loop             
                state++;                                                        
                break;                                                          
            case 1:                                                             
                // Now we wait for our operation to complete.  When it's        
                // complete our pa_output_devices is filled out, and we move 
                // along to the next state                                      
                if (pa_operation_get_state(pa_op) == PA_OPERATION_DONE) {       
                    pa_operation_unref(pa_op);                                  
                                                                                
                    // Now we perform another operation to get the source       
                    // (input device) list just like before.  This time we pass 
                    // a pointer to our input structure                         
                    pa_op = pa_context_get_source_info_list(pa_ctx,             
                            m_pa_sourcelist_cb,                                   
                            self);                                                  
                    // Update the state so we know what to do next              
                    state++;                                                    
                }                                                               
                break;                                                          
            case 2:                                                             
                if (pa_operation_get_state(pa_op) == PA_OPERATION_DONE) {
                    pa_operation_unref(pa_op);
                    // Now wo perform another operation to get the card list.
                    pa_op = pa_context_get_card_info_list(pa_ctx,
                            m_pa_cardlist_cb, self);
                    state++;
                }
                break;
            case 3:
                if (pa_operation_get_state(pa_op) == PA_OPERATION_DONE) {
                    pa_operation_unref(pa_op);
                    // Now to get the server info
                    pa_op = pa_context_get_server_info(pa_ctx,
                            m_pa_server_info_cb, self);
                    state++;
                }
                break;
            case 4:
                if (pa_operation_get_state(pa_op) == PA_OPERATION_DONE) {
                    pa_operation_unref(pa_op);
                    // Now to get the server info
                    pa_op = pa_context_get_sink_input_info_list(pa_ctx,
                            m_pa_sinkinputlist_info_cb, self);
                    state++;
                }
                break;
            case 5:
                if (pa_operation_get_state(pa_op) == PA_OPERATION_DONE) {
                    pa_operation_unref(pa_op);
                    // Now to get the server info
                    pa_op = pa_context_get_source_output_info_list(pa_ctx,
                            m_pa_sourceoutputlist_info_cb, self);
                    state++;
                }
                break;
            case 6:
                if (pa_operation_get_state(pa_op) == PA_OPERATION_DONE) {       
                    // Now we're done, clean up and disconnect and return       
                    pa_operation_unref(pa_op);                                  
                    pa_context_disconnect(pa_ctx);                              
                    pa_context_unref(pa_ctx);                                   
                    pa_mainloop_free(pa_ml);
                    Py_INCREF(Py_True);                                                    
                    return Py_True;     
                }                                                               
                break;                                                          
            default:                                                            
                // We should never see this state                               
                Py_INCREF(Py_False);                                                    
                return Py_False;     
        }                               
        // Iterate the main loop and go again.  The second argument is whether  
        // or not the iteration should block until something is ready to be     
        // done.  Set it to zero for non-blocking.                              
        pa_mainloop_iterate(pa_ml, 1, NULL);                                    
    }

    Py_INCREF(Py_True);                                                    
    return Py_True;             
}

static PyObject *m_get_output_devices(DeepinPulseAudioObject *self) 
{
    if (self->output_devices) {
        Py_INCREF(self->output_devices);
        return self->output_devices;
    } else {
        Py_INCREF(Py_None);
        return Py_None;
    }
}

static PyObject *m_get_input_devices(DeepinPulseAudioObject *self)          
{                                                                               
    if (self->input_devices) {
        Py_INCREF(self->input_devices);
        return self->input_devices;                                                                
    } else {
        Py_INCREF(Py_None);
        return Py_None;
    }
}        

static PyObject *m_get_playback_streams(DeepinPulseAudioObject *self)
{
    if (self->playback_streams) {
        Py_INCREF(self->playback_streams);
        return self->playback_streams;
    } else {
        Py_INCREF(Py_None);
        return Py_None;
    }
}

static PyObject *m_get_record_streams(DeepinPulseAudioObject *self)
{
    if (self->record_stream) {
        Py_INCREF(self->record_stream);
        return self->record_stream;
    } else {
        Py_INCREF(Py_None);
        return Py_None;
    }
}

static PyObject *m_get_output_ports(DeepinPulseAudioObject *self) 
{
    Py_INCREF(self->output_ports);
    return self->output_ports;
}

static PyObject *m_get_output_ports_by_index(DeepinPulseAudioObject *self, PyObject *args)
{
    int device = -1;
    
    if (!PyArg_ParseTuple(args, "i", &device)) {
        ERROR("invalid arguments to get_output_ports_by_index");
        return NULL;
    }
    if (self->output_devices && PyDict_Contains(self->output_devices, INT(device))) {
        return PyDict_GetItemString(PyDict_GetItem(
                        self->output_devices, INT(device)), "ports");
    } else {
        Py_INCREF(Py_None);
        return Py_None;
    }
}

static PyObject *m_get_input_ports(DeepinPulseAudioObject *self)               
{
    Py_INCREF(self->input_ports);
    return self->input_ports;
}

static PyObject *m_get_input_ports_by_index(DeepinPulseAudioObject *self, PyObject *args)
{                                                                                  
    int device = -1;

    if (!PyArg_ParseTuple(args, "i", &device)) {
        ERROR("invalid arguments to get_output_channels");
        return NULL;
    }
    if (self->input_devices && PyDict_Contains(self->input_devices, INT(device))) {
        return PyDict_GetItemString(PyDict_GetItem(
                        self->input_devices, INT(device)), "ports");
    } else {
        Py_INCREF(Py_None);
        return Py_None;
    }
}    

static PyObject *m_get_output_channels(DeepinPulseAudioObject *self)
{
    Py_INCREF(self->output_channels);
    return self->output_channels;
}

static PyObject *m_get_output_channels_by_index(DeepinPulseAudioObject *self, PyObject *args) 
{
    int device = -1;
    
    if (!PyArg_ParseTuple(args, "i", &device)) {
        ERROR("invalid arguments to get_output_channels");
        return NULL;
    }
    
    if (self->output_channels && PyDict_Contains(self->output_channels, INT(device))) {
        return PyDict_GetItem(self->output_channels, INT(device));
    } else {
        Py_INCREF(Py_None);
        return Py_None;
    }
}

static PyObject *m_get_input_channels(DeepinPulseAudioObject *self)
{
    Py_INCREF(self->input_channels);
    return self->input_channels;
}

static PyObject *m_get_input_channels_by_index(DeepinPulseAudioObject *self, PyObject *args)
{                                                                               
    int device = -1;
    
    if (!PyArg_ParseTuple(args, "i", &device)) {
        ERROR("invalid arguments to get_input_channels");                      
        return NULL;                                                            
    } 
                                                                                
    if (self->input_channels && PyDict_Contains(self->input_channels, INT(device))) {
        return PyDict_GetItem(self->input_channels, INT(device));
    } else {
        Py_INCREF(Py_None);
        return Py_None;
    }
}             

static PyObject *m_get_output_active_ports(DeepinPulseAudioObject *self)
{
    Py_INCREF(self->output_active_ports);
    return self->output_active_ports;
}

static PyObject *m_get_output_active_ports_by_index(DeepinPulseAudioObject *self, PyObject *args)
{                                                                               
    int device = -1;
    
    if (!PyArg_ParseTuple(args, "i", &device)) {
        ERROR("invalid arguments to get_output_active_ports");                      
        return NULL; 
    }                                                                           
                                                                                
    if (self->output_active_ports && PyDict_Contains(self->output_active_ports, INT(device))) {
        return PyDict_GetItem(self->output_active_ports, INT(device));
    } else {
        Py_INCREF(Py_None);
        return Py_None;
    }
}           

static PyObject *m_get_input_active_ports(DeepinPulseAudioObject *self)
{
    Py_INCREF(self->input_active_ports);
    return self->input_active_ports;
}

static PyObject *m_get_input_active_ports_by_index(DeepinPulseAudioObject *self, PyObject *args)
{                                                                                  
    int device = -1;
    
    if (!PyArg_ParseTuple(args, "i", &device)) {
        ERROR("invalid arguments to get_input_active_ports");                          
        return NULL;                                                               
    }                                                                              
                                                                                   
    if (self->input_active_ports && PyDict_Contains(self->input_active_ports, INT(device))) {
        return PyDict_GetItem(self->input_active_ports, INT(device));
    } else {
        Py_INCREF(Py_None);
        return Py_None;
    }
}                

static PyObject *m_get_output_mute(DeepinPulseAudioObject *self)
{
    Py_INCREF(self->output_mute);
    return self->output_mute;
}

static PyObject *m_get_output_mute_by_index(DeepinPulseAudioObject *self, PyObject *args) 
{
    int device = -1;

    if (!PyArg_ParseTuple(args, "i", &device)) {                                   
        ERROR("invalid arguments to get_output_mute");                              
        return NULL;                                                               
    }                                                                              

    if (self->output_devices && PyDict_Contains(self->output_devices, INT(device))) {
        return PyDict_GetItemString(PyDict_GetItem(
                        self->output_devices, INT(device)), "mute");
    } else {
        Py_INCREF(Py_None);
        return Py_None;
    }
}

static PyObject *m_get_input_mute(DeepinPulseAudioObject *self)
{
    Py_INCREF(self->input_mute);
    return self->input_mute;
}

static PyObject *m_get_input_mute_by_index(DeepinPulseAudioObject *self, PyObject *args) 
{                                                                               
    int device = -1;
    
    if (!PyArg_ParseTuple(args, "i", &device)) {
        ERROR("invalid arguments to get_input_mute");                              
        return NULL;                                                            
    }                                                                           
                                                                                
    if (self->input_devices && PyDict_Contains(self->input_devices, INT(device))) {
        return PyDict_GetItemString(PyDict_GetItem(
                        self->input_devices, INT(device)),"mute");
    } else {
        Py_INCREF(Py_None);
        return Py_None;
    }
}                

static PyObject *m_get_output_volume(DeepinPulseAudioObject *self)
{
    Py_INCREF(self->output_volume);
    return self->output_volume;
}

static PyObject *m_get_output_volume_by_index(DeepinPulseAudioObject *self, PyObject *args) 
{                                                                               
    int device = -1;
    
    if (!PyArg_ParseTuple(args, "i", &device)) {
        ERROR("invalid arguments to get_output_volume");                              
        return NULL;                                                            
    }                                                                           
                                                                                
    if (self->output_volume && PyDict_Contains(self->output_volume, INT(device))) {
        return PyDict_GetItem(self->output_volume, INT(device));
    } else {
        Py_INCREF(Py_None);
        return Py_None;
    }
}           

static PyObject *m_get_input_volume(DeepinPulseAudioObject *self)
{
    Py_INCREF(self->input_volume);
    return self->input_volume;
}

static PyObject *m_get_input_volume_by_index(DeepinPulseAudioObject *self, PyObject *args)
{                                                                               
    int device = -1;
    
    if (!PyArg_ParseTuple(args, "i", &device)) {
        ERROR("invalid arguments to get_input_volume");                            
        return NULL;                                                            
    }                                                                           
                                                                                
    if (self->input_volume && PyDict_Contains(self->input_volume, INT(device))) {
        return PyDict_GetItem(self->input_volume, INT(device));             
    } else {
        Py_INCREF(self->input_volume);
        return self->input_volume;                                             
    }
}                                                                                        

static PyObject *m_get_fallback_sink(DeepinPulseAudioObject *self)
{
    if (self->server_info && PyDict_Contains(self->server_info, STRING("fallback_sink"))) {
        return PyDict_GetItemString(self->server_info, "fallback_sink");
    } else {
        Py_INCREF(Py_None);
        return Py_None;
    }
}

static PyObject *m_get_fallback_source(DeepinPulseAudioObject *self)
{
    if (self->server_info && PyDict_Contains(self->server_info, STRING("fallback_source"))) {
        return PyDict_GetItemString(self->server_info, "fallback_source");
    } else {
        Py_INCREF(Py_None);
        return Py_None;
    }
}

/*
static PyObject *m_set_output_active_port(DeepinPulseAudioObject *self, 
                                          PyObject *args) 
{
    int index = 0;
    char *port = NULL;
    pa_operation *pa_op = NULL;   

    if (!PyArg_ParseTuple(args, "ns", &index, &port)) {
        ERROR("invalid arguments to set_output_active_port");
        return NULL;
    }
                                                                                
    pa_op = pa_context_set_sink_port_by_index(self->pa_ctx, 
                                              index, 
                                              port, 
                                              NULL, 
                                              NULL);                                       
    pa_operation_unref(pa_op);                                  

    RETURN_TRUE;
}

static PyObject *m_set_input_active_port(DeepinPulseAudioObject *self,         
                                         PyObject *args)                       
{                                                                               
    int index = 0;                                                              
    char *port = NULL;                                                          
    pa_operation *pa_op = NULL;                                                 
                                                                                
    if (!PyArg_ParseTuple(args, "ns", &index, &port)) {                         
        ERROR("invalid arguments to set_input_active_port");                   
        return NULL;                                                            
    }                                                                           
                                                                                
    pa_op = pa_context_set_source_port_by_index(self->pa_ctx,               
                                                index,                
                                                port,                 
                                                NULL,                 
                                                NULL);                
    pa_operation_unref(pa_op);                                  

    RETURN_TRUE;
}

static PyObject *m_set_output_mute(DeepinPulseAudioObject *self, 
                                   PyObject *args) 
{
    int index = 0;
    PyObject *mute = NULL;
    pa_operation *pa_op = NULL;   

    if (!PyArg_ParseTuple(args, "nO", &index, &mute)) {
        ERROR("invalid arguments to set_output_mute");
        return NULL;
    }
    
    if (!PyBool_Check(mute)) {                                                 
        Py_INCREF(Py_False);                                                    
        return Py_False;                                                        
    } 
    
    pa_op = pa_context_set_sink_mute_by_index(self->pa_ctx, 
                                              index, 
                                              mute == Py_True ? 1 : 0, 
                                              NULL, 
                                              NULL);                                       
    pa_operation_unref(pa_op);                                  

    RETURN_TRUE;
}

static PyObject *m_set_input_mute(DeepinPulseAudioObject *self, 
                                  PyObject *args) 
{
    int index = 0;
    PyObject *mute = NULL;
    pa_operation *pa_op = NULL;   

    if (!PyArg_ParseTuple(args, "nO", &index, &mute)) {
        ERROR("invalid arguments to set_input_mute");
        return NULL;
    }
    
    if (!PyBool_Check(mute)) {                                                 
        Py_INCREF(Py_False);                                                    
        return Py_False;                                                        
    }

    pa_op = pa_context_set_source_mute_by_index(self->pa_ctx, 
                                                index, 
                                                mute == Py_True ? 1 : 0, 
                                                NULL, 
                                                NULL);                                       
    pa_operation_unref(pa_op);                                  

    RETURN_TRUE;
}

static PyObject *m_set_output_volume(DeepinPulseAudioObject *self, 
                                     PyObject *args) 
{
    int index = -1;
    PyObject *volume = NULL;
    pa_operation *pa_op = NULL;
    pa_cvolume output_volume;
    int channel_num = 1, i;
    Py_ssize_t tuple_size = 0;

    if (!PyArg_ParseTuple(args, "nO", &index, &volume)) {
        ERROR("invalid arguments to set_output_volume");
        return NULL;
    }

    if (PyList_Check(volume)) 
        volume = PyList_AsTuple(volume);
    
    if (!PyTuple_Check(volume)) 
        RETURN_FALSE;

    if (!PyDict_Contains(self->output_devices, INT(index))) {
        Py_INCREF(Py_False);
        return Py_False;
    }
    if (!PyDict_Contains(self->output_channels, INT(index))) {
        Py_INCREF(Py_False);
        return Py_False;
    }
    channel_num = PyInt_AsLong(PyDict_GetItemString(PyDict_GetItem(
                                    self->output_channels, INT(index)), "channels"));

    memset(&output_volume, 0, sizeof(pa_cvolume));
    
    tuple_size = PyTuple_Size(volume);
    output_volume.channels = channel_num;
    if (tuple_size > channel_num) 
        tuple_size = channel_num;
    
    for (i = 0; i < tuple_size; i++) 
        output_volume.values[i] = PyInt_AsLong(PyTuple_GetItem(volume, i));

    pa_op = pa_context_set_sink_volume_by_index(self->pa_ctx,
                                                index,
                                                &output_volume,
                                                NULL,
                                                NULL);
    pa_operation_unref(pa_op);

    RETURN_TRUE;
}

static PyObject *m_set_output_volume_with_balance(DeepinPulseAudioObject *self,
                                                  PyObject *args)
{
    int index = -1;
    long int volume;
    float balance;
    pa_mainloop *pa_ml = NULL;
    pa_mainloop_api *pa_mlapi = NULL;
    pa_context *pa_ctx = NULL;
    pa_operation *pa_op = NULL;
    pa_cvolume output_volume;
    pa_channel_map output_channel_map;
    int state = 0;
    int pa_ready = 0;
    int channel_num = 1, i;

    PyObject *channel_map_list = NULL;

    if (!PyArg_ParseTuple(args, "nlf", &index, &volume, &balance)) {
        ERROR("invalid arguments to set_output_volume");
        return NULL;
    }

    if (!PyDict_Contains(self->output_devices, INT(index))) {
        Py_INCREF(Py_False);
        return Py_False;
    }
    if (!PyDict_Contains(self->output_channels, INT(index))) {
        Py_INCREF(Py_False);
        return Py_False;
    }
    channel_num = PyInt_AsLong(PyDict_GetItemString(PyDict_GetItem(
                                    self->output_channels, INT(index)), "channels"));
    channel_map_list = PyDict_GetItemString(PyDict_GetItem(
                                    self->output_channels, INT(index)), "map");
    if (!channel_map_list) {
        Py_INCREF(Py_False);
        return Py_False;
    }

    pa_ml = pa_mainloop_new();
    pa_mlapi = pa_mainloop_get_api(pa_ml);
    pa_ctx = pa_context_new(pa_mlapi, PACKAGE);

    pa_context_connect(pa_ctx, NULL, 0, NULL);
    pa_context_set_state_callback(pa_ctx, m_pa_state_cb, &pa_ready);
    memset(&output_volume, 0, sizeof(pa_cvolume));
    memset(&output_channel_map, 0, sizeof(pa_channel_map));

    pa_cvolume_set(&output_volume, channel_num, volume);
    output_channel_map.channels = channel_num;
    for (i = 0; i < PyList_Size(channel_map_list); i++) {
        output_channel_map.map[i] = PyInt_AsLong(PyList_GetItem(channel_map_list, i));
    }
    // set balance
    pa_cvolume_set_balance(&output_volume, &output_channel_map, balance);

    for (;;) {
        if (pa_ready == 0) {
            pa_mainloop_iterate(pa_ml, 1, NULL);
            continue;
        }
        if (pa_ready == 2) {
            pa_context_disconnect(pa_ctx);
            pa_context_unref(pa_ctx);
            pa_mainloop_free(pa_ml);
            Py_INCREF(Py_False);
            return Py_False;
        }
        switch (state) {
            case 0:
                pa_op = pa_context_set_sink_volume_by_index(pa_ctx,
                                                            index,
                                                            &output_volume,
                                                            NULL,
                                                            NULL);
                state++;
                break;
            case 1:
                if (pa_operation_get_state(pa_op) == PA_OPERATION_DONE) {
                    pa_operation_unref(pa_op);
                    //pa_context_disconnect(pa_ctx);
                    //pa_context_unref(pa_ctx);
                    //pa_mainloop_free(pa_ml);
                    Py_INCREF(Py_True);
                    return Py_True;
                }
                break;
            default:
                Py_INCREF(Py_False);
                return Py_False;
        }
        pa_mainloop_iterate(pa_ml, 1, NULL);
    }

    Py_INCREF(Py_True);
    return Py_True;
}

static PyObject *m_set_input_volume(DeepinPulseAudioObject *self, 
                                    PyObject *args) 
{
    int index = 0;
    PyObject *volume = NULL;
    pa_operation *pa_op = NULL;
    pa_cvolume pa_input_volume;
    int channel_num = 1;
    int i;
    Py_ssize_t tuple_size = 0;

    if (!PyArg_ParseTuple(args, "nO", &index, &volume)) {
        ERROR("invalid arguments to set_input_volume");
        return NULL;
    }

    if (PyList_Check(volume)) {
        volume = PyList_AsTuple(volume);
    }
    if (!PyTuple_Check(volume)) {
        Py_INCREF(Py_False);
        return Py_False;
    }

    if (!PyDict_Contains(self->input_devices, INT(index))) {
        Py_INCREF(Py_False);
        return Py_False;
    }
    if (!PyDict_Contains(self->input_channels, INT(index))) {
        Py_INCREF(Py_False);
        return Py_False;
    }
    channel_num = PyInt_AsLong(PyDict_GetItemString(PyDict_GetItem(
                                    self->input_channels, INT(index)), "channels"));

    memset(&pa_input_volume, 0, sizeof(pa_cvolume));

    tuple_size = PyTuple_Size(volume);
    pa_input_volume.channels = channel_num;
    if (tuple_size > channel_num) 
        tuple_size = channel_num;

    for (i = 0; i < tuple_size; i++) 
        pa_input_volume.values[i] = PyInt_AsLong(PyTuple_GetItem(volume, i));

    pa_op = pa_context_set_source_volume_by_index(self->pa_ctx,
                                                  index,
                                                  &pa_input_volume,
                                                  NULL,
                                                  NULL);
    pa_operation_unref(pa_op);
    
    RETURN_TRUE;
}

static PyObject *m_set_input_volume_with_balance(DeepinPulseAudioObject *self,
                                                 PyObject *args)
{
    int index = -1;
    long int volume;
    float balance;
    pa_operation *pa_op = NULL;
    pa_cvolume input_volume;
    pa_channel_map input_channel_map;
    int channel_num = 1;
    int i;

    PyObject *channel_map_list = NULL;

    if (!PyArg_ParseTuple(args, "nlf", &index, &volume, &balance)) {
        ERROR("invalid arguments to set_output_volume");
        return NULL;
    }

    if (!PyDict_Contains(self->input_devices, INT(index))) {
        Py_INCREF(Py_False);
        return Py_False;
    }
    if (!PyDict_Contains(self->input_channels, INT(index))) {
        Py_INCREF(Py_False);
        return Py_False;
    }
    channel_num = PyInt_AsLong(PyDict_GetItemString(PyDict_GetItem(
                                    self->input_channels, INT(index)), "channels"));
    channel_map_list = PyDict_GetItemString(PyDict_GetItem(
                                    self->input_channels, INT(index)), "map");
    if (!channel_map_list) {
        Py_INCREF(Py_False);
        return Py_False;
    }

    memset(&input_volume, 0, sizeof(pa_cvolume));
    memset(&input_channel_map, 0, sizeof(pa_channel_map));

    pa_cvolume_set(&input_volume, channel_num, volume);
    input_channel_map.channels = channel_num;
    for (i = 0; i < PyList_Size(channel_map_list); i++) 
        input_channel_map.map[i] = PyInt_AsLong(PyList_GetItem(channel_map_list, i));

    // set balance
    pa_cvolume_set_balance(&input_volume, &input_channel_map, balance);

    pa_op = pa_context_set_source_volume_by_index(self->pa_ctx,
                                                  index,
                                                  &input_volume,
                                                  NULL,
                                                  NULL);
    pa_operation_unref(pa_op);

    RETURN_TRUE;
}

static PyObject *m_set_fallback_sink(DeepinPulseAudioObject *self,
                                     PyObject *args)
{
    pa_operation *pa_op = NULL;
    char *name = NULL;

    if (!PyArg_ParseTuple(args, "s", &name)) {
        ERROR("invalid arguments to set_fallback_sink");
        return NULL;
    }

    pa_op = pa_context_set_default_sink(self->pa_ctx, name, NULL, NULL);
    pa_operation_unref(pa_op);

    RETURN_TRUE;
}

static PyObject *m_set_fallback_source(DeepinPulseAudioObject *self,
                                       PyObject *args)
{
    pa_operation *pa_op = NULL;
    char *name = NULL;

    if (!PyArg_ParseTuple(args, "s", &name)) {
        ERROR("invalid arguments to set_fallback_source");
        return NULL;
    }

    pa_op = pa_context_set_default_source(self->pa_ctx, name, NULL, NULL);
    pa_operation_unref(pa_op);

    RETURN_TRUE;
}
*/

static void m_pa_server_info_cb(pa_context *c, 
                                const pa_server_info *i, 
                                void *userdata)
{
    if (!c || !i || !userdata) 
        return;
    
    DeepinPulseAudioObject *self = (DeepinPulseAudioObject *) userdata;
    
    PyDict_SetItemString(self->server_info,
                         "user_name",
                         STRING(i->user_name));
    PyDict_SetItemString(self->server_info,
                         "host_name",
                         STRING(i->host_name));
    PyDict_SetItemString(self->server_info,
                         "server_version",
                         STRING(i->server_version));
    PyDict_SetItemString(self->server_info,
                         "server_name",
                         STRING(i->server_name));

    PyDict_SetItemString(self->server_info,
                         "fallback_sink",
                         STRING(i->default_sink_name));
    PyDict_SetItemString(self->server_info,
                         "fallback_source",
                         STRING(i->default_source_name));
    PyDict_SetItemString(self->server_info,
                         "cookie",
                         INT(i->cookie));

}

static void m_pa_cardlist_cb(pa_context *c,
                             const pa_card_info *i,
                             int eol,
                             void *userdata)
{
    if (!c || !i || eol > 0 || !userdata) 
        return;

    DeepinPulseAudioObject *self = (DeepinPulseAudioObject *) userdata;

    PyObject *card_dict = NULL;         // a dict save the card info
    PyObject *profile_list = NULL;      // card_profile_info list
    PyObject *profile_dict = NULL;
    PyObject *port_list = NULL;         // card_port_info
    PyObject *port_dict = NULL;
    PyObject *active_profile = NULL;
    PyObject *prop_dict = NULL;
    PyObject *key = NULL;

    int ctr = 0;
    const char *prop_key;
    void *prop_state = NULL;

    card_dict = PyDict_New();
    if (!card_dict) {
        printf("PyDict_New error");
        return;
    }

    active_profile = PyDict_New();
    if (!active_profile) {
        printf("PyDict_New error");
        return;
    }

    prop_dict = PyDict_New();
    if (!prop_dict) {
        printf("PyDict_New error");
        return;
    }

    profile_list = PyList_New(0);
    if (!profile_list) {
        printf("PyList_New error");
        return;
    }
    port_list = PyList_New(0);
    if (!port_list) {
        printf("PyList_New error");
        return;
    }
    PyDict_SetItemString(card_dict, "name", STRING(i->name));
    PyDict_SetItemString(card_dict, "n_profiles", INT(i->n_profiles));

    // profile list
    for (ctr = 0; ctr < i->n_profiles; ctr++) {
        profile_dict = PyDict_New();
        if (!profile_dict) {
            printf("PyDict_New error");
            return;
        }
        PyDict_SetItemString(profile_dict, "name",
                             STRING(i->profiles[ctr].name));

        PyDict_SetItemString(profile_dict, "description",
                             STRING(i->profiles[ctr].description));

        PyDict_SetItemString(profile_dict, "n_sinks",
                             INT(i->profiles[ctr].n_sinks));

        PyDict_SetItemString(profile_dict, "n_sources",
                             INT(i->profiles[ctr].n_sources));
        PyList_Append(profile_list, profile_dict);
    }
    PyDict_SetItemString(card_dict, "profiles", profile_list);
    // active profile
    if (i->active_profile) {
        PyDict_SetItemString(active_profile, "name",
                             STRING(i->active_profile->name));
        PyDict_SetItemString(active_profile, "description",
                             STRING(i->active_profile->description));
        PyDict_SetItemString(active_profile, "n_sinks",
                             INT(i->active_profile->n_sinks));
        PyDict_SetItemString(active_profile, "n_sources",
                             INT(i->active_profile->n_sources));
        PyDict_SetItemString(card_dict, "active_profile", active_profile);
    } else {
        Py_INCREF(Py_None);
        PyDict_SetItemString(card_dict, "active_profile", Py_None);
    }
    // proplist
    while ((prop_key=pa_proplist_iterate(i->proplist, &prop_state))) {
        PyDict_SetItemString(prop_dict, prop_key,
                             STRING(pa_proplist_gets(i->proplist, prop_key)));
    }
    PyDict_SetItemString(card_dict, "proplist", prop_dict);
    PyDict_SetItemString(card_dict, "n_ports", INT(i->n_ports));
    // ports list
    for (ctr = 0; ctr < i->n_ports; ctr++) {
        port_dict = PyDict_New();
        if (!port_dict) {
            printf("PyDict_New error");
            return;
        }
        PyDict_SetItemString(port_dict, "name", 
                             STRING(i->ports[ctr]->name));
        PyDict_SetItemString(port_dict, "description", 
                             STRING(i->ports[ctr]->description));
        PyDict_SetItemString(port_dict, "available", 
                             INT(i->ports[ctr]->available));
        PyDict_SetItemString(port_dict, "direction", 
                             INT(i->ports[ctr]->direction));
        PyDict_SetItemString(port_dict, "n_profiles", 
                             INT(i->ports[ctr]->n_profiles));
        PyList_Append(port_list, port_dict);
    }
    PyDict_SetItemString(card_dict, "ports", port_list);

    key = INT(i->index);
    PyDict_SetItem(self->card_devices, key, card_dict);
}

static void m_pa_sinklist_cb(pa_context *c, 
                             const pa_sink_info *l, 
                             int eol, 
                             void *userdata) 
{
    if (!c || !l || eol > 0 || !userdata) 
        return;
   
    DeepinPulseAudioObject *self = userdata;

    pa_sink_port_info **ports  = NULL;                                          
    pa_sink_port_info *port = NULL;        
    pa_sink_port_info *active_port = NULL;
    PyObject *key = NULL;
    PyObject *channel_value = NULL;
    PyObject *active_port_value = NULL;
    PyObject *volume_value = NULL;
    PyObject *prop_dict = NULL;
    PyObject *port_list = NULL;
    int i = 0;
    const char *prop_key;
    void *prop_state = NULL;

    channel_value = PyList_New(0);
    if (!channel_value) {
        printf("PyList_New error");
        return;
    }
    volume_value = PyList_New(0);
    if (!volume_value) {
        printf("PyList_New error");
        return;
    }
    prop_dict = PyDict_New();
    if (!prop_dict) {
        printf("PyDict_New error");
        return;
    }
    port_list = PyList_New(0);
    if (!port_list) {
        printf("PyList_New error");
        return;
    }
                                                                                
    key = INT(l->index);
    while ((prop_key = pa_proplist_iterate(l->proplist, &prop_state))) {
        PyDict_SetItemString(prop_dict, prop_key,
                             STRING(pa_proplist_gets(l->proplist, prop_key)));
    }
    // channel list
    for (i = 0; i < l->channel_map.channels; i++) {
        PyList_Append(channel_value, INT(l->channel_map.map[i]));
    }
    PyDict_SetItem(self->output_channels, key,
                   Py_BuildValue("{sisnsO}", 
                                 "can_balance", pa_channel_map_can_balance(&l->channel_map),
                                 "channels", l->channel_map.channels,
                                 "map", channel_value));
    // ports list
    ports = l->ports;   
    for (i = 0; i < l->n_ports; i++) {                                  
        port = ports[i];   
        PyList_Append(port_list, Py_BuildValue("(ssi)",
                                               port->name,
                                               port->description,
                                               port->available)); 
    }                
    // active port
    active_port = l->active_port;
    if (active_port) {
        active_port_value = Py_BuildValue("(ssi)", 
                                          active_port->name,
                                          active_port->description, 
                                          active_port->available);
        PyDict_SetItem(self->output_active_ports, key, active_port_value);
    } else {
        PyDict_SetItem(self->output_active_ports, key, Py_None);
    }
    // volume list
    for (i = 0; i < l->volume.channels; i++) {
        PyList_Append(volume_value, INT(l->volume.values[i]));
    }
    PyDict_SetItem(self->output_volume, key, volume_value);
    PyDict_SetItem(self->output_devices, key,
                   Py_BuildValue("{sssssisIsOsOsO}",
                                 "name", l->name,
                                 "description", l->description,
                                 "base_volume", l->base_volume,
                                 "n_ports", l->n_ports,
                                 "mute", PyBool_FromLong(l->mute),
                                 "ports", port_list,
                                 "proplist", prop_dict));
}                   

// See above.  This callback is pretty much identical to the previous
static void m_pa_sourcelist_cb(pa_context *c, 
                               const pa_source_info *l, 
                               int eol, 
                               void *userdata) 
{
    if (!c || !l || eol > 0 || !userdata) 
        return;

    DeepinPulseAudioObject *self = (DeepinPulseAudioObject *) userdata;
    
    pa_source_port_info **ports = NULL;                                         
    pa_source_port_info *port = NULL;      
    pa_source_port_info *active_port = NULL;
    PyObject *key = NULL;
    PyObject *channel_value = NULL;
    PyObject *active_port_value = NULL;
    PyObject *volume_value = NULL;
    PyObject *prop_dict = NULL;
    PyObject *port_list = NULL;
    int i = 0;                                                                  
    const char *prop_key;
    void *prop_state = NULL;
                                                                                
    if (eol > 0 || !l) {                                                              
        return;                                                                 
    }

    channel_value = PyList_New(0);
    if (!channel_value) {
        printf("PyList_New error");
        return;
    }
    volume_value = PyList_New(0);
    if (!volume_value) {
        printf("PyList_New error");
        return;
    }
    prop_dict = PyDict_New();
    if (!prop_dict) {
        printf("PyDict_New error\n");
        return;
    }
        
    port_list = PyList_New(0);
    if (!port_list) {
        printf("PyList_New error");
        return;
    }
                                                                                
    key = INT(l->index);
    while ((prop_key=pa_proplist_iterate(l->proplist, &prop_state))) {
        PyDict_SetItemString(prop_dict, prop_key,
                             STRING(pa_proplist_gets(l->proplist, prop_key)));
    }
    // channel list
    for (i = 0; i < l->channel_map.channels; i++) {                    
        PyList_Append(channel_value, INT(l->channel_map.map[i]));         
    }                                                                   
    PyDict_SetItem(self->input_channels, key,
                   Py_BuildValue("{sisnsO}",
                                 "can_balance", pa_channel_map_can_balance(&l->channel_map),
                                 "channels", l->channel_map.channels,
                                 "map", channel_value));
    // ports list
    ports = l->ports;                                                   
    for (i = 0; i < l->n_ports; i++) {                                  
        port = ports[i];                                                
        PyList_Append(port_list, Py_BuildValue("(ssi)",
                                               port->name,
                                               port->description,
                                               port->available));
    } 
    // active port
    active_port = l->active_port;                                       
    if (active_port) {
        active_port_value = Py_BuildValue("(ssi)",
                                          active_port->name,
                                          active_port->description,         
                                          active_port->available);               
        PyDict_SetItem(self->input_active_ports, key, active_port_value);
    } else {
        PyDict_SetItem(self->input_active_ports, key, Py_None);
    }
    // volume list
    for (i = 0; i < l->volume.channels; i++) {
        PyList_Append(volume_value, INT(l->volume.values[i]));
    }
    PyDict_SetItem(self->input_volume, key, volume_value);
    PyDict_SetItem(self->input_devices, key,
                   Py_BuildValue("{sssssisIsOsOsO}",
                                 "name", l->name,
                                 "description", l->description,
                                 "base_volume", l->base_volume,
                                 "n_ports", l->n_ports,
                                 "mute", PyBool_FromLong(l->mute),
                                 "ports", port_list,
                                 "proplist", prop_dict));    
}                   

static void m_pa_sinkinputlist_info_cb(pa_context *c,
                                       const pa_sink_input_info *l,
                                       int eol,
                                       void *userdata)
{
    if (!c || !l || eol > 0 || !userdata) 
        return;

    DeepinPulseAudioObject *self = userdata;
    
    PyObject *key = NULL;
    PyObject *volume_value = NULL;
    PyObject *channel_value = NULL;
    PyObject *prop_dict = NULL;
    int i;
    const char *prop_key;
    void *prop_state = NULL;

    channel_value = PyList_New(0);
    if (!channel_value) {
        printf("PyList_New error");
        return;
    }
    volume_value = PyList_New(0);
    if (!volume_value) {
        printf("PyList_New error");
        return;
    }
    prop_dict = PyDict_New();
    if (!prop_dict) {
        printf("PyDict_New error\n");
        return;
    }

    key = INT(l->index);
    // proplist
    while ((prop_key=pa_proplist_iterate(l->proplist, &prop_state))) {
        PyDict_SetItemString(prop_dict, prop_key,
                             STRING(pa_proplist_gets(l->proplist, prop_key)));
    }
    // channel list
    for (i = 0; i < l->channel_map.channels; i++) {
        PyList_Append(channel_value, INT(l->channel_map.map[i]));
    }
    //volume list
    for (i = 0; i < l->volume.channels; i++) {
        PyList_Append(volume_value, INT(l->volume.values[i]));
    }
    PyDict_SetItem(self->playback_streams, key,
                   Py_BuildValue("{sssisisisOsssssOsisOsisisi}",
                                 "name", l->name,
                                 "owner_module", l->owner_module,
                                 "client", l->client,
                                 "sink", l->sink,
                                 "channel", channel_value,
                                 "resample_method", l->resample_method,
                                 "driver", l->driver,
                                 "proplist", prop_dict,
                                 "corked", l->corked,
                                 "volume", volume_value,
                                 "mute", l->mute,
                                 "has_volume", l->has_volume,
                                 "volume_writable", l->volume_writable));
}

static void m_pa_sourceoutputlist_info_cb(pa_context *c,
                                          const pa_source_output_info *l,
                                          int eol,
                                          void *userdata)
{
    if (!c || !l || eol > 0 || !userdata) 
        return;

    DeepinPulseAudioObject *self = userdata;
    
    PyObject *key = NULL;
    PyObject *volume_value = NULL;
    PyObject *channel_value = NULL;
    PyObject *prop_dict = NULL;
    int i;
    const char *prop_key;
    void *prop_state = NULL;

    channel_value = PyList_New(0);
    if (!channel_value) {
        printf("PyList_New error");
        return;
    }
    volume_value = PyList_New(0);
    if (!volume_value) {
        printf("PyList_New error");
        return;
    }
    prop_dict = PyDict_New();
    if (!prop_dict) {
        printf("PyDict_New error\n");
        return;
    }

    key = INT(l->index);
    // proplist
    while ((prop_key=pa_proplist_iterate(l->proplist, &prop_state))) {
        PyDict_SetItemString(prop_dict, prop_key,
                             STRING(pa_proplist_gets(l->proplist, prop_key)));
    }
    // channel list
    for (i = 0; i < l->channel_map.channels; i++) {
        PyList_Append(channel_value, INT(l->channel_map.map[i]));
    }
    //volume list
    for (i = 0; i < l->volume.channels; i++) {
        PyList_Append(volume_value, INT(l->volume.values[i]));
    }
    PyDict_SetItem(self->record_stream, key,
                   Py_BuildValue("{sssisisisOsssssOsisOsisisi}",
                                 "name", l->name,
                                 "owner_module", l->owner_module,
                                 "client", l->client,
                                 "source", l->source,
                                 "channel", channel_value,
                                 "resample_method", l->resample_method,
                                 "driver", l->driver,
                                 "proplist", prop_dict,
                                 "corked", l->corked,
                                 "volume", volume_value,
                                 "mute", l->mute,
                                 "has_volume", l->has_volume,
                                 "volume_writable", l->volume_writable));
}
