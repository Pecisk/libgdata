/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * GData Client
 * Copyright (C) Philip Withnall 2009 <philip@tecnocode.co.uk>
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
 * SECTION:gdata-calendar-service
 * @short_description: GData Calendar service object
 * @stability: Unstable
 * @include: gdata/services/calendar/gdata-calendar-service.h
 *
 * #GDataCalendarService is a subclass of #GDataService for communicating with the GData API of Google Calendar. It supports querying
 * for, inserting, editing and deleting events from calendars, as well as operations on the calendars themselves.
 *
 * For more details of Google Calendar's GData API, see the <ulink type="http" url="http://code.google.com/apis/calendar/docs/2.0/reference.html">
 * online documentation</ulink>.
 *
 * Each calendar accessible through the service has an access control list (ACL) which defines the level of access to the calendar to each user, and
 * which users the calendar is shared with. For more information about ACLs for calendars, see the
 * <ulink type="http" url="http://code.google.com/apis/calendar/data/2.0/developers_guide_protocol.html#SharingACalendar">online documentation on
 * sharing calendars</ulink>.
 *
 * <example>
 * 	<title>Retrieving the Access Control List for a Calendar</title>
 * 	<programlisting>
 *	GDataCalendarService *service;
 *	GDataCalendarCalendar *calendar;
 *	GDataFeed *acl_feed;
 *	GDataAccessRule *rule, *new_rule;
 *	GDataLink *acl_link;
 *	GList *i;
 *	GError *error = NULL;
 *
 *	/<!-- -->* Create a service and retrieve a calendar to work on *<!-- -->/
 *	service = create_calendar_service ();
 *	calendar = get_calendar (service);
 *
 *	/<!-- -->* Query the service for the ACL for the given calendar *<!-- -->/
 *	acl_feed = gdata_access_handler_get_rules (GDATA_ACCESS_HANDLER (calendar), GDATA_SERVICE (service), NULL, NULL, NULL, &error);
 *
 *	g_object_unref (calendar);
 *
 *	if (error != NULL) {
 *		g_error ("Error getting ACL feed for calendar: %s", error->message);
 *		g_error_free (error);
 *		g_object_unref (service);
 *		return;
 *	}
 *
 *	/<!-- -->* Iterate through the ACL *<!-- -->/
 *	for (i = gdata_feed_get_entries (acl_feed); i != NULL; i = i->next) {
 *		const gchar *scope_value;
 *
 *		rule = GDATA_ACCESS_RULE (i->data);
 *
 *		/<!-- -->* Do something with the access rule here. As an example, we update the rule applying to test@gmail.com and delete all
 *		 * the other rules. We then insert another rule for example@gmail.com below. *<!-- -->/
 *		gdata_access_rule_get_scope (rule, NULL, &scope_value);
 *		if (scope_value != NULL && strcmp (scope_value, "test@gmail.com") == 0) {
 *			GDataAccessRule *updated_rule;
 *
 *			/<!-- -->* Update the rule to make test@gmail.com an editor (full read/write access to the calendar, but they can't change
 *			 * the ACL). *<!-- -->/
 *			gdata_access_rule_set_role (rule, GDATA_CALENDAR_ACCESS_ROLE_EDITOR);
 *			updated_rule = GDATA_ACCESS_RULE (gdata_service_update_entry (GDATA_SERVICE (service), GDATA_ENTRY (rule), NULL, &error));
 *
 *			if (error != NULL) {
 *				g_error ("Error updating access rule for %s: %s", scope_value, error->message);
 *				g_error_free (error);
 *				g_object_unref (acl_feed);
 *				g_object_unref (service);
 *				return;
 *			}
 *
 *			g_object_unref (updated_rule);
 *		} else {
 *			/<!-- -->* Delete any rule which doesn't apply to test@gmail.com *<!-- -->/
 *			gdata_service_delete_entry (GDATA_SERVICE (service), GDATA_ENTRY (rule), NULL, &error);
 *
 *			if (error != NULL) {
 *				g_error ("Error deleting access rule for %s: %s", scope_value, error->message);
 *				g_error_free (error);
 *				g_object_unref (acl_feed);
 *				g_object_unref (service);
 *				return;
 *			}
 *		}
 *	}
 *
 *	g_object_unref (acl_feed);
 *
 *	/<!-- -->* Create and insert a new access rule for example@gmail.com which allows then to view free/busy information for events in the
 *	 * calendar, but doesn't allow them to view the full event details. *<!-- -->/
 *	rule = gdata_access_rule_new (NULL);
 *	gdata_access_rule_set_role (rule, GDATA_CALENDAR_ACCESS_ROLE_FREE_BUSY);
 *	gdata_access_rule_set_scope (rule, GDATA_ACCESS_SCOPE_USER, "example@gmail.com");
 *
 *	acl_link = gdata_entry_look_up_link (GDATA_ENTRY (calendar), GDATA_LINK_ACCESS_CONTROL_LIST);
 *	new_rule = GDATA_ACCESS_RULE (gdata_service_insert_entry (GDATA_SERVICE (service), gdata_link_get_uri (acl_link), GDATA_ENTRY (rule),
 *	                                                          NULL, &error));
 *
 *	g_object_unref (rule);
 *	g_object_unref (service);
 *
 *	if (error != NULL) {
 *		g_error ("Error inserting access rule: %s", error->message);
 *		g_error_free (error);
 *		return;
 *	}
 *
 *	g_object_unref (acl_link);
 * 	</programlisting>
 * </example>
 **/

