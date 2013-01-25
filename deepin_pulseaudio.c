/* 
 * Copyright (C) 2013 Deepin, Inc.
 *               2013 Zhai Xiang
 *
 * Author:     Zhai Xiang <zhaixiang@linuxdeepin.com>
 * Maintainer: Zhai Xiang <zhaixiang@linuxdeepin.com>
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

#define DEVICE_NUM 16

#define INT(v) PyInt_FromLong(v)
#define DOUBLE(v) PyFloat_FromDouble(v)
#define STRING(v) PyString_FromString(v)
#define ERROR(v) PyErr_SetString(PyExc_TypeError, v)

/* Safe XDECREF for object states that handles nested deallocations */
#define ZAP(v) do {\
    PyObject *tmp = (PyObject *)(v); \
    (v) = NULL; \
    Py_XDECREF(tmp); \
} while (0)

// Field list is here: http://0pointer.de/lennart/projects/pulseaudio/doxygen/structpa__sink__info.html
typedef struct pa_devicelist {                                                  
    uint8_t initialized;                                                        
    char name[512];                                                             
    uint32_t index;                                                             
    char description[256];                                                      
} pa_devicelist_t;          

typedef struct {
    PyObject_HEAD
    PyObject *dict; /* Python attributes dictionary */
    pa_devicelist_t *pa_input_devicelist;
    pa_devicelist_t *pa_output_devicelist;
    PyObject *input_portlist;
    PyObject *output_portlist;
} DeepinPulseAudioObject;

static PyObject *m_deepin_pulseaudio_object_constants = NULL;
static PyTypeObject *m_DeepinPulseAudio_Type = NULL;

static DeepinPulseAudioObject *m_init_deepin_pulseaudio_object();
static DeepinPulseAudioObject *m_new(PyObject *self, PyObject *args);

static PyMethodDef deepin_pulseaudio_methods[] = 
{
    {"new", m_new, METH_NOARGS, "Deepin PulseAudio Construction"}, 
    {NULL, NULL, 0, NULL}
};

static PyObject *m_delete(DeepinPulseAudioObject *self);
static void m_pa_state_cb(pa_context *c, void *userdata);                                
static void m_pa_sinklist_cb(pa_context *c, const pa_sink_info *l, int eol, void *userdata);
static void m_pa_sourcelist_cb(pa_context *c, const pa_source_info *l, int eol, void *userdata);
static PyObject *m_get_devicelist(DeepinPulseAudioObject *self);
static PyObject *m_get_output_portlist(DeepinPulseAudioObject *self);
static PyObject *m_get_input_portlist(DeepinPulseAudioObject *self);
static PyObject *m_get_output_devicelist(DeepinPulseAudioObject *self);
static PyObject *m_get_input_devicelist(DeepinPulseAudioObject *self);

static PyMethodDef deepin_pulseaudio_object_methods[] = 
{
    {"delete", m_delete, METH_NOARGS, "Deepin PulseAudio Destruction"}, 
    {"get_devicelist", m_get_devicelist, METH_NOARGS, "Get Device List"}, 
    {"get_output_portlist", 
     m_get_output_portlist, 
     METH_NOARGS, 
     "Get output port list"}, 
    {"get_input_portlist",                                                     
     m_get_input_portlist,                                                     
     METH_NOARGS,                                                               
     "Get input port list"},    
    {"get_output_devicelist",                                                      
     m_get_output_devicelist,                                                      
     METH_NOARGS,                                                               
     "Get output device list"},  
    {"get_input_devicelist",                                                   
     m_get_input_devicelist,                                                   
     METH_NOARGS,                                                               
     "Get input device list"},      
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
    self->pa_input_devicelist = NULL;
    self->pa_output_devicelist = NULL;
    self->input_portlist = NULL;
    self->output_portlist = NULL;

    return self;
}

