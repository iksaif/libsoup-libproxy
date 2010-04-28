
# Installation

Copy soup-proxy-resolver-libproxy.c soup-proxy-resolver-libproxy.h in
your source tree and build them :)

# Dependencies

libsoup-2.4 (tested with 2.29.5)
libproxy-1.0 (tested with 0.3.1)

# Usage

  #include "soup-proxy-resolver-libproxy.h"

  SoupSession *session = webkit_get_default_session();
  SoupSessionFeature *feature;

  soup_session_remove_feature_by_type(session, SOUP_TYPE_PROXY_RESOLVER);
  feature = SOUP_SESSION_FEATURE (soup_proxy_resolver_libproxy_new ());
  soup_session_add_feature (session, feature);
  g_object_unref (feature);
