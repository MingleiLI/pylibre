/**
 *
 * @file uri.c  URI handling
 *
 * URIs are represented by an eight-tuple with the elements in the same
 * order as in libre's struct uri.
 */
#define PY_SSIZE_T_CLEAN 1
#include <Python.h>
#include <re.h>
#include "core.h"

static const char py_uri_encode_doc[] =
	"Encode a URI tuple into a string.\n"
	"\n"
	"Take a eight-tuple with the URI components and returns the\n"
	"string representation of the URI.\n";

static PyObject *py_uri_encode(PyObject *self, PyObject *args)
{
	struct uri uri;
	unsigned port;
	char *uri_str;
	PyObject *res;
	int err;

	(void) self;

	if (!PyArg_ParseTuple(args, "(s#z#z#z#iIz#z#)",
		   	      &uri.scheme.p, (Py_ssize_t *) &uri.scheme.l,
			      &uri.user.p, (Py_ssize_t *) &uri.user.l,
			      &uri.password.p, (Py_ssize_t *) &uri.password.l,
			      &uri.host.p, (Py_ssize_t *) &uri.host.l,
			      &uri.af,
			      &port,
			      &uri.params.p, (Py_ssize_t *) &uri.params.l,
			      &uri.headers.p, (Py_ssize_t *) &uri.headers.l))
	{
		return NULL;
	}
	if (port > 0xffff) {
		return PyErr_Format(PyExc_ValueError,
			            "port outside of allowed range: %u",
				    port);
	}
	uri.port = (uint16_t) port;
	err = re_sdprintf(&uri_str, "%H", uri_encode, &uri);
	if (err != 0) {
		return librepython_set_error(librepython_error, err, NULL);
	}
	res = Py_BuildValue("s", uri_str);
	mem_deref(uri_str);
	return res;
}


static const char py_uri_decode_doc[] =
	"Decode a URI string into a tuple.\n"
	"\n"
	"Takes a string and returns an eight-tuple with the URI\n"
	"components.\n";

static PyObject *py_uri_decode(PyObject *self, PyObject *arg)
{
	struct pl uri_str;
	struct uri uri;
	int err;

	(void) self;

	if (PyString_AsStringAndSize(arg, (char **) &uri_str.p,
				     (Py_ssize_t *) &uri_str.l))
	{
		return NULL;
	}
	err = uri_decode(&uri, &uri_str);
	if (err != 0) {
		return librepython_set_error(librepython_error, err, NULL);
	}
	return Py_BuildValue("(s#z#z#z#iIz#z#)",
		   	     uri.scheme.p, (Py_ssize_t) uri.scheme.l,
			     uri.user.p, (Py_ssize_t) uri.user.l,
			     uri.password.p, (Py_ssize_t) uri.password.l,
			     uri.host.p, (Py_ssize_t) uri.host.l,
			     uri.af,
			     (unsigned int) uri.port,
			     uri.params.p, (Py_ssize_t) uri.params.l,
			     uri.headers.p, (Py_ssize_t) uri.headers.l);
}


static const char py_uri_param_get_doc[] =
	"Get a URI parameter and possibly the value of it.\n"
	"\n"
	"Takes two string, one the parameter string from the URI tuple,\n"
	"and the other a parameter name. Returns a string with the\n"
	"parameter value. Raises KeyError the name was not found.\n";

static PyObject *py_uri_param_get(PyObject *self, PyObject *args)
{
	struct pl param;
	struct pl pname;
	struct pl pvalue;
	int err;

	(void) self;

	if (!PyArg_ParseTuple(args, "s#s#",
			     &param.p, (Py_ssize_t *) &param.l,
			     &pname.p, (Py_ssize_t *) &pname.l))
	{
		return NULL;
	}
	err = uri_param_get(&param, &pname, &pvalue);
	if (err == ENOENT) {
		return librepython_set_error_pl(PyExc_KeyError, &pname);
	}
	else if (err) {
		return librepython_set_error(librepython_error, err, NULL);
	}
	return Py_BuildValue("z#", pvalue.p, (Py_ssize_t) pvalue.l);
}


static const char py_uri_params_apply_doc[] =
	"Execute a callable for all URI parameters.\n"
	"\n"
	"Takes a single callable and calls it for each URI parameter\n"
	"with name and value.\n";