static DeepinPulseAudioObject *m_new(PyObject *dummy, PyObject *args) 
{
    DeepinPulseAudioObject *self = NULL;
 
    self = m_init_deepin_pulseaudio_object();
    if (!self)
        return NULL;

    self->pa_input_devicelist = malloc(sizeof(pa_devicelist_t) * DEVICE_NUM);
    if (!self->pa_input_devicelist) {
        ERROR("allocation error");
        m_delete(self);
        return NULL;
    }

    self->pa_output_devicelist = malloc(sizeof(pa_devicelist_t) * DEVICE_NUM);
    if (!self->pa_output_devicelist) {
        ERROR("allocation error");
        m_delete(self);
        return NULL;
    }
    
    return self;
}

static PyObject *m_delete(DeepinPulseAudioObject *self) 
{
    if (self->pa_input_devicelist) {
        free(self->pa_input_devicelist);
        self->pa_input_devicelist = NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *m_get_devicelist(DeepinPulseAudioObject *self) 
{
    // Define our pulse audio loop and connection variables                     
    pa_mainloop *pa_ml;                                                         
    pa_mainloop_api *pa_mlapi;                                                  
    pa_operation *pa_op;                                                        
    pa_context *pa_ctx;                                                         
                                                                                
    // We'll need these state variables to keep track of our requests           
    int state = 0;                                                              
    int pa_ready = 0;                                                           
                                                                                
    // Initialize our device lists                                              
    memset(self->pa_input_devicelist, 0, sizeof(pa_devicelist_t) * DEVICE_NUM);                             
    memset(self->pa_output_devicelist, 0, sizeof(pa_devicelist_t) * DEVICE_NUM);                            
                                                                                
    // Create a mainloop API and connection to the default server               
    pa_ml = pa_mainloop_new();                                                  
    pa_mlapi = pa_mainloop_get_api(pa_ml);                                      
    pa_ctx = pa_context_new(pa_mlapi, "deepin");                                  
                                                                                
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
                // complete our pa_output_devicelist is filled out, and we move 
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

static PyObject *m_get_output_portlist(DeepinPulseAudioObject *self) 
{
    Py_INCREF(self->output_portlist);
    return self->output_portlist;
}

static PyObject *m_get_input_portlist(DeepinPulseAudioObject *self)               
{                                                                                  
    Py_INCREF(self->input_portlist);                                              
    return self->input_portlist;                                                  
}    

static PyObject *m_get_output_devicelist(DeepinPulseAudioObject *self) 
{
    PyObject *list = PyList_New(0);
    int ctr = 0;

    for (ctr = 0; ctr < 16; ctr++) {                                            
        if (!self->pa_output_devicelist[ctr].initialized) {                          
            break;                                                              
        }
        PyList_Append(list, 
                      Py_BuildValue("(ssd)", 
                                    self->pa_output_devicelist[ctr].description, 
                                    self->pa_output_devicelist[ctr].name, 
                                    self->pa_output_devicelist[ctr].index));
        /*
        printf("=======[ Output Device #%d ]=======\n", ctr + 1);                 
        printf("Description: %s\n", self->pa_output_devicelist[ctr].description);        
        printf("Name: %s\n", self->pa_output_devicelist[ctr].name);                   
        printf("Index: %d\n", self->pa_output_devicelist[ctr].index);                 
        printf("\n");              
        */
    }   

    return list;
}

static PyObject *m_get_input_devicelist(DeepinPulseAudioObject *self)          
{                                                                               
    PyObject *list = PyList_New(0);                                             
    int ctr = 0;                                                                 
                                                                                
    for (ctr = 0; ctr < DEVICE_NUM; ctr++) {                                             
        if (!self->pa_input_devicelist[ctr].initialized) {                           
            break;                                                              
        }                                                                       
        PyList_Append(list,                                                     
                      Py_BuildValue("(ssd)",                                    
                                    self->pa_input_devicelist[ctr].description,
                                    self->pa_input_devicelist[ctr].name,       
                                    self->pa_input_devicelist[ctr].index));    
        /*                                                                      
        printf("=======[ Output Device #%d ]=======\n", ctr + 1);                 
        printf("Description: %s\n", self->pa_input_devicelist[ctr].description);        
        printf("Name: %s\n", self->pa_input_devicelist[ctr].name);                   
        printf("Index: %d\n", self->pa_input_devicelist[ctr].index);                 
        printf("\n");                                                           
        */                                                                      
    }                                                                           
                                                                                
    return list;                                                                
}        

// This callback gets called when our context changes state.  We really only    
// care about when it's ready or if it has failed                               
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

// pa_mainloop will call this function when it's ready to tell us about a sink. 
// Since we're not threading, there's no need for mutexes on the devicelist     
// structure                                                                    
static void m_pa_sinklist_cb(pa_context *c, 
                             const pa_sink_info *l, 
                             int eol, 
                             void *userdata) 
{
    DeepinPulseAudioObject *self = userdata;
    pa_devicelist_t *pa_devicelist = self->pa_output_devicelist;                                  
    pa_sink_port_info **ports  = NULL;                                          
    pa_sink_port_info *port = NULL;                                             
    int ctr = 0;                                                                
    int i = 0;                                                                  
                                                                                
    // If eol is set to a positive number, you're at the end of the list        
    if (eol > 0) {                                                              
        return;                                                                 
    }

    self->output_portlist = PyList_New(0);
    if (!self->output_portlist) {
        printf("PyList_New error");
        return;
    }
                                                                                
    // We know we've allocated 16 slots to hold devices.  Loop through our      
    // structure and find the first one that's "uninitialized."  Copy the       
    // contents into it and we're done.  If we receive more than 16 devices,    
    // they're going to get dropped.  You could make this dynamically allocate  
    // space for the device list, but this is a simple example.                 
    for (ctr = 0; ctr < DEVICE_NUM; ctr++) {                                            
        if (! pa_devicelist[ctr].initialized) {                                 
            strncpy(pa_devicelist[ctr].name, l->name, 511);                     
            strncpy(pa_devicelist[ctr].description, l->description, 255);       
            ports = l->ports;   
            for (i = 0; i < l->n_ports; i++) {                                  
                port = ports[i];   
                PyList_Append(self->output_portlist, 
                              Py_BuildValue("(ss)", 
                                            port->description, 
                                            port->name));
                //printf("DEBUG sink %s %s\n", port->name, port->description);         
            }                                                                   
            pa_devicelist[ctr].index = l->index;                                
            pa_devicelist[ctr].initialized = 1;                                 
            break;                                                              
        }                                                                       
    }                                                                           
}                   

// See above.  This callback is pretty much identical to the previous
static void m_pa_sourcelist_cb(pa_context *c, 
                               const pa_source_info *l, 
                               int eol, 
                               void *userdata) 
{
    DeepinPulseAudioObject *self = userdata;
    pa_devicelist_t *pa_devicelist = self->pa_input_devicelist;                                  
    pa_source_port_info **ports = NULL;                                         
    pa_source_port_info *port = NULL;                                           
    int ctr = 0;                                                                
    int i = 0;                                                                  
                                                                                
    if (eol > 0) {                                                              
        return;                                                                 
    }

    self->input_portlist = PyList_New(0);
    if (!self->input_portlist) {
        printf("PyList_New error");
        return;
    }
                                                                                
    for (ctr = 0; ctr < DEVICE_NUM; ctr++) {                                            
        if (!pa_devicelist[ctr].initialized) {                                 
            strncpy(pa_devicelist[ctr].name, l->name, 511);                     
            strncpy(pa_devicelist[ctr].description, l->description, 255);       
            ports = l->ports;                                                   
            for (i = 0; i < l->n_ports; i++) {                                  
                port = ports[i];                                                
                PyList_Append(self->input_portlist,                            
                              Py_BuildValue("(ss)",                             
                                            port->description,                  
                                            port->name));    
                //printf("DEBUG %s %s\n", port->name, port->description);         
            }                                                                   
            pa_devicelist[ctr].index = l->index;                                
            pa_devicelist[ctr].initialized = 1;                                 
            break;                                                              
        }                                                                       
    }                                                                           
}                   
