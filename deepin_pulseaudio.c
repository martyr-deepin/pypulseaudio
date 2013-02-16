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
#include <pthread.h>

#define PACKAGE "Deepin PulseAudio Python Binding"
#define DEVICE_NUM 16

#define INT(v) PyInt_FromLong(v)
#define STRING(v) PyString_FromString(v)
#define ERROR(v) PyErr_SetString(PyExc_TypeError, v)

/* Safe XDECREF for object states that handles nested deallocations */
#define ZAP(v) do {\
    PyObject *tmp = (PyObject *)(v); \
    (v) = NULL; \
    Py_XDECREF(tmp); \
} while (0)

typedef struct {
    PyObject_HEAD
    PyObject *dict; /* Python attributes dictionary */
    pthread_t thread;
    int state;
    PyObject *sink_changed_cb;
    PyObject *source_changed_cb;
    PyObject *card_changed_cb;
    PyObject *server_info;
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
} DeepinPulseAudioObject;

/* TODO: pthread mutex to lock pa_context_get_state, because pa_context_get_XXX 
 * is atomic operation, it need mutex locker when several threads set the value 
 * at same time
 */
static pthread_mutex_t m_mutex = PTHREAD_MUTEX_INITIALIZER;
static PyObject *m_deepin_pulseaudio_object_constants = NULL;
static PyTypeObject *m_DeepinPulseAudio_Type = NULL;

static DeepinPulseAudioObject *m_init_deepin_pulseaudio_object();
static void m_pa_context_subscribe_cb(pa_context *c,                           
                                      pa_subscription_event_type_t t,          
                                      uint32_t idx,                            
                                      void *userdata);
static void *m_pa_connect_loop_cb(void *arg);
static DeepinPulseAudioObject *m_new(PyObject *self, PyObject *args);

static PyMethodDef deepin_pulseaudio_methods[] = 
{
    {"new", m_new, METH_NOARGS, "Deepin PulseAudio Construction"}, 
    {NULL, NULL, 0, NULL}
};

static PyObject *m_delete(DeepinPulseAudioObject *self);
static PyObject *m_connect(DeepinPulseAudioObject *self, PyObject *args);
static void m_pa_state_cb(pa_context *c, void *userdata);                                
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
static PyObject *m_get_server_info(DeepinPulseAudioObject *self);
static PyObject *m_get_cards(DeepinPulseAudioObject *self);
static PyObject *m_get_devices(DeepinPulseAudioObject *self);
static PyObject *m_get_output_devices(DeepinPulseAudioObject *self);
static PyObject *m_get_input_devices(DeepinPulseAudioObject *self);

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
    {"connect", m_connect, METH_VARARGS, "Connect signal callback"}, 
    {"get_server_info", m_get_server_info, METH_NOARGS, "Get server info"},
    {"get_cards", m_get_cards, METH_NOARGS, "Get card list"}, 
    {"get_devices", m_get_devices, METH_NOARGS, "Get device list"}, 

    {"get_output_devices", m_get_output_devices, METH_NOARGS, "Get output device list"},  
    {"get_input_devices", m_get_input_devices, METH_NOARGS, "Get input device list"},      

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
    self->thread = 0;
    self->state = 0;
    self->sink_changed_cb = NULL;
    self->source_changed_cb = NULL;
    self->server_info = NULL;
    self->card_changed_cb = NULL;
    self->card_devices = NULL;
    self->input_devices = NULL;
    self->output_devices = NULL;
    self->input_ports = NULL;
    self->output_ports = NULL;
    self->input_channels = NULL;
    self->output_channels = NULL;
    self->input_active_ports = NULL;
    self->output_active_ports = NULL;

    return self;
}

static void m_pa_client_info_cb(pa_context *c,                                  
                                const pa_client_info *info,                        
                                int eol,                                        
                                void *userdata)                                 
{                                 
    if (!info) 
        return;

    printf("DEBUG client info %s\n", info ? info->name : NULL);                                   
}

static void m_pa_sink_info_cb(pa_context *c, 
                              const pa_sink_info *info, 
                              int eol, 
                              void *userdata) 
{
    DeepinPulseAudioObject *self = (DeepinPulseAudioObject *) userdata;
    PyObject *volumes = NULL;
    int i = 0;
    PyGILState_STATE gstate;

    if (!info) 
        return;

    /* TODO: thread lock */
    gstate = PyGILState_Ensure();

    if (self->sink_changed_cb) {
        volumes = PyTuple_New(info->volume.channels);
        for (i = 0; i < info->volume.channels; i++) 
            PyTuple_SetItem(volumes, i, INT(info->volume.values[i]));
        PyEval_CallFunction(self->sink_changed_cb, 
            "(snOns)", 
            info->name, 
            INT(info->index), 
            volumes, 
            INT(info->mute), 
            info->active_port->name);
    }
    
    /* FIXME: why can not unlock ?! 
    PyGILState_Release(gstate);
    */
}