#include <config.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <libsoup/soup.h>
#include <string.h>

#include "gdata-calendar-service.h"
#include "gdata-batchable.h"
#include "gdata-service.h"
#include "gdata-private.h"
#include "gdata-query.h"
#include "gdata-calendar-feed.h"

/* Standards reference here: http://code.google.com/apis/calendar/docs/2.0/reference.html */

G_DEFINE_TYPE_WITH_CODE (GDataCalendarService, gdata_calendar_service, GDATA_TYPE_SERVICE, G_IMPLEMENT_INTERFACE (GDATA_TYPE_BATCHABLE, NULL))

static void
gdata_calendar_service_class_init (GDataCalendarServiceClass *klass)
{
	GDataServiceClass *service_class = GDATA_SERVICE_CLASS (klass);
	service_class->service_name = "cl";
	service_class->feed_type = GDATA_TYPE_CALENDAR_FEED;
}

static void
gdata_calendar_service_init (GDataCalendarService *self)
{
	/* Nothing to see here */
}

/**
 * gdata_calendar_service_new:
 * @client_id: your application's client ID
 *
 * Creates a new #GDataCalendarService. The @client_id must be unique for your application, and as registered with Google.
 *
 * Return value: a new #GDataCalendarService, or %NULL
 **/
GDataCalendarService *
gdata_calendar_service_new (const gchar *client_id)
{
	g_return_val_if_fail (client_id != NULL, NULL);
	return g_object_new (GDATA_TYPE_CALENDAR_SERVICE, "client-id", client_id, NULL);
}

/**
 * gdata_calendar_service_query_all_calendars:
 * @self: a #GDataCalendarService
 * @query: (allow-none): a #GDataQuery with the query parameters, or %NULL
 * @cancellable: optional #GCancellable object, or %NULL
 * @progress_callback: (scope call): a #GDataQueryProgressCallback to call when an entry is loaded, or %NULL
 * @progress_user_data: (closure): data to pass to the @progress_callback function
 * @error: a #GError, or %NULL
 *
 * Queries the service to return a list of all calendars from the authenticated account which match the given
 * @query. It will return all calendars the user has read access to, including primary, secondary and imported
 * calendars.
 *
 * For more details, see gdata_service_query().
 *
 * Return value: (transfer full): a #GDataFeed of query results; unref with g_object_unref()
 **/
