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
#include <pulse/glib-mainloop.h>

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
    pa_glib_mainloop *pa_ml;
    pa_context *pa_ctx;
    pa_mainloop_api *pa_mlapi;
    PyObject *state_cb; /* callback */                                       
    PyObject *event_cb;
    PyObject *sink_new_cb;
    PyObject *sink_changed_cb;                                                  
    PyObject *sink_removed_cb;                                                  
    PyObject *source_new_cb;                                                    
    PyObject *source_changed_cb;                                                
    PyObject *source_removed_cb;                                                
    PyObject *card_new_cb;                                                      
    PyObject *card_changed_cb;                                                  
    PyObject *card_removed_cb;                                                  
    PyObject *server_new_cb;                                                    
    PyObject *server_changed_cb;                                                
    PyObject *server_removed_cb;                                                
    PyObject *sink_input_new_cb;                                                
    PyObject *sink_input_changed_cb;                                            
    PyObject *sink_input_removed_cb;                                            
    PyObject *source_output_new_cb;                                             
    PyObject *source_output_changed_cb;                                         
    PyObject *source_output_removed_cb;
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
static void m_pa_context_subscribe_cb(pa_context *c,                            
                                      pa_subscription_event_type_t t,           
                                      uint32_t idx,                             
                                      void *userdata);                          
static void m_context_state_cb(pa_context *c, void *userdata);
static DeepinPulseAudioObject *m_new(PyObject *self, PyObject *args);
static PyObject *m_pa_volume_get_balance(PyObject *self, PyObject *args);

static PyMethodDef deepin_pulseaudio_small_methods[] = 
{
    {"new", (PyCFunction)m_new, METH_NOARGS, "Deepin PulseAudio Construction"}, 
    {"volume_get_balance", m_pa_volume_get_balance, METH_VARARGS, "Get volume balance"},
    {NULL, NULL, 0, NULL}
};

static PyObject *m_delete(DeepinPulseAudioObject *self);
static void m_pa_server_info_cb(pa_context *c,
                                const pa_server_info *i,
                                void *userdata);
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
static PyObject *m_get_output_devices(DeepinPulseAudioObject *self);
static PyObject *m_get_input_devices(DeepinPulseAudioObject *self);
static PyObject *m_get_playback_streams(DeepinPulseAudioObject *self);
static PyObject *m_get_record_streams(DeepinPulseAudioObject *self);

static PyObject *m_set_output_active_port(DeepinPulseAudioObject *self, PyObject *args);
static PyObject *m_set_input_active_port(DeepinPulseAudioObject *self, PyObject *args);

static PyObject *m_set_output_mute(DeepinPulseAudioObject *self, PyObject *args);
static PyObject *m_set_input_mute(DeepinPulseAudioObject *self, PyObject *args);

static PyObject *m_set_output_volume(DeepinPulseAudioObject *self, PyObject *args);
static PyObject *m_set_output_volume_with_balance(DeepinPulseAudioObject *self, PyObject *args);
static PyObject *m_set_input_volume(DeepinPulseAudioObject *self, PyObject *args);
static PyObject *m_set_input_volume_with_balance(DeepinPulseAudioObject *self, PyObject *args);

static PyObject *m_set_sink_input_mute(DeepinPulseAudioObject *self, PyObject *args);
static PyObject *m_set_sink_input_volume(DeepinPulseAudioObject *self, PyObject *args);

static PyObject *m_set_fallback_sink(DeepinPulseAudioObject *self, PyObject *args);
static PyObject *m_set_fallback_source(DeepinPulseAudioObject *self, PyObject *args);

static PyObject *m_connect_to_pulse(DeepinPulseAudioObject *self, PyObject *args);
static PyObject *m_connect(DeepinPulseAudioObject *self, PyObject *args);

