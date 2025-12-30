#ifndef Py_INTERNAL_CALL_H
#define Py_INTERNAL_CALL_H
#ifdef __cplusplus
extern "C" {
#endif

#ifndef Py_BUILD_CORE
#  error "this header requires Py_BUILD_CORE define"
#endif

#include <stdbool.h>

#include "pycore_pystate.h"       // _PyThreadState_GET()
#include "pycore_frame.h"         // _PyInterpreterFrame
#include "pycore_global_strings.h" // _Py_ID()
#include "pycore_pyerrors.h"      // _PyErr_Clear()
#include "pycore_unicodeobject.h" // _PyUnicode_Ready()
#include "pydtrace.h"             // PyDTrace_CALL_ENTRY*

// When WITH_DTRACE is not enabled or the generated probes header is absent,
// provide no-op fallbacks so the inline helper can still compile.
#ifndef PyDTrace_CALL_ENTRY_ENABLED
#  define PyDTrace_CALL_ENTRY_ENABLED() (0)
#endif
#ifndef PyDTrace_CALL_ENTRY
#  define PyDTrace_CALL_ENTRY(arg0, arg1, arg2) ((void)0)
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

    PyObject *attr = PyObject_GetAttr(module, &_Py_ID(__module__));
    if (attr != NULL) {
        const char *result = _PyDTrace_UTF8View(tstate, attr, fallback);
        Py_DECREF(attr);
        return result;
    }

    _PyErr_Clear(tstate);

    attr = PyObject_GetAttr(module, &_Py_ID(__name__));
    if (attr != NULL) {
        const char *result = _PyDTrace_UTF8View(tstate, attr, fallback);
        Py_DECREF(attr);
        return result;
    }

    _PyErr_Clear(tstate);
    return fallback;
}

static inline void
_PyDTrace_CALL_ENTRY_PROBE(PyThreadState *tstate, PyObject *callable)
{
    if (!PyDTrace_CALL_ENTRY_ENABLED()) {
        return;
    }

    const char *filename = "?";
    const char *funcname = "?";
    const char *modulename = "?";
    bool have_filename = false;
    bool have_funcname = false;
    bool have_modulename = false;

    if (PyFunction_Check(callable)) {
        PyFunctionObject *func = (PyFunctionObject *)callable;
        PyCodeObject *code = (PyCodeObject *)func->func_code;
        if (code != NULL) {
            filename = _PyDTrace_UTF8View(tstate, code->co_filename, filename);
            funcname = _PyDTrace_UTF8View(tstate, code->co_name, funcname);
            have_filename = true;
            have_funcname = true;
        }

        PyObject *globals = func->func_globals;
        if (globals != NULL && PyDict_CheckExact(globals)) {
            PyObject *modname = PyDict_GetItemWithError(globals, &_Py_ID(__name__));
            if (modname != NULL) {
                modulename = _PyDTrace_UTF8View(tstate, modname, modulename);
                have_modulename = true;
            }
            else if (_PyErr_Occurred(tstate)) {
                _PyErr_Clear(tstate);
            }
        }
    }
    else if (PyCFunction_Check(callable)) {
        PyCFunctionObject *cfunc = (PyCFunctionObject *)callable;
        if (cfunc->m_ml != NULL && cfunc->m_ml->ml_name != NULL) {
            funcname = cfunc->m_ml->ml_name;
            have_funcname = true;
        }
        modulename = _PyDTrace_ModuleNameFromObject(tstate, cfunc->m_module, modulename);
        filename = modulename;
        have_modulename = modulename != NULL && modulename[0] != '\0' && modulename[0] != '?';
        have_filename = have_modulename;
    }
    else if (PyMethodDescr_Check(callable)) {
        PyMethodDescrObject *descr = (PyMethodDescrObject *)callable;
        if (descr->d_method != NULL && descr->d_method->ml_name != NULL) {
            funcname = descr->d_method->ml_name;
            have_funcname = true;
        }
        if (descr->d_common.d_type != NULL) {
            modulename = _PyDTrace_ModuleNameFromObject(
                tstate, (PyObject *)descr->d_common.d_type, modulename);
            filename = modulename;
            have_modulename = modulename != NULL && modulename[0] != '\0' && modulename[0] != '?';
            have_filename = have_modulename;
        }
    }
    else if (PyType_Check(callable)) {
        PyTypeObject *type = (PyTypeObject *)callable;
        if (type->tp_name != NULL) {
            funcname = type->tp_name;
            have_funcname = true;
        }
        modulename = _PyDTrace_ModuleNameFromObject(tstate, callable, modulename);
        filename = modulename;
        have_modulename = modulename != NULL && modulename[0] != '\0' && modulename[0] != '?';
        have_filename = have_modulename;
    }

    _PyInterpreterFrame *frame = tstate->cframe ? tstate->cframe->current_frame : NULL;
    if (frame != NULL) {
        PyCodeObject *code = frame->f_code;
        if (code != NULL) {
            if (!have_filename) {
                filename = _PyDTrace_UTF8View(tstate, code->co_filename, filename);
            }
            if (!have_funcname) {
                funcname = _PyDTrace_UTF8View(tstate, code->co_name, funcname);
            }
        }

        PyObject *globals = frame->f_globals;
        if (!have_modulename && globals != NULL && PyDict_CheckExact(globals)) {
            PyObject *modname = PyDict_GetItemWithError(globals, &_Py_ID(__name__));
            if (modname != NULL) {
                modulename = _PyDTrace_UTF8View(tstate, modname, modulename);
                have_modulename = true;
            }
            else if (_PyErr_Occurred(tstate)) {
                _PyErr_Clear(tstate);
            }
        }
    }

    PyDTrace_CALL_ENTRY(filename, funcname, modulename);
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

    _PyDTrace_CALL_ENTRY_PROBE(tstate, callable);

    func = _PyVectorcall_FunctionInline(callable);
    if (func == NULL) {
        Py_ssize_t nargs = PyVectorcall_NARGS(nargsf);
        return _PyObject_MakeTpCall(tstate, callable, args, nargs, kwnames);
    }
    res = func(callable, args, nargsf, kwnames);
    return _Py_CheckFunctionResult(tstate, callable, res, NULL);
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
