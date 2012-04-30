/**
 * @file sip.c  SIP functions
 *
 * Copyright (C) 2010 - 2012 Creytiv.com
 */
#include <Python.h>
#include <re.h>
#include "core.h"


enum {HASH_SIZE = 8};


typedef struct {
	PyObject_HEAD

	/* python members */
	PyObject *sipreg_callback;

	/* libre members */
	struct dnsc *dnsc;
	struct sip *sip;
	struct sipreg *reg;
	const char *username;
	const char *password;
} Sip;


static void sip_exit_handler(void *arg)
{
	Sip *self = arg;

	(void)self;

	re_cancel();
}


static int sip_auth_handler(char **username, char **password,
			    const char *realm, void *arg)
{
	Sip *self = arg;
	int err;

	err  = str_dup(username, self->username);
	err |= str_dup(password, self->password);

	return err;
}


static void sipreg_resp_handler(int err, const struct sip_msg *msg, void *arg)
{
	Sip *self = arg;
	PyObject *arglist;

	if (err) {
		re_printf("sip resp ERROR: %s\n", strerror(err));
		return;
	}

	arglist = Py_BuildValue("(is#)",
				msg->scode, msg->reason.p, msg->reason.l);
	PyObject_CallObject(self->sipreg_callback, arglist);
	Py_DECREF(arglist);
}


static int dns_init(Sip *self)
{
	struct sa nsv[8];
	uint32_t nsn;
	int err;

	nsn = ARRAY_SIZE(nsv);

	err = dns_srv_get(NULL, 0, nsv, &nsn);
	if (err)
		return err;

	return dnsc_alloc(&self->dnsc, NULL, nsv, nsn);
}


static int
Sip_init(Sip *self, PyObject *args, PyObject *kwds)
{
	static char *kwlist[] = {"username", "password", "callback", NULL};
	struct sa laddr;
	int err;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "ssO", kwlist,
					 &self->username, &self->password,
					 &self->sipreg_callback))
		return -1;

	if (!PyCallable_Check(self->sipreg_callback)) {
		PyErr_SetString(PyExc_TypeError, "parameter must be callable");
		return -1;
	}
	Py_XINCREF(self->sipreg_callback);

	err = net_default_source_addr_get(AF_INET, &laddr);
	if (err)
		goto out;

	err = dns_init(self);
	if (err)
		goto out;

	err = sip_alloc(&self->sip, self->dnsc,
			HASH_SIZE, HASH_SIZE, HASH_SIZE,
			"Python libre", sip_exit_handler, self);
	if (err)
		goto out;

	err  = sip_transp_add(self->sip, SIP_TRANSP_UDP, &laddr);
	err |= sip_transp_add(self->sip, SIP_TRANSP_TCP, &laddr);
	if (err)
		goto out;

 out:
	if (err)
		PyErr_SetString(PyExc_RuntimeError, strerror(err));

	return err ? -1 : 0;
}


static void Sip_dealloc(Sip *self)
{
	mem_deref(self->reg);
	mem_deref(self->dnsc);

	sip_close(self->sip, true);
	mem_deref(self->sip);

	PyObject_Del(self);
}


static PyObject *
libre_sipreg_register(Sip *self, PyObject *args, PyObject *keywds)
{
	static char *kwlist[] = {"reg_uri", "to_uri", "from_uri",
				 "cuser", NULL};
	char *reg_uri, *to_uri, *from_uri, *cuser;
	int err = 0;

	if (!PyArg_ParseTupleAndKeywords(args, keywds, "ssss", kwlist,
					 &reg_uri, &to_uri, &from_uri, &cuser))
		return NULL;

	self->reg = mem_deref(self->reg);
	err = sipreg_register(&self->reg, self->sip, reg_uri, to_uri,
			      from_uri, 3600, cuser, NULL, 0, 0,
			      sip_auth_handler, self, false,
			      sipreg_resp_handler, self, NULL, NULL);
	if (err) {
		PyErr_SetString(PyExc_RuntimeError, strerror(err));
		return NULL;
	}

	Py_RETURN_NONE;
}


static PyMethodDef SipMethods[] = {

	{"register", (PyCFunction)libre_sipreg_register,
	 METH_VARARGS | METH_KEYWORDS, "SIP Register client"},

	{NULL, NULL, 0, NULL}        /* Sentinel */
};


static PyTypeObject SipType = {
	PyObject_HEAD_INIT(NULL)
	0,				/* ob_size           */
	"libre.Sip",			/* tp_name           */
	sizeof(Sip),			/* tp_basicsize      */
	0,				/* tp_itemsize       */
	(destructor)Sip_dealloc,	/* tp_dealloc        */
	0,				/* tp_print          */
	0,				/* tp_getattr        */
	0,				/* tp_setattr        */
	0,				/* tp_compare        */
	0,				/* tp_repr           */
	0,				/* tp_as_number      */
	0,				/* tp_as_sequence    */
	0,				/* tp_as_mapping     */
	0,				/* tp_hash           */
	0,				/* tp_call           */
	0,				/* tp_str            */
	0,				/* tp_getattro       */
	0,				/* tp_setattro       */
	0,				/* tp_as_buffer      */
	Py_TPFLAGS_DEFAULT,		/* tp_flags          */
	"SIP Class",			/* tp_doc            */
	0,				/* tp_traverse       */
	0,				/* tp_clear          */
	0,				/* tp_richcompare    */
	0,				/* tp_weaklistoffset */
	0,				/* tp_iter           */
	0,				/* tp_iternext       */
	SipMethods,	     		/* tp_methods        */
	0,				/* tp_members        */
	0,				/* tp_getset         */
	0,				/* tp_base           */
	0,				/* tp_dict           */
	0,				/* tp_descr_get      */
	0,				/* tp_descr_set      */
	0,				/* tp_dictoffset     */
	(initproc)Sip_init,		/* tp_init           */
};


void pylibre_initsip(PyObject *m)
{
	SipType.tp_new = PyType_GenericNew;
	if (PyType_Ready(&SipType) < 0)
		return;

	Py_INCREF(&SipType);
	PyModule_AddObject(m, "Sip", (PyObject *)&SipType);
}
