/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * GData Client
 * Copyright (C) Philip Withnall 2008-2009 <philip@tecnocode.co.uk>
 *
 * GData Client is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * GData Client is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GData Client.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION:gdata-service
 * @short_description: GData service object
 * @stability: Unstable
 * @include: gdata/gdata-service.h
 *
 * #GDataService represents a GData API service, typically a website using the GData API, such as YouTube or Google Calendar. One
 * #GDataService instance is required to issue queries to the service, handle insertions, updates and deletions, and generally
 * communicate with the online service.
 **/

#include <config.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <libsoup/soup.h>
#include <string.h>

#include "gdata-service.h"
#include "gdata-private.h"
#include "gdata-marshal.h"

GQuark
gdata_service_error_quark (void)
{
	return g_quark_from_static_string ("gdata-service-error-quark");
}

GQuark
gdata_authentication_error_quark (void)
{
	return g_quark_from_static_string ("gdata-authentication-error-quark");
}

static void gdata_service_dispose (GObject *object);
static void gdata_service_finalize (GObject *object);
static void gdata_service_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void gdata_service_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static gboolean real_parse_authentication_response (GDataService *self, guint status, const gchar *response_body, gint length, GError **error);
static void real_append_query_headers (GDataService *self, SoupMessage *message);
static void real_parse_error_response (GDataService *self, guint status, const gchar *reason_phrase,
				       const gchar *response_body, gint length, GError **error);

struct _GDataServicePrivate {
	SoupSession *session;

	gchar *username;
	gchar *password;
	gchar *auth_token;
	gchar *client_id;
	gboolean authenticated;
};

enum {
	PROP_CLIENT_ID = 1,
	PROP_USERNAME,
	PROP_PASSWORD,
	PROP_AUTHENTICATED
};

enum {
	SIGNAL_CAPTCHA_CHALLENGE,
	LAST_SIGNAL
};

static guint service_signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (GDataService, gdata_service, G_TYPE_OBJECT)
#define GDATA_SERVICE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GDATA_TYPE_SERVICE, GDataServicePrivate))