static PyMethodDef deepin_pulseaudio_object_methods[] = 
{
    {"delete", (PyCFunction)m_delete, METH_NOARGS, "Deepin PulseAudio destruction"}, 
    {"connect_to_pulse", (PyCFunction)m_connect_to_pulse, METH_VARARGS, "Connect to PulseAudio"},
    {"connect", (PyCFunction)m_connect, METH_VARARGS, "Connect signal callback"},

    {"get_server_info", (PyCFunction)m_get_server_info, METH_NOARGS, "Get server info"},
    {"get_cards", (PyCFunction)m_get_cards, METH_NOARGS, "Get card list"}, 
    {"get_output_devices", (PyCFunction)m_get_output_devices, METH_NOARGS, "Get output device list"},  
    {"get_input_devices", (PyCFunction)m_get_input_devices, METH_NOARGS, "Get input device list"},      
    {"get_playback_streams", (PyCFunction)m_get_playback_streams, METH_NOARGS, "Get playback stream list"},
    {"get_record_streams", (PyCFunction)m_get_record_streams, METH_NOARGS, "Get record stream list"},

    {"set_output_active_port", (PyCFunction)m_set_output_active_port, METH_VARARGS, "Set output active port"}, 
    {"set_input_active_port", (PyCFunction)m_set_input_active_port, METH_VARARGS, "Set input active port"}, 

    {"set_output_mute", (PyCFunction)m_set_output_mute, METH_VARARGS, "Set output mute"}, 
    {"set_input_mute", (PyCFunction)m_set_input_mute, METH_VARARGS, "Set input mute"}, 

    {"set_output_volume", (PyCFunction)m_set_output_volume, METH_VARARGS, "Set output volume"}, 
    {"set_output_volume_with_balance", (PyCFunction)m_set_output_volume_with_balance, METH_VARARGS, "Set output volume"}, 
    {"set_input_volume", (PyCFunction)m_set_input_volume, METH_VARARGS, "Set input volume"}, 
    {"set_input_volume_with_balance", (PyCFunction)m_set_input_volume_with_balance, METH_VARARGS, "Set input volume"}, 

    {"set_sink_input_mute", (PyCFunction)m_set_sink_input_mute, METH_VARARGS, "Set sink_input mute"},
    {"set_sink_input_volume", (PyCFunction)m_set_sink_input_volume, METH_VARARGS, "Set sink_input volume"},
    
    {"set_fallback_sink", (PyCFunction)m_set_fallback_sink, METH_VARARGS, "Set fallback sink"},
    {"set_fallback_source", (PyCFunction)m_set_fallback_source, METH_VARARGS, "Set fallback source"},
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

PyMODINIT_FUNC initdeepin_pulseaudio_small() 
{
    PyObject *m = NULL;
             
    m_DeepinPulseAudio_Type = &DeepinPulseAudio_Type;
    DeepinPulseAudio_Type.ob_type = &PyType_Type;

    m = Py_InitModule("deepin_pulseaudio_small", deepin_pulseaudio_small_methods);
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

// TODO 删除一些对象
static DeepinPulseAudioObject *m_init_deepin_pulseaudio_object() 
{
    DeepinPulseAudioObject *self = NULL;

    self = (DeepinPulseAudioObject *) PyObject_GC_New(DeepinPulseAudioObject, 
                                                      m_DeepinPulseAudio_Type);
    if (!self)
        return NULL;
    
    PyObject_GC_Track(self);

    self->dict = NULL;
    
    self->state_cb = NULL;
    self->event_cb = NULL;
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

    self->pa_ml = NULL;                                                         
    self->pa_ctx = NULL;                                                        
    self->pa_mlapi = NULL;                                                      
                                                                                
    self->sink_new_cb = NULL;                                                   
    self->sink_changed_cb = NULL;                                               
    self->sink_removed_cb = NULL;                                               
    self->source_new_cb = NULL;                                                 
    self->source_changed_cb = NULL;                                             
    self->source_removed_cb = NULL;                                             
    self->card_new_cb = NULL;                                                   
    self->card_changed_cb = NULL;                                               
    self->card_removed_cb = NULL;                                               
    self->server_new_cb = NULL;                                                 
    self->server_changed_cb = NULL;                                             
    self->server_removed_cb = NULL;                                             
    self->sink_input_new_cb = NULL;                                             
    self->sink_input_changed_cb = NULL;                                         
    self->sink_input_removed_cb = NULL;                                         
    self->source_output_new_cb = NULL;                                          
    self->source_output_changed_cb = NULL;                                      
    self->source_output_removed_cb = NULL;

    return self;
}

static void m_pa_state_cb(pa_context *c, void *userdata) 
{
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

    self->event_cb = PyDict_New();
    if (!self->event_cb) {
        ERROR("PyDict_New error");
        m_delete(self);
        return NULL;
    }

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
    
    self->pa_ml = pa_glib_mainloop_new(g_main_context_default());
    if (!self->pa_ml) {
        ERROR("pa_glib_mainloop_new() failed");
        m_delete(self);
        return NULL;
    }

    self->pa_mlapi = pa_glib_mainloop_get_api(self->pa_ml);
    if (!self->pa_mlapi) {
        ERROR("pa_glib_mainloop_get_api() failed");
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
// TODO 删除一些对象
static PyObject *m_delete(DeepinPulseAudioObject *self) 
{
    if (self->server_info) {
        Py_XDECREF(self->server_info);
        self->server_info = NULL;
    }

    if (self->card_devices) {
        Py_XDECREF(self->card_devices);
        self->card_devices = NULL;
    }

    if (self->input_devices) {
        Py_XDECREF(self->input_devices);
        self->input_devices = NULL;
    }

    if (self->output_devices) {
        Py_XDECREF(self->output_devices);
        self->output_devices = NULL;
    }

    if (self->input_ports) {
        Py_XDECREF(self->input_ports);
        self->input_ports = NULL;
    }

    if (self->output_ports) {
        Py_XDECREF(self->output_ports);
        self->output_ports = NULL;
    }

    if (self->input_channels) {
        Py_XDECREF(self->input_channels);
        self->input_channels = NULL;
    }

    if (self->output_channels) {
        Py_XDECREF(self->output_channels);
        self->output_channels = NULL;
    }

    if (self->input_active_ports) {
        Py_XDECREF(self->input_active_ports);
        self->input_active_ports = NULL;
    }

    if (self->output_active_ports) {
        Py_XDECREF(self->output_active_ports);
        self->output_active_ports = NULL;
    }

    if (self->input_mute) {
        Py_XDECREF(self->input_mute);
        self->input_mute = NULL;
    }

    if (self->output_mute) {
        Py_XDECREF(self->output_mute);
        self->output_mute = NULL;
    }

    if (self->input_volume) {
        Py_XDECREF(self->input_volume);
        self->input_volume = NULL;
    }

    if (self->output_volume) {
        Py_XDECREF(self->output_volume);
        self->output_volume = NULL;
    }

    if (self->playback_streams) {
        Py_XDECREF(self->playback_streams);
        self->playback_streams = NULL;
    }

    if (self->record_stream) {
        Py_XDECREF(self->record_stream);
        self->record_stream = NULL;
    }

    if (self->pa_ctx) {                                                         
        pa_context_disconnect(self->pa_ctx);                                    
        pa_context_unref(self->pa_ctx);                                         
        self->pa_ctx = NULL;                                                    
    }                                                                           
                                                                                
    if (self->pa_ml) {                                                          
        pa_glib_mainloop_free(self->pa_ml);                                     
        self->pa_ml = NULL;                                                     
    }

    Py_INCREF(Py_None);
    return Py_None;
}

//**********************************
// pa get function
static PyObject *m_get_server_info(DeepinPulseAudioObject *self)
{
    if (!self->pa_ctx || pa_context_get_state(self->pa_ctx) != PA_CONTEXT_READY) {
        RETURN_FALSE;
    }
    pa_operation *pa_op = NULL;
    if (!(pa_op = pa_context_get_server_info(self->pa_ctx, m_pa_server_info_cb, self))) {
        printf("pa_context_get_server_info() failed");
        RETURN_FALSE;
    }
    pa_operation_unref(pa_op);
    RETURN_TRUE;
}

static PyObject *m_get_cards(DeepinPulseAudioObject *self) 
{
    if (!self->pa_ctx || pa_context_get_state(self->pa_ctx) != PA_CONTEXT_READY) {
        RETURN_FALSE;
    }
    pa_operation *pa_op = NULL;
    if (!(pa_op = pa_context_get_card_info_list(self->pa_ctx, m_pa_cardlist_cb, self))) {
        printf("pa_context_get_card_info_list() failed");
        RETURN_FALSE;
    }
    pa_operation_unref(pa_op);
    RETURN_TRUE;
}

/* http://freedesktop.org/software/pulseaudio/doxygen/introspect.html#sinksrc_subsec */

static PyObject *m_get_output_devices(DeepinPulseAudioObject *self) 
{
    if (!self->pa_ctx || pa_context_get_state(self->pa_ctx) != PA_CONTEXT_READY) {
        RETURN_FALSE;
    }
    pa_operation *pa_op = NULL;
    if (!(pa_op = pa_context_get_sink_info_list(self->pa_ctx, m_pa_sinklist_cb, self))) {
        printf("pa_context_get_sink_info_list() failed");
        RETURN_FALSE;
    }
    pa_operation_unref(pa_op);
    RETURN_TRUE;
}

static PyObject *m_get_input_devices(DeepinPulseAudioObject *self)          
{                                                                               
    if (!self->pa_ctx || pa_context_get_state(self->pa_ctx) != PA_CONTEXT_READY) {
        RETURN_FALSE;
    }
    pa_operation *pa_op = NULL;
    if (!(pa_op = pa_context_get_source_info_list(self->pa_ctx, m_pa_sourcelist_cb, self))) {
        printf("pa_context_get_source_info_list() failed");
        RETURN_FALSE;
    }
    pa_operation_unref(pa_op);
    RETURN_TRUE;
}

static PyObject *m_get_playback_streams(DeepinPulseAudioObject *self)
{
    if (!self->pa_ctx || pa_context_get_state(self->pa_ctx) != PA_CONTEXT_READY) {
        RETURN_FALSE;
    }
    pa_operation *pa_op = NULL;
    if (!(pa_op = pa_context_get_sink_input_info_list(self->pa_ctx, m_pa_sinkinputlist_info_cb, self))) {
        printf("pa_context_get_sink_input_info_list() failed");
        RETURN_FALSE;
    }
    pa_operation_unref(pa_op);
    RETURN_TRUE;
}

static PyObject *m_get_record_streams(DeepinPulseAudioObject *self)
{
    if (!self->pa_ctx || pa_context_get_state(self->pa_ctx) != PA_CONTEXT_READY) {
        RETURN_FALSE;
    }
    pa_operation *pa_op = NULL;
    if (!(pa_op = pa_context_get_source_output_info_list(self->pa_ctx, m_pa_sourceoutputlist_info_cb, self))) {
        printf("pa_context_get_source_output_info_list() failed");
        RETURN_FALSE;
    }
    pa_operation_unref(pa_op);
    RETURN_TRUE;
}

//******************************************
// pa set function
static PyObject *m_set_output_active_port(DeepinPulseAudioObject *self, 
                                          PyObject *args) 
{
    int index = 0;
    char *port = NULL;
    pa_mainloop *pa_ml = NULL;                                                         
    pa_mainloop_api *pa_mlapi = NULL;                                                  
    pa_context *pa_ctx = NULL;                                                         
    pa_operation *pa_op = NULL;   
    int state = 0;
    int pa_ready = 0;

    if (!PyArg_ParseTuple(args, "ns", &index, &port)) {
        ERROR("invalid arguments to set_output_active_port");
        return NULL;
    }
                                                                                
    pa_ml = pa_mainloop_new();                                                  
    pa_mlapi = pa_mainloop_get_api(pa_ml);                                      
    pa_ctx = pa_context_new(pa_mlapi, PACKAGE);                                
                                                                                
    pa_context_connect(pa_ctx, NULL, 0, NULL);
    pa_context_set_state_callback(pa_ctx, m_pa_state_cb, &pa_ready);            

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
                pa_op = pa_context_set_sink_port_by_index(pa_ctx, 
                                                          index, 
                                                          port, 
                                                          NULL, 
                                                          NULL);                                       
                state++;                                                        
                break;                                                          
            case 1:                                                             
                if (pa_operation_get_state(pa_op) == PA_OPERATION_DONE) {                    
                    pa_operation_unref(pa_op);                                  
                    pa_context_disconnect(pa_ctx);                              
                    pa_context_unref(pa_ctx);                                   
                    pa_mainloop_free(pa_ml);                                    
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

    Py_INCREF(Py_False);
    return Py_False;
}

static PyObject *m_set_input_active_port(DeepinPulseAudioObject *self,         
                                         PyObject *args)                       
{                                                                               
    int index = 0;                                                              
    char *port = NULL;                                                          
    pa_mainloop *pa_ml = NULL;                                                      
    pa_mainloop_api *pa_mlapi = NULL;                                               
    pa_context *pa_ctx = NULL;                                                      
    pa_operation *pa_op = NULL;                                                 
    int state = 0;                                                              
    int pa_ready = 0;                                                           
                                                                                
    if (!PyArg_ParseTuple(args, "ns", &index, &port)) {                         
        ERROR("invalid arguments to set_input_active_port");                   
        return NULL;                                                            
    }                                                                           
                                                                                
    pa_ml = pa_mainloop_new();                                                  
    pa_mlapi = pa_mainloop_get_api(pa_ml);                                      
    pa_ctx = pa_context_new(pa_mlapi, PACKAGE);                                
                                                                                
    pa_context_connect(pa_ctx, NULL, 0, NULL);                                  
    pa_context_set_state_callback(pa_ctx, m_pa_state_cb, &pa_ready);            
                                                                                
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
                pa_op = pa_context_set_source_port_by_index(pa_ctx,               
                                                            index,                
                                                            port,                 
                                                            NULL,                 
                                                            NULL);                
                state++;                                                        
                break;                                                          
            case 1:                                                             
                if (pa_operation_get_state(pa_op) == PA_OPERATION_DONE) {       
                    pa_operation_unref(pa_op);                                  
                    pa_context_disconnect(pa_ctx);                              
                    pa_context_unref(pa_ctx);                                   
                    pa_mainloop_free(pa_ml);                                    
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
                                                                                
    Py_INCREF(Py_False);                                                        
    return Py_False;                                                            
}

static PyObject *m_set_output_mute(DeepinPulseAudioObject *self, 
                                   PyObject *args) 
{
    int index = 0;
    PyObject *mute = NULL;
    pa_mainloop *pa_ml = NULL;                                                         
    pa_mainloop_api *pa_mlapi = NULL;                                                  
    pa_context *pa_ctx = NULL;                                                         
    pa_operation *pa_op = NULL;   
    int state = 0;
    int pa_ready = 0;

    if (!PyArg_ParseTuple(args, "nO", &index, &mute)) {
        ERROR("invalid arguments to set_output_mute");
        return NULL;
    }
    
    if (!PyBool_Check(mute)) {                                                 
        Py_INCREF(Py_False);                                                    
        return Py_False;                                                        
    } 
                                                                            
    pa_ml = pa_mainloop_new();                                                  
    pa_mlapi = pa_mainloop_get_api(pa_ml);                                      
    pa_ctx = pa_context_new(pa_mlapi, PACKAGE);                                
                                                                                
    pa_context_connect(pa_ctx, NULL, 0, NULL);
    pa_context_set_state_callback(pa_ctx, m_pa_state_cb, &pa_ready);            

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
                pa_op = pa_context_set_sink_mute_by_index(pa_ctx, 
                    index, mute == Py_True ? 1 : 0, NULL, NULL);                                       
                state++;                                                        
                break;                                                          
            case 1:                                                             
                if (pa_operation_get_state(pa_op) == PA_OPERATION_DONE) {                    
                    pa_operation_unref(pa_op);                                  
                    pa_context_disconnect(pa_ctx);                              
                    pa_context_unref(pa_ctx);                                   
                    pa_mainloop_free(pa_ml);                                    
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

    Py_INCREF(Py_False);
    return Py_False;
}

static PyObject *m_set_input_mute(DeepinPulseAudioObject *self, 
                                  PyObject *args) 
{
    int index = 0;
    PyObject *mute = NULL;
    pa_mainloop *pa_ml = NULL;                                                         
    pa_mainloop_api *pa_mlapi = NULL;                                                  
    pa_context *pa_ctx = NULL;                                                         
    pa_operation *pa_op = NULL;   
    int state = 0;
    int pa_ready = 0;

    if (!PyArg_ParseTuple(args, "nO", &index, &mute)) {
        ERROR("invalid arguments to set_input_mute");
        return NULL;
    }
    
    if (!PyBool_Check(mute)) {                                                 
        Py_INCREF(Py_False);                                                    
        return Py_False;                                                        
    }

    pa_ml = pa_mainloop_new();                                                  
    pa_mlapi = pa_mainloop_get_api(pa_ml);                                      
    pa_ctx = pa_context_new(pa_mlapi, PACKAGE);                                
                                                                                
    pa_context_connect(pa_ctx, NULL, 0, NULL);
    pa_context_set_state_callback(pa_ctx, m_pa_state_cb, &pa_ready);            

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
                pa_op = pa_context_set_source_mute_by_index(pa_ctx, 
                    index, mute == Py_True ? 1 : 0, NULL, NULL);                                       
                state++;                                                        
                break;                                                          
            case 1:                                                             
                if (pa_operation_get_state(pa_op) == PA_OPERATION_DONE) {                    
                    pa_operation_unref(pa_op);                                  
                    pa_context_disconnect(pa_ctx);                              
                    pa_context_unref(pa_ctx);                                   
                    pa_mainloop_free(pa_ml);                                    
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

    Py_INCREF(Py_False);
    return Py_False;
}

static PyObject *m_set_output_volume(DeepinPulseAudioObject *self, 
                                     PyObject *args) 
{
    int index = -1;
    PyObject *volume = NULL;
    pa_mainloop *pa_ml = NULL;
    pa_mainloop_api *pa_mlapi = NULL;
    pa_context *pa_ctx = NULL;
    pa_operation *pa_op = NULL;
    pa_cvolume output_volume;
    int state = 0;
    int pa_ready = 0;
    int channel_num = 1, i;
    Py_ssize_t tuple_size = 0;

    if (!PyArg_ParseTuple(args, "nO", &index, &volume)) {
        ERROR("invalid arguments to set_output_volume");
        return NULL;
    }

    if (PyList_Check(volume)) {
        volume = PyList_AsTuple(volume);
    }
    if (!PyTuple_Check(volume)){
        Py_INCREF(Py_False);
        return Py_False;
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

    pa_ml = pa_mainloop_new();
    pa_mlapi = pa_mainloop_get_api(pa_ml);
    pa_ctx = pa_context_new(pa_mlapi, PACKAGE);

    pa_context_connect(pa_ctx, NULL, 0, NULL);
    pa_context_set_state_callback(pa_ctx, m_pa_state_cb, &pa_ready);
    memset(&output_volume, 0, sizeof(pa_cvolume));
    
    tuple_size = PyTuple_Size(volume);
    output_volume.channels = channel_num;
    if (tuple_size > channel_num) {
        tuple_size = channel_num;
    }
    for (i = 0; i < tuple_size; i++) {
            output_volume.values[i] = PyInt_AsLong(PyTuple_GetItem(volume, i));
    }

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
                    pa_context_disconnect(pa_ctx);
                    pa_context_unref(pa_ctx);
                    pa_mainloop_free(pa_ml);
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
                    pa_context_disconnect(pa_ctx);
                    pa_context_unref(pa_ctx);
                    pa_mainloop_free(pa_ml);
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
    pa_mainloop *pa_ml = NULL;
    pa_mainloop_api *pa_mlapi = NULL;
    pa_context *pa_ctx = NULL;
    pa_operation *pa_op = NULL;
    pa_cvolume pa_input_volume;
    int state = 0;
    int pa_ready = 0;
    int channel_num = 1, i;
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

    pa_ml = pa_mainloop_new();
    pa_mlapi = pa_mainloop_get_api(pa_ml);
    pa_ctx = pa_context_new(pa_mlapi, PACKAGE);

    pa_context_connect(pa_ctx, NULL, 0, NULL);
    pa_context_set_state_callback(pa_ctx, m_pa_state_cb, &pa_ready);
    memset(&pa_input_volume, 0, sizeof(pa_cvolume));

    tuple_size = PyTuple_Size(volume);
    pa_input_volume.channels = channel_num;
    if (tuple_size > channel_num) {
        tuple_size = channel_num;
    }
    for (i = 0; i < tuple_size; i++) {
        pa_input_volume.values[i] = PyInt_AsLong(PyTuple_GetItem(volume, i));
    }

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
                pa_op = pa_context_set_source_volume_by_index(pa_ctx,
                                                              index,
                                                              &pa_input_volume,
                                                              NULL,
                                                              NULL);
                state++;
                break;
            case 1:
                if (pa_operation_get_state(pa_op) == PA_OPERATION_DONE) {
                    pa_operation_unref(pa_op);
                    pa_context_disconnect(pa_ctx);
                    pa_context_unref(pa_ctx);
                    pa_mainloop_free(pa_ml);
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

static PyObject *m_set_input_volume_with_balance(DeepinPulseAudioObject *self,
                                                 PyObject *args)
{
    int index = -1;
    long int volume;
    float balance;
    pa_mainloop *pa_ml = NULL;
    pa_mainloop_api *pa_mlapi = NULL;
    pa_context *pa_ctx = NULL;
    pa_operation *pa_op = NULL;
    pa_cvolume input_volume;
    pa_channel_map input_channel_map;
    int state = 0;
    int pa_ready = 0;
    int channel_num = 1, i;

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

    pa_ml = pa_mainloop_new();
    pa_mlapi = pa_mainloop_get_api(pa_ml);
    pa_ctx = pa_context_new(pa_mlapi, PACKAGE);

    pa_context_connect(pa_ctx, NULL, 0, NULL);
    pa_context_set_state_callback(pa_ctx, m_pa_state_cb, &pa_ready);
    memset(&input_volume, 0, sizeof(pa_cvolume));
    memset(&input_channel_map, 0, sizeof(pa_channel_map));

    pa_cvolume_set(&input_volume, channel_num, volume);
    input_channel_map.channels = channel_num;
    for (i = 0; i < PyList_Size(channel_map_list); i++) {
        input_channel_map.map[i] = PyInt_AsLong(PyList_GetItem(channel_map_list, i));
    }
    // set balance
    pa_cvolume_set_balance(&input_volume, &input_channel_map, balance);

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
                pa_op = pa_context_set_source_volume_by_index(pa_ctx,
                                                              index,
                                                              &input_volume,
                                                              NULL,
                                                              NULL);
                state++;
                break;
            case 1:
                if (pa_operation_get_state(pa_op) == PA_OPERATION_DONE) {
                    pa_operation_unref(pa_op);
                    pa_context_disconnect(pa_ctx);
                    pa_context_unref(pa_ctx);
                    pa_mainloop_free(pa_ml);
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

static PyObject *m_set_sink_input_mute(DeepinPulseAudioObject *self,
                                       PyObject *args)
{
    pa_mainloop *pa_ml = NULL;
    pa_mainloop_api *pa_mlapi = NULL;
    pa_context *pa_ctx = NULL;
    pa_operation *pa_op = NULL;
    int index = 0;
    PyObject *mute = NULL;
    int pa_ready = 0;
    int state = 0;

    if (!PyArg_ParseTuple(args, "nO", &index, &mute)) {
        ERROR("invalid arguments to set_sink_input_mute");
        return NULL;
    }

    if (!PyBool_Check(mute)) {                                                 
        Py_INCREF(Py_False);                                                    
        return Py_False;                                                        
    }

    pa_ml = pa_mainloop_new();
    pa_mlapi = pa_mainloop_get_api(pa_ml);
    pa_ctx = pa_context_new(pa_mlapi, PACKAGE);

    pa_context_connect(pa_ctx, NULL, 0, NULL);
    pa_context_set_state_callback(pa_ctx, m_pa_state_cb, &pa_ready);

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
                pa_op = pa_context_set_sink_input_mute(pa_ctx, 
                    index, mute == Py_True ? 1 : 0, NULL, NULL);                                       
                state++;
                break;
            case 1:
                if (pa_operation_get_state(pa_op) == PA_OPERATION_DONE) {
                    pa_operation_unref(pa_op);
                    pa_context_disconnect(pa_ctx);
                    pa_context_unref(pa_ctx);
                    pa_mainloop_free(pa_ml);
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

static PyObject *m_set_sink_input_volume(DeepinPulseAudioObject *self,
                                         PyObject *args)
{
    int index = 0;
    PyObject *volume = NULL;
    pa_mainloop *pa_ml = NULL;
    pa_mainloop_api *pa_mlapi = NULL;
    pa_context *pa_ctx = NULL;
    pa_operation *pa_op = NULL;
    pa_cvolume pa_sink_input_volume;
    int state = 0;
    int pa_ready = 0;
    int channel_num = 1, i;
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

    if (!PyDict_Contains(self->playback_streams, INT(index))) {
        Py_INCREF(Py_False);
        return Py_False;
    }
    channel_num = PyList_Size(PyDict_GetItemString(PyDict_GetItem(
                                self->playback_streams, INT(index)), "channel"));

    pa_ml = pa_mainloop_new();
    pa_mlapi = pa_mainloop_get_api(pa_ml);
    pa_ctx = pa_context_new(pa_mlapi, PACKAGE);

    pa_context_connect(pa_ctx, NULL, 0, NULL);
    pa_context_set_state_callback(pa_ctx, m_pa_state_cb, &pa_ready);
    memset(&pa_sink_input_volume, 0, sizeof(pa_cvolume));

    tuple_size = PyTuple_Size(volume);
    pa_sink_input_volume.channels = channel_num;
    if (tuple_size > channel_num) {
        tuple_size = channel_num;
    }
    for (i = 0; i < tuple_size; i++) {
        pa_sink_input_volume.values[i] = PyInt_AsLong(PyTuple_GetItem(volume, i));
    }

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
                pa_op = pa_context_set_sink_input_volume(pa_ctx,
                                                         index,
                                                         &pa_sink_input_volume,
                                                         NULL,
                                                         NULL);
                state++;
                break;
            case 1:
                if (pa_operation_get_state(pa_op) == PA_OPERATION_DONE) {
                    pa_operation_unref(pa_op);
                    pa_context_disconnect(pa_ctx);
                    pa_context_unref(pa_ctx);
                    pa_mainloop_free(pa_ml);
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

static PyObject *m_set_fallback_sink(DeepinPulseAudioObject *self,
                                     PyObject *args)
{
    pa_mainloop *pa_ml = NULL;
    pa_mainloop_api *pa_mlapi = NULL;
    pa_context *pa_ctx = NULL;
    pa_operation *pa_op = NULL;
    char *name = NULL;
    int pa_ready = 0;
    int state = 0;

    if (!PyArg_ParseTuple(args, "s", &name)) {
        ERROR("invalid arguments to set_fallback_sink");
        return NULL;
    }

    pa_ml = pa_mainloop_new();
    pa_mlapi = pa_mainloop_get_api(pa_ml);
    pa_ctx = pa_context_new(pa_mlapi, PACKAGE);

    pa_context_connect(pa_ctx, NULL, 0, NULL);
    pa_context_set_state_callback(pa_ctx, m_pa_state_cb, &pa_ready);

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
                pa_op = pa_context_set_default_sink(pa_ctx, name, NULL, NULL);
                state++;
                break;
            case 1:
                if (pa_operation_get_state(pa_op) == PA_OPERATION_DONE) {
                    pa_operation_unref(pa_op);
                    pa_context_disconnect(pa_ctx);
                    pa_context_unref(pa_ctx);
                    pa_mainloop_free(pa_ml);
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

static PyObject *m_set_fallback_source(DeepinPulseAudioObject *self,
                                       PyObject *args)
{
    pa_mainloop *pa_ml = NULL;
    pa_mainloop_api *pa_mlapi = NULL;
    pa_context *pa_ctx = NULL;
    pa_operation *pa_op = NULL;
    char *name = NULL;
    int pa_ready = 0;
    int state = 0;

    if (!PyArg_ParseTuple(args, "s", &name)) {
        ERROR("invalid arguments to set_fallback_source");
        return NULL;
    }

    pa_ml = pa_mainloop_new();
    pa_mlapi = pa_mainloop_get_api(pa_ml);
    pa_ctx = pa_context_new(pa_mlapi, PACKAGE);

    pa_context_connect(pa_ctx, NULL, 0, NULL);
    pa_context_set_state_callback(pa_ctx, m_pa_state_cb, &pa_ready);

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
                pa_op = pa_context_set_default_source(pa_ctx, name, NULL, NULL);
                state++;
                break;
            case 1:
                if (pa_operation_get_state(pa_op) == PA_OPERATION_DONE) {
                    pa_operation_unref(pa_op);
                    pa_context_disconnect(pa_ctx);
                    pa_context_unref(pa_ctx);
                    pa_mainloop_free(pa_ml);
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

//*****************************************
// pulseaudio get info callback
static void m_pa_server_info_cb(pa_context *c, 
                                const pa_server_info *i, 
                                void *userdata)
{
    if (!c || !i || !userdata) 
        return;
    
    printf("pa_server_info_cb\n");
    DeepinPulseAudioObject *self = (DeepinPulseAudioObject *) userdata;
    PyObject *tmp_obj = NULL;
    PyObject *server_dict = NULL;

    server_dict = PyDict_New();
    if (!server_dict) {
        printf("PyDict_New error");
        return;
    }
    
    tmp_obj = STRING(i->user_name);
    PyDict_SetItemString(server_dict, "user_name", tmp_obj);
    Py_DecRef(tmp_obj);

    tmp_obj = STRING(i->host_name);
    PyDict_SetItemString(server_dict, "host_name", tmp_obj);
    Py_DecRef(tmp_obj);

    tmp_obj = STRING(i->server_version);
    PyDict_SetItemString(server_dict, "server_version", tmp_obj);
    Py_DecRef(tmp_obj);

    tmp_obj = STRING(i->server_name);
    PyDict_SetItemString(server_dict, "server_name", tmp_obj);
    Py_DecRef(tmp_obj);

    tmp_obj = STRING(i->default_sink_name);
    PyDict_SetItemString(server_dict, "fallback_sink", tmp_obj);
    Py_DecRef(tmp_obj);

    tmp_obj = STRING(i->default_source_name);
    PyDict_SetItemString(server_dict, "fallback_source", tmp_obj);
    Py_DecRef(tmp_obj);

    tmp_obj = INT(i->cookie);
    PyDict_SetItemString(server_dict, "cookie", tmp_obj);
    Py_DecRef(tmp_obj);

    PyObject *func = NULL;
    if (self->state_cb && PyDict_Check(self->state_cb) && ((func=PyDict_GetItemString(self->state_cb, "server")) != NULL)) {
        if (PyCallable_Check(func)) {
            PyGILState_STATE gstate;
            gstate = PyGILState_Ensure();
            PyEval_CallFunction(func, "(OO)", self, server_dict);
            PyGILState_Release(gstate);
        }
    }
}

static void m_pa_cardlist_cb(pa_context *c,
                             const pa_card_info *i,
                             int eol,
                             void *userdata)
{
    if (!userdata || eol || !c || !i)
        return;

    DeepinPulseAudioObject *self = (DeepinPulseAudioObject *) userdata;

    PyObject *card_dict = NULL;         // a dict save the card info
    PyObject *profile_list = NULL;      // card_profile_info list
    PyObject *profile_dict = NULL;
    PyObject *port_list = NULL;         // card_port_info
    PyObject *port_dict = NULL;
    PyObject *active_profile = NULL;
    PyObject *prop_dict = NULL;

    int ctr = 0;
    const char *prop_key;
    void *prop_state = NULL;
    PyObject *tmp_obj = NULL;

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

    tmp_obj = STRING(i->name);
    PyDict_SetItemString(card_dict, "name", tmp_obj);
    Py_DecRef(tmp_obj);

    tmp_obj = INT(i->n_profiles);
    PyDict_SetItemString(card_dict, "n_profiles", tmp_obj);
    Py_DecRef(tmp_obj);

    // profile list
    for (ctr = 0; ctr < i->n_profiles; ctr++) {
        profile_dict = PyDict_New();
        if (!profile_dict) {
            printf("PyDict_New error");
            return;
        }
        tmp_obj = STRING(i->profiles[ctr].name);
        PyDict_SetItemString(profile_dict, "name", tmp_obj);
        Py_DecRef(tmp_obj);

        tmp_obj = STRING(i->profiles[ctr].description);
        PyDict_SetItemString(profile_dict, "description", tmp_obj);
        Py_DecRef(tmp_obj);

        tmp_obj = INT(i->profiles[ctr].n_sinks);
        PyDict_SetItemString(profile_dict, "n_sinks", tmp_obj);
        Py_DecRef(tmp_obj);

        tmp_obj = INT(i->profiles[ctr].n_sources);
        PyDict_SetItemString(profile_dict, "n_sources", tmp_obj);
        Py_DecRef(tmp_obj);

        PyList_Append(profile_list, profile_dict);
        Py_DecRef(profile_dict);
    }
    PyDict_SetItemString(card_dict, "profiles", profile_list);
    Py_DecRef(profile_list);

    // active profile
    if (i->active_profile) {
        tmp_obj = STRING(i->active_profile->name);
        PyDict_SetItemString(active_profile, "name", tmp_obj);
        Py_DecRef(tmp_obj);

        tmp_obj = STRING(i->active_profile->description);
        PyDict_SetItemString(active_profile, "description", tmp_obj);
        Py_DecRef(tmp_obj);

        tmp_obj = INT(i->active_profile->n_sinks);
        PyDict_SetItemString(active_profile, "n_sinks", tmp_obj);
        Py_DecRef(tmp_obj);

        tmp_obj = INT(i->active_profile->n_sources);
        PyDict_SetItemString(active_profile, "n_sources", tmp_obj);
        Py_DecRef(tmp_obj);

        PyDict_SetItemString(card_dict, "active_profile", active_profile);
        Py_DecRef(active_profile);
    } else {
        PyDict_SetItemString(card_dict, "active_profile", Py_None);
        Py_DecRef(active_profile);
    }
    // proplist
    while ((prop_key=pa_proplist_iterate(i->proplist, &prop_state))) {
        tmp_obj = STRING(pa_proplist_gets(i->proplist, prop_key));
        PyDict_SetItemString(prop_dict, prop_key, tmp_obj);
        Py_DecRef(tmp_obj);
    }
    PyDict_SetItemString(card_dict, "proplist", prop_dict);
    Py_DecRef(prop_dict);

    tmp_obj = INT(i->n_ports);
    PyDict_SetItemString(card_dict, "n_ports", tmp_obj);
    Py_DecRef(tmp_obj);

    // ports list
    for (ctr = 0; ctr < i->n_ports; ctr++) {
        port_dict = PyDict_New();
        if (!port_dict) {
            printf("PyDict_New error");
            return;
        }
        tmp_obj = STRING(i->ports[ctr]->name);
        PyDict_SetItemString(port_dict, "name", tmp_obj);
        Py_DecRef(tmp_obj);

        tmp_obj = STRING(i->ports[ctr]->description);
        PyDict_SetItemString(port_dict, "description", tmp_obj);
        Py_DecRef(tmp_obj);

        tmp_obj = INT(i->ports[ctr]->available);
        PyDict_SetItemString(port_dict, "available", tmp_obj);
        Py_DecRef(tmp_obj);

        tmp_obj = INT(i->ports[ctr]->direction);
        PyDict_SetItemString(port_dict, "direction", tmp_obj);
        Py_DecRef(tmp_obj);

        tmp_obj = INT(i->ports[ctr]->n_profiles);
        PyDict_SetItemString(port_dict, "n_profiles", tmp_obj);
        Py_DecRef(tmp_obj);

        PyList_Append(port_list, port_dict);
        Py_DecRef(port_dict);
    }
    PyDict_SetItemString(card_dict, "ports", port_list);
    Py_DecRef(port_list);

    PyObject *func = NULL;
    if (self->state_cb && PyDict_Check(self->state_cb) && ((func=PyDict_GetItemString(self->state_cb, "card")) != NULL)) {
        if (PyCallable_Check(func)) {
            PyGILState_STATE gstate;
            gstate = PyGILState_Ensure();
            PyEval_CallFunction(func, "(OOi)", self, card_dict, i->index);
            PyGILState_Release(gstate);
        }
    }
}

static void m_pa_sinklist_cb(pa_context *c, 
                             const pa_sink_info *l, 
                             int eol, 
                             void *userdata) 
{
    if (!userdata || eol || !c || !l)
        return;
   
    DeepinPulseAudioObject *self = userdata;

    pa_sink_port_info **ports  = NULL;                                          
    pa_sink_port_info *port = NULL;        
    pa_sink_port_info *active_port = NULL;
    PyObject *channel_value = NULL;
    PyObject *ret_active_port_value = NULL;
    PyObject *ret_volume_value = NULL;
    PyObject *prop_dict = NULL;
    PyObject *port_list = NULL;
    int i = 0;
    const char *prop_key;
    void *prop_state = NULL;
    PyObject *tmp_obj = NULL;

    PyObject *ret_channel_dict = NULL;
    ret_channel_dict = PyDict_New();
    if (!ret_channel_dict) {
        printf("PyDict_New error");
        return;
    }

    PyObject *ret_dev_dict = NULL;
    ret_dev_dict = PyDict_New();
    if (!ret_dev_dict) {
        printf("PyDict_New error");
        return;
    }

    channel_value = PyList_New(0);
    if (!channel_value) {
        printf("PyList_New error");
        return;
    }
    ret_volume_value = PyList_New(0);
    if (!ret_volume_value) {
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
                                                                                
    while ((prop_key = pa_proplist_iterate(l->proplist, &prop_state))) {
        tmp_obj = STRING(pa_proplist_gets(l->proplist, prop_key));
        PyDict_SetItemString(prop_dict, prop_key, tmp_obj);
        Py_DecRef(tmp_obj);
    }
    // channel list
    for (i = 0; i < l->channel_map.channels; i++) {
        tmp_obj = INT(l->channel_map.map[i]);
        PyList_Append(channel_value, tmp_obj);
        Py_DecRef(tmp_obj);
    }
    ret_channel_dict = Py_BuildValue("{sisnsO}", 
                            "can_balance", pa_channel_map_can_balance(&l->channel_map),
                            "channels", l->channel_map.channels,
                            "map", channel_value);
    Py_DecRef(channel_value);

    // ports list
    ports = l->ports;   
    for (i = 0; i < l->n_ports; i++) {
        port = ports[i];
        tmp_obj = Py_BuildValue("(ssi)",
                                port->name,
                                port->description,
                                port->available);
        PyList_Append(port_list, tmp_obj); 
        Py_DecRef(tmp_obj);
    }
    // active port
    active_port = l->active_port;
    if (active_port) {
        ret_active_port_value = Py_BuildValue("(ssi)", 
                                          active_port->name,
                                          active_port->description, 
                                          active_port->available);
    } else {
        ret_active_port_value = Py_None;
        Py_INCREF(Py_None);
    }
    // volume list
    for (i = 0; i < l->volume.channels; i++) {
        tmp_obj = INT(l->volume.values[i]);
        PyList_Append(ret_volume_value, tmp_obj);
        Py_DecRef(tmp_obj);
    }
    PyObject *tmp_obj1 = PyBool_FromLong(l->mute);
    ret_dev_dict = Py_BuildValue("{sssssisIsOsOsO}",
                            "name", l->name,
                            "description", l->description,
                            "base_volume", l->base_volume,
                            "n_ports", l->n_ports,
                            "mute", tmp_obj1,
                            "ports", port_list,
                            "proplist", prop_dict);
    Py_DecRef(tmp_obj1);
    Py_DecRef(port_list);
    Py_DecRef(prop_dict);

    PyObject *func = NULL;
    if (self->state_cb && PyDict_Check(self->state_cb) && ((func=PyDict_GetItemString(self->state_cb, "sink")) != NULL)) {
        if (PyCallable_Check(func)) {
            PyGILState_STATE gstate;
            gstate = PyGILState_Ensure();
            PyEval_CallFunction(func, "(OOOOOi)",
                                self, ret_channel_dict,
                                ret_active_port_value,
                                ret_volume_value,
                                ret_dev_dict,
                                l->index);
            PyGILState_Release(gstate);
        }
    }
}

// See above.  This callback is pretty much identical to the previous
static void m_pa_sourcelist_cb(pa_context *c, 
                               const pa_source_info *l, 
                               int eol, 
                               void *userdata) 
{
    if (!userdata || eol || !c || !l)
        return;

    DeepinPulseAudioObject *self = (DeepinPulseAudioObject *) userdata;
    
    pa_source_port_info **ports = NULL;                                         
    pa_source_port_info *port = NULL;      
    pa_source_port_info *active_port = NULL;
    PyObject *channel_value = NULL;
    PyObject *ret_active_port_value = NULL;
    PyObject *ret_volume_value = NULL;
    PyObject *prop_dict = NULL;
    PyObject *port_list = NULL;
    int i = 0;                                                                  
    const char *prop_key;
    void *prop_state = NULL;
    PyObject *tmp_obj = NULL;
                                                                                
    PyObject *ret_channel_dict = NULL;
    ret_channel_dict = PyDict_New();
    if (!ret_channel_dict) {
        printf("PyDict_New error");
        return;
    }

    PyObject *ret_dev_dict = NULL;
    ret_dev_dict = PyDict_New();
    if (!ret_dev_dict) {
        printf("PyDict_New error");
        return;
    }

    channel_value = PyList_New(0);
    if (!channel_value) {
        printf("PyList_New error");
        return;
    }
    ret_volume_value = PyList_New(0);
    if (!ret_volume_value) {
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
                                                                                
    while ((prop_key=pa_proplist_iterate(l->proplist, &prop_state))) {
        tmp_obj = STRING(pa_proplist_gets(l->proplist, prop_key));
        PyDict_SetItemString(prop_dict, prop_key,tmp_obj);
        Py_DecRef(tmp_obj);
    }
    // channel list
    for (i = 0; i < l->channel_map.channels; i++) {                    
        tmp_obj = INT(l->channel_map.map[i]);
        PyList_Append(channel_value, tmp_obj);
        Py_DecRef(tmp_obj);
    }                                                                   
    ret_channel_dict = Py_BuildValue("{sisnsO}",
                            "can_balance", pa_channel_map_can_balance(&l->channel_map),
                            "channels", l->channel_map.channels,
                            "map", channel_value);
    Py_DecRef(channel_value);

    // ports list
    ports = l->ports;                                                   
    for (i = 0; i < l->n_ports; i++) {                                  
        port = ports[i];                                                
        tmp_obj = Py_BuildValue("(ssi)",
                                port->name,
                                port->description,
                                port->available);
        PyList_Append(port_list, tmp_obj);
        Py_DecRef(tmp_obj);
    } 
    // active port
    active_port = l->active_port;                                       
    if (active_port) {
        ret_active_port_value = Py_BuildValue("(ssi)",
                                          active_port->name,
                                          active_port->description,         
                                          active_port->available);               
    } else {
        ret_active_port_value = Py_None;
        Py_INCREF(Py_None);
    }
    // volume list
    for (i = 0; i < l->volume.channels; i++) {
        tmp_obj = INT(l->volume.values[i]);
        PyList_Append(ret_volume_value, tmp_obj);
        Py_DecRef(tmp_obj);
    }
    PyObject *tmp_obj1 = PyBool_FromLong(l->mute);
    ret_dev_dict = Py_BuildValue("{sssssisIsOsOsO}",
                            "name", l->name,
                            "description", l->description,
                            "base_volume", l->base_volume,
                            "n_ports", l->n_ports,
                            "mute", tmp_obj1,
                            "ports", port_list,
                            "proplist", prop_dict);
    Py_DecRef(tmp_obj1);
    Py_DecRef(port_list);
    Py_DecRef(prop_dict);

    PyObject *func = NULL;
    if (self->state_cb && PyDict_Check(self->state_cb) && ((func=PyDict_GetItemString(self->state_cb, "source")) != NULL)) {
        if (PyCallable_Check(func)) {
            PyGILState_STATE gstate;
            gstate = PyGILState_Ensure();
            PyEval_CallFunction(func, "(OOOOOi)",
                                self, ret_channel_dict,
                                ret_active_port_value,
                                ret_volume_value,
                                ret_dev_dict,
                                l->index);
            PyGILState_Release(gstate);
        }
    }
}

static void m_pa_sinkinputlist_info_cb(pa_context *c,
                                       const pa_sink_input_info *l,
                                       int eol,
                                       void *userdata)
{
    if (!userdata || eol || !c || !l)
        return;

    DeepinPulseAudioObject *self = userdata;
    
    PyObject *volume_value = NULL;
    PyObject *channel_value = NULL;
    PyObject *prop_dict = NULL;
    int i;
    const char *prop_key;
    void *prop_state = NULL;
    PyObject *tmp_obj = NULL;
    PyObject *retval = NULL;

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

    // proplist
    while ((prop_key=pa_proplist_iterate(l->proplist, &prop_state))) {
        tmp_obj = STRING(pa_proplist_gets(l->proplist, prop_key));
        PyDict_SetItemString(prop_dict, prop_key, tmp_obj);
        Py_DecRef(tmp_obj);
    }
    // channel list
    for (i = 0; i < l->channel_map.channels; i++) {
        tmp_obj = INT(l->channel_map.map[i]);
        PyList_Append(channel_value, tmp_obj);
        Py_DecRef(tmp_obj);
    }
    //volume list
    for (i = 0; i < l->volume.channels; i++) {
        tmp_obj = INT(l->volume.values[i]);
        PyList_Append(volume_value, tmp_obj);
        Py_DecRef(tmp_obj);
    }
    retval = Py_BuildValue("{sssisisisOsssssOsisOsisisi}",
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
                            "volume_writable", l->volume_writable);
    Py_DecRef(channel_value);
    Py_DecRef(prop_dict);
    Py_DecRef(volume_value);

    PyObject *func = NULL;
    if (self->state_cb && PyDict_Check(self->state_cb) && ((func=PyDict_GetItemString(self->state_cb, "sinkinput")) != NULL)) {
        if (PyCallable_Check(func)) {
            PyGILState_STATE gstate;
            gstate = PyGILState_Ensure();
            PyEval_CallFunction(func, "(OOi)", self, retval, l->index);
            PyGILState_Release(gstate);
        }
    }
}

static void m_pa_sourceoutputlist_info_cb(pa_context *c,
                                          const pa_source_output_info *l,
                                          int eol,
                                          void *userdata)
{
    if (!userdata || eol || !c || !l)
        return;

    DeepinPulseAudioObject *self = userdata;
    
    PyObject *volume_value = NULL;
    PyObject *channel_value = NULL;
    PyObject *prop_dict = NULL;
    int i;
    const char *prop_key;
    void *prop_state = NULL;
    PyObject *tmp_obj = NULL;
    PyObject *retval = NULL;

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

    // proplist
    while ((prop_key=pa_proplist_iterate(l->proplist, &prop_state))) {
        tmp_obj = STRING(pa_proplist_gets(l->proplist, prop_key));
        PyDict_SetItemString(prop_dict, prop_key,tmp_obj);
        Py_DecRef(tmp_obj);
    }
    // channel list
    for (i = 0; i < l->channel_map.channels; i++) {
        tmp_obj = INT(l->channel_map.map[i]);
        PyList_Append(channel_value, tmp_obj);
        Py_DecRef(tmp_obj);
    }
    //volume list
    for (i = 0; i < l->volume.channels; i++) {
        tmp_obj = INT(l->volume.values[i]);
        PyList_Append(volume_value, tmp_obj);
        Py_DecRef(tmp_obj);
    }
    retval = Py_BuildValue("{sssisisisOsssssOsisOsisisi}",
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
                            "volume_writable", l->volume_writable);
    Py_DecRef(channel_value);
    Py_DecRef(prop_dict);
    Py_DecRef(volume_value);

    PyObject *func = NULL;
    if (self->state_cb && PyDict_Check(self->state_cb) && ((func=PyDict_GetItemString(self->state_cb, "sourceoutput")) != NULL)) {
        if (PyCallable_Check(func)) {
            PyGILState_STATE gstate;
            gstate = PyGILState_Ensure();
            PyEval_CallFunction(func, "(OOi)", self, retval, l->index);
            PyGILState_Release(gstate);
        }
    }
}

//****************************************
// remove event callback
static void m_pa_event_removed_cb(DeepinPulseAudioObject *self,
                                  uint32_t index,
                                  const char *key)
{
    if (!self || !key) 
        return;
    printf("removed: %s\n", key);
    PyObject *callback = PyDict_GetItemString(self->event_cb, key);
    if (!callback || !PyCallable_Check(callback)) {
        return;
    }

    PyGILState_STATE gstate;
    gstate = PyGILState_Ensure();
    PyEval_CallFunction(callback, "(Oi)", self, index);
    PyGILState_Release(gstate);
}

static void m_pa_context_subscribe_cb(pa_context *c,                           
                                      pa_subscription_event_type_t t,          
                                      uint32_t idx,                            
                                      void *userdata)                          
{                                                                               
    if (!c || !userdata) 
        return;

    DeepinPulseAudioObject *self = (DeepinPulseAudioObject *) userdata;

    switch (t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) {
        case PA_SUBSCRIPTION_EVENT_SINK: {
            if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_REMOVE)
                m_pa_event_removed_cb(self, idx, "sink-removed");
            else {
                pa_operation *o;
                if (!(o = pa_context_get_sink_info_by_index(c, idx, m_pa_sinklist_cb, self))) {
                    printf("pa_context_get_sink_info_by_index() failed");
                    return;
                }
                pa_operation_unref(o);
            }
            break;
        }
        case PA_SUBSCRIPTION_EVENT_SOURCE: {
            if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_REMOVE)
                m_pa_event_removed_cb(self, idx, "source-removed");
            else {
                pa_operation *o;
                if (!(o = pa_context_get_source_info_by_index(c, idx, m_pa_sourcelist_cb, self))) {
                    printf("pa_context_get_source_info_by_index() failed");
                    return;
                }
                pa_operation_unref(o);
            }
            break;
        }
        case PA_SUBSCRIPTION_EVENT_SINK_INPUT: {
            if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_REMOVE)
                m_pa_event_removed_cb(self, idx, "sinkinput-removed");
            else {
                pa_operation *o;
                if (!(o = pa_context_get_sink_input_info(c, idx, m_pa_sinkinputlist_info_cb, self))) {
                    printf("pa_context_get_sink_input_info() failed");
                    return;
                }
                pa_operation_unref(o);
            }
            break;
        }
        case PA_SUBSCRIPTION_EVENT_SOURCE_OUTPUT: {
            if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_REMOVE)
                m_pa_event_removed_cb(self, idx, "sourceoutput-removed");
            else {
                pa_operation *o;
                if (!(o = pa_context_get_source_output_info(c, idx, m_pa_sourceoutputlist_info_cb, self))) {
                    printf("pa_context_get_sink_input_info() failed");
                    return;
                }
                pa_operation_unref(o);
            }
            break;
        }
        case PA_SUBSCRIPTION_EVENT_CLIENT:                                      
            break;                                                              
        case PA_SUBSCRIPTION_EVENT_SERVER: {
            pa_operation *o;
            if (!(o = pa_context_get_server_info(c, m_pa_server_info_cb, self))) {
                printf("pa_context_get_server_info() failed");
                return;
            }
            pa_operation_unref(o);
            break;
        }
        case PA_SUBSCRIPTION_EVENT_CARD: {
            if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_REMOVE)
                m_pa_event_removed_cb(self, idx, "card-removed");
            else {
                pa_operation *o;
                if (!(o = pa_context_get_card_info_by_index(c, idx, m_pa_cardlist_cb, self))) {
                    printf("pa_context_get_card_info_by_index() failed");
                    return;
                }
                pa_operation_unref(o);
            }
            break;
        }
    }
}

static void m_context_state_cb(pa_context *c, void *userdata) 
{
    if (!c || !userdata) 
        return;

    DeepinPulseAudioObject *self = (DeepinPulseAudioObject *) userdata;                         
    pa_operation *pa_op = NULL;

    /*printf("context_state:%d\n", pa_context_get_state(c));*/
    /*printf("PA_CONTEXT_UNCONNECTED: %d\n", PA_CONTEXT_UNCONNECTED);*/
    /*printf("PA_CONTEXT_CONNECTING: %d\n", PA_CONTEXT_CONNECTING);*/
    /*printf("PA_CONTEXT_AUTHORIZING: %d\n", PA_CONTEXT_AUTHORIZING);*/
    /*printf("PA_CONTEXT_SETTING_NAME: %d\n", PA_CONTEXT_SETTING_NAME);*/
    /*printf("PA_CONTEXT_READY: %d\n", PA_CONTEXT_READY);*/
    /*printf("PA_CONTEXT_FAILED: %d\n", PA_CONTEXT_FAILED);*/
    /*printf("PA_CONTEXT_TERMINATED: %d\n", PA_CONTEXT_TERMINATED);*/

    switch (pa_context_get_state(c)) {                                          
        case PA_CONTEXT_UNCONNECTED:
        case PA_CONTEXT_CONNECTING:
        case PA_CONTEXT_AUTHORIZING:
        case PA_CONTEXT_SETTING_NAME:
            break;
                                                                                
        case PA_CONTEXT_READY: {
            pa_context_set_subscribe_callback(c, m_pa_context_subscribe_cb, self);

            /*if (!(pa_op = pa_context_subscribe(c, (pa_subscription_mask_t)          */
                                           /*(PA_SUBSCRIPTION_MASK_ALL), NULL, NULL))) {*/
            if (!(pa_op = pa_context_subscribe(c, (pa_subscription_mask_t)
                                           (PA_SUBSCRIPTION_MASK_SINK|
                                            PA_SUBSCRIPTION_MASK_SOURCE|
                                            PA_SUBSCRIPTION_MASK_SINK_INPUT|
                                            PA_SUBSCRIPTION_MASK_SOURCE_OUTPUT|
                                            PA_SUBSCRIPTION_MASK_CLIENT|
                                            PA_SUBSCRIPTION_MASK_SERVER|
                                            PA_SUBSCRIPTION_MASK_CARD), NULL, NULL))) {
                printf("pa_context_subscribe() failed\n");
                return;
            }
            pa_operation_unref(pa_op);

            if (!(pa_op = pa_context_get_server_info(c, m_pa_server_info_cb, self))) {
                printf("pa_context_get_server_info() failed");
                return;
            }
            pa_operation_unref(pa_op);

            if (!(pa_op = pa_context_get_card_info_list(c, m_pa_cardlist_cb, self))) {
                printf("pa_context_get_card_info_list() failed");
                return;
            }
            pa_operation_unref(pa_op);

            if (!(pa_op = pa_context_get_sink_info_list(c, m_pa_sinklist_cb, self))) {
                printf("pa_context_get_sink_info_list() failed");
                return;
            }
            pa_operation_unref(pa_op);

            if (!(pa_op = pa_context_get_source_info_list(c, m_pa_sourcelist_cb, self))) {
                printf("pa_context_get_source_info_list() failed");
                return;
            }
            pa_operation_unref(pa_op);

            if (!(pa_op = pa_context_get_sink_input_info_list(c, m_pa_sinkinputlist_info_cb, self))) {
                printf("pa_context_get_sink_input_info_list() failed");
                return;
            }
            pa_operation_unref(pa_op);

            if (!(pa_op = pa_context_get_source_output_info_list(c, m_pa_sourceoutputlist_info_cb, self))) {
                printf("pa_context_get_source_output_info_list() failed");
                return;
            }
            pa_operation_unref(pa_op);
            break;
        }
                                                                                
        case PA_CONTEXT_FAILED:                                                 
            pa_context_unref(self->pa_ctx);                                          
            self->pa_ctx = NULL;                                                     
                                                                                
            printf("Connection failed, attempting reconnect\n");          
            g_timeout_add_seconds(13, (GSourceFunc)m_connect_to_pulse, self);               
            return;                                                             
                                                                                
        case PA_CONTEXT_TERMINATED:                                             
        default:        
            {
                PyObject *key, *value;
                Py_ssize_t pos = 0;
                while (PyDict_Next(self->event_cb, &pos, &key, &value)) {
                    printf("event_cb: %d %s\n", pos, PyString_AsString(key));
                }
                printf("pa_context terminated\n");            
                return;                                                             
            }
    }
}

static PyObject *m_connect_to_pulse(DeepinPulseAudioObject *self, PyObject *args)
{
    if (self->pa_ctx) {
        RETURN_FALSE;
    }
    PyObject *cb_fun = NULL;
    if (!PyArg_ParseTuple(args, "O", &cb_fun)) {
        ERROR("invalid arguments to connect_to_pulse");
        return NULL;
    }
    Py_XINCREF(cb_fun);                                                   
    self->state_cb = cb_fun;

    self->pa_ctx = pa_context_new(self->pa_mlapi, PACKAGE);
    if (!self->pa_ctx) {
        printf("pa_context_new() failed\n");
        RETURN_FALSE;
    }

    pa_context_set_state_callback(self->pa_ctx, m_context_state_cb, self);

    if (pa_context_connect(self->pa_ctx, NULL, PA_CONTEXT_NOFAIL, NULL) < 0) {
        if (pa_context_errno(self->pa_ctx) == PA_ERR_INVALID) {
            printf("Connection to PulseAudio failed. Automatic retry in 13s\n");
            RETURN_FALSE;
        } else {
            m_delete(self);
            RETURN_FALSE;
        }
    }
    RETURN_TRUE;
}

static PyObject *m_connect(DeepinPulseAudioObject *self, PyObject *args)         
{                                                                               
    PyObject *signal = NULL;
    PyObject *callback = NULL;
                                                                                
    if (!PyArg_ParseTuple(args, "OO:set_callback", &signal, &callback)) {
        ERROR("invalid arguments to connect");
        return NULL;
    }
                                                                                
    if (!PyCallable_Check(callback) || !PyString_CheckExact(signal)) {
        Py_INCREF(Py_False);
        return Py_False;
    }

    if (PyDict_Contains(self->event_cb, signal)) {
        PyDict_DelItem(self->event_cb, signal);
    }
    PyDict_SetItem(self->event_cb, signal, callback);
    RETURN_TRUE;
}