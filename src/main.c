/**
 * @file main.c  Main functions
 *
 * Copyright (C) 2010 - 2012 Creytiv.com
 */
#include <Python.h>
#include <re.h>
#include "core.h"


static void re_signal_handler(int sig)
{
	re_cancel();
}


static PyObject *py_main(PyObject *self)
{
	int err;

	err = re_main(re_signal_handler);

	return Py_BuildValue("i", err);
}


static PyObject *py_cancel(PyObject *self)
{
	re_cancel();
	Py_RETURN_NONE;
}


static PyMethodDef LibreMethods[] = {

	{"main",   (PyCFunction)py_main,   METH_NOARGS, "Start main loop" },
	{"cancel", (PyCFunction)py_cancel, METH_NOARGS, "Cancel main loop"},

	{NULL, NULL, 0, NULL}        /* Sentinel */
};


PyObject *pylibre_initmain(void)
{
	return Py_InitModule3("libre", LibreMethods, "Main module");
}