GDataFeed *
gdata_calendar_service_query_all_calendars (GDataCalendarService *self, GDataQuery *query, GCancellable *cancellable,
                                            GDataQueryProgressCallback progress_callback, gpointer progress_user_data, GError **error)
{
	GDataFeed *feed;
	gchar *request_uri;

	g_return_val_if_fail (GDATA_IS_CALENDAR_SERVICE (self), NULL);
	g_return_val_if_fail (query == NULL || GDATA_IS_QUERY (query), NULL);
	g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* Ensure we're authenticated first */
	if (gdata_service_is_authenticated (GDATA_SERVICE (self)) == FALSE) {
		g_set_error_literal (error, GDATA_SERVICE_ERROR, GDATA_SERVICE_ERROR_AUTHENTICATION_REQUIRED,
		                     _("You must be authenticated to query all calendars."));
		return NULL;
	}

	request_uri = g_strconcat (_gdata_service_get_scheme (), "://www.google.com/calendar/feeds/default/allcalendars/full", NULL);
	feed = gdata_service_query (GDATA_SERVICE (self), request_uri, query, GDATA_TYPE_CALENDAR_CALENDAR,
	                            cancellable, progress_callback, progress_user_data, error);
	g_free (request_uri);

	return feed;
}

/**
 * gdata_calendar_service_query_all_calendars_async: (skip)
 * @self: a #GDataCalendarService
 * @query: (allow-none): a #GDataQuery with the query parameters, or %NULL
 * @cancellable: optional #GCancellable object, or %NULL
 * @progress_callback: a #GDataQueryProgressCallback to call when an entry is loaded, or %NULL
 * @progress_user_data: (closure): data to pass to the @progress_callback function
 * @callback: a #GAsyncReadyCallback to call when authentication is finished
 * @user_data: (closure): data to pass to the @callback function
 *
 * Queries the service to return a list of all calendars from the authenticated account which match the given
 * @query. @self and @query are all reffed when this function is called, so can safely be unreffed after
 * this function returns.
 *
 * For more details, see gdata_calendar_service_query_all_calendars(), which is the synchronous version of
 * this function, and gdata_service_query_async(), which is the base asynchronous query function.
 **/
void
gdata_calendar_service_query_all_calendars_async (GDataCalendarService *self, GDataQuery *query, GCancellable *cancellable,
                                                  GDataQueryProgressCallback progress_callback, gpointer progress_user_data,
                                                  GAsyncReadyCallback callback, gpointer user_data)
{
	gchar *request_uri;

	g_return_if_fail (GDATA_IS_CALENDAR_SERVICE (self));
	g_return_if_fail (query == NULL || GDATA_IS_QUERY (query));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
	g_return_if_fail (callback != NULL);

	/* Ensure we're authenticated first */
	if (gdata_service_is_authenticated (GDATA_SERVICE (self)) == FALSE) {
		g_simple_async_report_error_in_idle (G_OBJECT (self), callback, user_data,
		                                     GDATA_SERVICE_ERROR, GDATA_SERVICE_ERROR_AUTHENTICATION_REQUIRED,
		                                     _("You must be authenticated to query all calendars."));
		return;
	}

	request_uri = g_strconcat (_gdata_service_get_scheme (), "://www.google.com/calendar/feeds/default/allcalendars/full", NULL);
	gdata_service_query_async (GDATA_SERVICE (self), request_uri, query, GDATA_TYPE_CALENDAR_CALENDAR,
	                           cancellable, progress_callback, progress_user_data, callback, user_data);
	g_free (request_uri);
}

/**
 * gdata_calendar_service_query_own_calendars:
 * @self: a #GDataCalendarService
 * @query: (allow-none): a #GDataQuery with the query parameters, or %NULL
 * @cancellable: optional #GCancellable object, or %NULL
 * @progress_callback: (scope call): a #GDataQueryProgressCallback to call when an entry is loaded, or %NULL
 * @progress_user_data: (closure): data to pass to the @progress_callback function
 * @error: a #GError, or %NULL
 *
 * Queries the service to return a list of calendars from the authenticated account which match the given
 * @query, and the authenticated user owns. (i.e. They have full read/write access to the calendar, as well
 * as the ability to set permissions on the calendar.)
 *
 * For more details, see gdata_service_query().
 *
 * Return value: (transfer full): a #GDataFeed of query results; unref with g_object_unref()
 **/