/* Calls the callable in arg with name and val as arguments. Returns
 * EPIPE if a Python exception is thrown.
 */
static int callable_apply_handler(const struct pl *name,
				  const struct pl *val, void *arg)
{
	PyObject *callable = (PyObject *) arg;
	PyObject *res;

	res = PyObject_CallFunction(callable, "s#s#",
				    (char *) name->p, (Py_ssize_t) name->l,
				    (char *) val->p, (Py_ssize_t) val->l);
	if (res == NULL) {
		return EPIPE;
	}
	Py_XDECREF(res);
	return 0;
}

static PyObject *py_uri_params_apply(PyObject *self, PyObject *args)
{
	struct pl params;
	PyObject *callable;
	int err;

	(void) self;

	if (!PyArg_ParseTuple(args, "s#O",
			      &params.p, (Py_ssize_t *) &params.l,
			      &callable))
	{
		return NULL;
	}
	if (!PyCallable_Check(callable)) {
		return PyErr_Format(PyExc_TypeError,
				    "argument must be a callable");
	}
	err = uri_params_apply(&params, callable_apply_handler, callable);
	if (err == EPIPE) {
		return NULL;
	}
	else if (err) {
		return librepython_set_error(librepython_error, err, NULL);
	}
	Py_RETURN_NONE;
}


static const char py_uri_params_list_doc[] =
	"Return a list of all URI parameters.\n";

/* Adds pairs to the list in arg. Return EPIPE in case of a Python
 * exception.
 */
static int list_apply_handler(const struct pl *name, const struct pl *val,
			      void *arg)
{
	PyObject *list = (PyObject *) arg;
	PyObject *pair = NULL;
	int res = 0;

	pair = Py_BuildValue("(s#s#)",
			     (char *) name->p, (Py_ssize_t) name->l,
			     (char *) val->p, (Py_ssize_t) val->l);
	if (pair == NULL) {
		res = EPIPE;
		goto out;
	}
	if (PyList_Append(list, pair) == -1) {
		res = EPIPE;
		goto out;
	}
out:
	Py_XDECREF(pair);
	return res;
}

static PyObject *py_uri_params_list(PyObject *self, PyObject *args)
{
	struct pl params;
	PyObject *list;
	int err;

	(void) self;

	if (!PyArg_ParseTuple(args, "s#",
			      &params.p, (Py_ssize_t *) &params.l))
	{
		return NULL;
	}
	list = PyList_New(0);
	if (list == NULL) {
		return NULL;
	}
	err = uri_params_apply(&params, list_apply_handler, list);
	if (err) {
		Py_XDECREF(list);
		if (err == EPIPE) {
			return NULL;
		}
		else {
			return librepython_set_error(librepython_error, err,
						     NULL);
		}
	}
	return list;
}


static const char py_uri_header_get_doc[] =
	"Get a URI header and possibly the value of it.\n"
	"\n"
	"Takes two string, one the parameter string from the URI tuple,\n"
	"and the other a parameter name. Returns a string with the\n"
	"parameter value. Raises KeyError the name was not found.\n";

static PyObject *py_uri_header_get(PyObject *self, PyObject *args)
{
	struct pl headers;
	struct pl name;
	struct pl value;
	int err;

	(void) self;

	if (!PyArg_ParseTuple(args, "s#s#",
			     &headers.p, (Py_ssize_t *) &headers.l,
			     &name.p, (Py_ssize_t *) &name.l))
	{
		return NULL;
	}
	err = uri_param_get(&headers, &name, &value);
	if (err == ENOENT) {
		return librepython_set_error_pl(PyExc_KeyError, &name);
	}
	else if (err) {
		return librepython_set_error(librepython_error, err, NULL);
	}
	return Py_BuildValue("z#", value.p, (Py_ssize_t) value.l);
}


const static char py_uri_headers_apply_doc[] =
	"Execute a callable for all URI headers.\n"
	"\n"
	"Takes a single callable and calls it for each URI header\n"
	"with name and value.\n";

