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
    // TODO: Thank Tanu https://bugs.freedesktop.org/show_bug.cgi?id=61328
    // it can not use pa_threaded_mainloop for PyGtk GMainLoop
    // PyEval_CallFunction() is called from the thread created by pa_threaded_mainloop. 
    // The GMainLoop runs in a different thread, so if PyEval_CallFunction() expects 
    // to be run in the GMainLoop thread, things will explode!
    pa_context *pa_ctx;
    pa_mainloop_api *pa_mlapi;
    PyObject *get_cards_cb; /* callback */
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
} DeepinPulseAudioSignalObject;

static PyObject *m_deepin_pulseaudio_signal_object_constants = NULL;
static PyTypeObject *m_DeepinPulseAudioSignal_Type = NULL;

static DeepinPulseAudioSignalObject *m_init_deepin_pulseaudio_signal_object();
static void m_pa_context_subscribe_cb(pa_context *c,                           
                                      pa_subscription_event_type_t t,          
                                      uint32_t idx,                            
                                      void *userdata);
static void m_context_state_cb(pa_context *c, void *userdata);
static DeepinPulseAudioSignalObject *m_new(PyObject *self, PyObject *args);

static PyMethodDef deepin_pulseaudio_signal_methods[] = 
{
    {"new", m_new, METH_NOARGS, "Deepin PulseAudio Signal Construction"}, 
    {NULL, NULL, 0, NULL}
};

static PyObject *m_delete(DeepinPulseAudioSignalObject *self);
static PyObject *m_connect_to_pulse(DeepinPulseAudioSignalObject *self);
static PyObject *m_connect(DeepinPulseAudioSignalObject *self, PyObject *args);

static PyMethodDef deepin_pulseaudio_signal_object_methods[] = 
{
    {"delete", m_delete, METH_NOARGS, "Deepin PulseAudio destruction"}, 
    {"connect_to_pulse", m_connect_to_pulse, METH_NOARGS, "Connect to PulseAudio"}, 
    {"connect", m_connect, METH_VARARGS, "Connect signal callback"}, 
    {NULL, NULL, 0, NULL}
};

static void m_deepin_pulseaudio_signal_dealloc(DeepinPulseAudioSignalObject *self) 
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

static PyObject *m_deepin_pulseaudio_signal_getattr(DeepinPulseAudioSignalObject *dpo, 
                                             char *name) 
{
    return m_getattr((PyObject *)dpo, 
                     name, 
                     dpo->dict, 
                     m_deepin_pulseaudio_signal_object_constants, 
                     deepin_pulseaudio_signal_object_methods);
}

static PyObject *m_deepin_pulseaudio_signal_setattr(DeepinPulseAudioSignalObject *dpo, 
                                             char *name, 
                                             PyObject *v) 
{
    return m_setattr(&dpo->dict, name, v);
}