GDataFeed *
gdata_calendar_service_query_own_calendars (GDataCalendarService *self, GDataQuery *query, GCancellable *cancellable,
                                            GDataQueryProgressCallback progress_callback, gpointer progress_user_data, GError **error)
{
	GDataFeed *feed;
	gchar *request_uri;

	g_return_val_if_fail (GDATA_IS_CALENDAR_SERVICE (self), NULL);
	g_return_val_if_fail (query == NULL || GDATA_IS_QUERY (query), NULL);
	g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* Ensure we're authenticated first */
	if (gdata_service_is_authenticated (GDATA_SERVICE (self)) == FALSE) {
		g_set_error_literal (error, GDATA_SERVICE_ERROR, GDATA_SERVICE_ERROR_AUTHENTICATION_REQUIRED,
		                     _("You must be authenticated to query your own calendars."));
		return NULL;
	}

	request_uri = g_strconcat (_gdata_service_get_scheme (), "://www.google.com/calendar/feeds/default/owncalendars/full", NULL);
	feed = gdata_service_query (GDATA_SERVICE (self), request_uri, query, GDATA_TYPE_CALENDAR_CALENDAR, cancellable,
	                            progress_callback, progress_user_data, error);
	g_free (request_uri);

	return feed;
}

/**
 * gdata_calendar_service_query_own_calendars_async: (skip)
 * @self: a #GDataCalendarService
 * @query: (allow-none): a #GDataQuery with the query parameters, or %NULL
 * @cancellable: optional #GCancellable object, or %NULL
 * @progress_callback: a #GDataQueryProgressCallback to call when an entry is loaded, or %NULL
 * @progress_user_data: (closure): data to pass to the @progress_callback function
 * @callback: a #GAsyncReadyCallback to call when authentication is finished
 * @user_data: (closure): data to pass to the @callback function
 *
 * Queries the service to return a list of calendars from the authenticated account which match the given
 * @query, and the authenticated user owns. @self and @query are all reffed when this function is called,
 * so can safely be unreffed after this function returns.
 *
 * For more details, see gdata_calendar_service_query_own_calendars(), which is the synchronous version of
 * this function, and gdata_service_query_async(), which is the base asynchronous query function.
 **/
void
gdata_calendar_service_query_own_calendars_async (GDataCalendarService *self, GDataQuery *query, GCancellable *cancellable,
                                                  GDataQueryProgressCallback progress_callback, gpointer progress_user_data,
                                                  GAsyncReadyCallback callback, gpointer user_data)
{
	gchar *request_uri;

	g_return_if_fail (GDATA_IS_CALENDAR_SERVICE (self));
	g_return_if_fail (query == NULL || GDATA_IS_QUERY (query));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
	g_return_if_fail (callback != NULL);

	/* Ensure we're authenticated first */
	if (gdata_service_is_authenticated (GDATA_SERVICE (self)) == FALSE) {
		g_simple_async_report_error_in_idle (G_OBJECT (self), callback, user_data,
		                                     GDATA_SERVICE_ERROR, GDATA_SERVICE_ERROR_AUTHENTICATION_REQUIRED,
		                                     _("You must be authenticated to query your own calendars."));
		return;
	}

	request_uri = g_strconcat (_gdata_service_get_scheme (), "://www.google.com/calendar/feeds/default/owncalendars/full", NULL);
	gdata_service_query_async (GDATA_SERVICE (self), request_uri, query, GDATA_TYPE_CALENDAR_CALENDAR,
	                           cancellable, progress_callback, progress_user_data, callback, user_data);
	g_free (request_uri);
}