static void m_pa_context_subscribe_cb(pa_context *c,                           
                                      pa_subscription_event_type_t t,          
                                      uint32_t idx,                            
                                      void *userdata)                          
{                                                                               
    DeepinPulseAudioObject *self = (DeepinPulseAudioObject *) userdata;
        
    switch (t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) {                          
        case PA_SUBSCRIPTION_EVENT_SINK:
            if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_NEW) {
                printf("DEBUG sink %d new\n", idx);
            } else if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_CHANGE) {
                pa_context_get_sink_info_by_index(c, idx, m_pa_sink_info_cb, NULL);
            } else if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_REMOVE) {
                printf("DEBUG sink %d removed\n", idx);
                PyObject *key = NULL;
                key = INT(idx);
                if (self->output_active_ports && PyDict_Contains(self->output_active_ports, key)) {
                    PyDict_DelItem(self->output_active_ports, key);
                }
                if (self->output_channels && PyDict_Contains(self->output_channels, key)) {
                    PyDict_DelItem(self->output_channels, key);
                }
                if (self->output_devices && PyDict_Contains(self->output_devices, key)) {
                    PyDict_DelItem(self->output_devices, key);
                }
                if (self->output_mute && PyDict_Contains(self->output_mute, key)) {
                    PyDict_DelItem(self->output_mute, key);
                }
                if (self->output_ports && PyDict_Contains(self->output_ports, key)) {
                    PyDict_DelItem(self->output_ports, key);
                }
                if (self->output_volume && PyDict_Contains(self->output_volume, key)) {
                    PyDict_DelItem(self->output_volume, key);
                }
            }
            break;
        case PA_SUBSCRIPTION_EVENT_SOURCE:
            if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_NEW) {
                printf("DEBUG source %d new\n", idx);
            } else if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_CHANGE) {
                //pa_context_get_sink_info_by_index(c, idx, m_pa_sink_info_cb, NULL);
            } else if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_REMOVE) {
                printf("DEBUG source %d removed\n", idx);
                PyObject *key = NULL;
                key = INT(idx);
                if (self->input_active_ports && PyDict_Contains(self->input_active_ports, key)) {
                    PyDict_DelItem(self->input_active_ports, key);
                }
                if (self->input_channels && PyDict_Contains(self->input_channels, key)) {
                    PyDict_DelItem(self->input_channels, key);
                }
                if (self->input_devices && PyDict_Contains(self->input_devices, key)) {
                    PyDict_DelItem(self->input_devices, key);
                }
                if (self->input_mute && PyDict_Contains(self->input_mute, key)) {
                    PyDict_DelItem(self->input_mute, key);
                }
                if (self->input_ports && PyDict_Contains(self->input_ports, key)) {
                    PyDict_DelItem(self->input_ports, key);
                }
                if (self->input_volume && PyDict_Contains(self->input_volume, key)) {
                    PyDict_DelItem(self->input_volume, key);
                }
            }
            break;
        case PA_SUBSCRIPTION_EVENT_CLIENT:                                      
            if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_REMOVE) {
            } else {                                                            
                pa_context_get_client_info(c, idx, m_pa_client_info_cb, NULL);  
            }                                                                   
            break;                                                              
        case PA_SUBSCRIPTION_EVENT_SERVER:
            break;
        case PA_SUBSCRIPTION_EVENT_CARD:
            break;
    }
}

