/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * GData Client
 * Copyright (C) Philip Withnall 2009–2010 <philip@tecnocode.co.uk>
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
 * SECTION:gdata-calendar-event
 * @short_description: GData Calendar event object
 * @stability: Unstable
 * @include: gdata/services/calendar/gdata-calendar-event.h
 *
 * #GDataCalendarEvent is a subclass of #GDataEntry to represent an event on a calendar from Google Calendar.
 *
 * For more details of Google Calendar's GData API, see the <ulink type="http" url="http://code.google.com/apis/calendar/docs/2.0/reference.html">
 * online documentation</ulink>.
 *
 * <example>
 * 	<title>Adding a New Event to the Default Calendar</title>
 * 	<programlisting>
 *	GDataCalendarService *service;
 *	GDataCalendarEvent *event, *new_event;
 *	GDataGDWhere *where;
 *	GDataGDWho *who;
 *	GDataGDWhen *when;
 *	GTimeVal current_time;
 *	GError *error = NULL;
 *
 *	/<!-- -->* Create a service *<!-- -->/
 *	service = create_calendar_service ();
 *
 *	/<!-- -->* Create the new event *<!-- -->/
 *	event = gdata_calendar_event_new (NULL);
 *
 *	gdata_entry_set_title (GDATA_ENTRY (event), "Event Title");
 *	gdata_entry_set_content (GDATA_ENTRY (event), "Event description. This should be a few sentences long.");
 *	gdata_calendar_event_set_status (event, GDATA_GD_EVENT_STATUS_CONFIRMED);
 *
 *	where = gdata_gd_where_new (NULL, "Description of the location", NULL);
 *	gdata_calendar_event_add_place (event, where);
 *	g_object_unref (where);
 *
 *	who = gdata_gd_who_new (GDATA_GD_WHO_EVENT_ORGANIZER, "John Smith", "john.smith@gmail.com");
 *	gdata_calendar_event_add_person (event, who);
 *	g_object_unref (who);
 *
 *	g_get_current_time (&current_time);
 *	when = gdata_gd_when_new (current_time.tv_sec, current_time.tv_sec + 3600, FALSE);
 *	gdata_calendar_event_add_time (event, when);
 *	g_object_unref (when);
 *
 *	/<!-- -->* Insert the event in the calendar *<!-- -->/
 *	new_event = gdata_calendar_service_insert_event (service, event, NULL, &error);
 *
 *	g_object_unref (event);
 *	g_object_unref (service);
 *
 *	if (error != NULL) {
 *		g_error ("Error inserting event: %s", error->message);
 *		g_error_free (error);
 *		return NULL;
 *	}
 *
 *	/<!-- -->* Do something with the new_event here, such as return it to the user or store its ID for later usage *<!-- -->/
 *
 *	g_object_unref (new_event);
 * 	</programlisting>
 * </example>
 **/

#include <config.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <libxml/parser.h>
#include <string.h>

#include "gdata-calendar-event.h"
#include "gdata-private.h"
#include "gdata-service.h"
#include "gdata-parser.h"
#include "gdata-types.h"
#include "gdata-comparable.h"

static GObject *gdata_calendar_event_constructor (GType type, guint n_construct_params, GObjectConstructParam *construct_params);
static void gdata_calendar_event_dispose (GObject *object);
static void gdata_calendar_event_finalize (GObject *object);
static void gdata_calendar_event_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void gdata_calendar_event_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void get_xml (GDataParsable *parsable, GString *xml_string);
static gboolean parse_xml (GDataParsable *parsable, xmlDoc *doc, xmlNode *node, gpointer user_data, GError **error);
static void get_namespaces (GDataParsable *parsable, GHashTable *namespaces);

struct _GDataCalendarEventPrivate {
	gint64 edited;
	gchar *status;
	gchar *visibility;
	gchar *transparency;
	gchar *uid;
	guint sequence;
	GList *times; /* GDataGDWhen */
	guint guests_can_modify : 1;
	guint guests_can_invite_others : 1;
	guint guests_can_see_guests : 1;
	guint anyone_can_add_self : 1;
	GList *people; /* GDataGDWho */
	GList *places; /* GDataGDWhere */
	gchar *recurrence;
	gchar *original_event_id;
	gchar *original_event_uri;
};

enum {
	PROP_EDITED = 1,
	PROP_STATUS,
	PROP_VISIBILITY,
	PROP_TRANSPARENCY,
	PROP_UID,
	PROP_SEQUENCE,
	PROP_GUESTS_CAN_MODIFY,
	PROP_GUESTS_CAN_INVITE_OTHERS,
	PROP_GUESTS_CAN_SEE_GUESTS,
	PROP_ANYONE_CAN_ADD_SELF,
	PROP_RECURRENCE,
	PROP_ORIGINAL_EVENT_ID,
	PROP_ORIGINAL_EVENT_URI
};

G_DEFINE_TYPE (GDataCalendarEvent, gdata_calendar_event, GDATA_TYPE_ENTRY)

