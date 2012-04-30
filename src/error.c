/**
 * @file error.c  Error Handling
 *
 * Copyright (C) 2012 Creytiv.com
 */
#define PY_SSIZE_T_CLEAN 1
#include <Python.h>
#include <re.h>
#include "core.h"


PyObject *pylibre_error;


PyObject *pylibre_set_error(PyObject *exc, int error, const char *str)
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
	else {
		PyErr_SetNone(exc);
	}

	return NULL;
}


PyObject *pylibre_set_error_pl(PyObject *exc, const struct pl *pl)
{
	PyObject *v;

	v = Py_BuildValue("z#", (char *) pl->p, (Py_ssize_t) pl->l);
	if (v != NULL) {
		PyErr_SetObject(exc, v);
		Py_DECREF(v);
	}
	else {
		PyErr_SetNone(exc);
	}

	return NULL;
}


void pylibre_initerror(PyObject *m)
{
	pylibre_error = PyErr_NewException("libre.error",
	                                       PyExc_IOError, NULL);
	if (pylibre_error == NULL) {
		return;
	}
	Py_INCREF(pylibre_error);
	PyModule_AddObject(m, "error", pylibre_error);
}
