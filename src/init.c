/**
 * @file init.c  Init functions
 *
 * Copyright (C) 2010 - 2012 Creytiv.com
 */
#include <Python.h>
#include <re.h>
#include "core.h"


static void exit_handler(void)
{
	libre_close();

	/* Check for memory leaks */
	tmr_debug();
	mem_debug();
}


PyMODINIT_FUNC initlibre(void)
{
	PyObject *m;
	int err;

	Py_AtExit(exit_handler);

	/* Initialize libre */
	err = libre_init();
	if (err) {
		re_fprintf(stderr, "could not initialize libre: %s\n",
			   strerror(err));
		return;
	}

	m = pylibre_initmain();

	pylibre_initerror(m);
	pylibre_initsip(m);
	pylibre_inituri(m);
}