/**
 * gdata_calendar_service_query_events:
 * @self: a #GDataCalendarService
 * @calendar: a #GDataCalendarCalendar
 * @query: (allow-none): a #GDataQuery with the query parameters, or %NULL
 * @cancellable: optional #GCancellable object, or %NULL
 * @progress_callback: (scope call): a #GDataQueryProgressCallback to call when an entry is loaded, or %NULL
 * @progress_user_data: (closure): data to pass to the @progress_callback function
 * @error: a #GError, or %NULL
 *
 * Queries the service to return a list of events in the given @calendar, which match @query.
 *
 * For more details, see gdata_service_query().
 *
 * Return value: (transfer full): a #GDataFeed of query results; unref with g_object_unref()
 **/
GDataFeed *
gdata_calendar_service_query_events (GDataCalendarService *self, GDataCalendarCalendar *calendar, GDataQuery *query, GCancellable *cancellable,
                                     GDataQueryProgressCallback progress_callback, gpointer progress_user_data, GError **error)
{
	const gchar *uri;

	g_return_val_if_fail (GDATA_IS_CALENDAR_SERVICE (self), NULL);
	g_return_val_if_fail (GDATA_IS_CALENDAR_CALENDAR (calendar), NULL);
	g_return_val_if_fail (query == NULL || GDATA_IS_QUERY (query), NULL);
	g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* Ensure we're authenticated first */
	if (gdata_service_is_authenticated (GDATA_SERVICE (self)) == FALSE) {
		g_set_error_literal (error, GDATA_SERVICE_ERROR, GDATA_SERVICE_ERROR_AUTHENTICATION_REQUIRED,
		                     _("You must be authenticated to query your own calendars."));
		return NULL;
	}

	/* Use the calendar's content src */
	uri = gdata_entry_get_content_uri (GDATA_ENTRY (calendar));
	if (uri == NULL) {
		/* Erroring out is probably the safest thing to do */
		g_set_error_literal (error, GDATA_SERVICE_ERROR, GDATA_SERVICE_ERROR_PROTOCOL_ERROR,
		                     _("The calendar did not have a content URI."));
		return NULL;
	}

	/* Execute the query */
	return gdata_service_query (GDATA_SERVICE (self), uri, query, GDATA_TYPE_CALENDAR_EVENT, cancellable,
	                            progress_callback, progress_user_data, error);
}

/**
 * gdata_calendar_service_query_events_async: (skip)
 * @self: a #GDataCalendarService
 * @calendar: a #GDataCalendarCalendar
 * @query: (allow-none): a #GDataQuery with the query parameters, or %NULL
 * @cancellable: optional #GCancellable object, or %NULL
 * @progress_callback: a #GDataQueryProgressCallback to call when an entry is loaded, or %NULL
 * @progress_user_data: (closure): data to pass to the @progress_callback function
 * @callback: a #GAsyncReadyCallback to call when the query is finished
 * @user_data: (closure): data to pass to the @callback function
 *
 * Queries the service to return a list of events in the given @calendar, which match @query. @self, @calendar and @query are all reffed when this
 * function is called, so can safely be unreffed after this function returns.
 *
 * Get the results of the query using gdata_service_query_finish() in the @callback.
 *
 * For more details, see gdata_calendar_service_query_events(), which is the synchronous version of this function, and gdata_service_query_async(),
 * which is the base asynchronous query function.
 *
 * Since: 0.8.0
 **/
