/**
 * @file core.h  Internal API
 *
 * Copyright (C) 2010 - 2012 Creytiv.com
 */

/* exception.c
 */
extern PyObject *librepython_error;
PyObject *librepython_set_error(PyObject *exc, int error, const char *str);
PyObject *librepython_set_error_pl(PyObject *exc, struct pl *pl);
void librepython_initerror(PyObject *m);

PyObject *librepython_initmain(void);
void librepython_initsip(PyObject *m);
void librepython_inituri(PyObject *m);