/* http://freedesktop.org/software/pulseaudio/doxygen/subscribe.html */
static void *m_pa_connect_loop_cb(void *arg) 
{
    DeepinPulseAudioObject *self = (DeepinPulseAudioObject *) arg;

    /* TODO: We do not need pa_threaded_mainloop because we can support 
     * pthread mutex by ourself
     */
    pa_mainloop *pa_ml = NULL;                                                  
    pa_mainloop_api *pa_mlapi = NULL;                                           
    pa_context *pa_ctx = NULL;
    pa_operation *pa_op = NULL;                                                 
    int pa_ready;                                                               
                                                                                
RE_CONN:                                                                        
    // We'll need these state variables to keep track of our requests           
    self->state = 0;                                                                
    pa_ready = 0;                                                               
                                                                                
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
            /* wait for a while to reconnect to pulse server */                 
            sleep(13);                                                           
            goto RE_CONN;                                                       
        }                                                                       
        // At this point, we're connected to the server and ready to make          
        // requests                                                             
        switch (self->state) {                                                      
            // State 0: we haven't done anything yet
            case 0:                                                             
                pa_context_set_subscribe_callback(pa_ctx,                       
                m_pa_context_subscribe_cb,                                      
                self);                                                          
                pa_op = pa_context_subscribe(pa_ctx,                            
                        PA_SUBSCRIPTION_MASK_SINK|                             
                        PA_SUBSCRIPTION_MASK_SOURCE|                            
                        PA_SUBSCRIPTION_MASK_SINK_INPUT|                        
                        PA_SUBSCRIPTION_MASK_SOURCE_OUTPUT|                     
                        PA_SUBSCRIPTION_MASK_CLIENT|                            
                        PA_SUBSCRIPTION_MASK_SERVER|                            
                        PA_SUBSCRIPTION_MASK_CARD,                             
                        NULL,                                                   
                        NULL);
                self->state++;                                                      
                break;                                                          
            case 1:
                pthread_mutex_lock(&m_mutex);
                if (pa_operation_get_state(pa_op) == PA_OPERATION_DONE) {          
                    pa_operation_unref(pa_op);
                    self->state++; 
                }
                pthread_mutex_unlock(&m_mutex);
                break;
            case 2:
                usleep(100);
                break;                                                   
            case 3:                                                             
                // Now we're done, clean up and disconnect and return           
                printf("DEBUG disconnect from pulse server\n");
                pa_context_disconnect(pa_ctx);                                
                pa_context_unref(pa_ctx);
                pa_mainloop_free(pa_ml);
                return NULL;                                                    
            default:                                                            
                // We should never see this state
                return NULL;                                                    
        }                                                                       
        // Iterate the main loop and go again.  The second argument is whether  
        // or not the iteration should block until something is ready to be        
        // done.  Set it to zero for non-blocking.                              
        pa_mainloop_iterate(pa_ml, 1, NULL);                                    
    }                                                                           
                                                                                
    return NULL;                                                                
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

    self->input_ports = PyDict_New();
    if (!self->input_ports) {
        printf("PyDict_New error");
        return;
    }

    self->output_mute = PyDict_New();                                            
    if (!self->output_mute) {                                                    
        ERROR("PyDict_New error");                                              
        m_delete(self);                                                         
        return NULL;                                                            
    } 

    self->output_ports = PyDict_New();
    if (!self->output_ports) {
        printf("PyDict_New error");
        return;
    }

    self->output_volume = PyDict_New();
    if (!self->output_volume) {
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

    pthread_create(&self->thread, NULL, m_pa_connect_loop_cb, self);

    return self;
}

