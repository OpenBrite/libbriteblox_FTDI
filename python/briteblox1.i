/* File: briteblox1.i */

%module(docstring="Python interface to libbriteblox1") briteblox1
%feature("autodoc","1");

#ifdef DOXYGEN
%include "briteblox1_doc.i"
#endif

%{
#include "Python.h"

PyObject* convertString( const char *v, Py_ssize_t len )
{
#if PY_MAJOR_VERSION >= 3
  return PyBytes_FromStringAndSize(v, len);
#else
  return PyString_FromStringAndSize(v, len);
#endif
}
%}

%include <typemaps.i>
%include <cstring.i>

%typemap(in) unsigned char* = char*;

%immutable briteblox_version_info::version_str;
%immutable briteblox_version_info::snapshot_str;

%rename("%(strip:[briteblox_])s") "";

%newobject briteblox_new;
%typemap(newfree) struct briteblox_context *briteblox "briteblox_free($1);";
%delobject briteblox_free;

%define briteblox_usb_find_all_docstring
"usb_find_all(context, vendor, product) -> (return_code, devlist)"
%enddef
%feature("autodoc", briteblox_usb_find_all_docstring) briteblox_usb_find_all;
%typemap(in,numinputs=0) SWIGTYPE** OUTPUT ($*ltype temp) %{ $1 = &temp; %}
%typemap(argout) SWIGTYPE** OUTPUT %{ $result = SWIG_Python_AppendOutput($result, SWIG_NewPointerObj((void*)*$1,$*descriptor,0)); %}
%apply SWIGTYPE** OUTPUT { struct briteblox_device_list **devlist };
    int briteblox_usb_find_all(struct briteblox_context *briteblox, struct briteblox_device_list **devlist,
                          int vendor, int product);
%clear struct briteblox_device_list **devlist;

%define briteblox_usb_get_strings_docstring
"usb_get_strings(context, device) -> (return_code, manufacturer, description, serial)"
%enddef
%feature("autodoc", briteblox_usb_get_strings_docstring) briteblox_usb_get_strings;
%apply char *OUTPUT { char * manufacturer, char * description, char * serial };
%cstring_bounded_output( char * manufacturer, 256 );
%cstring_bounded_output( char * description, 256 );
%cstring_bounded_output( char * serial, 256 );
%typemap(default,noblock=1) int mnf_len, int desc_len, int serial_len { $1 = 256; }
    int briteblox_usb_get_strings(struct briteblox_context *briteblox, struct libusb_device *dev,
                             char * manufacturer, int mnf_len,
                             char * description, int desc_len,
                             char * serial, int serial_len);
%clear char * manufacturer, char * description, char * serial;
%clear int mnf_len, int desc_len, int serial_len;

%define briteblox_read_data_docstring
"read_data(context) -> (return_code, buf)"
%enddef
%feature("autodoc", briteblox_read_data_docstring) briteblox_read_data;
%typemap(in,numinputs=1) (unsigned char *buf, int size) %{ $2 = PyInt_AsLong($input);$1 = (unsigned char*)malloc($2*sizeof(char)); %}
%typemap(argout) (unsigned char *buf, int size) %{ if(result<0) $2=0; $result = SWIG_Python_AppendOutput($result, convertString((char*)$1, $2)); free($1); %}
    int briteblox_read_data(struct briteblox_context *briteblox, unsigned char *buf, int size);
%clear (unsigned char *buf, int size);

%apply int *OUTPUT { unsigned int *chunksize };
    int briteblox_read_data_get_chunksize(struct briteblox_context *briteblox, unsigned int *chunksize);
    int briteblox_write_data_get_chunksize(struct briteblox_context *briteblox, unsigned int *chunksize);
%clear unsigned int *chunksize;

%define briteblox_read_pins_docstring
"read_pins(context) -> (return_code, pins)"
%enddef
%feature("autodoc", briteblox_read_pins_docstring) briteblox_read_pins;
%typemap(in,numinputs=0) unsigned char *pins ($*ltype temp) %{ $1 = &temp; %}
%typemap(argout) (unsigned char *pins) %{ $result = SWIG_Python_AppendOutput($result, convertString((char*)$1, 1)); %}
    int briteblox_read_pins(struct briteblox_context *briteblox, unsigned char *pins);
%clear unsigned char *pins;

%typemap(in,numinputs=0) unsigned char *latency ($*ltype temp) %{ $1 = &temp; %}
%typemap(argout) (unsigned char *latency) %{ $result = SWIG_Python_AppendOutput($result, convertString((char*)$1, 1)); %}
    int briteblox_get_latency_timer(struct briteblox_context *briteblox, unsigned char *latency);
%clear unsigned char *latency;

%apply short *OUTPUT { unsigned short *status };
    int briteblox_poll_modem_status(struct briteblox_context *briteblox, unsigned short *status);
%clear unsigned short *status;

%apply int *OUTPUT { int* value };
    int briteblox_get_eeprom_value(struct briteblox_context *briteblox, enum briteblox_eeprom_value value_name, int* value);
%clear int* value;

%typemap(in,numinputs=1) (unsigned char *buf, int size) %{ $2 = PyInt_AsLong($input);$1 = (unsigned char*)malloc($2*sizeof(char)); %}
%typemap(argout) (unsigned char *buf, int size) %{ if(result<0) $2=0; $result = SWIG_Python_AppendOutput($result, convertString((char*)$1, $2)); free($1); %}
    int briteblox_get_eeprom_buf(struct briteblox_context *briteblox, unsigned char * buf, int size);
%clear (unsigned char *buf, int size);

%define briteblox_read_eeprom_location_docstring
"read_eeprom_location(context, eeprom_addr) -> (return_code, eeprom_val)"
%enddef
%feature("autodoc", briteblox_read_eeprom_location_docstring) briteblox_read_eeprom_location;
%apply short *OUTPUT { unsigned short *eeprom_val };
    int briteblox_read_eeprom_location (struct briteblox_context *briteblox, int eeprom_addr, unsigned short *eeprom_val);
%clear unsigned short *eeprom_val;

%define briteblox_read_eeprom_docstring
"read_eeprom(context) -> (return_code, eeprom)"
%enddef
%feature("autodoc", briteblox_read_eeprom_docstring) briteblox_read_eeprom;

%define briteblox_read_chipid_docstring
"briteblox_read_chipid(context) -> (return_code, chipid)"
%enddef
%feature("autodoc", briteblox_read_chipid_docstring) briteblox_read_chipid;
%apply int *OUTPUT { unsigned int *chipid };
    int briteblox_read_chipid(struct briteblox_context *briteblox, unsigned int *chipid);
%clear unsigned int *chipid;

%include briteblox.h
%{
#include <briteblox.h>
%}

%include briteblox_i.h
%{
#include <briteblox_i.h>
%}

%pythoncode %{
__version__ = get_library_version().version_str
%}
