/**
 * @file core.h  Internal API
 *
 * Copyright (C) 2010 - 2012 Creytiv.com
 */


extern PyObject *pylibre_error;

PyObject *pylibre_set_error(PyObject *exc, int error, const char *str);
PyObject *pylibre_set_error_pl(PyObject *exc, const struct pl *pl);
void pylibre_initerror(PyObject *m);


PyObject *pylibre_initmain(void);
void pylibre_initsip(PyObject *m);
void pylibre_inituri(PyObject *m);