static void
gdata_calendar_event_class_init (GDataCalendarEventClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GDataParsableClass *parsable_class = GDATA_PARSABLE_CLASS (klass);
	GDataEntryClass *entry_class = GDATA_ENTRY_CLASS (klass);

	g_type_class_add_private (klass, sizeof (GDataCalendarEventPrivate));

	gobject_class->constructor = gdata_calendar_event_constructor;
	gobject_class->get_property = gdata_calendar_event_get_property;
	gobject_class->set_property = gdata_calendar_event_set_property;
	gobject_class->dispose = gdata_calendar_event_dispose;
	gobject_class->finalize = gdata_calendar_event_finalize;

	parsable_class->parse_xml = parse_xml;
	parsable_class->get_xml = get_xml;
	parsable_class->get_namespaces = get_namespaces;

	entry_class->kind_term = "http://schemas.google.com/g/2005#event";

	/**
	 * GDataCalendarEvent:edited:
	 *
	 * The last time the event was edited. If the event has not been edited yet, the content indicates the time it was created.
	 *
	 * For more information, see the <ulink type="http" url="http://www.atomenabled.org/developers/protocol/#appEdited">
	 * Atom Publishing Protocol specification</ulink>.
	 **/
	g_object_class_install_property (gobject_class, PROP_EDITED,
	                                 g_param_spec_int64 ("edited",
	                                                     "Edited", "The last time the event was edited.",
	                                                     -1, G_MAXINT64, -1,
	                                                     G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

	/**
	 * GDataCalendarEvent:status:
	 *
	 * The scheduling status of the event. For example: %GDATA_GD_EVENT_STATUS_CANCELED or %GDATA_GD_EVENT_STATUS_CONFIRMED.
	 *
	 * For more information, see the <ulink type="http" url="http://code.google.com/apis/gdata/elements.html#gdEventStatus">
	 * GData specification</ulink>.
	 *
	 * Since: 0.2.0
	 **/
	g_object_class_install_property (gobject_class, PROP_STATUS,
	                                 g_param_spec_string ("status",
	                                                      "Status", "The scheduling status of the event.",
	                                                      NULL,
	                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * GDataCalendarEvent:visibility:
	 *
	 * The event's visibility to calendar users. For example: %GDATA_GD_EVENT_VISIBILITY_PUBLIC or %GDATA_GD_EVENT_VISIBILITY_DEFAULT.
	 *
	 * For more information, see the <ulink type="http" url="http://code.google.com/apis/gdata/elements.html#gdVisibility">
	 * GData specification</ulink>.
	 **/
	g_object_class_install_property (gobject_class, PROP_VISIBILITY,
	                                 g_param_spec_string ("visibility",
	                                                      "Visibility", "The event's visibility to calendar users.",
	                                                      NULL,
	                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * GDataCalendarEvent:transparency:
	 *
	 * How the event is marked as consuming time on a calendar. For example: %GDATA_GD_EVENT_TRANSPARENCY_OPAQUE or
	 * %GDATA_GD_EVENT_TRANSPARENCY_TRANSPARENT.
	 *
	 * For more information, see the <ulink type="http" url="http://code.google.com/apis/gdata/elements.html#gdTransparency">
	 * GData specification</ulink>.
	 **/
	g_object_class_install_property (gobject_class, PROP_TRANSPARENCY,
	                                 g_param_spec_string ("transparency",
	                                                      "Transparency", "How the event is marked as consuming time on a calendar.",
	                                                      NULL,
	                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * GDataCalendarEvent:uid:
	 *
	 * The globally unique identifier (UID) of the event as defined in Section 4.8.4.7 of <ulink type="http"
	 * url="http://www.ietf.org/rfc/rfc2445.txt">RFC 2445</ulink>.
	 **/
	g_object_class_install_property (gobject_class, PROP_UID,
	                                 g_param_spec_string ("uid",
	                                                      "UID", "The globally unique identifier (UID) of the event.",
	                                                      NULL,
	                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * GDataCalendarEvent:sequence:
	 *
	 * The revision sequence number of the event as defined in Section 4.8.7.4 of <ulink type="http"
	 * url="http://www.ietf.org/rfc/rfc2445.txt">RFC 2445</ulink>.
	 **/
	g_object_class_install_property (gobject_class, PROP_SEQUENCE,
	                                 g_param_spec_uint ("sequence",
	                                                    "Sequence", "The revision sequence number of the event.",
	                                                    0, G_MAXUINT, 0,
	                                                    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * GDataCalendarEvent:guests-can-modify:
	 *
	 * Indicates whether attendees may modify the original event, so that changes are visible to organizers and other attendees.
	 * Otherwise, any changes made by attendees will be restricted to that attendee's calendar.
	 *
	 * For more information, see the <ulink type="http" url="http://code.google.com/apis/calendar/docs/2.0/reference.html#gCalguestsCanModify">
	 * GData specification</ulink>.
	 **/
	g_object_class_install_property (gobject_class, PROP_GUESTS_CAN_MODIFY,
	                                 g_param_spec_boolean ("guests-can-modify",
	                                                       "Guests can modify", "Indicates whether attendees may modify the original event.",
	                                                       FALSE,
	                                                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * GDataCalendarEvent:guests-can-invite-others:
	 *
	 * Indicates whether attendees may invite others to the event.
	 *
	 * For more information, see the <ulink type="http"
	 * url="http://code.google.com/apis/calendar/docs/2.0/reference.html#gCalguestsCanInviteOthers">GData specification</ulink>.
	 **/
	g_object_class_install_property (gobject_class, PROP_GUESTS_CAN_INVITE_OTHERS,
	                                 g_param_spec_boolean ("guests-can-invite-others",
	                                                       "Guests can invite others", "Indicates whether attendees may invite others.",
	                                                       FALSE,
	                                                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * GDataCalendarEvent:guests-can-see-guests:
	 *
	 * Indicates whether attendees can see other people invited to the event.
	 *
	 * For more information, see the <ulink type="http" url="http://code.google.com/apis/calendar/docs/2.0/reference.html#gCalguestsCanSeeGuests">
	 * GData specification</ulink>.
	 **/
	g_object_class_install_property (gobject_class, PROP_GUESTS_CAN_SEE_GUESTS,
	                                 g_param_spec_boolean ("guests-can-see-guests",
	                                                       "Guests can see guests", "Indicates whether attendees can see other people invited.",
	                                                       FALSE,
	                                                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * GDataCalendarEvent:anyone-can-add-self:
	 *
	 * Indicates whether anyone can invite themselves to the event, by adding themselves to the attendee list.
	 **/
	g_object_class_install_property (gobject_class, PROP_ANYONE_CAN_ADD_SELF,
	                                 g_param_spec_boolean ("anyone-can-add-self",
	                                                       "Anyone can add self", "Indicates whether anyone can invite themselves to the event.",
	                                                       FALSE,
	                                                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * GDataCalendarEvent:recurrence:
	 *
	 * Represents the dates and times when a recurring event takes place. The returned string is in iCal format, as a list of properties.
	 *
	 * For more information, see the <ulink type="http" url="http://code.google.com/apis/gdata/elements.html#gdRecurrence">
	 * GData specification</ulink>.
	 *
	 * Note: gdata_calendar_event_add_time() and gdata_calendar_event_set_recurrence() are mutually
	 * exclusive. See the documentation for gdata_calendar_event_add_time() for details.
	 *
	 * Since: 0.3.0
	 **/
	g_object_class_install_property (gobject_class, PROP_RECURRENCE,
	                                 g_param_spec_string ("recurrence",
	                                                      "Recurrence", "Represents the dates and times when a recurring event takes place.",
	                                                      NULL,
	                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * GDataCalendarEvent:original-event-id:
	 *
	 * The event ID for the original event, if this event is an exception to a recurring event.
	 *
	 * Since: 0.3.0
	 **/
	g_object_class_install_property (gobject_class, PROP_ORIGINAL_EVENT_ID,
	                                 g_param_spec_string ("original-event-id",
	                                                      "Original event ID", "The event ID for the original event.",
	                                                      NULL,
	                                                      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

	/**
	 * GDataCalendarEvent:original-event-uri:
	 *
	 * The event URI for the original event, if this event is an exception to a recurring event.
	 *
	 * Since: 0.3.0
	 **/
	g_object_class_install_property (gobject_class, PROP_ORIGINAL_EVENT_URI,
	                                 g_param_spec_string ("original-event-uri",
	                                                      "Original event URI", "The event URI for the original event.",
	                                                      NULL,
	                                                      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
}

static void
gdata_calendar_event_init (GDataCalendarEvent *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GDATA_TYPE_CALENDAR_EVENT, GDataCalendarEventPrivate);
	self->priv->edited = -1;
}

static GObject *
gdata_calendar_event_constructor (GType type, guint n_construct_params, GObjectConstructParam *construct_params)
{
	GObject *object;

	/* Chain up to the parent class */
	object = G_OBJECT_CLASS (gdata_calendar_event_parent_class)->constructor (type, n_construct_params, construct_params);

	if (_gdata_parsable_is_constructed_from_xml (GDATA_PARSABLE (object)) == FALSE) {
		GDataCalendarEventPrivate *priv = GDATA_CALENDAR_EVENT (object)->priv;
		GTimeVal time_val;

		/* Set the edited property to the current time (creation time). We don't do this in *_init() since that would cause
		 * setting it from parse_xml() to fail (duplicate element). */
		g_get_current_time (&time_val);
		priv->edited = time_val.tv_sec;
	}

	return object;
}

static void
gdata_calendar_event_dispose (GObject *object)
{
	GDataCalendarEventPrivate *priv = GDATA_CALENDAR_EVENT (object)->priv;

	if (priv->times != NULL) {
		g_list_foreach (priv->times, (GFunc) g_object_unref, NULL);
		g_list_free (priv->times);
	}
	priv->times = NULL;

	if (priv->people != NULL) {
		g_list_foreach (priv->people, (GFunc) g_object_unref, NULL);
		g_list_free (priv->people);
	}
	priv->people = NULL;

	if (priv->places != NULL) {
		g_list_foreach (priv->places, (GFunc) g_object_unref, NULL);
		g_list_free (priv->places);
	}
	priv->places = NULL;

	/* Chain up to the parent class */
	G_OBJECT_CLASS (gdata_calendar_event_parent_class)->dispose (object);
}

static void
gdata_calendar_event_finalize (GObject *object)
{
	GDataCalendarEventPrivate *priv = GDATA_CALENDAR_EVENT (object)->priv;

	g_free (priv->status);
	g_free (priv->visibility);
	g_free (priv->transparency);
	g_free (priv->uid);
	g_free (priv->recurrence);
	g_free (priv->original_event_id);
	g_free (priv->original_event_uri);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (gdata_calendar_event_parent_class)->finalize (object);
}

static void
gdata_calendar_event_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	GDataCalendarEventPrivate *priv = GDATA_CALENDAR_EVENT (object)->priv;

	switch (property_id) {
		case PROP_EDITED:
			g_value_set_int64 (value, priv->edited);
			break;
		case PROP_STATUS:
			g_value_set_string (value, priv->status);
			break;
		case PROP_VISIBILITY:
			g_value_set_string (value, priv->visibility);
			break;
		case PROP_TRANSPARENCY:
			g_value_set_string (value, priv->transparency);
			break;
		case PROP_UID:
			g_value_set_string (value, priv->uid);
			break;
		case PROP_SEQUENCE:
			g_value_set_uint (value, priv->sequence);
			break;
		case PROP_GUESTS_CAN_MODIFY:
			g_value_set_boolean (value, priv->guests_can_modify);
			break;
		case PROP_GUESTS_CAN_INVITE_OTHERS:
			g_value_set_boolean (value, priv->guests_can_invite_others);
			break;
		case PROP_GUESTS_CAN_SEE_GUESTS:
			g_value_set_boolean (value, priv->guests_can_see_guests);
			break;
		case PROP_ANYONE_CAN_ADD_SELF:
			g_value_set_boolean (value, priv->anyone_can_add_self);
			break;
		case PROP_RECURRENCE:
			g_value_set_string (value, priv->recurrence);
			break;
		case PROP_ORIGINAL_EVENT_ID:
			g_value_set_string (value, priv->original_event_id);
			break;
		case PROP_ORIGINAL_EVENT_URI:
			g_value_set_string (value, priv->original_event_uri);
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
gdata_calendar_event_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	GDataCalendarEvent *self = GDATA_CALENDAR_EVENT (object);

	switch (property_id) {
		case PROP_STATUS:
			gdata_calendar_event_set_status (self, g_value_get_string (value));
			break;
		case PROP_VISIBILITY:
			gdata_calendar_event_set_visibility (self, g_value_get_string (value));
			break;
		case PROP_TRANSPARENCY:
			gdata_calendar_event_set_transparency (self, g_value_get_string (value));
			break;
		case PROP_UID:
			gdata_calendar_event_set_uid (self, g_value_get_string (value));
			break;
		case PROP_SEQUENCE:
			gdata_calendar_event_set_sequence (self, g_value_get_uint (value));
			break;
		case PROP_GUESTS_CAN_MODIFY:
			gdata_calendar_event_set_guests_can_modify (self, g_value_get_boolean (value));
			break;
		case PROP_GUESTS_CAN_INVITE_OTHERS:
			gdata_calendar_event_set_guests_can_invite_others (self, g_value_get_boolean (value));
			break;
		case PROP_GUESTS_CAN_SEE_GUESTS:
			gdata_calendar_event_set_guests_can_see_guests (self, g_value_get_boolean (value));
			break;
		case PROP_ANYONE_CAN_ADD_SELF:
			gdata_calendar_event_set_anyone_can_add_self (self, g_value_get_boolean (value));
			break;
		case PROP_RECURRENCE:
			gdata_calendar_event_set_recurrence (self, g_value_get_string (value));
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static gboolean
parse_xml (GDataParsable *parsable, xmlDoc *doc, xmlNode *node, gpointer user_data, GError **error)
{
	gboolean success;
	GDataCalendarEvent *self = GDATA_CALENDAR_EVENT (parsable);

	if (gdata_parser_is_namespace (node, "http://www.w3.org/2007/app") == TRUE &&
	    gdata_parser_int64_time_from_element (node, "edited", P_REQUIRED | P_NO_DUPES, &(self->priv->edited), &success, error) == TRUE) {
		return success;
	} else if (gdata_parser_is_namespace (node, "http://schemas.google.com/g/2005") == TRUE) {
		if (gdata_parser_object_from_element_setter (node, "when", P_REQUIRED, GDATA_TYPE_GD_WHEN,
		                                             gdata_calendar_event_add_time, self, &success, error) == TRUE ||
		    gdata_parser_object_from_element_setter (node, "who", P_REQUIRED, GDATA_TYPE_GD_WHO,
		                                             gdata_calendar_event_add_person, self, &success, error) == TRUE ||
		    gdata_parser_object_from_element_setter (node, "where", P_REQUIRED, GDATA_TYPE_GD_WHERE,
		                                             gdata_calendar_event_add_place, self, &success, error) == TRUE) {
			return success;
		} else if (xmlStrcmp (node->name, (xmlChar*) "eventStatus") == 0) {
			/* gd:eventStatus */
			xmlChar *value = xmlGetProp (node, (xmlChar*) "value");
			if (value == NULL)
				return gdata_parser_error_required_property_missing (node, "value", error);
			self->priv->status = (gchar*) value;
		} else if (xmlStrcmp (node->name, (xmlChar*) "visibility") == 0) {
			/* gd:visibility */
			xmlChar *value = xmlGetProp (node, (xmlChar*) "value");
			if (value == NULL)
				return gdata_parser_error_required_property_missing (node, "value", error);
			self->priv->visibility = (gchar*) value;
		} else if (xmlStrcmp (node->name, (xmlChar*) "transparency") == 0) {
			/* gd:transparency */
			xmlChar *value = xmlGetProp (node, (xmlChar*) "value");
			if (value == NULL)
				return gdata_parser_error_required_property_missing (node, "value", error);
			self->priv->transparency = (gchar*) value;
		} else if (xmlStrcmp (node->name, (xmlChar*) "recurrence") == 0) {
			/* gd:recurrence */
			self->priv->recurrence = (gchar*) xmlNodeListGetString (doc, node->children, TRUE);
		} else if (xmlStrcmp (node->name, (xmlChar*) "originalEvent") == 0) {
			/* gd:originalEvent */
			self->priv->original_event_id = (gchar*) xmlGetProp (node, (xmlChar*) "id");
			self->priv->original_event_uri = (gchar*) xmlGetProp (node, (xmlChar*) "href");
		} else {
			return GDATA_PARSABLE_CLASS (gdata_calendar_event_parent_class)->parse_xml (parsable, doc, node, user_data, error);
		}
	} else if (gdata_parser_is_namespace (node, "http://schemas.google.com/gCal/2005") == TRUE) {
		if (xmlStrcmp (node->name, (xmlChar*) "uid") == 0) {
			/* gCal:uid */
			xmlChar *value = xmlGetProp (node, (xmlChar*) "value");
			if (value == NULL)
				return gdata_parser_error_required_property_missing (node, "value", error);
			self->priv->uid = (gchar*) value;
		} else if (xmlStrcmp (node->name, (xmlChar*) "sequence") == 0) {
			/* gCal:sequence */
			xmlChar *value;
			guint value_uint;

			value = xmlGetProp (node, (xmlChar*) "value");
			if (value == NULL)
				return gdata_parser_error_required_property_missing (node, "value", error);
			else
				value_uint = strtoul ((gchar*) value, NULL, 10);
			xmlFree (value);

			gdata_calendar_event_set_sequence (self, value_uint);
		} else if (xmlStrcmp (node->name, (xmlChar*) "guestsCanModify") == 0) {
			/* gCal:guestsCanModify */
			gboolean guests_can_modify;
			if (gdata_parser_boolean_from_property (node, "value", &guests_can_modify, -1, error) == FALSE)
				return FALSE;
			gdata_calendar_event_set_guests_can_modify (self, guests_can_modify);
		} else if (xmlStrcmp (node->name, (xmlChar*) "guestsCanInviteOthers") == 0) {
			/* gCal:guestsCanInviteOthers */
			gboolean guests_can_invite_others;
			if (gdata_parser_boolean_from_property (node, "value", &guests_can_invite_others, -1, error) == FALSE)
				return FALSE;
			gdata_calendar_event_set_guests_can_invite_others (self, guests_can_invite_others);
		} else if (xmlStrcmp (node->name, (xmlChar*) "guestsCanSeeGuests") == 0) {
			/* gCal:guestsCanSeeGuests */
			gboolean guests_can_see_guests;
			if (gdata_parser_boolean_from_property (node, "value", &guests_can_see_guests, -1, error) == FALSE)
				return FALSE;
			gdata_calendar_event_set_guests_can_see_guests (self, guests_can_see_guests);
		} else if (xmlStrcmp (node->name, (xmlChar*) "anyoneCanAddSelf") == 0) {
			/* gCal:anyoneCanAddSelf */
			gboolean anyone_can_add_self;
			if (gdata_parser_boolean_from_property (node, "value", &anyone_can_add_self, -1, error) == FALSE)
				return FALSE;
			gdata_calendar_event_set_anyone_can_add_self (self, anyone_can_add_self);
		} else {
			return GDATA_PARSABLE_CLASS (gdata_calendar_event_parent_class)->parse_xml (parsable, doc, node, user_data, error);
		}
	} else {
		return GDATA_PARSABLE_CLASS (gdata_calendar_event_parent_class)->parse_xml (parsable, doc, node, user_data, error);
	}

	return TRUE;
}

static void
get_child_xml (GList *list, GString *xml_string)
{
	GList *i;

	for (i = list; i != NULL; i = i->next)
		_gdata_parsable_get_xml (GDATA_PARSABLE (i->data), xml_string, FALSE);
}

static void
get_xml (GDataParsable *parsable, GString *xml_string)
{
	GDataCalendarEventPrivate *priv = GDATA_CALENDAR_EVENT (parsable)->priv;

	/* Chain up to the parent class */
	GDATA_PARSABLE_CLASS (gdata_calendar_event_parent_class)->get_xml (parsable, xml_string);

	/* Add all the Calendar-specific XML */

	/* TODO: gd:comments? */

	if (priv->status != NULL)
		gdata_parser_string_append_escaped (xml_string, "<gd:eventStatus value='", priv->status, "'/>");

	if (priv->visibility != NULL)
		gdata_parser_string_append_escaped (xml_string, "<gd:visibility value='", priv->visibility, "'/>");

	if (priv->transparency != NULL)
		gdata_parser_string_append_escaped (xml_string, "<gd:transparency value='", priv->transparency, "'/>");

	if (priv->uid != NULL)
		gdata_parser_string_append_escaped (xml_string, "<gCal:uid value='", priv->uid, "'/>");

	if (priv->sequence != 0)
		g_string_append_printf (xml_string, "<gCal:sequence value='%u'/>", priv->sequence);

	if (priv->guests_can_modify == TRUE)
		g_string_append (xml_string, "<gCal:guestsCanModify value='true'/>");
	else
		g_string_append (xml_string, "<gCal:guestsCanModify value='false'/>");

	if (priv->guests_can_invite_others == TRUE)
		g_string_append (xml_string, "<gCal:guestsCanInviteOthers value='true'/>");
	else
		g_string_append (xml_string, "<gCal:guestsCanInviteOthers value='false'/>");

	if (priv->guests_can_see_guests == TRUE)
		g_string_append (xml_string, "<gCal:guestsCanSeeGuests value='true'/>");
	else
		g_string_append (xml_string, "<gCal:guestsCanSeeGuests value='false'/>");

	if (priv->anyone_can_add_self == TRUE)
		g_string_append (xml_string, "<gCal:anyoneCanAddSelf value='true'/>");
	else
		g_string_append (xml_string, "<gCal:anyoneCanAddSelf value='false'/>");

	if (priv->recurrence != NULL)
		gdata_parser_string_append_escaped (xml_string, "<gd:recurrence>", priv->recurrence, "</gd:recurrence>");

	get_child_xml (priv->times, xml_string);
	get_child_xml (priv->people, xml_string);
	get_child_xml (priv->places, xml_string);

	/* TODO:
	 * - Finish supporting all tags
	 * - Check all tags here are valid for insertions and updates
	 */
}

static void
get_namespaces (GDataParsable *parsable, GHashTable *namespaces)
{
	/* Chain up to the parent class */
	GDATA_PARSABLE_CLASS (gdata_calendar_event_parent_class)->get_namespaces (parsable, namespaces);

	g_hash_table_insert (namespaces, (gchar*) "gd", (gchar*) "http://schemas.google.com/g/2005");
	g_hash_table_insert (namespaces, (gchar*) "gCal", (gchar*) "http://schemas.google.com/gCal/2005");
	g_hash_table_insert (namespaces, (gchar*) "app", (gchar*) "http://www.w3.org/2007/app");
}

/**
 * gdata_calendar_event_new:
 * @id: (allow-none): the event's ID, or %NULL
 *
 * Creates a new #GDataCalendarEvent with the given ID and default properties.
 *
 * Return value: a new #GDataCalendarEvent; unref with g_object_unref()
 **/
GDataCalendarEvent *
gdata_calendar_event_new (const gchar *id)
{
	return GDATA_CALENDAR_EVENT (g_object_new (GDATA_TYPE_CALENDAR_EVENT, "id", id, NULL));
}

/**
 * gdata_calendar_event_get_edited:
 * @self: a #GDataCalendarEvent
 *
 * Gets the #GDataCalendarEvent:edited property. If the property is unset, <code class="literal">-1</code> will be returned.
 *
 * Return value: the UNIX timestamp for the time the event was last edited, or <code class="literal">-1</code>
 **/
gint64
gdata_calendar_event_get_edited (GDataCalendarEvent *self)
{
	g_return_val_if_fail (GDATA_IS_CALENDAR_EVENT (self), -1);
	return self->priv->edited;
}

/**
 * gdata_calendar_event_get_status:
 * @self: a #GDataCalendarEvent
 *
 * Gets the #GDataCalendarEvent:status property.
 *
 * Return value: the event status, or %NULL
 *
 * Since: 0.2.0
 **/
const gchar *
gdata_calendar_event_get_status (GDataCalendarEvent *self)
{
	g_return_val_if_fail (GDATA_IS_CALENDAR_EVENT (self), NULL);
	return self->priv->status;
}

/**
 * gdata_calendar_event_set_status:
 * @self: a #GDataCalendarEvent
 * @status: (allow-none): a new event status, or %NULL
 *
 * Sets the #GDataCalendarEvent:status property to the new status, @status.
 *
 * Set @status to %NULL to unset the property in the event.
 *
 * Since: 0.2.0
 **/
void
gdata_calendar_event_set_status (GDataCalendarEvent *self, const gchar *status)
{
	g_return_if_fail (GDATA_IS_CALENDAR_EVENT (self));

	g_free (self->priv->status);
	self->priv->status = g_strdup (status);
	g_object_notify (G_OBJECT (self), "status");
}

/**
 * gdata_calendar_event_get_visibility:
 * @self: a #GDataCalendarEvent
 *
 * Gets the #GDataCalendarEvent:visibility property.
 *
 * Return value: the event visibility, or %NULL
 **/
const gchar *
gdata_calendar_event_get_visibility (GDataCalendarEvent *self)
{
	g_return_val_if_fail (GDATA_IS_CALENDAR_EVENT (self), NULL);
	return self->priv->visibility;
}

/**
 * gdata_calendar_event_set_visibility:
 * @self: a #GDataCalendarEvent
 * @visibility: (allow-none): a new event visibility, or %NULL
 *
 * Sets the #GDataCalendarEvent:visibility property to the new visibility, @visibility.
 *
 * Set @visibility to %NULL to unset the property in the event.
 **/
void
gdata_calendar_event_set_visibility (GDataCalendarEvent *self, const gchar *visibility)
{
	g_return_if_fail (GDATA_IS_CALENDAR_EVENT (self));

	g_free (self->priv->visibility);
	self->priv->visibility = g_strdup (visibility);
	g_object_notify (G_OBJECT (self), "visibility");
}

/**
 * gdata_calendar_event_get_transparency:
 * @self: a #GDataCalendarEvent
 *
 * Gets the #GDataCalendarEvent:transparency property.
 *
 * Return value: the event transparency, or %NULL
 **/
const gchar *
gdata_calendar_event_get_transparency (GDataCalendarEvent *self)
{
	g_return_val_if_fail (GDATA_IS_CALENDAR_EVENT (self), NULL);
	return self->priv->transparency;
}

/**
 * gdata_calendar_event_set_transparency:
 * @self: a #GDataCalendarEvent
 * @transparency: (allow-none): a new event transparency, or %NULL
 *
 * Sets the #GDataCalendarEvent:transparency property to the new transparency, @transparency.
 *
 * Set @transparency to %NULL to unset the property in the event.
 **/
void
gdata_calendar_event_set_transparency (GDataCalendarEvent *self, const gchar *transparency)
{
	g_return_if_fail (GDATA_IS_CALENDAR_EVENT (self));

	g_free (self->priv->transparency);
	self->priv->transparency = g_strdup (transparency);
	g_object_notify (G_OBJECT (self), "transparency");
}

/**
 * gdata_calendar_event_get_uid:
 * @self: a #GDataCalendarEvent
 *
 * Gets the #GDataCalendarEvent:uid property.
 *
 * Return value: the event's UID, or %NULL
 **/
const gchar *
gdata_calendar_event_get_uid (GDataCalendarEvent *self)
{
	g_return_val_if_fail (GDATA_IS_CALENDAR_EVENT (self), NULL);
	return self->priv->uid;
}

/**
 * gdata_calendar_event_set_uid:
 * @self: a #GDataCalendarEvent
 * @uid: (allow-none): a new event UID, or %NULL
 *
 * Sets the #GDataCalendarEvent:uid property to the new UID, @uid.
 *
 * Set @uid to %NULL to unset the property in the event.
 **/
void
gdata_calendar_event_set_uid (GDataCalendarEvent *self, const gchar *uid)
{
	/* TODO: is modifying this allowed? */
	g_return_if_fail (GDATA_IS_CALENDAR_EVENT (self));

	g_free (self->priv->uid);
	self->priv->uid = g_strdup (uid);
	g_object_notify (G_OBJECT (self), "uid");
}

/**
 * gdata_calendar_event_get_sequence:
 * @self: a #GDataCalendarEvent
 *
 * Gets the #GDataCalendarEvent:sequence property.
 *
 * Return value: the event's sequence number
 **/
guint
gdata_calendar_event_get_sequence (GDataCalendarEvent *self)
{
	g_return_val_if_fail (GDATA_IS_CALENDAR_EVENT (self), 0);
	return self->priv->sequence;
}

/**
 * gdata_calendar_event_set_sequence:
 * @self: a #GDataCalendarEvent
 * @sequence: a new sequence number, or <code class="literal">0</code>
 *
 * Sets the #GDataCalendarEvent:sequence property to the new sequence number, @sequence.
 **/
void
gdata_calendar_event_set_sequence (GDataCalendarEvent *self, guint sequence)
{
	g_return_if_fail (GDATA_IS_CALENDAR_EVENT (self));
	self->priv->sequence = sequence;
	g_object_notify (G_OBJECT (self), "sequence");
}

/**
 * gdata_calendar_event_get_guests_can_modify:
 * @self: a #GDataCalendarEvent
 *
 * Gets the #GDataCalendarEvent:guests-can-modify property.
 *
 * Return value: %TRUE if attendees can modify the original event, %FALSE otherwise
 **/
gboolean
gdata_calendar_event_get_guests_can_modify (GDataCalendarEvent *self)
{
	g_return_val_if_fail (GDATA_IS_CALENDAR_EVENT (self), FALSE);
	return self->priv->guests_can_modify;
}

/**
 * gdata_calendar_event_set_guests_can_modify:
 * @self: a #GDataCalendarEvent
 * @guests_can_modify: %TRUE if attendees can modify the original event, %FALSE otherwise
 *
 * Sets the #GDataCalendarEvent:guests-can-modify property to @guests_can_modify.
 **/
void
gdata_calendar_event_set_guests_can_modify (GDataCalendarEvent *self, gboolean guests_can_modify)
{
	g_return_if_fail (GDATA_IS_CALENDAR_EVENT (self));
	self->priv->guests_can_modify = guests_can_modify;
	g_object_notify (G_OBJECT (self), "guests-can-modify");
}

/**
 * gdata_calendar_event_get_guests_can_invite_others:
 * @self: a #GDataCalendarEvent
 *
 * Gets the #GDataCalendarEvent:guests-can-invite-others property.
 *
 * Return value: %TRUE if attendees can invite others to the event, %FALSE otherwise
 **/
gboolean
gdata_calendar_event_get_guests_can_invite_others (GDataCalendarEvent *self)
{
	g_return_val_if_fail (GDATA_IS_CALENDAR_EVENT (self), FALSE);
	return self->priv->guests_can_invite_others;
}

/**
 * gdata_calendar_event_set_guests_can_invite_others:
 * @self: a #GDataCalendarEvent
 * @guests_can_invite_others: %TRUE if attendees can invite others to the event, %FALSE otherwise
 *
 * Sets the #GDataCalendarEvent:guests-can-invite-others property to @guests_can_invite_others.
 **/
void
gdata_calendar_event_set_guests_can_invite_others (GDataCalendarEvent *self, gboolean guests_can_invite_others)
{
	g_return_if_fail (GDATA_IS_CALENDAR_EVENT (self));
	self->priv->guests_can_invite_others = guests_can_invite_others;
	g_object_notify (G_OBJECT (self), "guests-can-invite-others");
}

/**
 * gdata_calendar_event_get_guests_can_see_guests:
 * @self: a #GDataCalendarEvent
 *
 * Gets the #GDataCalendarEvent:guests-can-see-guests property.
 *
 * Return value: %TRUE if attendees can see who's attending the event, %FALSE otherwise
 **/
gboolean
gdata_calendar_event_get_guests_can_see_guests (GDataCalendarEvent *self)
{
	g_return_val_if_fail (GDATA_IS_CALENDAR_EVENT (self), FALSE);
	return self->priv->guests_can_see_guests;
}

/**
 * gdata_calendar_event_set_guests_can_see_guests:
 * @self: a #GDataCalendarEvent
 * @guests_can_see_guests: %TRUE if attendees can see who's attending the event, %FALSE otherwise
 *
 * Sets the #GDataCalendarEvent:guests-can-see-guests property to @guests_can_see_guests.
 **/
void
gdata_calendar_event_set_guests_can_see_guests (GDataCalendarEvent *self, gboolean guests_can_see_guests)
{
	g_return_if_fail (GDATA_IS_CALENDAR_EVENT (self));
	self->priv->guests_can_see_guests = guests_can_see_guests;
	g_object_notify (G_OBJECT (self), "guests-can-see-guests");
}

/**
 * gdata_calendar_event_get_anyone_can_add_self:
 * @self: a #GDataCalendarEvent
 *
 * Gets the #GDataCalendarEvent:anyone-can-add-self property.
 *
 * Return value: %TRUE if anyone can add themselves as an attendee to the event, %FALSE otherwise
 **/
gboolean
gdata_calendar_event_get_anyone_can_add_self (GDataCalendarEvent *self)
{
	g_return_val_if_fail (GDATA_IS_CALENDAR_EVENT (self), FALSE);
	return self->priv->anyone_can_add_self;
}

/**
 * gdata_calendar_event_set_anyone_can_add_self:
 * @self: a #GDataCalendarEvent
 * @anyone_can_add_self: %TRUE if anyone can add themselves as an attendee to the event, %FALSE otherwise
 *
 * Sets the #GDataCalendarEvent:anyone-can-add-self property to @anyone_can_add_self.
 **/
void
gdata_calendar_event_set_anyone_can_add_self (GDataCalendarEvent *self, gboolean anyone_can_add_self)
{
	g_return_if_fail (GDATA_IS_CALENDAR_EVENT (self));
	self->priv->anyone_can_add_self = anyone_can_add_self;
	g_object_notify (G_OBJECT (self), "anyone-can-add-self");
}

/**
 * gdata_calendar_event_add_person:
 * @self: a #GDataCalendarEvent
 * @who: a #GDataGDWho to add
 *
 * Adds the person @who to the event as a guest (attendee, organiser, performer, etc.), and increments its reference count.
 *
 * Duplicate people will not be added to the list.
 **/
void
gdata_calendar_event_add_person (GDataCalendarEvent *self, GDataGDWho *who)
{
	g_return_if_fail (GDATA_IS_CALENDAR_EVENT (self));
	g_return_if_fail (GDATA_IS_GD_WHO (who));

	if (g_list_find_custom (self->priv->people, who, (GCompareFunc) gdata_comparable_compare) == NULL)
		self->priv->people = g_list_append (self->priv->people, g_object_ref (who));
}

/**
 * gdata_calendar_event_get_people:
 * @self: a #GDataCalendarEvent
 *
 * Gets a list of the people attending the event.
 *
 * Return value: (element-type GData.GDWho) (transfer none): a #GList of #GDataGDWho<!-- -->s, or %NULL
 *
 * Since: 0.2.0
 **/
GList *
gdata_calendar_event_get_people (GDataCalendarEvent *self)
{
	g_return_val_if_fail (GDATA_IS_CALENDAR_EVENT (self), NULL);
	return self->priv->people;
}

/**
 * gdata_calendar_event_add_place:
 * @self: a #GDataCalendarEvent
 * @where: a #GDataGDWhere to add
 *
 * Adds the place @where to the event as a location and increments its reference count.
 *
 * Duplicate places will not be added to the list.
 **/
void
gdata_calendar_event_add_place (GDataCalendarEvent *self, GDataGDWhere *where)
{
	g_return_if_fail (GDATA_IS_CALENDAR_EVENT (self));
	g_return_if_fail (GDATA_IS_GD_WHERE (where));

	if (g_list_find_custom (self->priv->places, where, (GCompareFunc) gdata_comparable_compare) == NULL)
		self->priv->places = g_list_append (self->priv->places, g_object_ref (where));
}

/**
 * gdata_calendar_event_get_places:
 * @self: a #GDataCalendarEvent
 *
 * Gets a list of the locations associated with the event.
 *
 * Return value: (element-type GData.GDWhere) (transfer none): a #GList of #GDataGDWhere<!-- -->s, or %NULL
 *
 * Since: 0.2.0
 **/
GList *
gdata_calendar_event_get_places (GDataCalendarEvent *self)
{
	g_return_val_if_fail (GDATA_IS_CALENDAR_EVENT (self), NULL);
	return self->priv->places;
}

/**
 * gdata_calendar_event_add_time:
 * @self: a #GDataCalendarEvent
 * @when: a #GDataGDWhen to add
 *
 * Adds @when to the event as a time period when the event happens, and increments its reference count.
 *
 * Duplicate times will not be added to the list.
 *
 * Note: gdata_calendar_event_add_time() and gdata_calendar_event_set_recurrence() are mutually
 * exclusive, as the server doesn't support positive exceptions to recurrence rules. If recurrences
 * are required, use gdata_calendar_event_set_recurrence(). Note that this means reminders cannot
 * be set for the event, as they are only supported by #GDataGDWhen. No checks are performed for
 * these forbidden conditions, as to do so would break libgdata's API; if both a recurrence is set
 * and a specific time is added, the server will return an error when the #GDataCalendarEvent is
 * inserted using gdata_service_insert_entry().
 *
 * Since: 0.2.0
 **/
void
gdata_calendar_event_add_time (GDataCalendarEvent *self, GDataGDWhen *when)
{
	g_return_if_fail (GDATA_IS_CALENDAR_EVENT (self));
	g_return_if_fail (GDATA_IS_GD_WHEN (when));

	if (g_list_find_custom (self->priv->times, when, (GCompareFunc) gdata_comparable_compare) == NULL)
		self->priv->times = g_list_append (self->priv->times, g_object_ref (when));
}

/**
 * gdata_calendar_event_get_times:
 * @self: a #GDataCalendarEvent
 *
 * Gets a list of the time periods associated with the event.
 *
 * Return value: (element-type GData.GDWhen) (transfer none): a #GList of #GDataGDWhen<!-- -->s, or %NULL
 *
 * Since: 0.2.0
 **/
GList *
gdata_calendar_event_get_times (GDataCalendarEvent *self)
{
	g_return_val_if_fail (GDATA_IS_CALENDAR_EVENT (self), NULL);
	return self->priv->times;
}

/**
 * gdata_calendar_event_get_primary_time:
 * @self: a #GDataCalendarEvent
 * @start_time: (out caller-allocates): a #gint64 for the start time, or %NULL
 * @end_time: (out caller-allocates): a #gint64 for the end time, or %NULL
 * @when: (out callee-allocates) (transfer none): a #GDataGDWhen for the primary time structure, or %NULL
 *
 * Gets the first time period associated with the event, conveniently returning just its start and
 * end times if required.
 *
 * If there are no time periods, or more than one time period, associated with the event, %FALSE will
 * be returned, and the parameters will remain unmodified.
 *
 * Return value: %TRUE if there is only one time period associated with the event, %FALSE otherwise
 *
 * Since: 0.2.0
 **/
gboolean
gdata_calendar_event_get_primary_time (GDataCalendarEvent *self, gint64 *start_time, gint64 *end_time, GDataGDWhen **when)
{
	GDataGDWhen *primary_when;

	g_return_val_if_fail (GDATA_IS_CALENDAR_EVENT (self), FALSE);

	if (self->priv->times == NULL || self->priv->times->next != NULL)
		return FALSE;

	primary_when = GDATA_GD_WHEN (self->priv->times->data);
	if (start_time != NULL)
		*start_time = gdata_gd_when_get_start_time (primary_when);
	if (end_time != NULL)
		*end_time = gdata_gd_when_get_end_time (primary_when);
	if (when != NULL)
		*when = primary_when;

	return TRUE;
}

/**
 * gdata_calendar_event_get_recurrence:
 * @self: a #GDataCalendarEvent
 *
 * Gets the #GDataCalendarEvent:recurrence property.
 *
 * Return value: the event recurrence patterns, or %NULL
 *
 * Since: 0.3.0
 **/
const gchar *
gdata_calendar_event_get_recurrence (GDataCalendarEvent *self)
{
	g_return_val_if_fail (GDATA_IS_CALENDAR_EVENT (self), NULL);
	return self->priv->recurrence;
}

/**
 * gdata_calendar_event_set_recurrence:
 * @self: a #GDataCalendarEvent
 * @recurrence: (allow-none): a new event recurrence, or %NULL
 *
 * Sets the #GDataCalendarEvent:recurrence property to the new recurrence, @recurrence.
 *
 * Set @recurrence to %NULL to unset the property in the event.
 *
 * Note: gdata_calendar_event_add_time() and gdata_calendar_event_set_recurrence() are mutually
 * exclusive. See the documentation for gdata_calendar_event_add_time() for details.
 *
 * Since: 0.3.0
 **/
void
gdata_calendar_event_set_recurrence (GDataCalendarEvent *self, const gchar *recurrence)
{
	g_return_if_fail (GDATA_IS_CALENDAR_EVENT (self));

	g_free (self->priv->recurrence);
	self->priv->recurrence = g_strdup (recurrence);
	g_object_notify (G_OBJECT (self), "recurrence");
}

/**
 * gdata_calendar_event_get_original_event_details:
 * @self: a #GDataCalendarEvent
 * @event_id: (out callee-allocates) (transfer full): return location for the original event's ID, or %NULL
 * @event_uri: (out callee-allocates) (transfer full): return location for the original event's URI, or %NULL
 *
 * Gets details of the original event, if this event is an exception to a recurring event. The original
 * event's ID and the URI of the event's XML are returned in @event_id and @event_uri, respectively.
 *
 * If this event is not an exception to a recurring event, @event_id and @event_uri will be set to %NULL.
 * See gdata_calendar_event_is_exception() to determine more simply whether an event is an exception to a
 * recurring event.
 *
 * If both @event_id and @event_uri are %NULL, this function is a no-op. Otherwise, they should both be
 * freed with g_free().
 *
 * Since: 0.3.0
 **/
void
gdata_calendar_event_get_original_event_details (GDataCalendarEvent *self, gchar **event_id, gchar **event_uri)
{
	g_return_if_fail (GDATA_IS_CALENDAR_EVENT (self));

	if (event_id != NULL)
		*event_id = g_strdup (self->priv->original_event_id);
	if (event_uri != NULL)
		*event_uri = g_strdup (self->priv->original_event_uri);
}

/**
 * gdata_calendar_event_is_exception:
 * @self: a #GDataCalendarEvent
 *
 * Determines whether the event is an exception to a recurring event. If it is, details of the original event
 * can be retrieved using gdata_calendar_event_get_original_event_details().
 *
 * Return value: %TRUE if the event is an exception, %FALSE otherwise
 *
 * Since: 0.3.0
 **/
gboolean
gdata_calendar_event_is_exception (GDataCalendarEvent *self)
{
	g_return_val_if_fail (GDATA_IS_CALENDAR_EVENT (self), FALSE);
	return (self->priv->original_event_id != NULL && self->priv->original_event_uri != NULL) ? TRUE : FALSE;
}