static void
gdata_service_class_init (GDataServiceClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (GDataServicePrivate));

	gobject_class->set_property = gdata_service_set_property;
	gobject_class->get_property = gdata_service_get_property;
	gobject_class->dispose = gdata_service_dispose;
	gobject_class->finalize = gdata_service_finalize;

	klass->service_name = "xapi";
	klass->authentication_uri = "https://www.google.com/accounts/ClientLogin";
	klass->parse_authentication_response = real_parse_authentication_response;
	klass->append_query_headers = real_append_query_headers;
	klass->parse_error_response = real_parse_error_response;

	/**
	 * GDataService:client-id:
	 *
	 * A client ID for your application (see the
	 * <ulink url="http://code.google.com/apis/youtube/2.0/developers_guide_protocol_api_query_parameters.html#clientsp" type="http">
	 * YouTube reference documentation</ulink>).
	 **/
	g_object_class_install_property (gobject_class, PROP_CLIENT_ID,
				g_param_spec_string ("client-id",
					"Client ID", "A client ID for your application (see http://code.google.com/apis/youtube/2.0/developers_guide_protocol_api_query_parameters.html#clientsp).",
					NULL,
					G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * GDataService:username:
	 *
	 * The user's Google or YouTube username for authentication.
	 **/
	g_object_class_install_property (gobject_class, PROP_USERNAME,
				g_param_spec_string ("username",
					"Username", "The user's Google or YouTube username for authentication.",
					NULL,
					G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

	/**
	 * GDataService:password:
	 *
	 * The user's account password for authentication.
	 **/
	g_object_class_install_property (gobject_class, PROP_PASSWORD,
				g_param_spec_string ("password",
					"Password", "The user's account password for authentication.",
					NULL,
					G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

	/**
	 * GDataService:authenticated:
	 *
	 * Whether the user is authenticated (logged in) with the service.
	 **/
	g_object_class_install_property (gobject_class, PROP_AUTHENTICATED,
				g_param_spec_boolean ("authenticated",
					"Authenticated", "Whether the user is authenticated (logged in) with the service.",
					FALSE,
					G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

	/**
	 * GDataService::captcha-challenge:
	 * @service: the #GDataService which received the challenge
	 * @uri: the URI of the CAPTCHA image to be used
	 *
	 * The #GDataService::captcha-challenge signal is emitted during the authentication process if
	 * the service requires a CAPTCHA to be completed. The URI of a CAPTCHA image is given, and the
	 * program should display this to the user, and return their response (the text displayed in the
	 * image). There is no timeout imposed by the library for the response.
	 *
	 * Return value: the text in the CAPTCHA image
	 **/
	service_signals[SIGNAL_CAPTCHA_CHALLENGE] = g_signal_new ("captcha-challenge",
				G_TYPE_FROM_CLASS (klass),
				G_SIGNAL_RUN_LAST,
				0, NULL, NULL,
				gdata_marshal_STRING__OBJECT_STRING,
				G_TYPE_STRING, 1, G_TYPE_STRING);
}

static void
gdata_service_init (GDataService *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GDATA_TYPE_SERVICE, GDataServicePrivate);
	self->priv->session = soup_session_sync_new ();
}

static void
gdata_service_dispose (GObject *object)
{
	GDataServicePrivate *priv = GDATA_SERVICE_GET_PRIVATE (object);

	if (priv->session != NULL)
		g_object_unref (priv->session);
	priv->session = NULL;

	/* Chain up to the parent class */
	G_OBJECT_CLASS (gdata_service_parent_class)->dispose (object);
}

static void
gdata_service_finalize (GObject *object)
{
	GDataServicePrivate *priv = GDATA_SERVICE_GET_PRIVATE (object);

	g_free (priv->username);
	g_free (priv->password);
	g_free (priv->auth_token);
	g_free (priv->client_id);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (gdata_service_parent_class)->finalize (object);
}

static void
gdata_service_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	GDataServicePrivate *priv = GDATA_SERVICE_GET_PRIVATE (object);

	switch (property_id) {
		case PROP_CLIENT_ID:
			g_value_set_string (value, priv->client_id);
			break;
		case PROP_USERNAME:
			g_value_set_string (value, priv->username);
			break;
		case PROP_PASSWORD:
			g_value_set_string (value, priv->password);
			break;
		case PROP_AUTHENTICATED:
			g_value_set_boolean (value, priv->authenticated);
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
gdata_service_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	GDataServicePrivate *priv = GDATA_SERVICE_GET_PRIVATE (object);

	switch (property_id) {
		case PROP_CLIENT_ID:
			priv->client_id = g_value_dup_string (value);
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static gboolean
real_parse_authentication_response (GDataService *self, guint status, const gchar *response_body, gint length, GError **error)
{
	gchar *auth_start, *auth_end;

	/* Parse the response */
	auth_start = strstr (response_body, "Auth=");
	if (auth_start == NULL)
		goto protocol_error;
	auth_start += strlen ("Auth=");

	auth_end = strstr (auth_start, "\n");
	if (auth_end == NULL)
		goto protocol_error;

	self->priv->auth_token = g_strndup (auth_start, auth_end - auth_start);
	if (self->priv->auth_token == NULL || strlen (self->priv->auth_token) == 0)
		goto protocol_error;

	return TRUE;

protocol_error:
	g_set_error_literal (error, GDATA_SERVICE_ERROR, GDATA_SERVICE_ERROR_PROTOCOL_ERROR,
			     _("The server returned a malformed response."));
	return FALSE;
}

static void
real_append_query_headers (GDataService *self, SoupMessage *message)
{
	gchar *authorisation_header;

	g_assert (message != NULL);

	/* Set the authorisation header */
	if (self->priv->auth_token != NULL) {
		authorisation_header = g_strdup_printf ("GoogleLogin auth=%s", self->priv->auth_token);
		soup_message_headers_append (message->request_headers, "Authorization", authorisation_header);
		g_free (authorisation_header);
	}

	/* Set the GData-Version header to tell it we want to use the v2 API */
	soup_message_headers_append (message->request_headers, "GData-Version", "2");
}

static void
real_parse_error_response (GDataService *self, guint status, const gchar *reason_phrase, const gchar *response_body, gint length, GError **error)
{
	g_set_error (error, GDATA_SERVICE_ERROR, GDATA_SERVICE_ERROR_WITH_QUERY,
		     _("Error code %u when querying: %s"), status, reason_phrase);
}

typedef struct {
	/* Input */
	gchar *username;
	gchar *password;

	/* Output */
	GDataService *service;
	gboolean success;
} AuthenticateAsyncData;

static void
authenticate_async_data_free (AuthenticateAsyncData *self)
{
	g_free (self->username);
	g_free (self->password);

	g_slice_free (AuthenticateAsyncData, self);
}

static gboolean
set_authentication_details_cb (AuthenticateAsyncData *data)
{
	GObject *service = G_OBJECT (data->service);
	GDataServicePrivate *priv = data->service->priv;

	g_free (priv->username);
	priv->username = g_strdup (data->username);
	g_free (priv->password);
	priv->password = g_strdup (data->password);
	priv->authenticated = TRUE;

	g_object_freeze_notify (service);
	g_object_notify (service, "username");
	g_object_notify (service, "password");
	g_object_notify (service, "authenticated");
	g_object_thaw_notify (service);

	return FALSE;
}

static void
authenticate_thread (GSimpleAsyncResult *result, GDataService *service, GCancellable *cancellable)
{
	GError *error = NULL;
	AuthenticateAsyncData *data = g_simple_async_result_get_op_res_gpointer (result);

	/* Check to see if it's been cancelled already */
	if (g_cancellable_set_error_if_cancelled (cancellable, &error) == TRUE) {
		g_simple_async_result_set_from_error (result, error);
		g_error_free (error);
		return;
	}

	/* Authenticate and return */
	data->success = gdata_service_authenticate (service, data->username, data->password, cancellable, &error);
	if (data->success == FALSE) {
		g_simple_async_result_set_from_error (result, error);
		g_error_free (error);
	}

	/* Update the authentication details held by the service.
	 * This should always be executed before the callback in g_simple_async_result_run_in_thread ---
	 * if not, data will already have been freed, and crashage will happen. */
	data->service = service;
	g_idle_add ((GSourceFunc) set_authentication_details_cb, data);
}

/**
 * gdata_service_authenticate_async:
 * @self: a #GDataService
 * @username: the user's username
 * @password: the user's password
 * @cancellable: optional #GCancellable object, or %NULL
 * @callback: a #GAsyncReadyCallback to call when authentication is finished
 * @user_data: data to pass to the @callback function
 *
 * Authenticates the #GDataService with the online service using the given @username and @password. @self, @username and
 * @password are all reffed/copied when this function is called, so can safely be freed after this function returns.
 *
 * For more details, see gdata_service_authenticate(), which is the synchronous version of this function.
 *
 * When the operation is finished, @callback will be called. You can then call gdata_service_authenticate_finish()
 * to get the results of the operation.
 **/
void
gdata_service_authenticate_async (GDataService *self, const gchar *username, const gchar *password,
				  GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
	GSimpleAsyncResult *result;
	AuthenticateAsyncData *data;

	g_return_if_fail (GDATA_IS_SERVICE (self));
	g_return_if_fail (username != NULL);
	g_return_if_fail (password != NULL);

	data = g_slice_new (AuthenticateAsyncData);
	data->username = g_strdup (username);
	data->password = g_strdup (password);

	result = g_simple_async_result_new (G_OBJECT (self), callback, user_data, gdata_service_authenticate_async);
	g_simple_async_result_set_op_res_gpointer (result, data, (GDestroyNotify) authenticate_async_data_free);
	g_simple_async_result_run_in_thread (result, (GSimpleAsyncThreadFunc) authenticate_thread, G_PRIORITY_DEFAULT, cancellable);
	g_object_unref (result);
}

/**
 * gdata_service_authenticate_finish:
 * @self: a #GDataService
 * @async_result: a #GAsyncResult
 * @error: a #GError, or %NULL
 *
 * Finishes an asynchronous authentication operation started with gdata_service_authenticate_async().
 *
 * Return value: %TRUE if authentication was successful, %FALSE otherwise
 **/
gboolean
gdata_service_authenticate_finish (GDataService *self, GAsyncResult *async_result, GError **error)
{
	GSimpleAsyncResult *result = G_SIMPLE_ASYNC_RESULT (async_result);
	AuthenticateAsyncData *data;

	g_return_val_if_fail (GDATA_IS_SERVICE (self), FALSE);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (async_result), FALSE);

	g_warn_if_fail (g_simple_async_result_get_source_tag (result) == gdata_service_authenticate_async);

	if (g_simple_async_result_propagate_error (result, error) == TRUE)
		return FALSE;

	data = g_simple_async_result_get_op_res_gpointer (result);
	if (data->success == TRUE)
		return TRUE;

	g_assert_not_reached ();
}

static gboolean
authenticate (GDataService *self, const gchar *username, const gchar *password, gchar *captcha_token, gchar *captcha_answer,
	      GCancellable *cancellable, GError **error)
{
	GDataServicePrivate *priv = self->priv;
	GDataServiceClass *klass;
	SoupMessage *message;
	gchar *request_body;
	guint status;
	gboolean retval;

	g_return_val_if_fail (GDATA_IS_SERVICE (self), FALSE);
	g_return_val_if_fail (username != NULL, FALSE);
	g_return_val_if_fail (password != NULL, FALSE);

	/* Prepare the request */
	klass = GDATA_SERVICE_GET_CLASS (self);
	request_body = soup_form_encode ("accountType", "HOSTED_OR_GOOGLE",
					 "Email", username,
					 "Passwd", password,
					 "service", klass->service_name,
					 "source", priv->client_id,
					 (captcha_token == NULL) ? NULL : "logintoken", captcha_token,
					 "loginanswer", captcha_answer,
					 NULL);

	/* Free the CAPTCHA token and answer if necessary */
	g_free (captcha_token);
	g_free (captcha_answer);

	/* Build the message */
	message = soup_message_new (SOUP_METHOD_POST, klass->authentication_uri);
	soup_message_set_request (message, "application/x-www-form-urlencoded", SOUP_MEMORY_TAKE, request_body, strlen (request_body));

	/* Send the message */
	status = soup_session_send_message (priv->session, message);

	/* Check for cancellation */
	if (g_cancellable_set_error_if_cancelled (cancellable, error) == TRUE) {
		g_object_unref (message);
		return FALSE;
	}

	if (status != 200) {
		const gchar *response_body = message->response_body->data;
		gchar *error_start, *error_end, *uri_start, *uri_end, *uri = NULL;

		/* Error */
		error_start = strstr (response_body, "Error=");
		if (error_start == NULL)
			goto protocol_error;
		error_start += strlen ("Error=");

		error_end = strstr (error_start, "\n");
		if (error_end == NULL)
			goto protocol_error;

		if (strncmp (error_start, "CaptchaRequired", error_end - error_start) == 0) {
			const gchar *captcha_base_uri = "http://www.google.com/accounts/";
			gchar *captcha_start, *captcha_end, *captcha_uri, *captcha_answer;
			guint captcha_base_uri_length;

			/* CAPTCHA required to log in */
			captcha_start = strstr (response_body, "CaptchaUrl=");
			if (captcha_start == NULL)
				goto protocol_error;
			captcha_start += strlen ("CaptchaUrl=");

			captcha_end = strstr (captcha_start, "\n");
			if (captcha_end == NULL)
				goto protocol_error;

			/* Do some fancy memory stuff to save ourselves another alloc */
			captcha_base_uri_length = strlen (captcha_base_uri);
			captcha_uri = g_malloc (captcha_base_uri_length + (captcha_end - captcha_start) + 1);
			memcpy (captcha_uri, captcha_base_uri, captcha_base_uri_length);
			memcpy (captcha_uri + captcha_base_uri_length, captcha_start, (captcha_end - captcha_start));
			captcha_uri[captcha_base_uri_length + (captcha_end - captcha_start)] = '\0';

			/* Request a CAPTCHA answer from the application */
			g_signal_emit (self, service_signals[SIGNAL_CAPTCHA_CHALLENGE], 0, captcha_uri, &captcha_answer);
			g_free (captcha_uri);

			if (captcha_answer == NULL || *captcha_answer == '\0') {
				g_set_error_literal (error, GDATA_AUTHENTICATION_ERROR, GDATA_AUTHENTICATION_ERROR_CAPTCHA_REQUIRED,
						     _("A CAPTCHA must be filled out to log in."));
				goto login_error;
			}

			/* Get the CAPTCHA token */
			captcha_start = strstr (response_body, "CaptchaToken=");
			if (captcha_start == NULL)
				goto protocol_error;
			captcha_start += strlen ("CaptchaToken=");

			captcha_end = strstr (captcha_start, "\n");
			if (captcha_end == NULL)
				goto protocol_error;

			/* Save the CAPTCHA token and answer, and attempt to log in with them */
			g_object_unref (message);

			return authenticate (self, username, password, g_strndup (captcha_start, captcha_end - captcha_start), captcha_answer,
					     cancellable, error);
		} else if (strncmp (error_start, "Unknown", error_end - error_start) == 0) {
			goto protocol_error; /* TODO: is this really a protocol error? It's an error with *our* code */
		} else if (strncmp (error_start, "BadAuthentication", error_end - error_start) == 0) {
			/* TODO: Looks like Error=BadAuthentication errors don't return a URI */
			g_set_error_literal (error, GDATA_AUTHENTICATION_ERROR, GDATA_AUTHENTICATION_ERROR_BAD_AUTHENTICATION,
					     _("Your username or password were incorrect."));
			goto login_error;
		}

		/* Get the information URI */
		uri_start = strstr (response_body, "Url=");
		if (uri_start == NULL)
			goto protocol_error;
		uri_start += strlen ("Url=");

		uri_end = strstr (uri_start, "\n");
		if (uri_end == NULL)
			goto protocol_error;

		uri = g_strndup (uri_start, uri_end - uri_start);

		if (strncmp (error_start, "NotVerified", error_end - error_start) == 0) {
			g_set_error (error, GDATA_AUTHENTICATION_ERROR, GDATA_AUTHENTICATION_ERROR_NOT_VERIFIED,
				     _("Your account's e-mail address has not been verified. (%s)"), uri);
			goto login_error;
		} else if (strncmp (error_start, "TermsNotAgreed", error_end - error_start) == 0) {
			g_set_error (error, GDATA_AUTHENTICATION_ERROR, GDATA_AUTHENTICATION_ERROR_TERMS_NOT_AGREED,
				     _("You have not agreed to the service's terms and conditions. (%s)"), uri);
			goto login_error;
		} else if (strncmp (error_start, "AccountDeleted", error_end - error_start) == 0) {
			g_set_error (error, GDATA_AUTHENTICATION_ERROR, GDATA_AUTHENTICATION_ERROR_ACCOUNT_DELETED,
				     _("This account has been deleted. (%s)"), uri);
			goto login_error;
		} else if (strncmp (error_start, "AccountDisabled", error_end - error_start) == 0) {
			g_set_error (error, GDATA_AUTHENTICATION_ERROR, GDATA_AUTHENTICATION_ERROR_ACCOUNT_DISABLED,
				     _("This account has been disabled. (%s)"), uri);
			goto login_error;
		} else if (strncmp (error_start, "ServiceDisabled", error_end - error_start) == 0) {
			g_set_error (error, GDATA_AUTHENTICATION_ERROR, GDATA_AUTHENTICATION_ERROR_SERVICE_DISABLED,
				     _("This account's access to this service has been disabled. (%s)"), uri);
			goto login_error;
		} else if (strncmp (error_start, "ServiceUnavailable", error_end - error_start) == 0) {
			g_set_error (error, GDATA_SERVICE_ERROR, GDATA_SERVICE_ERROR_UNAVAILABLE,
				     _("This service is not available at the moment. (%s)"), uri);
			goto login_error;
		}

		/* Unknown error type! */
		goto protocol_error;

login_error:
		g_free (uri);
		goto general_error;
	}

	g_assert (message->response_body->data != NULL);

	retval = klass->parse_authentication_response (self, status, message->response_body->data, message->response_body->length, error);
	g_object_unref (message);

	g_object_freeze_notify (G_OBJECT (self));
	priv->authenticated = retval;

	if (retval == TRUE) {
		/* Update several properties the service holds */
		g_free (priv->username);
		priv->username = g_strdup (username);
		g_free (priv->password);
		priv->password = g_strdup (password);

		g_object_notify (G_OBJECT (self), "username");
		g_object_notify (G_OBJECT (self), "password");
	}

	g_object_notify (G_OBJECT (self), "authenticated");
	g_object_thaw_notify (G_OBJECT (self));

	return retval;

protocol_error:
	g_set_error_literal (error, GDATA_SERVICE_ERROR, GDATA_SERVICE_ERROR_PROTOCOL_ERROR,
			     _("The server returned a malformed response."));

general_error:
	g_object_unref (message);
	priv->authenticated = FALSE;
	g_object_notify (G_OBJECT (self), "authenticated");

	return FALSE;
}

/**
 * gdata_service_authenticate:
 * @self: a #GDataService
 * @username: the user's username
 * @password: the user's password
 * @cancellable: optional #GCancellable object, or %NULL
 * @error: a #GError, or %NULL
 *
 * Authenticates the #GDataService with the online service using @username and @password; i.e. logs into the service with the given
 * user account.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by triggering the @cancellable object from another thread.
 * If the operation was cancelled, the error %G_IO_ERROR_CANCELLED will be returned.
 *
 * A %GDATA_AUTHENTICATION_ERROR_BAD_AUTHENTICATION will be returned if authentication failed due to an incorrect username or password.
 * Other #GDataAuthenticationError errors can be returned for other conditions.
 *
 * If the service requires a CAPTCHA to be completed, the #GDataService::captcha-challenge signal will be emitted. The return value from
 * a signal handler for the signal should be the text from the image. If the text is %NULL or empty, authentication will fail with a
 * %GDATA_AUTHENTICATION_ERROR_CAPTCHA_REQUIRED error. Otherwise, authentication will be automatically and transparently restarted with
 * the new CAPTCHA details.
 *
 * A %GDATA_SERVICE_ERROR_PROTOCOL_ERROR will be returned if the server's responses were invalid. Subclasses of #GDataService can override
 * parsing the authentication response, and may return their own error codes. See their documentation for more details.
 *
 * Return value: %TRUE if authentication was successful, %FALSE otherwise
 **/
gboolean
gdata_service_authenticate (GDataService *self, const gchar *username, const gchar *password, GCancellable *cancellable, GError **error)
{
	return authenticate (self, username, password, NULL, NULL, cancellable, error);
}

guint
_gdata_service_send_message (GDataService *self, SoupMessage *message, GError **error)
{
	/* Based on code from evolution-data-server's libgdata:
	 *  Ebby Wiselyn <ebbywiselyn@gmail.com>
	 *  Jason Willis <zenbrother@gmail.com>
	 *
	 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
	 */

	soup_message_set_flags (message, SOUP_MESSAGE_NO_REDIRECT);
	soup_session_send_message (self->priv->session, message);
	soup_message_set_flags (message, 0);

	if (SOUP_STATUS_IS_REDIRECTION (message->status_code)) {
		SoupURI *new_uri;
		const gchar *new_location;

		new_location = soup_message_headers_get (message->response_headers, "Location");
		g_return_val_if_fail (new_location != NULL, SOUP_STATUS_NONE);

		new_uri = soup_uri_new_with_base (soup_message_get_uri (message), new_location);
		if (new_uri == NULL) {
			gchar *uri_string = soup_uri_to_string (new_uri, FALSE);
			g_set_error (error, GDATA_SERVICE_ERROR, GDATA_SERVICE_ERROR_PROTOCOL_ERROR,
				     _("Invalid redirect URI: %s"), uri_string);
			g_free (uri_string);
			return SOUP_STATUS_NONE;
		}

		soup_message_set_uri (message, new_uri);
		soup_uri_free (new_uri);

		soup_session_send_message (self->priv->session, message);
	}

	return message->status_code;
}

typedef struct {
	/* Input */
	gchar *feed_uri;
	GDataQuery *query;
	GDataEntryParserFunc parser_func;

	/* Output */
	GDataFeed *feed;
	GDataQueryProgressCallback progress_callback;
	gpointer progress_user_data;
} QueryAsyncData;

static void
query_async_data_free (QueryAsyncData *self)
{
	g_free (self->feed_uri);
	if (self->query)
		g_object_unref (self->query);
	if (self->feed)
		g_object_unref (self->feed);

	g_slice_free (QueryAsyncData, self);
}

static void
query_thread (GSimpleAsyncResult *result, GDataService *service, GCancellable *cancellable)
{
	GError *error = NULL;
	QueryAsyncData *data = g_simple_async_result_get_op_res_gpointer (result);

	/* Check to see if it's been cancelled already */
	if (g_cancellable_set_error_if_cancelled (cancellable, &error) == TRUE) {
		g_simple_async_result_set_from_error (result, error);
		g_error_free (error);
		return;
	}

	/* Execute the query and return */
	data->feed = gdata_service_query (service, data->feed_uri, data->query, data->parser_func, cancellable,
					  data->progress_callback, data->progress_user_data, &error);
	if (data->feed == NULL) {
		g_simple_async_result_set_from_error (result, error);
		g_error_free (error);
	}
}

/**
 * gdata_service_query_async:
 * @self: a #GDataService
 * @feed_uri: the feed URI to query, including the host name and protocol
 * @query: a #GDataQuery with the query parameters, or %NULL
 * @parser_func: a #GDataEntryParserFunc to build a #GDataEntry from the XML
 * @cancellable: optional #GCancellable object, or %NULL
 * @progress_callback: a #GDataQueryProgressCallback to call when an entry is loaded, or %NULL
 * @progress_user_data: data to pass to the @progress_callback function
 * @callback: a #GAsyncReadyCallback to call when authentication is finished
 * @user_data: data to pass to the @callback function
 *
 * Queries the service's @feed_uri feed to build a #GDataFeed. @self, @feed_uri and
 * @query are all reffed/copied when this function is called, so can safely be freed after this function returns.
 *
 * For more details, see gdata_service_query(), which is the synchronous version of this function.
 *
 * When the operation is finished, @callback will be called. You can then call gdata_service_query_finish()
 * to get the results of the operation.
 **/
void
gdata_service_query_async (GDataService *self, const gchar *feed_uri, GDataQuery *query, GDataEntryParserFunc parser_func, GCancellable *cancellable,
			   GDataQueryProgressCallback progress_callback, gpointer progress_user_data,
			   GAsyncReadyCallback callback, gpointer user_data)
{
	GSimpleAsyncResult *result;
	QueryAsyncData *data;

	g_return_if_fail (GDATA_IS_SERVICE (self));
	g_return_if_fail (feed_uri != NULL);
	g_return_if_fail (parser_func != NULL);

	data = g_slice_new (QueryAsyncData);
	data->feed_uri = g_strdup (feed_uri);
	data->query = (query != NULL) ? g_object_ref (query) : NULL;
	data->parser_func = parser_func;
	data->progress_callback = progress_callback;
	data->progress_user_data = progress_user_data;

	result = g_simple_async_result_new (G_OBJECT (self), callback, user_data, gdata_service_query_async);
	g_simple_async_result_set_op_res_gpointer (result, data, (GDestroyNotify) query_async_data_free);
	g_simple_async_result_run_in_thread (result, (GSimpleAsyncThreadFunc) query_thread, G_PRIORITY_DEFAULT, cancellable);
	g_object_unref (result);
}

/**
 * gdata_service_query_finish:
 * @self: a #GDataService
 * @async_result: a #GAsyncResult
 * @error: a #GError, or %NULL
 *
 * Finishes an asynchronous query operation started with gdata_service_query_async().
 *
 * Return value: a #GDataFeed of query results; unref with g_object_unref()
 **/
GDataFeed *
gdata_service_query_finish (GDataService *self, GAsyncResult *async_result, GError **error)
{
	GSimpleAsyncResult *result = G_SIMPLE_ASYNC_RESULT (async_result);
	QueryAsyncData *data;

	g_return_val_if_fail (GDATA_IS_SERVICE (self), NULL);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (async_result), NULL);

	g_warn_if_fail (g_simple_async_result_get_source_tag (result) == gdata_service_query_async);

	if (g_simple_async_result_propagate_error (result, error) == TRUE)
		return NULL;

	data = g_simple_async_result_get_op_res_gpointer (result);
	if (data->feed != NULL)
		return g_object_ref (data->feed);

	g_assert_not_reached ();
}

/**
 * gdata_service_query:
 * @self: a #GDataService
 * @feed_uri: the feed URI to query, including the host name and protocol
 * @query: a #GDataQuery with the query parameters, or %NULL
 * @parser_func: a #GDataEntryParserFunc to build a #GDataEntry from the XML
 * @cancellable: optional #GCancellable object, or %NULL
 * @progress_callback: a #GDataQueryProgressCallback to call when an entry is loaded, or %NULL
 * @progress_user_data: data to pass to the @progress_callback function
 * @error: a #GError, or %NULL
 *
 * Queries the service's @feed_uri feed to build a #GDataFeed.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by triggering the @cancellable object from another thread.
 * If the operation was cancelled, the error %G_IO_ERROR_CANCELLED will be returned.
 *
 * A %GDATA_SERVICE_ERROR_WITH_QUERY will be returned if the server indicates there is a problem with the query, but subclasses may override
 * this and return their own errors. See their documentation for more details.
 *
 * For each entry in the response feed, @progress_callback will be called in the main thread. If there was an error parsing the XML response,
 * a #GDataParserError will be returned.
 *
 * If the query is successful and the feed supports pagination, @query will be updated with the pagination URIs, and the next or previous page
 * can then be loaded by calling gdata_query_next_page() or gdata_query_previous_page() before running the query again.
 *
 * Return value: a #GDataFeed of query results; unref with g_object_unref()
 **/
GDataFeed *
gdata_service_query (GDataService *self, const gchar *feed_uri, GDataQuery *query, GDataEntryParserFunc parser_func,
		     GCancellable *cancellable, GDataQueryProgressCallback progress_callback, gpointer progress_user_data, GError **error)
{
	GDataServiceClass *klass;
	GDataFeed *feed;
	SoupMessage *message;
	gchar *query_uri;
	guint status;
	GDataLink *link;

	g_return_val_if_fail (GDATA_IS_SERVICE (self), NULL);

	if (query != NULL) {
		query_uri = gdata_query_get_query_uri (query, feed_uri);
		message = soup_message_new (SOUP_METHOD_GET, query_uri);
		g_free (query_uri);
	} else {
		message = soup_message_new (SOUP_METHOD_GET, feed_uri);
	}

	/* Make sure subclasses set their headers */
	klass = GDATA_SERVICE_GET_CLASS (self);
	if (klass->append_query_headers != NULL)
		klass->append_query_headers (self, message);

	/* Send the message */
	status = soup_session_send_message (self->priv->session, message);

	/* Check for cancellation */
	if (g_cancellable_set_error_if_cancelled (cancellable, error) == TRUE) {
		g_object_unref (message);
		return FALSE;
	}

	if (status != 200) {
		/* Error */
		g_assert (klass->parse_error_response != NULL);
		klass->parse_error_response (self, status, message->reason_phrase, message->response_body->data, message->response_body->length, error);
		g_object_unref (message);
		return FALSE;
	}

	g_assert (message->response_body->data != NULL);

	feed = _gdata_feed_new_from_xml (message->response_body->data, message->response_body->length, parser_func,
					 progress_callback, progress_user_data, error);
	g_object_unref (message);

	/* Update the query with the next and previous URIs from the feed */
	if (query != NULL) {
		link = gdata_feed_look_up_link (feed, "next");
		if (link != NULL)
			_gdata_query_set_next_uri (query, link->href);
		link = gdata_feed_look_up_link (feed, "previous");
		if (link != NULL)
			_gdata_query_set_previous_uri (query, link->href);
	}

	return feed;
}

/**
 * gdata_service_insert_entry:
 * @self: a #GDataService
 * @upload_uri: the URI to which the upload should be sent
 * @entry: the #GDataEntry to upload
 * @parser_func: a #GDataEntryParserFunc to build a #GDataEntry from the updated XML
 * @cancellable: optional #GCancellable object, or %NULL
 * @error: a #GError, or %NULL
 *
 * Inserts @entry by uploading it to the online service at @upload_uri. For more information about the concept of inserting entries, see
 * the <ulink type="http" url="http://code.google.com/apis/gdata/docs/2.0/basics.html#InsertingEntry">online documentation</ulink> for the GData
 * protocol.
 *
 * The service will return an updated version of the entry, which is the return value of this function on success.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by triggering the @cancellable object from another thread.
 * If the operation was cancelled, the error %G_IO_ERROR_CANCELLED will be returned.
 *
 * If the entry is marked as already having been inserted a %GDATA_SERVICE_ERROR_ENTRY_ALREADY_INSERTED error will be returned immediately
 * (there will be no network requests).
 *
 * If there is an error inserting the entry, a %GDATA_SERVICE_ERROR_WITH_INSERTION error will be returned. Currently, subclasses
 * <emphasis>cannot</emphasis> cannot override this or provide more specific errors.
 *
 * Return value: an updated #GDataEntry, or %NULL
 **/
GDataEntry *
gdata_service_insert_entry (GDataService *self, const gchar *upload_uri, GDataEntry *entry, GDataEntryParserFunc parser_func,
			    GCancellable *cancellable, GError **error)
{
	GDataServiceClass *klass;
	SoupMessage *message;
	gchar *upload_data;
	guint status;
	xmlDoc *doc;
	xmlNode *node;

	g_return_val_if_fail (GDATA_IS_SERVICE (self), NULL);
	g_return_val_if_fail (upload_uri != NULL, NULL);
	g_return_val_if_fail (GDATA_IS_ENTRY (entry), NULL);
	g_return_val_if_fail (parser_func != NULL, NULL);

	if (gdata_entry_is_inserted (entry) == TRUE) {
		g_set_error_literal (error, GDATA_SERVICE_ERROR, GDATA_SERVICE_ERROR_ENTRY_ALREADY_INSERTED,
				     _("The entry has already been inserted."));
		return NULL;
	}

	message = soup_message_new (SOUP_METHOD_POST, upload_uri);

	/* Make sure subclasses set their headers */
	klass = GDATA_SERVICE_GET_CLASS (self);
	if (klass->append_query_headers != NULL)
		klass->append_query_headers (self, message);

	/* Append the data */
	upload_data = gdata_entry_get_xml (entry);
	soup_message_set_request (message, "application/atom+xml", SOUP_MEMORY_TAKE, upload_data, strlen (upload_data));

	/* Send the message */
	status = _gdata_service_send_message (self, message, error);
	if (status == SOUP_STATUS_NONE) {
		g_object_unref (message);
		return NULL;
	}

	/* Check for cancellation */
	if (g_cancellable_set_error_if_cancelled (cancellable, error) == TRUE) {
		g_object_unref (message);
		return NULL;
	}

	if (status != 201) {
		/* Error */
		/* TODO: Handle errors more specifically, making sure to set authenticated where appropriate */
		g_set_error (error, GDATA_SERVICE_ERROR, GDATA_SERVICE_ERROR_WITH_INSERTION,
			     _("TODO: error code %u when inserting"), status);
		g_object_unref (message);
		return NULL;
	}

	/* Build the updated entry */
	g_assert (message->response_body->data != NULL);

	/* Parse the XML */
	doc = xmlReadMemory (message->response_body->data, message->response_body->length, "entry.xml", NULL, 0);
	g_object_unref (message);

	if (doc == NULL) {
		xmlError *xml_error = xmlGetLastError ();
		g_set_error (error, GDATA_PARSER_ERROR, GDATA_PARSER_ERROR_PARSING_STRING,
			     _("Error parsing XML: %s"),
			     xml_error->message);
		return NULL;
	}

	/* Get the root element */
	node = xmlDocGetRootElement (doc);
	if (node == NULL) {
		/* XML document's empty */
		xmlFreeDoc (doc);
		g_set_error (error, GDATA_PARSER_ERROR, GDATA_PARSER_ERROR_EMPTY_DOCUMENT,
			     _("Error parsing XML: %s"),
			     _("Empty document."));
		return NULL;
	}

	if (xmlStrcmp (node->name, (xmlChar*) "entry") != 0) {
		/* No <entry> element (required) */
		xmlFreeDoc (doc);
		gdata_parser_error_required_element_missing ("entry", "root", error);
		return NULL;
	}

	return parser_func (doc, node, error);
}

/**
 * gdata_service_update_entry:
 * @self: a #GDataService
 * @entry: the #GDataEntry to update
 * @parser_func: a #GDataEntryParserFunc to build a #GDataEntry from the updated XML
 * @cancellable: optional #GCancellable object, or %NULL
 * @error: a #GError, or %NULL
 *
 * Updates @entry by PUTting it to its <literal>edit</literal> link's URI. For more information about the concept of updating entries, see
 * the <ulink type="http" url="http://code.google.com/apis/gdata/docs/2.0/basics.html#UpdatingEntry">online documentation</ulink> for the GData
 * protocol.
 *
 * The service will return an updated version of the entry, which is the return value of this function on success.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by triggering the @cancellable object from another thread.
 * If the operation was cancelled, the error %G_IO_ERROR_CANCELLED will be returned.
 *
 * If there is an error updating the entry, a %GDATA_SERVICE_ERROR_WITH_UPDATE error will be returned. Currently, subclasses
 * <emphasis>cannot</emphasis> cannot override this or provide more specific errors.
 *
 * Return value: an updated #GDataEntry, or %NULL
 **/
GDataEntry *
gdata_service_update_entry (GDataService *self, GDataEntry *entry, GDataEntryParserFunc parser_func, GCancellable *cancellable, GError **error)
{
	GDataServiceClass *klass;
	GDataLink *link;
	SoupMessage *message;
	gchar *upload_data;
	guint status;
	xmlDoc *doc;
	xmlNode *node;

	g_return_val_if_fail (GDATA_IS_SERVICE (self), NULL);
	g_return_val_if_fail (GDATA_IS_ENTRY (entry), NULL);
	g_return_val_if_fail (parser_func != NULL, NULL);

	/* Get the edit URI */
	link = gdata_entry_look_up_link (entry, "edit");
	g_assert (link != NULL);
	message = soup_message_new (SOUP_METHOD_PUT, link->href);

	/* Make sure subclasses set their headers */
	klass = GDATA_SERVICE_GET_CLASS (self);
	if (klass->append_query_headers != NULL)
		klass->append_query_headers (self, message);

	/* Append the data */
	upload_data = gdata_entry_get_xml (entry);
	soup_message_set_request (message, "application/atom+xml", SOUP_MEMORY_TAKE, upload_data, strlen (upload_data));

	/* Send the message */
	status = _gdata_service_send_message (self, message, error);
	if (status == SOUP_STATUS_NONE) {
		g_object_unref (message);
		return NULL;
	}

	/* Check for cancellation */
	if (g_cancellable_set_error_if_cancelled (cancellable, error) == TRUE) {
		g_object_unref (message);
		return NULL;
	}

	if (status != 200) {
		/* Error */
		/* TODO: Handle errors more specifically, making sure to set authenticated where appropriate */
		g_set_error (error, GDATA_SERVICE_ERROR, GDATA_SERVICE_ERROR_WITH_UPDATE,
			     _("TODO: error code %u when updating"), status);
		g_object_unref (message);
		return NULL;
	}

	/* Build the updated entry */
	g_assert (message->response_body->data != NULL);

	/* Parse the XML */
	doc = xmlReadMemory (message->response_body->data, message->response_body->length, "entry.xml", NULL, 0);
	g_object_unref (message);

	if (doc == NULL) {
		xmlError *xml_error = xmlGetLastError ();
		g_set_error (error, GDATA_PARSER_ERROR, GDATA_PARSER_ERROR_PARSING_STRING,
			     _("Error parsing XML: %s"),
			     xml_error->message);
		return NULL;
	}

	/* Get the root element */
	node = xmlDocGetRootElement (doc);
	if (node == NULL) {
		/* XML document's empty */
		xmlFreeDoc (doc);
		g_set_error (error, GDATA_PARSER_ERROR, GDATA_PARSER_ERROR_EMPTY_DOCUMENT,
			     _("Error parsing XML: %s"),
			     _("Empty document."));
		return NULL;
	}

	if (xmlStrcmp (node->name, (xmlChar*) "entry") != 0) {
		/* No <entry> element (required) */
		xmlFreeDoc (doc);
		gdata_parser_error_required_element_missing ("entry", "root", error);
		return NULL;
	}

	return parser_func (doc, node, error);
}

/**
 * gdata_service_delete_entry:
 * @self: a #GDataService
 * @entry: the #GDataEntry to delete
 * @cancellable: optional #GCancellable object, or %NULL
 * @error: a #GError, or %NULL
 *
 * Deletes @entry from the server. For more information about the concept of updating entries, see the
 * <ulink type="http" url="http://code.google.com/apis/gdata/docs/2.0/basics.html#DeletingEntry">online documentation</ulink> for the GData
 * protocol.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by triggering the @cancellable object from another thread.
 * If the operation was cancelled, the error %G_IO_ERROR_CANCELLED will be returned.
 *
 * If there is an error deleting the entry, a %GDATA_SERVICE_ERROR_WITH_DELETION error will be returned. Currently, subclasses
 * <emphasis>cannot</emphasis> cannot override this or provide more specific errors.
 *
 * Return value: %TRUE on success, %FALSE otherwise
 **/
gboolean
gdata_service_delete_entry (GDataService *self, GDataEntry *entry, GCancellable *cancellable, GError **error)
{
	GDataServiceClass *klass;
	GDataLink *link;
	SoupMessage *message;
	guint status;

	g_return_val_if_fail (GDATA_IS_SERVICE (self), FALSE);
	g_return_val_if_fail (GDATA_IS_ENTRY (entry), FALSE);

	/* Get the edit URI */
	link = gdata_entry_look_up_link (entry, "edit");
	g_assert (link != NULL);
	message = soup_message_new (SOUP_METHOD_DELETE, link->href);

	/* Make sure subclasses set their headers */
	klass = GDATA_SERVICE_GET_CLASS (self);
	if (klass->append_query_headers != NULL)
		klass->append_query_headers (self, message);

	/* Send the message */
	status = _gdata_service_send_message (self, message, error);
	g_object_unref (message);
	if (status == SOUP_STATUS_NONE)
		return FALSE;

	/* Check for cancellation */
	if (g_cancellable_set_error_if_cancelled (cancellable, error) == TRUE)
		return FALSE;

	if (status != 200) {
		/* Error */
		/* TODO: Handle errors more specifically, making sure to set authenticated where appropriate */
		g_set_error (error, GDATA_SERVICE_ERROR, GDATA_SERVICE_ERROR_WITH_DELETION,
			     _("TODO: error code %u when deleting"), status);
		return FALSE;
	}

	return TRUE;
}

/**
 * gdata_service_set_proxy:
 * @self: a #GDataService
 * @proxy_uri: the proxy URI
 *
 * Sets the proxy URI on the #SoupSession used internally by the given #GDataService.
 * This forces all requests through the given proxy.
 **/
void
gdata_service_set_proxy (GDataService *self, SoupURI *proxy_uri)
{
	g_return_if_fail (GDATA_IS_SERVICE (self));
	g_object_set (self->priv->session, SOUP_SESSION_PROXY_URI, proxy_uri, NULL); 
}

/**
 * gdata_service_is_authenticated:
 * @self: a #GDataService
 *
 * Returns whether a user is authenticated with the online service through @self.
 * Authentication is performed by calling gdata_service_authenticate() or gdata_service_authenticate_async().
 *
 * Return value: %TRUE if a user is authenticated, %FALSE otherwise
 **/
gboolean
gdata_service_is_authenticated (GDataService *self)
{
	g_assert (GDATA_IS_SERVICE (self));
	return self->priv->authenticated;
}

void
_gdata_service_set_authenticated (GDataService *self, gboolean authenticated)
{
	g_assert (GDATA_IS_SERVICE (self));
	self->priv->authenticated = authenticated;
}

/**
 * gdata_service_get_client_id:
 * @self: a #GDataService
 *
 * Returns the service's client ID, as specified on constructing the #GDataService.
 *
 * Return value: the service's client ID
 **/
const gchar *
gdata_service_get_client_id (GDataService *self)
{
	g_return_val_if_fail (GDATA_IS_SERVICE (self), NULL);
	return self->priv->client_id;
}

/**
 * gdata_service_get_username:
 * @self: a #GDataService
 *
 * Returns the username of the currently-authenticated user, or %NULL if nobody is authenticated.
 *
 * Return value: the username of the currently-authenticated user, or %NULL
 **/
const gchar *
gdata_service_get_username (GDataService *self)
{
	g_return_val_if_fail (GDATA_IS_SERVICE (self), NULL);
	return self->priv->username;
}

/**
 * gdata_service_get_password:
 * @self: a #GDataService
 *
 * Returns the password of the currently-authenticated user, or %NULL if nobody is authenticated.
 *
 * Return value: the password of the currently-authenticated user, or %NULL
 **/
const gchar *
gdata_service_get_password (GDataService *self)
{
	g_return_val_if_fail (GDATA_IS_SERVICE (self), NULL);
	return self->priv->password;
}