static PyObject *py_uri_headers_apply(PyObject *self, PyObject *args)
{
	struct pl headers;
	PyObject *callable;
	int err;

	(void) self;

	if (!PyArg_ParseTuple(args, "s#O",
			      &headers.p, (Py_ssize_t *) &headers.l,
			      &callable))
	{
		return NULL;
	}
	if (!PyCallable_Check(callable)) {
		return PyErr_Format(PyExc_TypeError,
				    "argument must be a callable");
	}
	err = uri_params_apply(&headers, callable_apply_handler, callable);
	if (err == EPIPE) {
		return NULL;
	}
	else if (err) {
		return librepython_set_error(librepython_error, err, NULL);
	}
	Py_RETURN_NONE;
}


static const char py_uri_headers_list_doc[] =
	"Returns a list of all URI headers.\n";

static PyObject *py_uri_headers_list(PyObject *self, PyObject *args)
{
	struct pl headers;
	PyObject *list;
	int err;

	(void) self;

	if (!PyArg_ParseTuple(args, "s#",
			      &headers.p, (Py_ssize_t *) &headers.l))
	{
		return NULL;
	}
	list = PyList_New(0);
	if (list == NULL) {
		return NULL;
	}
	err = uri_params_apply(&headers, list_apply_handler, list);
	if (err) {
		Py_XDECREF(list);
		if (err == EPIPE) {
			return NULL;
		}
		else {
			return librepython_set_error(librepython_error, err,
						     NULL);
		}
	}
	return list;
}


static const char py_uri_cmp_doc[] =
	"Return whether the two URIs are equal.\n";

static PyObject *py_uri_cmp(PyObject *self, PyObject *args)
{
	struct uri l;
	struct uri r;
	unsigned int lport, rport;

	(void) self;

	if (!PyArg_ParseTuple(args, "(s#z#z#z#iIz#z#)(s#z#z#z#iIz#z#)",
		   	      &l.scheme.p, (Py_ssize_t *) &l.scheme.l,
			      &l.user.p, (Py_ssize_t *) &l.user.l,
			      &l.password.p, (Py_ssize_t *) &l.password.l,
			      &l.host.p, (Py_ssize_t *) &l.host.l,
			      &l.af,
			      &lport,
			      &l.params.p, (Py_ssize_t *) &l.params.l,
			      &l.headers.p, (Py_ssize_t *) &l.headers.l,
		   	      &r.scheme.p, (Py_ssize_t *) &r.scheme.l,
			      &r.user.p, (Py_ssize_t *) &r.user.l,
			      &r.password.p, (Py_ssize_t *) &r.password.l,
			      &r.host.p, (Py_ssize_t *) &r.host.l,
			      &r.af,
			      &rport,
			      &r.params.p, (Py_ssize_t *) &r.params.l,
			      &r.headers.p, (Py_ssize_t *) &r.headers.l))
	{
		return NULL;
	}
	if (lport > 0xffff || rport > 0xffff) {
		return PyErr_Format(PyExc_ValueError,
			            "port outside of allowed range");
	}
	l.port = lport;
	r.port = rport;
	if (uri_cmp(&l, &r)) {
		Py_RETURN_TRUE;
	}
	else {
		Py_RETURN_FALSE;
	}
}

/* Special URI escaping/unescaping */

/* Takes a single string object in args and runs it through a re_printf
 * handler, returning a string object.
 */
static PyObject *apply_printf_to_str(PyObject *args, re_printf_h *h)
{
	struct pl pl;
	char *res_str;
	PyObject *res;
	int err;

	if (!PyArg_ParseTuple(args, "s#", &pl.p, (Py_ssize_t *) &pl.l)) {
		return NULL;
	}
	err = re_sdprintf(&res_str, "%H", h, &pl);
	if (err != 0) {
		return librepython_set_error(librepython_error, err, NULL);
	}
	res = Py_BuildValue("s", res_str);
	mem_deref(res_str);
	return res;
}


static const char py_uri_user_escape_doc[] =
	"Return a escaped version of the user part of a URI.\n";

static PyObject *py_uri_user_escape(PyObject *self, PyObject *args)
{
	(void) self;
	return apply_printf_to_str(args, (re_printf_h *) uri_user_escape);
}


static const char py_uri_user_unescape_doc[] =
	"Return an unescaped version of the user part of a URI.\n";

static PyObject *py_uri_user_unescape(PyObject *self, PyObject *args)
{
	(void) self;
	return apply_printf_to_str(args, (re_printf_h *) uri_user_unescape);
}


static const char py_uri_password_escape_doc[] =
	"Return an escaped version of the password URI part.\n";

