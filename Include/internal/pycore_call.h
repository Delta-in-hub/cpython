#ifndef Py_INTERNAL_CALL_H
#define Py_INTERNAL_CALL_H
#ifdef __cplusplus
extern "C" {
#endif

#ifndef Py_BUILD_CORE
#  error "this header requires Py_BUILD_CORE define"
#endif

#include "pycore_pystate.h"       // _PyThreadState_GET()
#include "pycore_frame.h"         // _PyInterpreterFrame
#include "pycore_global_strings.h" // _Py_ID()
#include "pycore_pyerrors.h"      // _PyErr_Clear()
#include "pycore_unicodeobject.h" // _PyUnicode_Ready()

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

static inline void
_PyDTrace_CALL_ENTRY_PROBE(PyThreadState *tstate, PyObject *callable)
{
    if (!PyDTrace_CALL_ENTRY_ENABLED()) {
        return;
    }

    const char *filename = "?";
    const char *funcname = "?";
    const char *modulename = "?";
    int lineno = -1;

    _PyInterpreterFrame *frame = tstate->cframe ? tstate->cframe->current_frame : NULL;
    if (frame != NULL) {
        PyCodeObject *code = frame->f_code;
        if (code != NULL) {
            filename = _PyDTrace_UTF8View(tstate, code->co_filename, filename);
            funcname = _PyDTrace_UTF8View(tstate, code->co_name, funcname);
        }

        lineno = _PyInterpreterFrame_GetLine(frame);

        PyObject *globals = frame->f_globals;
        if (globals != NULL && PyDict_CheckExact(globals)) {
            PyObject *modname = PyDict_GetItemWithError(globals, &_Py_ID(__name__));
            if (modname != NULL) {
                modulename = _PyDTrace_UTF8View(tstate, modname, modulename);
            }
            else if (_PyErr_Occurred(tstate)) {
                _PyErr_Clear(tstate);
            }
        }
    }

    PyDTrace_CALL_ENTRY(filename, funcname, lineno, modulename, callable);
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