static PyObject *m_deepin_pulseaudio_signal_traverse(DeepinPulseAudioSignalObject *self, 
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

static PyObject *m_deepin_pulseaudio_signal_clear(DeepinPulseAudioSignalObject *self) 
{
    ZAP(self->dict);
    return 0;
}

static PyTypeObject DeepinPulseAudioSignal_Type = {
    PyObject_HEAD_INIT(NULL)
    0, 
    "deepin_pulseaudio_signal.new", 
    sizeof(DeepinPulseAudioSignalObject), 
    0, 
    (destructor)m_deepin_pulseaudio_signal_dealloc,
    0, 
    (getattrfunc)m_deepin_pulseaudio_signal_getattr, 
    (setattrfunc)m_deepin_pulseaudio_signal_setattr, 
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
    (traverseproc)m_deepin_pulseaudio_signal_traverse, 
    (inquiry)m_deepin_pulseaudio_signal_clear
};

PyMODINIT_FUNC initdeepin_pulseaudio_signal() 
{
    PyObject *m = NULL;
             
    m_DeepinPulseAudioSignal_Type = &DeepinPulseAudioSignal_Type;
    DeepinPulseAudioSignal_Type.ob_type = &PyType_Type;

    m = Py_InitModule("deepin_pulseaudio_signal", deepin_pulseaudio_signal_methods);
    if (!m)
        return;
    
    m_deepin_pulseaudio_signal_object_constants = PyDict_New();
}

static DeepinPulseAudioSignalObject *m_init_deepin_pulseaudio_signal_object() 
{
    DeepinPulseAudioSignalObject *self = NULL;

    self = (DeepinPulseAudioSignalObject *) PyObject_GC_New(DeepinPulseAudioSignalObject, 
                                                      m_DeepinPulseAudioSignal_Type);
    if (!self)
        return NULL;
    
    PyObject_GC_Track(self);

    self->dict = NULL;

    self->pa_ml = NULL;
    self->pa_ctx = NULL;
    self->pa_mlapi = NULL;
   
    self->get_cards_cb = NULL;
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

static void m_pa_sink_new_cb(pa_context *c,
                             const pa_sink_info *info,
                             int eol,
                             void *userdata)
{
    if (!c || !info || eol > 0 || !userdata) 
        return;

    DeepinPulseAudioSignalObject *self = (DeepinPulseAudioSignalObject *) userdata;

    if (self->sink_new_cb) 
        PyEval_CallFunction(self->sink_new_cb, "(Oi)", self, info->index);
}

static void m_pa_sink_changed_cb(pa_context *c, 
                                 const pa_sink_info *info, 
                                 int eol, 
                                 void *userdata) 
{
    if (!c || !info || eol > 0 || !userdata) {
        return;
    }

    DeepinPulseAudioSignalObject *self = (DeepinPulseAudioSignalObject *) userdata;

    if (self->sink_changed_cb) {
        PyEval_CallFunction(self->sink_changed_cb, "(Oi)", self, info->index);
    }
}

static void m_pa_source_new_cb(pa_context *c,                               
                                 const pa_source_info *info,                    
                                 int eol,                                       
                                 void *userdata)                                
{                                                                               
    if (!c || !info || eol > 0 || !userdata)                                    
        return;                                                                 

    DeepinPulseAudioSignalObject *self = (DeepinPulseAudioSignalObject *) userdata;         

    if (self->source_new_cb)                                      
        PyEval_CallFunction(self->source_new_cb, "(Oi)", self, info->index);
}

static void m_pa_source_changed_cb(pa_context *c,
                                 const pa_source_info *info,
                                 int eol,
                                 void *userdata)
{
    if (!c || !info || eol > 0 || !userdata) 
        return;

    DeepinPulseAudioSignalObject *self = (DeepinPulseAudioSignalObject *) userdata;

    if (self->source_changed_cb) 
        PyEval_CallFunction(self->source_changed_cb, "(Oi)", self, info->index);
}

static void m_pa_sink_input_new_cb(pa_context *c,
                                   const pa_sink_input_info *info,
                                   int eol,
                                   void *userdata)
{
    if (!c || !info || eol > 0 || !userdata) 
        return;

    DeepinPulseAudioSignalObject *self = (DeepinPulseAudioSignalObject *) userdata;

    if (self->sink_input_new_cb) 
        PyEval_CallFunction(self->sink_input_new_cb, "(Oi)", self, info->index);
}

static void m_pa_sink_input_changed_cb(pa_context *c,                               
                                       const pa_sink_input_info *info,                
                                       int eol,                                       
                                       void *userdata)                                
{                                                                                   
    if (!c || !info || eol > 0 || !userdata)                                        
        return;                                                                     
                                                                                    
    DeepinPulseAudioSignalObject *self = (DeepinPulseAudioSignalObject *) userdata;             

    if (self->sink_input_changed_cb)                                                          
        PyEval_CallFunction(self->sink_input_changed_cb, "(Oi)", self, info->index);               
}

static void m_pa_source_output_new_cb(pa_context *c,
                                        const pa_source_output_info *info,
                                        int eol,
                                        void *userdata)
{
    if (!c || !info || eol > 0 || !userdata) 
        return;

    DeepinPulseAudioSignalObject *self = (DeepinPulseAudioSignalObject *) userdata;

    if (self->source_output_new_cb) 
        PyEval_CallFunction(self->source_output_new_cb, "(Oi)", self, info->index);
}

static void m_pa_source_output_changed_cb(pa_context *c,                                
                                          const pa_source_output_info *info,          
                                          int eol,                                    
                                          void *userdata)                             
{                                                                                   
    if (!c || !info || eol > 0 || !userdata)                                        
        return;                                                                     
                                                                                    
    DeepinPulseAudioSignalObject *self = (DeepinPulseAudioSignalObject *) userdata;             

    if (self->source_output_changed_cb)                                                 
        PyEval_CallFunction(self->source_output_changed_cb, "(Oi)", self, info->index);    
}

static void m_pa_server_new_cb(pa_context *c,
                                 const pa_server_info *info,
                                 void *userdata)
{
    if (!c || !info || !userdata) 
        return;

    DeepinPulseAudioSignalObject *self = (DeepinPulseAudioSignalObject *) userdata;

    if (self->server_new_cb) 
        PyEval_CallFunction(self->server_new_cb, "O", self);
}

static void m_pa_server_changed_cb(pa_context *c,                                   
                                 const pa_server_info *info,                    
                                 void *userdata)                                    
{                                                                                   
    if (!c || !info || !userdata)                                                   
        return;                                                                     
                                                                                    
    DeepinPulseAudioSignalObject *self = (DeepinPulseAudioSignalObject *) userdata;             

    if (self->server_changed_cb)                                                        
        PyEval_CallFunction(self->server_changed_cb, "O", self);                         
}

static void m_pa_card_new_cb(pa_context *c,
                               const pa_card_info *info,
                               int eol,
                               void *userdata)
{
    if (!c || !info || eol > 0 || !userdata) 
        return;

    DeepinPulseAudioSignalObject *self = (DeepinPulseAudioSignalObject *) userdata;

    if (self->card_new_cb) 
        PyEval_CallFunction(self->card_new_cb, "(Oi)", self, info->index);
}

static void m_pa_card_changed_cb(pa_context *c,                                         
                               const pa_card_info *info,                            
                               int eol,                                             
                               void *userdata)                                      
{                                                                                   
    if (!c || !info || eol > 0 || !userdata)                                        
        return;                                                                     
                                                                                    
    DeepinPulseAudioSignalObject *self = (DeepinPulseAudioSignalObject *) userdata;             

    if (self->card_changed_cb)                                                          
        PyEval_CallFunction(self->card_changed_cb, "(Oi)", self, info->index);             
}

static void m_pa_removed_event_cb(pa_context *c,
                                  PyObject *callback,
                                  uint32_t idx,
                                  DeepinPulseAudioSignalObject *this)
{
    DeepinPulseAudioSignalObject *self = (DeepinPulseAudioSignalObject *) this;
    
    if (callback) 
        PyEval_CallFunction(callback, "(Oi)", self, idx);
}

static void m_pa_context_subscribe_cb(pa_context *c,                           
                                      pa_subscription_event_type_t t,          
                                      uint32_t idx,                            
                                      void *userdata)                          
{                                                                               
    if (!c || !userdata) 
        return;

    DeepinPulseAudioSignalObject *self = (DeepinPulseAudioSignalObject *) userdata;

    switch (t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) {                          
        case PA_SUBSCRIPTION_EVENT_SINK:
            if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_NEW) {
                pa_context_get_sink_info_by_index(c, idx, m_pa_sink_new_cb, self);
            } else if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_CHANGE) {
                pa_context_get_sink_info_by_index(c, idx, m_pa_sink_changed_cb, self);
            }
            break;
        case PA_SUBSCRIPTION_EVENT_SOURCE:
            if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_NEW) {
                pa_context_get_source_info_by_index(c, idx, m_pa_source_new_cb, self);
            } else if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_CHANGE) {
                pa_context_get_source_info_by_index(c, idx, m_pa_source_changed_cb, self);
            }
            break;
        case PA_SUBSCRIPTION_EVENT_SINK_INPUT:
            if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_NEW) {
                pa_context_get_sink_input_info(c, idx, m_pa_sink_input_new_cb, self);
            } else if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_CHANGE) {
                pa_context_get_sink_input_info(c, idx, m_pa_sink_input_changed_cb, self);
            }
            break;
        case PA_SUBSCRIPTION_EVENT_SOURCE_OUTPUT:
            if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_NEW) {
                pa_context_get_source_output_info(c, idx, m_pa_source_output_new_cb, self);
            } else if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_CHANGE) {
                pa_context_get_source_output_info(c, idx, m_pa_source_output_changed_cb, self);
            }
            break;
        case PA_SUBSCRIPTION_EVENT_CLIENT:                                      
            break;                                                              
        case PA_SUBSCRIPTION_EVENT_SERVER:
            if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_NEW) {
                pa_context_get_server_info(c, m_pa_server_new_cb, self);
            } else if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_CHANGE) {
                pa_context_get_server_info(c, m_pa_server_changed_cb, self);
            }
            break;
        case PA_SUBSCRIPTION_EVENT_CARD:
            if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_NEW) {
                pa_context_get_card_info_by_index(c, idx, m_pa_card_new_cb, self);
            } else if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_CHANGE) {
                pa_context_get_card_info_by_index(c, idx, m_pa_card_changed_cb, self);
            }    
            break;
    }
}

