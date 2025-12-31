#ifndef Py_INTERNAL_CALL_H
#define Py_INTERNAL_CALL_H
#ifdef __cplusplus
extern "C" {
#endif

#ifndef Py_BUILD_CORE
#  error "this header requires Py_BUILD_CORE define"
#endif

#include <stdbool.h>
#include <string.h>

#include "pycore_frame.h"         // _PyInterpreterFrame
#include "pycore_pystate.h"       // _PyThreadState_GET()
#include "pycore_global_strings.h" // _Py_ID()
#include "pycore_pyerrors.h"      // _PyErr_Clear()
#include "pycore_unicodeobject.h" // _PyUnicode_Ready()
#include "methodobject.h"         // PyCFunctionObject
#include "descrobject.h"          // PyMethodDescr_Check
#if defined(Py_BUILD_CORE) && !defined(Py_BUILD_CORE_MODULE)
#  include "pydtrace.h"             // PyDTrace_CALL_ENTRY*
#endif

#ifndef PyMethodDescr_Check
#  define PyMethodDescr_Check(op) Py_IS_TYPE((op), &PyMethodDescr_Type)
#endif

PyAPI_FUNC(PyObject *) _PyObject_Call_Prepend(
    PyThreadState *tstate,
    PyObject *callable,
    PyObject *obj,
    PyObject *args,
    PyObject *kwargs);

PyAPI_FUNC(PyObject *) _PyObject_FastCallDictTstate(
    PyThreadState *tstate,
    PyObject *callable,
    PyObject *const *args,
    size_t nargsf,
    PyObject *kwargs);

PyAPI_FUNC(PyObject *) _PyObject_Call(
    PyThreadState *tstate,
    PyObject *callable,
    PyObject *args,
    PyObject *kwargs);

extern PyObject * _PyObject_CallMethodFormat(
        PyThreadState *tstate, PyObject *callable, const char *format, ...);


// Static inline variant of public PyVectorcall_Function().
static inline vectorcallfunc
_PyVectorcall_FunctionInline(PyObject *callable)
{
    assert(callable != NULL);

    PyTypeObject *tp = Py_TYPE(callable);
    if (!PyType_HasFeature(tp, Py_TPFLAGS_HAVE_VECTORCALL)) {
        return NULL;
    }
    assert(PyCallable_Check(callable));

    Py_ssize_t offset = tp->tp_vectorcall_offset;
    assert(offset > 0);

    vectorcallfunc ptr;
    memcpy(&ptr, (char *) callable + offset, sizeof(ptr));
    return ptr;
}


static inline const char *
_PyDTrace_UTF8View(PyThreadState *tstate, PyObject *unicode, const char *fallback)
{
    if (!PyUnicode_Check(unicode)) {
        return fallback;
    }

    if (!PyUnicode_IS_READY(unicode)) {
        if (_PyUnicode_Ready(unicode) < 0) {
            _PyErr_Clear(tstate);
            return fallback;
        }
    }

    if (PyUnicode_IS_ASCII(unicode)) {
        return (const char *)PyUnicode_1BYTE_DATA(unicode);
    }

    const char *value = PyUnicode_AsUTF8(unicode);
    if (value == NULL) {
        _PyErr_Clear(tstate);
        return fallback;
    }
    return value;
}

static inline const char *
_PyDTrace_ModuleNameFromObject(PyThreadState *tstate, PyObject *module, const char *fallback)
{
    if (module == NULL) {
        return fallback;
    }

    if (PyUnicode_Check(module)) {
        return _PyDTrace_UTF8View(tstate, module, fallback);
    }

    if (PyModule_Check(module)) {
        PyObject *name = PyModule_GetNameObject(module);
        if (name != NULL) {
            const char *result = _PyDTrace_UTF8View(tstate, name, fallback);
            Py_DECREF(name);
            return result;
        }
        _PyErr_Clear(tstate);
        return fallback;
    }

    PyObject *attr = PyObject_GetAttrString(module, "__module__");
    if (attr != NULL) {
        const char *result = _PyDTrace_UTF8View(tstate, attr, fallback);
        Py_DECREF(attr);
        return result;
    }
    _PyErr_Clear(tstate);

    attr = PyObject_GetAttrString(module, "__name__");
    if (attr != NULL) {
        const char *result = _PyDTrace_UTF8View(tstate, attr, fallback);
        Py_DECREF(attr);
        return result;
    }
    _PyErr_Clear(tstate);

    return fallback;
}

static inline bool
_PyDTrace_IsUnknown(const char *value)
{
    return value != NULL && value[0] == '?' && value[1] == '\0';
}

typedef struct {
    const char *filename;
    const char *funcname;
    const char *modulename;
} _PyDTraceCallMetadata;

static inline bool
_PyDTrace_StringEquals(const char *value, size_t len,
                       const char *literal, size_t literal_len)
{
    return len == literal_len && memcmp(value, literal, literal_len) == 0;
}

#define _PyDTRACE_LITERAL(s) s, (sizeof(s) - 1)

static inline bool
_PyDTrace_IsWhitelistedName(const char *value)
{
    if (value == NULL || _PyDTrace_IsUnknown(value)) {
        return false;
    }

    size_t len = strlen(value);
    switch (len) {
        case 2:
            return _PyDTrace_StringEquals(value, len, _PyDTRACE_LITERAL("io")) ||
                   _PyDTrace_StringEquals(value, len, _PyDTRACE_LITERAL("re")) ||
                   _PyDTrace_StringEquals(value, len, _PyDTRACE_LITERAL("os"));
        case 3:
            return _PyDTrace_StringEquals(value, len, _PyDTRACE_LITERAL("abc")) ||
                   _PyDTrace_StringEquals(value, len, _PyDTRACE_LITERAL("_io")) ||
                   _PyDTrace_StringEquals(value, len, _PyDTRACE_LITERAL("sys"));
        case 4:
            return _PyDTrace_StringEquals(value, len, _PyDTRACE_LITERAL("enum")) ||
                   _PyDTrace_StringEquals(value, len, _PyDTRACE_LITERAL("site")) ||
                   _PyDTrace_StringEquals(value, len, _PyDTRACE_LITERAL("_abc")) ||
                   _PyDTrace_StringEquals(value, len, _PyDTRACE_LITERAL("_imp")) ||
                   _PyDTrace_StringEquals(value, len, _PyDTRACE_LITERAL("_sre"));
        case 5:
            return _PyDTrace_StringEquals(value, len, _PyDTRACE_LITERAL("types")) ||
                   _PyDTrace_StringEquals(value, len, _PyDTRACE_LITERAL("_stat")) ||
                   _PyDTrace_StringEquals(value, len, _PyDTRACE_LITERAL("posix"));
        case 6:
            return _PyDTrace_StringEquals(value, len, _PyDTRACE_LITERAL("codecs"));
        case 7:
            return _PyDTrace_StringEquals(value, len, _PyDTRACE_LITERAL("marshal")) ||
                   _PyDTrace_StringEquals(value, len, _PyDTRACE_LITERAL("copyreg")) ||
                   _PyDTrace_StringEquals(value, len, _PyDTRACE_LITERAL("reprlib")) ||
                   _PyDTrace_StringEquals(value, len, _PyDTRACE_LITERAL("weakref")) ||
                   _PyDTrace_StringEquals(value, len, _PyDTRACE_LITERAL("_codecs")) ||
                   _PyDTrace_StringEquals(value, len, _PyDTRACE_LITERAL("_thread"));
        case 8:
            return _PyDTrace_StringEquals(value, len, _PyDTRACE_LITERAL("builtins"));
        case 9:
            return _PyDTrace_StringEquals(value, len, _PyDTRACE_LITERAL("encodings")) ||
                   _PyDTrace_StringEquals(value, len, _PyDTRACE_LITERAL("zipimport"));
        case 10:
            return _PyDTrace_StringEquals(value, len, _PyDTRACE_LITERAL("re._parser"));
        case 11:
            return _PyDTrace_StringEquals(value, len, _PyDTRACE_LITERAL("collections")) ||
                   _PyDTrace_StringEquals(value, len, _PyDTRACE_LITERAL("genericpath")) ||
                   _PyDTrace_StringEquals(value, len, _PyDTRACE_LITERAL("<frozen os>"));
        case 12:
            return _PyDTrace_StringEquals(value, len, _PyDTRACE_LITERAL("re._compiler")) ||
                   _PyDTrace_StringEquals(value, len, _PyDTRACE_LITERAL("_collections"));
        case 13:
            return _PyDTrace_StringEquals(value, len, _PyDTRACE_LITERAL("re._constants")) ||
                   _PyDTrace_StringEquals(value, len, _PyDTRACE_LITERAL("_sitebuiltins"));
        case 15:
            return _PyDTrace_StringEquals(value, len, _PyDTRACE_LITERAL("collections.abc")) ||
                   _PyDTrace_StringEquals(value, len, _PyDTRACE_LITERAL("encodings.utf_8")) ||
                   _PyDTrace_StringEquals(value, len, _PyDTRACE_LITERAL("_distutils_hack"));
        case 16:
            return _PyDTrace_StringEquals(value, len, _PyDTRACE_LITERAL("_collections_abc")) ||
                   _PyDTrace_StringEquals(value, len, _PyDTRACE_LITERAL("<frozen getpath>"));
        case 17:
            return _PyDTrace_StringEquals(value, len, _PyDTRACE_LITERAL("_frozen_importlib"));
        case 26:
            return _PyDTrace_StringEquals(value, len, _PyDTRACE_LITERAL("_frozen_importlib_external"));
        default:
            return false;
    }
}

#undef _PyDTRACE_LITERAL

static inline bool
_PyDTrace_IsWhitelistedCall(const _PyDTraceCallMetadata *data)
{
    if (_PyDTrace_IsWhitelistedName(data->modulename)) {
        return true;
    }

    if (_PyDTrace_IsWhitelistedName(data->filename)) {
        return true;
    }

    return false;
}

static inline _PyDTraceCallMetadata
_PyDTrace_GetCallMetadata(PyThreadState *tstate, PyObject *callable)
{
#if defined(Py_BUILD_CORE) && !defined(Py_BUILD_CORE_MODULE)
    _PyDTraceCallMetadata data = {"?", "?", "?"};

    if (PyFunction_Check(callable)) {
        PyFunctionObject *func = (PyFunctionObject *)callable;
        PyCodeObject *code = (PyCodeObject *)func->func_code;
        if (code != NULL) {
            data.filename = _PyDTrace_UTF8View(tstate, code->co_filename, data.filename);
            PyObject *qualname = func->func_qualname;
            if (qualname != NULL) {
                data.funcname = _PyDTrace_UTF8View(tstate, qualname, data.funcname);
            }
            else {
                data.funcname = _PyDTrace_UTF8View(tstate, code->co_name, data.funcname);
            }
        }

        PyObject *globals = func->func_globals;
        if (globals != NULL && PyDict_CheckExact(globals)) {
            PyObject *modname = PyDict_GetItemWithError(globals, &_Py_ID(__name__));
            if (modname != NULL) {
                data.modulename = _PyDTrace_UTF8View(tstate, modname, data.modulename);
            }
            else if (_PyErr_Occurred(tstate)) {
                _PyErr_Clear(tstate);
            }
        }
    }
    else if (PyCFunction_Check(callable)) {
        PyCFunctionObject *cfunc = (PyCFunctionObject *)callable;
        if (cfunc->m_ml != NULL && cfunc->m_ml->ml_name != NULL) {
            data.funcname = cfunc->m_ml->ml_name;
        }
        data.modulename = _PyDTrace_ModuleNameFromObject(tstate, cfunc->m_module, data.modulename);
        data.filename = data.modulename;
    }
    else if (PyMethodDescr_Check(callable)) {
        PyMethodDescrObject *descr = (PyMethodDescrObject *)callable;
        if (descr->d_method != NULL && descr->d_method->ml_name != NULL) {
            data.funcname = descr->d_method->ml_name;
        }
        if (descr->d_common.d_type != NULL) {
            data.modulename = _PyDTrace_ModuleNameFromObject(
                tstate, (PyObject *)descr->d_common.d_type, data.modulename);
            data.filename = data.modulename;
        }
    }
    else if (PyType_Check(callable)) {
        PyTypeObject *type = (PyTypeObject *)callable;
        if (type->tp_name != NULL) {
            data.funcname = type->tp_name;
        }
        data.modulename = _PyDTrace_ModuleNameFromObject(tstate, callable, data.modulename);
        data.filename = data.modulename;
    }

    if (_PyDTrace_IsUnknown(data.filename) || _PyDTrace_IsUnknown(data.funcname)
        || _PyDTrace_IsUnknown(data.modulename))
    {
        _PyCFrame *cframe = tstate->cframe;
        _PyInterpreterFrame *frame = cframe != NULL ? cframe->current_frame : NULL;
        if (frame != NULL && frame->f_code != NULL) {
            if (_PyDTrace_IsUnknown(data.filename)) {
                data.filename = _PyDTrace_UTF8View(tstate, frame->f_code->co_filename, data.filename);
            }

            if (_PyDTrace_IsUnknown(data.funcname)) {
                PyObject *func_qualname = NULL;
                if (frame->f_func != NULL) {
                    func_qualname = frame->f_func->func_qualname;
                }
                if (func_qualname != NULL) {
                    data.funcname = _PyDTrace_UTF8View(tstate, func_qualname, data.funcname);
                }
                else {
                    data.funcname = _PyDTrace_UTF8View(tstate, frame->f_code->co_name, data.funcname);
                }
            }

            if (_PyDTrace_IsUnknown(data.modulename)) {
                PyObject *globals = frame->f_globals;
                if (globals != NULL && PyDict_CheckExact(globals)) {
                    PyObject *modname = PyDict_GetItemWithError(globals, &_Py_ID(__name__));
                    if (modname != NULL) {
                        data.modulename = _PyDTrace_UTF8View(tstate, modname, data.modulename);
                    }
                    else if (_PyErr_Occurred(tstate)) {
                        _PyErr_Clear(tstate);
                    }
                }
            }
        }
    }

    return data;
#else
    _PyDTraceCallMetadata data = {"?", "?", "?"};
    (void)tstate;
    (void)callable;
    return data;
#endif
}

static inline void
_PyDTrace_CALL_ENTRY_PROBE(PyThreadState *tstate, PyObject *callable)
{
#if defined(Py_BUILD_CORE) && !defined(Py_BUILD_CORE_MODULE)
    if (!PyDTrace_CALL_ENTRY_ENABLED()) {
        return;
    }

    PyObject *exc_type, *exc_value, *exc_tb;
    _PyErr_Fetch(tstate, &exc_type, &exc_value, &exc_tb);

    _PyDTraceCallMetadata data = _PyDTrace_GetCallMetadata(tstate, callable);
    if (_PyDTrace_IsWhitelistedCall(&data)) {
        _PyErr_Restore(tstate, exc_type, exc_value, exc_tb);
        return;
    }
    PyDTrace_CALL_ENTRY(data.filename, data.funcname, data.modulename);

    _PyErr_Restore(tstate, exc_type, exc_value, exc_tb);
#else
    (void)tstate;
    (void)callable;
#endif
}

static inline void
_PyDTrace_CALL_RETURN_PROBE(PyThreadState *tstate, PyObject *callable)
{
#if defined(Py_BUILD_CORE) && !defined(Py_BUILD_CORE_MODULE)
    if (!PyDTrace_CALL_RETURN_ENABLED()) {
        return;
    }

    PyObject *exc_type, *exc_value, *exc_tb;
    _PyErr_Fetch(tstate, &exc_type, &exc_value, &exc_tb);

    _PyDTraceCallMetadata data = _PyDTrace_GetCallMetadata(tstate, callable);
    if (_PyDTrace_IsWhitelistedCall(&data)) {
        _PyErr_Restore(tstate, exc_type, exc_value, exc_tb);
        return;
    }
    PyDTrace_CALL_RETURN(data.filename, data.funcname, data.modulename);

    _PyErr_Restore(tstate, exc_type, exc_value, exc_tb);
#else
    (void)tstate;
    (void)callable;
#endif
}


/* Call the callable object 'callable' with the "vectorcall" calling
   convention.

   args is a C array for positional arguments.

   nargsf is the number of positional arguments plus optionally the flag
   PY_VECTORCALL_ARGUMENTS_OFFSET which means that the caller is allowed to
   modify args[-1].

   kwnames is a tuple of keyword names. The values of the keyword arguments
   are stored in "args" after the positional arguments (note that the number
   of keyword arguments does not change nargsf). kwnames can also be NULL if
   there are no keyword arguments.

   keywords must only contain strings and all keys must be unique.

   Return the result on success. Raise an exception and return NULL on
   error. */
static inline PyObject *
_PyObject_VectorcallTstate(PyThreadState *tstate, PyObject *callable,
                           PyObject *const *args, size_t nargsf,
                           PyObject *kwnames)
{
    vectorcallfunc func;
    PyObject *res;

    assert(kwnames == NULL || PyTuple_Check(kwnames));
    assert(args != NULL || PyVectorcall_NARGS(nargsf) == 0);

    func = _PyVectorcall_FunctionInline(callable);
    if (func == NULL) {
        Py_ssize_t nargs = PyVectorcall_NARGS(nargsf);
        return _PyObject_MakeTpCall(tstate, callable, args, nargs, kwnames);
    }

    _PyDTrace_CALL_ENTRY_PROBE(tstate, callable);
    res = func(callable, args, nargsf, kwnames);
    res = _Py_CheckFunctionResult(tstate, callable, res, NULL);
    _PyDTrace_CALL_RETURN_PROBE(tstate, callable);
    return res;
}


static inline PyObject *
_PyObject_CallNoArgsTstate(PyThreadState *tstate, PyObject *func) {
    return _PyObject_VectorcallTstate(tstate, func, NULL, 0, NULL);
}


// Private static inline function variant of public PyObject_CallNoArgs()
static inline PyObject *
_PyObject_CallNoArgs(PyObject *func) {
    PyThreadState *tstate = _PyThreadState_GET();
    return _PyObject_VectorcallTstate(tstate, func, NULL, 0, NULL);
}


static inline PyObject *
_PyObject_FastCallTstate(PyThreadState *tstate, PyObject *func, PyObject *const *args, Py_ssize_t nargs)
{
    return _PyObject_VectorcallTstate(tstate, func, args, (size_t)nargs, NULL);
}


#ifdef __cplusplus
}
#endif
#endif /* !Py_INTERNAL_CALL_H */
