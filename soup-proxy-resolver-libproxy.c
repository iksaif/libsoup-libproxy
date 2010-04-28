/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * soup-proxy-resolver-libproxy.c: Automatic proxy "resolution"
 *
 * Copyright (C) 2010 Corentin Chary
 */

#include <string.h>
#include <stdlib.h>

#include <libsoup/soup-proxy-uri-resolver.h>
#include <libsoup/soup-message.h>
#include <libsoup/soup-misc.h>
#include <libsoup/soup-session-feature.h>

#include <proxy.h>

#include "soup-proxy-resolver-libproxy.h"

static pxProxyFactory *libproxy_factory;

typedef struct {
} SoupProxyResolverLibproxyPrivate;

#define SOUP_PROXY_RESOLVER_LIBPROXY_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), SOUP_TYPE_PROXY_RESOLVER_LIBPROXY, SoupProxyResolverLibproxyPrivate))

static void soup_proxy_resolver_libproxy_interface_init (SoupProxyURIResolverInterface *proxy_resolver_interface);

G_DEFINE_TYPE_EXTENDED (SoupProxyResolverLibproxy, soup_proxy_resolver_libproxy, G_TYPE_OBJECT, 0,
			G_IMPLEMENT_INTERFACE (SOUP_TYPE_SESSION_FEATURE, NULL)
			G_IMPLEMENT_INTERFACE (SOUP_TYPE_PROXY_URI_RESOLVER, soup_proxy_resolver_libproxy_interface_init))

static void get_proxy_uri_async (SoupProxyURIResolver  *proxy_uri_resolver,
				 SoupURI               *uri,
				 GMainContext          *async_context,
				 GCancellable          *cancellable,
				 SoupProxyURIResolverCallback callback,
				 gpointer               user_data);
static guint get_proxy_uri_sync (SoupProxyURIResolver  *proxy_uri_resolver,
				 SoupURI               *uri,
				 GCancellable          *cancellable,
				 SoupURI              **proxy_uri);
#include <stdio.h>
static void
soup_proxy_resolver_libproxy_init (SoupProxyResolverLibproxy *resolver_libproxy)
{
	if (!libproxy_factory)
		libproxy_factory = px_proxy_factory_new ();
}

static void
finalize (GObject *object)
{
	if (libproxy_factory)
		px_proxy_factory_free(libproxy_factory);

	G_OBJECT_CLASS (soup_proxy_resolver_libproxy_parent_class)->finalize (object);
}

static void
soup_proxy_resolver_libproxy_class_init (SoupProxyResolverLibproxyClass *libproxy_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (libproxy_class);

	g_type_class_add_private (libproxy_class, sizeof (SoupProxyResolverLibproxyPrivate));

	object_class->set_property = NULL;
	object_class->get_property = NULL;
	object_class->finalize = finalize;
}

static void
soup_proxy_resolver_libproxy_interface_init (SoupProxyURIResolverInterface *proxy_resolver_interface)
{
	proxy_resolver_interface->get_proxy_uri_async = get_proxy_uri_async;
	proxy_resolver_interface->get_proxy_uri_sync = get_proxy_uri_sync;
}

SoupProxyResolver *
soup_proxy_resolver_libproxy_new (void)
{
	return g_object_new (SOUP_TYPE_PROXY_RESOLVER_LIBPROXY, NULL);
}

static guint
get_proxy_for_uri (SoupURI *uri, SoupURI **proxy_uri)
{
	char *uristr, **proxies;
	gboolean got_proxy;
	int i;

	*proxy_uri = NULL;

	/* resolver_gnome is locked */

	uristr = soup_uri_to_string (uri, FALSE);
	proxies = px_proxy_factory_get_proxies (libproxy_factory, uristr);
	g_free (uristr);

	if (!proxies)
		return SOUP_STATUS_OK;

	got_proxy = FALSE;
	for (i = 0; proxies[i]; i++) {
		if (!strcmp (proxies[i], "direct://")) {
			got_proxy = TRUE;
			break;
		}
		if (strncmp (proxies[i], "http://", 7) == 0) {
			*proxy_uri = soup_uri_new (proxies[i]);
			got_proxy = TRUE;
			break;
		}
	}
	for (i = 0; proxies[i]; i++)
		free (proxies[i]);
	free (proxies);

	if (got_proxy)
		return SOUP_STATUS_OK;
	else
		return SOUP_STATUS_CANT_RESOLVE_PROXY;
}

typedef struct {
	SoupProxyURIResolver *proxy_uri_resolver;
	SoupURI *proxy_uri;
	GCancellable *cancellable;
	SoupProxyURIResolverCallback callback;
	gpointer user_data;
	guint status;
} SoupLibproxyAsyncData;

static gboolean
resolved_proxy (gpointer data)
{
	SoupLibproxyAsyncData *slad = data;

	slad->callback (slad->proxy_uri_resolver, slad->status,
			slad->proxy_uri, slad->user_data);
	g_object_unref (slad->proxy_uri_resolver);
	if (slad->cancellable)
		g_object_unref (slad->cancellable);
	if (slad->proxy_uri)
		soup_uri_free (slad->proxy_uri);
	g_slice_free (SoupLibproxyAsyncData, slad);

	return FALSE;
}

static void
get_proxy_uri_async (SoupProxyURIResolver  *proxy_uri_resolver,
		     SoupURI               *uri,
		     GMainContext          *async_context,
		     GCancellable          *cancellable,
		     SoupProxyURIResolverCallback callback,
		     gpointer               user_data)
{
	SoupLibproxyAsyncData *slad;

	slad = g_slice_new0 (SoupLibproxyAsyncData);
	slad->proxy_uri_resolver = g_object_ref (proxy_uri_resolver);
	slad->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
	slad->callback = callback;
	slad->user_data = user_data;

	slad->status = get_proxy_uri_sync(proxy_uri_resolver, uri, cancellable,
					  &slad->proxy_uri);

	soup_add_completion (async_context, resolved_proxy, slad);
}

static guint
get_proxy_uri_sync (SoupProxyURIResolver  *proxy_uri_resolver,
		    SoupURI               *uri,
		    GCancellable          *cancellable,
		    SoupURI              **proxy_uri)
{
	return get_proxy_for_uri (uri, proxy_uri);
}