static void m_context_state_cb(pa_context *c, void *userdata) 
{                
    if (!c || !userdata) 
        return;

    DeepinPulseAudioSignalObject *self = (DeepinPulseAudioSignalObject *) userdata;                         
    pa_operation *pa_op = NULL;

    switch (pa_context_get_state(c)) {                                          
        case PA_CONTEXT_UNCONNECTED:                                            
        case PA_CONTEXT_CONNECTING:                                             
        case PA_CONTEXT_AUTHORIZING:                                            
        case PA_CONTEXT_SETTING_NAME:                                           
            break;                                                              
                                                                                
        case PA_CONTEXT_READY:                                                
            pa_context_set_subscribe_callback(c, m_pa_context_subscribe_cb, self);

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
            break;
                                                                                
        case PA_CONTEXT_FAILED:                                                 
            pa_context_unref(self->pa_ctx);                                          
            self->pa_ctx = NULL;                                                     
                                                                                
            printf("Connection failed, attempting reconnect\n");          
            g_timeout_add_seconds(13, m_connect_to_pulse, self);               
            return;                                                             
                                                                                
        case PA_CONTEXT_TERMINATED:                                             
        default:        
            printf("pa_context terminated\n");            
            return;                                                             
    }
}

static PyObject *m_connect_to_pulse(DeepinPulseAudioSignalObject *self) 
{
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

static DeepinPulseAudioSignalObject *m_new(PyObject *dummy, PyObject *args) 
{
    DeepinPulseAudioSignalObject *self = NULL;

    self = m_init_deepin_pulseaudio_signal_object();
    if (!self)
        return NULL;

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

static PyObject *m_delete(DeepinPulseAudioSignalObject *self) 
{
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

static PyObject *m_connect(DeepinPulseAudioSignalObject *self, PyObject *args)         
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
    
    if (strcmp(signal, "get-cards") == 0) {
        Py_XINCREF(callback);
        Py_XDECREF(self->get_cards_cb);
        self->get_cards_cb = callback;
    }

    if (strcmp(signal, "sink-new") == 0) {                                         
        Py_XINCREF(callback);                                                       
        Py_XDECREF(self->sink_new_cb);                                           
        self->sink_new_cb = callback;
    }

    if (strcmp(signal, "sink-changed") == 0) {                                         
        Py_XINCREF(callback);                                                       
        Py_XDECREF(self->sink_changed_cb);                                           
        self->sink_changed_cb = callback;
    }

    if (strcmp(signal, "sink-removed") == 0) {                                         
        Py_XINCREF(callback);                                                       
        Py_XDECREF(self->sink_removed_cb);                                           
        self->sink_removed_cb = callback;
    }

    if (strcmp(signal, "source-new") == 0) {                                     
        Py_XINCREF(callback);                                                   
        Py_XDECREF(self->source_new_cb);                                         
        self->source_new_cb = callback;                                          
    }
    
    if (strcmp(signal, "source-changed") == 0) {                                     
        Py_XINCREF(callback);                                                   
        Py_XDECREF(self->source_changed_cb);                                         
        self->source_changed_cb = callback;                                          
    }

    if (strcmp(signal, "source-removed") == 0) {                                     
        Py_XINCREF(callback);                                                   
        Py_XDECREF(self->source_removed_cb);                                         
        self->source_removed_cb = callback;                                          
    }

    if (strcmp(signal, "card-new") == 0) {                                     
        Py_XINCREF(callback);                                                   
        Py_XDECREF(self->card_new_cb);                                         
        self->card_new_cb = callback;                                          
    }                                                                           
                                                                                
    if (strcmp(signal, "card-changed") == 0) {                                     
        Py_XINCREF(callback);                                                   
        Py_XDECREF(self->card_changed_cb);                                         
        self->card_changed_cb = callback;                                          
    }                                                                           
                                                                                
    if (strcmp(signal, "card-removed") == 0) {                                     
        Py_XINCREF(callback);                                                   
        Py_XDECREF(self->card_removed_cb);                                         
        self->card_removed_cb = callback;                                          
    }                                                                           
                                                                                

    if (strcmp(signal, "server-new") == 0) {                                     
        Py_XINCREF(callback);                                                   
        Py_XDECREF(self->server_new_cb);                                         
        self->server_new_cb = callback;                                          
    }                                                                           

    if (strcmp(signal, "server-changed") == 0) {                                     
        Py_XINCREF(callback);                                                   
        Py_XDECREF(self->server_changed_cb);                                         
        self->server_changed_cb = callback;                                          
    }                                                                           

    if (strcmp(signal, "server-removed") == 0) {                                     
        Py_XINCREF(callback);                                                   
        Py_XDECREF(self->server_removed_cb);                                         
        self->server_removed_cb = callback;                                          
    }                                                                           

    if (strcmp(signal, "sink-input-new") == 0) {                                     
        Py_XINCREF(callback);                                                   
        Py_XDECREF(self->sink_input_new_cb);                                         
        self->sink_input_new_cb = callback;                                          
    }                                                                           

    if (strcmp(signal, "sink-input-changed") == 0) {                                     
        Py_XINCREF(callback);                                                   
        Py_XDECREF(self->sink_input_changed_cb);                                         
        self->sink_input_changed_cb = callback;                                          
    }                                                                           

    if (strcmp(signal, "sink-input-removed") == 0) {                                     
        Py_XINCREF(callback);                                                   
        Py_XDECREF(self->sink_input_removed_cb);                                         
        self->sink_input_removed_cb = callback;                                          
    }                                                                           

    if (strcmp(signal, "source-output-new") == 0) {                                     
        Py_XINCREF(callback);                                                   
        Py_XDECREF(self->source_output_new_cb);                                         
        self->source_output_new_cb = callback;                                          
    }                                                                           

    if (strcmp(signal, "source-output-changed") == 0) {                                     
        Py_XINCREF(callback);                                                   
        Py_XDECREF(self->source_output_changed_cb);                                         
        self->source_output_changed_cb = callback;                                          
    }                                                                           

    if (strcmp(signal, "source-output-removed") == 0) {                                     
        Py_XINCREF(callback);                                                   
        Py_XDECREF(self->source_output_removed_cb);                                         
        self->source_output_removed_cb = callback;                                          
    }                                                                           

    Py_INCREF(Py_True);                                                         
    return Py_True;                                                             
}
