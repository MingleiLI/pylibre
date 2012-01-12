/**
 *
 * @file error.c  Error Handling
 *
 * Copyright (C) 2012 Creytiv.com
 */
#include <Python.h>
#include "core.h"

PyObject *librepython_error;

PyObject *librepython_set_error(PyObject *exc, int error, const char *str)
{
	PyObject *v;

	if (str == NULL) {
		str = strerror(error);
	}
	v = Py_BuildValue("(is)", error, str);
	if (v != NULL) {
		PyErr_SetObject(exc, v);
		Py_DECREF(v);
	}
	return NULL;
}


void librepython_initerror(PyObject *m)
{
	librepython_error = PyErr_NewException("libre.error",
	                                       PyExc_IOError, NULL);
	if (librepython_error == NULL) {
		return;
	}
	Py_INCREF(librepython_error);
	PyModule_AddObject(m, "error", librepython_error);
}