static PyObject *py_uri_password_escape(PyObject *self, PyObject *args)
{
	(void) self;
	return apply_printf_to_str(args, (re_printf_h *) uri_password_escape);
}


static const char py_uri_password_unescape_doc[] =
	"Return an unescaped version of the password URI part.\n";

static PyObject *py_uri_password_unescape(PyObject *self, PyObject *args)
{
	(void) self;
	return apply_printf_to_str(args,
				   (re_printf_h *) uri_password_unescape);
}


static const char py_uri_param_escape_doc[] =
	"Return an escaped version of a URI parameter value.\n";

static PyObject *py_uri_param_escape(PyObject *self, PyObject *args)
{
	(void) self;
	return apply_printf_to_str(args, (re_printf_h *) uri_param_escape);
}


static const char py_uri_param_unescape_doc[] =
	"Return an unescaped version of a URI parameter value.\n";

static PyObject *py_uri_param_unescape(PyObject *self, PyObject *args)
{
	(void) self;
	return apply_printf_to_str(args, (re_printf_h *) uri_param_unescape);
}


static const char py_uri_header_escape_doc[] =
	"Return an escaped version of one URI header name/value.\n";

static PyObject *py_uri_header_escape(PyObject *self, PyObject *args)
{
	(void) self;
	return apply_printf_to_str(args, (re_printf_h *) uri_header_escape);
}


static const char py_uri_header_unescape_doc[] =
	"Return an unescaped version of one URI header name/value.\n";

static PyObject *py_uri_header_unescape(PyObject *self, PyObject *args)
{
	(void) self;
	return apply_printf_to_str(args, (re_printf_h *) uri_header_unescape);
}


static PyMethodDef URIMethods[] = {
	{"encode", (PyCFunction) py_uri_encode, METH_VARARGS,
	 py_uri_encode_doc},
	{"decode", (PyCFunction) py_uri_decode, METH_O, py_uri_decode_doc},
	{"param_get", (PyCFunction) py_uri_param_get, METH_VARARGS,
	 py_uri_param_get_doc},
	{"params_apply", (PyCFunction) py_uri_params_apply, METH_VARARGS,
	 py_uri_params_apply_doc},
	{"params_list", (PyCFunction) py_uri_params_list, METH_VARARGS,
	 py_uri_params_list_doc},
	{"header_get", (PyCFunction) py_uri_header_get, METH_VARARGS,
	 py_uri_header_get_doc},
	{"headers_apply", (PyCFunction) py_uri_headers_apply, METH_VARARGS,
	 py_uri_headers_apply_doc},
	{"headers_list", (PyCFunction) py_uri_headers_list, METH_VARARGS,
	 py_uri_headers_list_doc},
	{"cmp", (PyCFunction) py_uri_cmp, METH_VARARGS, py_uri_cmp_doc},
	{"user_escape", (PyCFunction) py_uri_user_escape, METH_VARARGS,
	 py_uri_user_escape_doc},
	{"user_unescape", (PyCFunction) py_uri_user_unescape, METH_VARARGS,
	 py_uri_user_unescape_doc},
	{"password_escape", (PyCFunction) py_uri_password_escape,
	 METH_VARARGS, py_uri_password_escape_doc},
	{"password_unescape", (PyCFunction) py_uri_password_unescape,
	 METH_VARARGS, py_uri_param_unescape_doc},
	{"param_escape", (PyCFunction) py_uri_param_escape,
	 METH_VARARGS, py_uri_param_unescape_doc},
	{"param_unescape", (PyCFunction) py_uri_param_unescape,
	 METH_VARARGS, py_uri_password_unescape_doc},
	{"header_escape", (PyCFunction) py_uri_header_escape,
	 METH_VARARGS, py_uri_header_escape_doc},
	{"header_unescape", (PyCFunction) py_uri_header_unescape,
	 METH_VARARGS, py_uri_header_unescape_doc},


	{NULL, NULL, 0, NULL}        /* Sentinel */
};


void librepython_inituri(PyObject *m)
{
	PyObject *mod;

	mod = Py_InitModule3("libre.uri", URIMethods, "URI functions");
	if (mod == NULL)
		return;
	Py_INCREF(mod);
	PyModule_AddObject(m, "uri", mod);
}