void
gdata_calendar_service_query_events_async (GDataCalendarService *self, GDataCalendarCalendar *calendar, GDataQuery *query, GCancellable *cancellable,
                                           GDataQueryProgressCallback progress_callback, gpointer progress_user_data,
                                           GAsyncReadyCallback callback, gpointer user_data)
{
	const gchar *uri;

	g_return_if_fail (GDATA_IS_CALENDAR_SERVICE (self));
	g_return_if_fail (GDATA_IS_CALENDAR_CALENDAR (calendar));
	g_return_if_fail (query == NULL || GDATA_IS_QUERY (query));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
	g_return_if_fail (callback != NULL);

	/* Ensure we're authenticated first */
	if (gdata_service_is_authenticated (GDATA_SERVICE (self)) == FALSE) {
		g_simple_async_report_error_in_idle (G_OBJECT (self), callback, user_data,
		                                     GDATA_SERVICE_ERROR, GDATA_SERVICE_ERROR_AUTHENTICATION_REQUIRED,
		                                     _("You must be authenticated to query your own calendars."));
		return;
	}

	/* Use the calendar's content src */
	uri = gdata_entry_get_content_uri (GDATA_ENTRY (calendar));
	if (uri == NULL) {
		/* Erroring out is probably the safest thing to do */
		g_simple_async_report_error_in_idle (G_OBJECT (self), callback, user_data, GDATA_SERVICE_ERROR, GDATA_SERVICE_ERROR_PROTOCOL_ERROR,
		                                     _("The calendar did not have a content URI."));
		return;
	}

	/* Execute the query */
	gdata_service_query_async (GDATA_SERVICE (self), uri, query, GDATA_TYPE_CALENDAR_EVENT, cancellable, progress_callback, progress_user_data,
	                           callback, user_data);
}

/**
 * gdata_calendar_service_insert_event:
 * @self: a #GDataCalendarService
 * @event: the #GDataCalendarEvent to insert
 * @cancellable: optional #GCancellable object, or %NULL
 * @error: a #GError, or %NULL
 *
 * Inserts @event by uploading it to the online calendar service.
 *
 * For more details, see gdata_service_insert_entry().
 *
 * Return value: (transfer full): an updated #GDataCalendarEvent, or %NULL; unref with g_object_unref()
 *
 * Since: 0.2.0
 **/
GDataCalendarEvent *
gdata_calendar_service_insert_event (GDataCalendarService *self, GDataCalendarEvent *event, GCancellable *cancellable, GError **error)
{
	/* TODO: How do we choose which calendar? */
	gchar *uri;
	GDataEntry *entry;

	g_return_val_if_fail (GDATA_IS_CALENDAR_SERVICE (self), NULL);
	g_return_val_if_fail (GDATA_IS_CALENDAR_EVENT (event), NULL);
	g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	uri = g_strconcat (_gdata_service_get_scheme (), "://www.google.com/calendar/feeds/default/private/full", NULL);
	entry = gdata_service_insert_entry (GDATA_SERVICE (self), uri, GDATA_ENTRY (event), cancellable, error);
	g_free (uri);

	return GDATA_CALENDAR_EVENT (entry);
}

/**
 * gdata_calendar_service_insert_event_async:
 * @self: a #GDataCalendarService
 * @event: the #GDataCalendarEvent to insert
 * @cancellable: optional #GCancellable object, or %NULL
 * @callback: a #GAsyncReadyCallback to call when insertion is finished
 * @user_data: (closure): data to pass to the @callback function
 *
 * Inserts @event by uploading it to the online calendar service. @self and @event are both reffed when this function is called, so can safely be
 * unreffed after this function returns.
 *
 * @callback should call gdata_service_insert_entry_finish() to obtain a #GDataCalendarEvent representing the inserted event and to check for possible
 * errors.
 *
 * For more details, see gdata_calendar_service_insert_event(), which is the synchronous version of this function, and
 * gdata_service_insert_entry_async(), which is the base asynchronous insertion function.
 *
 * Since: 0.8.0
 **/
void
gdata_calendar_service_insert_event_async (GDataCalendarService *self, GDataCalendarEvent *event, GCancellable *cancellable,
                                           GAsyncReadyCallback callback, gpointer user_data)
{
	gchar *uri;

	g_return_if_fail (GDATA_IS_CALENDAR_SERVICE (self));
	g_return_if_fail (GDATA_IS_CALENDAR_EVENT (event));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

	uri = g_strconcat (_gdata_service_get_scheme (), "://www.google.com/calendar/feeds/default/private/full", NULL);
	gdata_service_insert_entry_async (GDATA_SERVICE (self), uri, GDATA_ENTRY (event), cancellable, callback, user_data);
	g_free (uri);
}