/* FIXME: fuzzy ... more object wait for destruction */
static PyObject *m_delete(DeepinPulseAudioObject *self) 
{
    /* Why state is 3, please see m_pa_connect_loop_cb switch case 3
    self->state = 3;
    if (self->thread) 
        pthread_cancel(&self->thread);
    */
    
    if (self->output_devices) {
        Py_DecRef(self->output_devices);
        self->output_devices = NULL;
    }

    if (self->input_devices) {
        Py_DecRef(self->input_devices);
        self->input_devices = NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *m_connect(DeepinPulseAudioObject *self, PyObject *args)         
{                                                                               
    char *signal = NULL;                                                         
    PyObject *callback = NULL;                                                      
                                                                                
    if (!PyArg_ParseTuple(args, "sO:set_callback", &signal, &callback)) {             
        ERROR("invalid arguments to connect");                                  
        return NULL;                                                            
    }                                                                           
                                                                                
    if (!PyCallable_Check(callback)) {                                              
        Py_INCREF(Py_False);                                                    
        return Py_False;                                                        
    }                                                                           
                                                                                
    if (strcmp(signal, "sink-changed") == 0) {                                         
        Py_XINCREF(callback);                                                       
        Py_XDECREF(self->sink_changed_cb);                                           
        self->sink_changed_cb = callback;
    }

    if (strcmp(signal, "source-changed") == 0) {                                     
        Py_XINCREF(callback);                                                   
        Py_XDECREF(self->source_changed_cb);                                         
        self->source_changed_cb = callback;                                          
    }

    if (strcmp(signal, "card-changed") == 0) {                                     
        Py_XINCREF(callback);                                                   
        Py_XDECREF(self->card_changed_cb);                                         
        self->card_changed_cb = callback;                                          
    }                                                                           
                                                                                
    Py_INCREF(Py_True);                                                         
    return Py_True;                                                             
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
    PyDict_Clear(self->output_devices);
    PyDict_Clear(self->input_devices);
    PyDict_Clear(self->output_channels);
    PyDict_Clear(self->input_channels);
    PyDict_Clear(self->output_active_ports);
    PyDict_Clear(self->input_active_ports);
    PyDict_Clear(self->output_mute);
    PyDict_Clear(self->input_mute);
    PyDict_Clear(self->output_volume);
    PyDict_Clear(self->input_volume);

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
    return self->output_devices;
}

static PyObject *m_get_input_devices(DeepinPulseAudioObject *self)          
{                                                                               
    return self->input_devices;                                                                
}        

static PyObject *m_get_output_ports(DeepinPulseAudioObject *self) 
{
    // TODO output_ports 目前为空
    return self->output_ports;
}

static PyObject *m_get_output_ports_by_index(DeepinPulseAudioObject *self, PyObject *args)
{
    int device = -1;
    
    if (!PyArg_ParseTuple(args, "i", &device)) {
        ERROR("invalid arguments to get_output_channels");
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
    // TODO input_ports 目前为空
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
    // TODO 目前output_mute为空
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
    // TODO 目前input_mute为空
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

// set function
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

    PyObject *key = NULL, *value = NULL;
    Py_ssize_t pos = 0;

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

    PyObject *key = NULL, *value = NULL;
    PyObject *channel_map_list = NULL;
    Py_ssize_t pos = 0;

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

    PyObject *key = NULL, *value = NULL;
    Py_ssize_t pos = 0;
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

    PyObject *key = NULL, *value = NULL;
    PyObject *channel_map_list = NULL;
    Py_ssize_t pos = 0;

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
        return Py_True;
    }

}

// This callback gets called when our context changes state.  We really only    
// care about when it's ready or if it has failed                               
static void m_pa_state_cb(pa_context *c, void *userdata) {                               
    pthread_mutex_lock(&m_mutex);
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
    pthread_mutex_unlock(&m_mutex);
}


static void m_pa_server_info_cb(pa_context *c, const pa_server_info *i, void *userdate)
{
    DeepinPulseAudioObject *self = userdate;
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
    DeepinPulseAudioObject *self = userdata;
    // If eol is set to a positive number, you're at the end of the list        
    if (eol > 0) {
        return;
    }
    PyObject *card_dict = NULL;         // a dict save the card info
    PyObject *profile_list = NULL;      // card_profile_info list
    PyObject *profile_dict = NULL;
    PyObject *port_list = NULL;         // card_port_info
    PyObject *port_dict = NULL;
    PyObject *active_profile = NULL;
    PyObject *prop_dict = NULL;
    PyObject *key = NULL, *sub_key = NULL;

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
    Py_DecRef(key);
}
// pa_mainloop will call this function when it's ready to tell us about a sink. 
// Since we're not threading, there's no need for mutexes on the devicelist     
// structure                                                                    
static void m_pa_sinklist_cb(pa_context *c, 
                             const pa_sink_info *l, 
                             int eol, 
                             void *userdata) 
{
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
                                                                                
    // If eol is set to a positive number, you're at the end of the list        
    if (eol > 0) {                                                              
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
        printf("PyDict_New error");
        return;
    }
    port_list = PyList_New(0);
    if (!port_list) {
        printf("PyList_New error");
        return;
    }
                                                                                
    // We know we've allocated 16 slots to hold devices.  Loop through our      
    // structure and find the first one that's "uninitialized."  Copy the       
    // contents into it and we're done.  If we receive more than 16 devices,    
    // they're going to get dropped.  You could make this dynamically allocate  
    // space for the device list, but this is a simple example.                 
    key = INT(l->index);
    while ((prop_key=pa_proplist_iterate(l->proplist, &prop_state))) {
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
    Py_DecRef(channel_value);
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
        Py_DecRef(active_port_value);
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
    Py_DecRef(key);
    Py_DecRef(volume_value);
}                   

// See above.  This callback is pretty much identical to the previous
static void m_pa_sourcelist_cb(pa_context *c, 
                               const pa_source_info *l, 
                               int eol, 
                               void *userdata) 
{
    DeepinPulseAudioObject *self = userdata;
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
                                                                                
    if (eol > 0) {                                                              
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
    Py_DecRef(channel_value);   
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
        Py_DecRef(active_port_value);                                       
    } else {
        PyDict_SetItem(self->input_active_ports, key, Py_None);
    }
    // volume list
    volume_value = PyList_New(0);
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
    Py_DecRef(key);
    Py_DecRef(volume_value);
}                   
