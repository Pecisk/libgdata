/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * GData Client
 * Copyright (C) Philip Withnall 2008-2009 <philip@tecnocode.co.uk>
 * 
 * GData Client is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * GData Client is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GData Client.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <libsoup/soup.h>
#include <string.h>

#include "gdata-youtube-service.h"
#include "gdata-service.h"
#include "gdata-private.h"
#include "gdata-parser.h"

/* Standards reference here: http://code.google.com/apis/youtube/2.0/reference.html */

GQuark
gdata_youtube_service_error_quark (void)
{
	return g_quark_from_static_string ("gdata-youtube-service-error-quark");
}

static void gdata_youtube_service_finalize (GObject *object);
static void gdata_youtube_service_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void gdata_youtube_service_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static gboolean parse_authentication_response (GDataService *self, guint status, const gchar *response_body, gint length, GError **error);
static void append_query_headers (GDataService *self, SoupMessage *message);
static void parse_error_response (GDataService *self, guint status, const gchar *reason_phrase,
				  const gchar *response_body, gint length, GError **error);

struct _GDataYouTubeServicePrivate {
	gchar *youtube_user;
	gchar *developer_key;
};

enum {
	PROP_DEVELOPER_KEY = 1,
	PROP_YOUTUBE_USER
};

G_DEFINE_TYPE (GDataYouTubeService, gdata_youtube_service, GDATA_TYPE_SERVICE)
#define GDATA_YOUTUBE_SERVICE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GDATA_TYPE_YOUTUBE_SERVICE, GDataYouTubeServicePrivate))

static void
gdata_youtube_service_class_init (GDataYouTubeServiceClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GDataServiceClass *service_class = GDATA_SERVICE_CLASS (klass);

	g_type_class_add_private (klass, sizeof (GDataYouTubeServicePrivate));

	gobject_class->set_property = gdata_youtube_service_set_property;
	gobject_class->get_property = gdata_youtube_service_get_property;
	gobject_class->finalize = gdata_youtube_service_finalize;

	service_class->service_name = "youtube";
	service_class->authentication_uri = "https://www.google.com/youtube/accounts/ClientLogin";
	service_class->parse_authentication_response = parse_authentication_response;
	service_class->append_query_headers = append_query_headers;
	service_class->parse_error_response = parse_error_response;

	g_object_class_install_property (gobject_class, PROP_DEVELOPER_KEY,
				g_param_spec_string ("developer-key",
					"Developer key", "Your YouTube developer API key.",
					NULL,
					G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (gobject_class, PROP_YOUTUBE_USER,
				g_param_spec_string ("youtube-user",
					"YouTube username", "The YouTube account username.",
					NULL,
					G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
}

static void
gdata_youtube_service_init (GDataYouTubeService *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GDATA_TYPE_YOUTUBE_SERVICE, GDataYouTubeServicePrivate);
}

static void
gdata_youtube_service_finalize (GObject *object)
{
	GDataYouTubeServicePrivate *priv = GDATA_YOUTUBE_SERVICE_GET_PRIVATE (object);

	g_free (priv->youtube_user);
	g_free (priv->developer_key);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (gdata_youtube_service_parent_class)->finalize (object);
}

static void
gdata_youtube_service_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	GDataYouTubeServicePrivate *priv = GDATA_YOUTUBE_SERVICE_GET_PRIVATE (object);

	switch (property_id) {
		case PROP_DEVELOPER_KEY:
			g_value_set_string (value, priv->developer_key);
			break;
		case PROP_YOUTUBE_USER:
			g_value_set_string (value, priv->youtube_user);
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
gdata_youtube_service_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	GDataYouTubeServicePrivate *priv = GDATA_YOUTUBE_SERVICE_GET_PRIVATE (object);

	switch (property_id) {
		case PROP_DEVELOPER_KEY:
			priv->developer_key = g_value_dup_string (value);
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static gboolean
parse_authentication_response (GDataService *self, guint status, const gchar *response_body, gint length, GError **error)
{
	GDataYouTubeServicePrivate *priv = GDATA_YOUTUBE_SERVICE (self)->priv;
	gchar *user_start, *user_end;

	/* Chain up to the parent method first */
	if (GDATA_SERVICE_CLASS (gdata_youtube_service_parent_class)->parse_authentication_response (self, status,
												     response_body, length, error) == FALSE) {
		return FALSE;
	}

	/* Parse the response */
	user_start = strstr (response_body, "YouTubeUser=");
	if (user_start == NULL)
		goto protocol_error;
	user_start += strlen ("YouTubeUser=");

	user_end = strstr (user_start, "\n");
	if (user_end == NULL)
		goto protocol_error;

	priv->youtube_user = g_strndup (user_start, user_end - user_start);
	if (priv->youtube_user == NULL || strlen (priv->youtube_user) == 0)
		goto protocol_error;

	return TRUE;

protocol_error:
	g_set_error_literal (error, GDATA_SERVICE_ERROR, GDATA_SERVICE_ERROR_PROTOCOL_ERROR,
			     _("The server returned a malformed response."));
	return FALSE;
}

static void
append_query_headers (GDataService *self, SoupMessage *message)
{
	GDataYouTubeServicePrivate *priv = GDATA_YOUTUBE_SERVICE (self)->priv;
	gchar *key_header;

	g_assert (message != NULL);

	/* Dev key and client headers */
	key_header = g_strdup_printf ("key=%s", priv->developer_key);
	soup_message_headers_append (message->request_headers, "X-GData-Key", key_header);
	g_free (key_header);

	soup_message_headers_append (message->request_headers, "X-GData-Client", gdata_service_get_client_id (self));

	/* Chain up to the parent class */
	GDATA_SERVICE_CLASS (gdata_youtube_service_parent_class)->append_query_headers (self, message);
}

static void
parse_error_response (GDataService *self, guint status, const gchar *reason_phrase, const gchar *response_body, gint length, GError **error)
{
	xmlDoc *doc;
	xmlNode *node;

	g_return_if_fail (response_body != NULL);

	if (length == -1)
		length = strlen (response_body);

	/* Parse the XML */
	doc = xmlReadMemory (response_body, length, "error.xml", NULL, 0);
	if (doc == NULL) {
		/* Chain up to the parent class */
		GDATA_SERVICE_CLASS (gdata_youtube_service_parent_class)->parse_error_response (self, status, reason_phrase,
												response_body, length, error);
		return;
	}

	/* Get the root element */
	node = xmlDocGetRootElement (doc);
	if (node == NULL) {
		/* XML document's empty; chain up to the parent class */
		xmlFreeDoc (doc);
		GDATA_SERVICE_CLASS (gdata_youtube_service_parent_class)->parse_error_response (self, status, reason_phrase,
												response_body, length, error);
		return;
	}

	if (xmlStrcmp (node->name, (xmlChar*) "errors") != 0) {
		/* No <errors> element (required); chain up to the parent class */
		xmlFreeDoc (doc);
		GDATA_SERVICE_CLASS (gdata_youtube_service_parent_class)->parse_error_response (self, status, reason_phrase,
												response_body, length, error);
		return;
	}

	/* Parse the actual errors */
	node = node->xmlChildrenNode;
	while (node != NULL) {
		xmlChar *domain = NULL, *code = NULL, *location = NULL;
		xmlNode *child_node = node->xmlChildrenNode;

		/* Get the error data */
		while (child_node != NULL) {
			if (xmlStrcmp (child_node->name, (xmlChar*) "domain") == 0)
				domain = xmlNodeListGetString (doc, child_node->xmlChildrenNode, TRUE);
			else if (xmlStrcmp (child_node->name, (xmlChar*) "code") == 0)
				code = xmlNodeListGetString (doc, child_node->xmlChildrenNode, TRUE);
			else if (xmlStrcmp (child_node->name, (xmlChar*) "location") == 0)
				location = xmlNodeListGetString (doc, child_node->xmlChildrenNode, TRUE);
			else {
				/* Unknown element */
				gdata_parser_error_unhandled_element ((gchar*) child_node->ns->prefix, (gchar*) child_node->name, "error", error);
				xmlFree (domain);
				xmlFree (code);
				xmlFree (location);
				xmlFreeDoc (doc);
				return;
			}

			child_node = child_node->next;
		}

		/* Create an error message, but only for the first error */
		if (*error == NULL) {
			/* See http://code.google.com/apis/youtube/2.0/developers_guide_protocol.html#Error_responses */
			if (xmlStrcmp (domain, (xmlChar*) "yt:service") == 0 && xmlStrcmp (code, (xmlChar*) "disabled_in_maintenance_mode") == 0) {
				/* Service disabled */
				g_set_error (error, GDATA_SERVICE_ERROR, GDATA_SERVICE_ERROR_UNAVAILABLE,
					     _("This service is not available at the moment."));
			} else if (xmlStrcmp (domain, (xmlChar*) "yt:authentication") == 0) {
				/* Authentication problem; make sure to set our status as unauthenticated */
				gdata_service_set_authenticated (GDATA_SERVICE (self), FALSE);
				g_set_error (error, GDATA_SERVICE_ERROR, GDATA_SERVICE_ERROR_AUTHENTICATION_REQUIRED,
					     _("You must be authenticated to do this."));
			} else if (xmlStrcmp (domain, (xmlChar*) "yt:quota") == 0) {
				/* Quota errors */
				if (xmlStrcmp (code, (xmlChar*) "too_many_recent_calls") == 0) {
					g_set_error (error, GDATA_YOUTUBE_SERVICE_ERROR, GDATA_YOUTUBE_SERVICE_ERROR_API_QUOTA_EXCEEDED,
						     _("You have made too many API calls recently. Please wait a few minutes and try again."));
				} else if (xmlStrcmp (code, (xmlChar*) "too_many_entries") == 0) {
					g_set_error (error, GDATA_YOUTUBE_SERVICE_ERROR, GDATA_YOUTUBE_SERVICE_ERROR_ENTRY_QUOTA_EXCEEDED,
						     _("You have exceeded your entry quota with the entry \"%s\"."
						       "Please delete some entries and try again."), location);
				} else {
					/* Protocol error */
					g_set_error (error, GDATA_SERVICE_ERROR, GDATA_SERVICE_ERROR_PROTOCOL_ERROR,
						     _("Unknown error code \"%s\" in domain \"%s\" received with location \"%s\"."),
						     code, domain, location);
				}
			} else {
				/* Unknown or validation (protocol) error */
				g_set_error (error, GDATA_SERVICE_ERROR, GDATA_SERVICE_ERROR_PROTOCOL_ERROR,
					     _("Unknown error code \"%s\" in domain \"%s\" received with location \"%s\"."),
					     code, domain, location);
			}
		} else {
			/* For all errors after the first, log the error in the terminal */
			g_debug ("Error message received in response: code \"%s\", domain \"%s\", location \"%s\".", code, domain, location);
		}

		xmlFree (domain);
		xmlFree (code);
		xmlFree (location);

		node = node->next;
	}
}

GDataYouTubeService *
gdata_youtube_service_new (const gchar *developer_key, const gchar *client_id)
{
	g_return_val_if_fail (developer_key != NULL, NULL);
	g_return_val_if_fail (client_id != NULL, NULL);

	return g_object_new (GDATA_TYPE_YOUTUBE_SERVICE,
			     "developer-key", developer_key,
			     "client-id", client_id,
			     NULL);
}

static const gchar *
standard_feed_type_to_feed_uri (GDataYouTubeStandardFeedType feed_type)
{
	switch (feed_type) {
	case GDATA_YOUTUBE_TOP_RATED_FEED:
		return "http://gdata.youtube.com/feeds/api/standardfeeds/top_rated";
	case GDATA_YOUTUBE_TOP_FAVORITES_FEED:
		return "http://gdata.youtube.com/feeds/api/standardfeeds/top_favorites";
	case GDATA_YOUTUBE_MOST_VIEWED_FEED:
		return "http://gdata.youtube.com/feeds/api/standardfeeds/most_viewed";
	case GDATA_YOUTUBE_MOST_POPULAR_FEED:
		return "http://gdata.youtube.com/feeds/api/standardfeeds/most_popular";
	case GDATA_YOUTUBE_MOST_RECENT_FEED:
		return "http://gdata.youtube.com/feeds/api/standardfeeds/most_recent";
	case GDATA_YOUTUBE_MOST_DISCUSSED_FEED:
		return "http://gdata.youtube.com/feeds/api/standardfeeds/most_discussed";
	case GDATA_YOUTUBE_MOST_LINKED_FEED:
		return "http://gdata.youtube.com/feeds/api/standardfeeds/most_linked";
	case GDATA_YOUTUBE_MOST_RESPONDED_FEED:
		return "http://gdata.youtube.com/feeds/api/standardfeeds/most_responded";
	case GDATA_YOUTUBE_RECENTLY_FEATURED_FEED:
		return "http://gdata.youtube.com/feeds/api/standardfeeds/recently_featured";
	case GDATA_YOUTUBE_WATCH_ON_MOBILE_FEED:
		return "http://gdata.youtube.com/feeds/api/standardfeeds/watch_on_mobile";
	default:
		g_assert_not_reached ();
	}
}

GDataFeed *
gdata_youtube_service_query_standard_feed (GDataYouTubeService *self, GDataYouTubeStandardFeedType feed_type, GDataQuery *query,
					   GCancellable *cancellable, GDataQueryProgressCallback progress_callback, gpointer progress_user_data,
					   GError **error)
{
	/* TODO: Support the "time" parameter, as well as category- and region-specific feeds */
	return gdata_service_query (GDATA_SERVICE (self), standard_feed_type_to_feed_uri (feed_type), query,
				    (GDataEntryParserFunc) _gdata_youtube_video_new_from_xml_node, cancellable,
				    progress_callback, progress_user_data, error);
}

void
gdata_youtube_service_query_standard_feed_async (GDataYouTubeService *self, GDataYouTubeStandardFeedType feed_type, GDataQuery *query,
						 GCancellable *cancellable, GDataQueryProgressCallback progress_callback, gpointer progress_user_data,
						 GAsyncReadyCallback callback, gpointer user_data)
{
	gdata_service_query_async (GDATA_SERVICE (self), standard_feed_type_to_feed_uri (feed_type), query,
				   (GDataEntryParserFunc) _gdata_youtube_video_new_from_xml_node, cancellable, progress_callback, progress_user_data,
				   callback, user_data);
}

GDataFeed *
gdata_youtube_service_query_videos (GDataYouTubeService *self, GDataQuery *query,
				    GCancellable *cancellable, GDataQueryProgressCallback progress_callback, gpointer progress_user_data,
				    GError **error)
{
	return gdata_service_query (GDATA_SERVICE (self), "http://gdata.youtube.com/feeds/api/videos", query,
				    (GDataEntryParserFunc) _gdata_youtube_video_new_from_xml_node, cancellable,
				    progress_callback, progress_user_data, error);
}

void
gdata_youtube_service_query_videos_async (GDataYouTubeService *self, GDataQuery *query,
					  GCancellable *cancellable, GDataQueryProgressCallback progress_callback, gpointer progress_user_data,
					  GAsyncReadyCallback callback, gpointer user_data)
{
	gdata_service_query_async (GDATA_SERVICE (self), "http://gdata.youtube.com/feeds/api/videos", query,
				   (GDataEntryParserFunc) _gdata_youtube_video_new_from_xml_node, cancellable, progress_callback, progress_user_data,
				   callback, user_data);
}

GDataFeed *
gdata_youtube_service_query_related (GDataYouTubeService *self, GDataYouTubeVideo *video, GDataQuery *query,
				     GCancellable *cancellable, GDataQueryProgressCallback progress_callback, gpointer progress_user_data,
				     GError **error)
{
	GDataLink *related_link;

	/* See if the video already has a rel="http://gdata.youtube.com/schemas/2007#video.related" link */
	related_link = gdata_entry_lookup_link (GDATA_ENTRY (video), "http://gdata.youtube.com/schemas/2007#video.related");
	if (related_link == NULL) {
		/* Erroring out is probably the safest thing to do */
		g_set_error_literal (error, GDATA_SERVICE_ERROR, GDATA_SERVICE_ERROR_PROTOCOL_ERROR,
				     _("The video didn't have a related videos <link>."));
		return NULL;
	}

	/* Execute the query */
	return gdata_service_query (GDATA_SERVICE (self), related_link->href, query,
				    (GDataEntryParserFunc) _gdata_youtube_video_new_from_xml_node, cancellable,
				    progress_callback, progress_user_data, error);
}

void
gdata_youtube_service_query_related_async (GDataYouTubeService *self, GDataYouTubeVideo *video, GDataQuery *query,
					   GCancellable *cancellable, GDataQueryProgressCallback progress_callback, gpointer progress_user_data,
					   GAsyncReadyCallback callback, gpointer user_data)
{
	GDataLink *related_link;

	/* See if the video already has a rel="http://gdata.youtube.com/schemas/2007#video.related" link */
	related_link = gdata_entry_lookup_link (GDATA_ENTRY (video), "http://gdata.youtube.com/schemas/2007#video.related");
	if (related_link == NULL) {
		/* Erroring out is probably the safest thing to do */
		g_simple_async_report_error_in_idle (G_OBJECT (self), callback, user_data,
						     GDATA_SERVICE_ERROR, GDATA_SERVICE_ERROR_PROTOCOL_ERROR,
						     _("The video didn't have a related videos <link>."));
		return;
	}

	gdata_service_query_async (GDATA_SERVICE (self), related_link->href, query,
				   (GDataEntryParserFunc) _gdata_youtube_video_new_from_xml_node, cancellable, progress_callback, progress_user_data,
				   callback, user_data);
}

/* TODO: Async variant */
GDataYouTubeVideo *
gdata_youtube_service_upload_video (GDataYouTubeService *self, GDataYouTubeVideo *video, GFile *video_file,
				    GCancellable *cancellable, GError **error)
{
	#define BOUNDARY_STRING "0xdeadbeef6e0808d5e6ed8bc168390bcc"

	GDataServiceClass *klass;
	SoupMessage *message;
	gchar *entry_xml, *upload_uri, *second_chunk_header, *upload_data, *video_contents, *i;
	const gchar *first_chunk_header, *footer;
	guint status;
	GFileInfo *video_file_info;
	gsize content_length, first_chunk_header_length, second_chunk_header_length, entry_xml_length, video_length, footer_length;

	g_return_val_if_fail (GDATA_IS_YOUTUBE_SERVICE (self), NULL);
	g_return_val_if_fail (GDATA_IS_YOUTUBE_VIDEO (video), NULL);

	if (gdata_entry_inserted (GDATA_ENTRY (video)) == TRUE) {
		g_set_error_literal (error, GDATA_SERVICE_ERROR, GDATA_SERVICE_ERROR_ENTRY_ALREADY_INSERTED,
				     _("The entry has already been inserted."));
		return NULL;
	}

	if (gdata_service_is_authenticated (GDATA_SERVICE (self)) == FALSE) {
		g_set_error_literal (error, GDATA_SERVICE_ERROR, GDATA_SERVICE_ERROR_AUTHENTICATION_REQUIRED,
				     _("You must be authenticated to upload a video."));
		return NULL;
	}

	upload_uri = g_strdup_printf ("http://uploads.gdata.youtube.com/feeds/api/users/%s/uploads", gdata_service_get_username (GDATA_SERVICE (self)));
	message = soup_message_new (SOUP_METHOD_POST, upload_uri);
	g_free (upload_uri);

	/* Make sure subclasses set their headers */
	klass = GDATA_SERVICE_GET_CLASS (self);
	if (klass->append_query_headers != NULL)
		klass->append_query_headers (GDATA_SERVICE (self), message);

	/* Get the data early so we can calculate the content length */
	if (g_file_load_contents (video_file, NULL, &video_contents, &video_length, NULL, error) == FALSE) {
		g_object_unref (message);
		return NULL;
	}

	entry_xml = gdata_entry_get_xml (GDATA_ENTRY (video));

	/* Check for cancellation */
	if (g_cancellable_set_error_if_cancelled (cancellable, error) == TRUE) {
		g_object_unref (message);
		g_free (entry_xml);
		return NULL;
	}

	video_file_info = g_file_query_info (video_file, "standard::display-name,standard::content-type", G_FILE_QUERY_INFO_NONE, NULL, error);
	if (video_file_info == NULL) {
		g_object_unref (message);
		g_free (entry_xml);
		return NULL;
	}

	/* Check for cancellation */
	if (g_cancellable_set_error_if_cancelled (cancellable, error) == TRUE) {
		g_object_unref (message);
		g_free (entry_xml);
		g_object_unref (video_file_info);
		return NULL;
	}

	/* Add video-upload--specific headers */
	soup_message_headers_append (message->request_headers, "Slug", g_file_info_get_display_name (video_file_info));

	first_chunk_header = "--" BOUNDARY_STRING "\nContent-Type: application/atom+xml; charset=UTF-8\n\n<?xml version='1.0'?>";
	second_chunk_header = g_strdup_printf ("\n--" BOUNDARY_STRING "\nContent-Type: %s\nContent-Transfer-Encoding: binary\n\n",
					       g_file_info_get_content_type (video_file_info));
	footer = "\n--" BOUNDARY_STRING "--";

	g_object_unref (video_file_info);

	first_chunk_header_length = strlen (first_chunk_header);
	second_chunk_header_length = strlen (second_chunk_header);
	footer_length = strlen (footer);
	entry_xml_length = strlen (entry_xml);

	content_length = first_chunk_header_length + entry_xml_length + second_chunk_header_length + video_length + footer_length;

	/* Build the upload data */
	upload_data = i = g_malloc (content_length);

	memcpy (upload_data, first_chunk_header, first_chunk_header_length);
	i += first_chunk_header_length;

	memcpy (i, entry_xml, entry_xml_length);
	i += entry_xml_length;
	g_free (entry_xml);

	memcpy (i, second_chunk_header, second_chunk_header_length);
	g_free (second_chunk_header);
	i += second_chunk_header_length;

	memcpy (i, video_contents, video_length);
	g_free (video_contents);
	i += video_length;

	memcpy (i, footer, footer_length);

	/* Append the data */
	soup_message_set_request (message, "multipart/related; boundary=" BOUNDARY_STRING, SOUP_MEMORY_TAKE, upload_data, content_length);

	/* Send the message */
	status = soup_session_send_message (gdata_service_get_session (GDATA_SERVICE (self)), message);

	/* Check for cancellation */
	if (g_cancellable_set_error_if_cancelled (cancellable, error) == TRUE) {
		g_object_unref (message);
		return NULL;
	}

	if (status != 201) {
		/* Error */
		parse_error_response (GDATA_SERVICE (self), status, message->reason_phrase,
				      message->response_body->data, message->response_body->length, error);
		g_object_unref (message);
		return NULL;
	}

	g_assert (message->response_body->data != NULL);

	return gdata_youtube_video_new_from_xml (message->response_body->data, (gint) message->response_body->length, error);
}

const gchar *
gdata_youtube_service_get_developer_key (GDataYouTubeService *self)
{
	g_return_val_if_fail (GDATA_IS_YOUTUBE_SERVICE (self), NULL);
	return self->priv->developer_key;
}

const gchar *
gdata_youtube_service_get_youtube_user (GDataYouTubeService *self)
{
	g_return_val_if_fail (GDATA_IS_YOUTUBE_SERVICE (self), NULL);
	return self->priv->youtube_user;
}
