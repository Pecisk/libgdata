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
 * SECTION:gdata-gd-phone-number
 * @short_description: GData phone number element
 * @stability: Unstable
 * @include: gdata/gd/gdata-gd-phone-number.h
 *
 * #GDataGDPhoneNumber represents a "phoneNumber" element from the
 * <ulink type="http" url="http://code.google.com/apis/gdata/docs/2.0/elements.html#gdPhoneNumber">GData specification</ulink>.
 *
 * Since: 0.4.0
 **/

#include <glib.h>
#include <libxml/parser.h>
#include <string.h>

#include "gdata-gd-phone-number.h"
#include "gdata-parsable.h"
#include "gdata-parser.h"
#include "gdata-comparable.h"

static void gdata_gd_phone_number_comparable_init (GDataComparableIface *iface);
static void gdata_gd_phone_number_finalize (GObject *object);
static void gdata_gd_phone_number_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void gdata_gd_phone_number_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static gboolean pre_parse_xml (GDataParsable *parsable, xmlDoc *doc, xmlNode *root_node, gpointer user_data, GError **error);
static gboolean parse_xml (GDataParsable *parsable, xmlDoc *doc, xmlNode *node, gpointer user_data, GError **error);
static void pre_get_xml (GDataParsable *parsable, GString *xml_string);
static void get_xml (GDataParsable *parsable, GString *xml_string);
static void get_namespaces (GDataParsable *parsable, GHashTable *namespaces);

struct _GDataGDPhoneNumberPrivate {
	gchar *number;
	gchar *uri;
	gchar *relation_type;
	gchar *label;
	gboolean is_primary;
};

enum {
	PROP_NUMBER = 1,
	PROP_URI,
	PROP_RELATION_TYPE,
	PROP_LABEL,
	PROP_IS_PRIMARY
};

G_DEFINE_TYPE_WITH_CODE (GDataGDPhoneNumber, gdata_gd_phone_number, GDATA_TYPE_PARSABLE,
                         G_IMPLEMENT_INTERFACE (GDATA_TYPE_COMPARABLE, gdata_gd_phone_number_comparable_init))

static void
gdata_gd_phone_number_class_init (GDataGDPhoneNumberClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GDataParsableClass *parsable_class = GDATA_PARSABLE_CLASS (klass);

	g_type_class_add_private (klass, sizeof (GDataGDPhoneNumberPrivate));

	gobject_class->get_property = gdata_gd_phone_number_get_property;
	gobject_class->set_property = gdata_gd_phone_number_set_property;
	gobject_class->finalize = gdata_gd_phone_number_finalize;

	parsable_class->pre_parse_xml = pre_parse_xml;
	parsable_class->parse_xml = parse_xml;
	parsable_class->pre_get_xml = pre_get_xml;
	parsable_class->get_xml = get_xml;
	parsable_class->get_namespaces = get_namespaces;
	parsable_class->element_name = "phoneNumber";
	parsable_class->element_namespace = "gd";

	/**
	 * GDataGDPhoneNumber:number:
	 *
	 * Human-readable phone number; may be in any telephone number format.
	 *
	 * For more information, see the
	 * <ulink type="http" url="http://code.google.com/apis/gdata/docs/2.0/elements.html#gdPhoneNumber">GData specification</ulink>.
	 *
	 * Since: 0.4.0
	 **/
	g_object_class_install_property (gobject_class, PROP_NUMBER,
	                                 g_param_spec_string ("number",
	                                                      "Number", "Human-readable phone number; may be in any telephone number format.",
	                                                      NULL,
	                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * GDataGDPhoneNumber:uri:
	 *
	 * An optional "tel URI" used to represent the number in a formal way. Useful for programmatic access, such as a VoIP/PSTN bridge.
	 *
	 * For more information, see the
	 * <ulink type="http" url="http://code.google.com/apis/gdata/docs/2.0/elements.html#gdPhoneNumber">GData specification</ulink>.
	 *
	 * Since: 0.4.0
	 **/
	g_object_class_install_property (gobject_class, PROP_URI,
	                                 g_param_spec_string ("uri",
	                                                      "URI", "An optional \"tel URI\" used to represent the number in a formal way.",
	                                                      NULL,
	                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * GDataGDPhoneNumber:relation-type:
	 *
	 * A programmatic value that identifies the type of phone number. For example: %GDATA_GD_PHONE_NUMBER_WORK or %GDATA_GD_PHONE_NUMBER_PAGER.
	 *
	 * For more information, see the
	 * <ulink type="http" url="http://code.google.com/apis/gdata/docs/2.0/elements.html#gdPhoneNumber">GData specification</ulink>.
	 *
	 * Since: 0.4.0
	 **/
	g_object_class_install_property (gobject_class, PROP_RELATION_TYPE,
	                                 g_param_spec_string ("relation-type",
	                                                      "Relation type", "A programmatic value that identifies the type of phone number.",
	                                                      NULL,
	                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * GDataGDPhoneNumber:label:
	 *
	 * A simple string value used to name this phone number. It allows UIs to display a label such as "Mobile", "Home", "Work", etc.
	 *
	 * For more information, see the
	 * <ulink type="http" url="http://code.google.com/apis/gdata/docs/2.0/elements.html#gdPhoneNumber">GData specification</ulink>.
	 *
	 * Since: 0.4.0
	 **/
	g_object_class_install_property (gobject_class, PROP_LABEL,
	                                 g_param_spec_string ("label",
	                                                      "Label", "A simple string value used to name this phone number.",
	                                                      NULL,
	                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * GDataGDPhoneNumber:is-primary:
	 *
	 * Indicates which phone number out of a group is primary.
	 *
	 * For more information, see the
	 * <ulink type="http" url="http://code.google.com/apis/gdata/docs/2.0/elements.html#gdPhoneNumber">GData specification</ulink>.
	 *
	 * Since: 0.4.0
	 **/
	g_object_class_install_property (gobject_class, PROP_IS_PRIMARY,
	                                 g_param_spec_boolean ("is-primary",
	                                                       "Primary?", "Indicates which phone number out of a group is primary.",
	                                                       FALSE,
	                                                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static gint
compare_with (GDataComparable *self, GDataComparable *other)
{
	GDataGDPhoneNumber *a = (GDataGDPhoneNumber*) self, *b = (GDataGDPhoneNumber*) other;
	gint number_comparison, uri_comparison;

	number_comparison = g_strcmp0 (a->priv->number, b->priv->number);
	uri_comparison = g_strcmp0 (a->priv->uri, b->priv->uri);

	/* NULL URIs should not compare equal. */
	if (a->priv->uri == NULL && b->priv->uri == NULL) {
		uri_comparison = 1;
	}

	/* If the numbers or the URIs are equal, the objects are equal. */
	if (number_comparison == 0 || uri_comparison == 0) {
		return 0;
	}

	return number_comparison;
}

static void
gdata_gd_phone_number_comparable_init (GDataComparableIface *iface)
{
	iface->compare_with = compare_with;
}

static void
gdata_gd_phone_number_init (GDataGDPhoneNumber *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GDATA_TYPE_GD_PHONE_NUMBER, GDataGDPhoneNumberPrivate);
}

static void
gdata_gd_phone_number_finalize (GObject *object)
{
	GDataGDPhoneNumberPrivate *priv = GDATA_GD_PHONE_NUMBER (object)->priv;

	g_free (priv->number);
	g_free (priv->uri);
	g_free (priv->relation_type);
	g_free (priv->label);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (gdata_gd_phone_number_parent_class)->finalize (object);
}

static void
gdata_gd_phone_number_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	GDataGDPhoneNumberPrivate *priv = GDATA_GD_PHONE_NUMBER (object)->priv;

	switch (property_id) {
		case PROP_NUMBER:
			g_value_set_string (value, priv->number);
			break;
		case PROP_URI:
			g_value_set_string (value, priv->uri);
			break;
		case PROP_RELATION_TYPE:
			g_value_set_string (value, priv->relation_type);
			break;
		case PROP_LABEL:
			g_value_set_string (value, priv->label);
			break;
		case PROP_IS_PRIMARY:
			g_value_set_boolean (value, priv->is_primary);
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
gdata_gd_phone_number_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	GDataGDPhoneNumber *self = GDATA_GD_PHONE_NUMBER (object);

	switch (property_id) {
		case PROP_NUMBER:
			gdata_gd_phone_number_set_number (self, g_value_get_string (value));
			break;
		case PROP_URI:
			gdata_gd_phone_number_set_uri (self, g_value_get_string (value));
			break;
		case PROP_RELATION_TYPE:
			gdata_gd_phone_number_set_relation_type (self, g_value_get_string (value));
			break;
		case PROP_LABEL:
			gdata_gd_phone_number_set_label (self, g_value_get_string (value));
			break;
		case PROP_IS_PRIMARY:
			gdata_gd_phone_number_set_is_primary (self, g_value_get_boolean (value));
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static gboolean
pre_parse_xml (GDataParsable *parsable, xmlDoc *doc, xmlNode *root_node, gpointer user_data, GError **error)
{
	xmlChar *number, *rel;
	gboolean primary_bool;
	GDataGDPhoneNumberPrivate *priv = GDATA_GD_PHONE_NUMBER (parsable)->priv;

	/* Is it the primary phone number? */
	if (gdata_parser_boolean_from_property (root_node, "primary", &primary_bool, 0, error) == FALSE)
		return FALSE;

	number = xmlNodeListGetString (doc, root_node->children, TRUE);
	if (number == NULL || *number == '\0') {
		xmlFree (number);
		return gdata_parser_error_required_content_missing (root_node, error);
	}

	rel = xmlGetProp (root_node, (xmlChar*) "rel");
	if (rel != NULL && *rel == '\0') {
		xmlFree (rel);
		xmlFree (number);
		return gdata_parser_error_required_property_missing (root_node, "rel", error);
	}

	gdata_gd_phone_number_set_number (GDATA_GD_PHONE_NUMBER (parsable), (gchar*) number);
	priv->uri = (gchar*) xmlGetProp (root_node, (xmlChar*) "uri");
	priv->relation_type = (gchar*) rel;
	priv->label = (gchar*) xmlGetProp (root_node, (xmlChar*) "label");
	priv->is_primary = primary_bool;

	xmlFree (number);

	return TRUE;
}

static gboolean
parse_xml (GDataParsable *parsable, xmlDoc *doc, xmlNode *node, gpointer user_data, GError **error)
{
	/* Textual content's handled in pre_parse_xml */
	if (node->type != XML_ELEMENT_NODE)
		return TRUE;

	return GDATA_PARSABLE_CLASS (gdata_gd_phone_number_parent_class)->parse_xml (parsable, doc, node, user_data, error);
}

static void
pre_get_xml (GDataParsable *parsable, GString *xml_string)
{
	GDataGDPhoneNumberPrivate *priv = GDATA_GD_PHONE_NUMBER (parsable)->priv;

	if (priv->uri != NULL)
		gdata_parser_string_append_escaped (xml_string, " uri='", priv->uri, "'");
	if (priv->relation_type != NULL)
		gdata_parser_string_append_escaped (xml_string, " rel='", priv->relation_type, "'");
	if (priv->label != NULL)
		gdata_parser_string_append_escaped (xml_string, " label='", priv->label, "'");

	if (priv->is_primary == TRUE)
		g_string_append (xml_string, " primary='true'");
	else
		g_string_append (xml_string, " primary='false'");
}

static void
get_xml (GDataParsable *parsable, GString *xml_string)
{
	gdata_parser_string_append_escaped (xml_string, NULL, GDATA_GD_PHONE_NUMBER (parsable)->priv->number, NULL);
}

static void
get_namespaces (GDataParsable *parsable, GHashTable *namespaces)
{
	g_hash_table_insert (namespaces, (gchar*) "gd", (gchar*) "http://schemas.google.com/g/2005");
}

/**
 * gdata_gd_phone_number_new:
 * @number: the phone number, in human-readable format
 * @relation_type: (allow-none): the relationship between the phone number and its owner, or %NULL
 * @label: (allow-none): a human-readable label for the phone number, or %NULL
 * @uri: (allow-none): a "tel URI" to represent the number formally (see
 * <ulink type="http" url="http://www.ietf.org/rfc/rfc3966.txt">RFC 3966</ulink>), or %NULL
 * @is_primary: %TRUE if this phone number is its owner's primary number, %FALSE otherwise
 *
 * Creates a new #GDataGDPhoneNumber. More information is available in the <ulink type="http"
 * url="http://code.google.com/apis/gdata/docs/2.0/elements.html#gdPhoneNumber">GData specification</ulink>.
 *
 * Return value: a new #GDataGDPhoneNumber, or %NULL; unref with g_object_unref()
 *
 * Since: 0.2.0
 **/
GDataGDPhoneNumber *
gdata_gd_phone_number_new (const gchar *number, const gchar *relation_type, const gchar *label, const gchar *uri, gboolean is_primary)
{
	g_return_val_if_fail (number != NULL && *number != '\0', NULL);
	g_return_val_if_fail (relation_type == NULL || *relation_type != '\0', NULL);
	return g_object_new (GDATA_TYPE_GD_PHONE_NUMBER, "number", number, "uri", uri, "relation-type", relation_type,
	                     "label", label, "is-primary", is_primary, NULL);
}

/**
 * gdata_gd_phone_number_get_number:
 * @self: a #GDataGDPhoneNumber
 *
 * Gets the #GDataGDPhoneNumber:number property.
 *
 * Return value: the phone number itself
 *
 * Since: 0.4.0
 **/
const gchar *
gdata_gd_phone_number_get_number (GDataGDPhoneNumber *self)
{
	g_return_val_if_fail (GDATA_IS_GD_PHONE_NUMBER (self), NULL);
	return self->priv->number;
}

/**
 * gdata_gd_phone_number_set_number:
 * @self: a #GDataGDPhoneNumber
 * @number: the new phone number
 *
 * Sets the #GDataGDPhoneNumber:number property to @number.
 *
 * Since: 0.4.0
 **/
void
gdata_gd_phone_number_set_number (GDataGDPhoneNumber *self, const gchar *number)
{
	g_return_if_fail (GDATA_IS_GD_PHONE_NUMBER (self));
	g_return_if_fail (number != NULL && *number != '\0');

	/* Trim leading and trailing whitespace from the number.
	 * See here: http://code.google.com/apis/gdata/docs/1.0/elements.html#gdPhoneNumber */
	g_free (self->priv->number);
	self->priv->number = gdata_parser_utf8_trim_whitespace (number);
	g_object_notify (G_OBJECT (self), "number");
}

/**
 * gdata_gd_phone_number_get_uri:
 * @self: a #GDataGDPhoneNumber
 *
 * Gets the #GDataGDPhoneNumber:uri property.
 *
 * Return value: the phone number's URI, or %NULL
 *
 * Since: 0.4.0
 **/
const gchar *
gdata_gd_phone_number_get_uri (GDataGDPhoneNumber *self)
{
	g_return_val_if_fail (GDATA_IS_GD_PHONE_NUMBER (self), NULL);
	return self->priv->uri;
}

/**
 * gdata_gd_phone_number_set_uri:
 * @self: a #GDataGDPhoneNumber
 * @uri: (allow-none): the new URI for the phone number, or %NULL
 *
 * Sets the #GDataGDPhoneNumber:uri property to @uri.
 *
 * Set @uri to %NULL to unset the property in the phone number.
 *
 * Since: 0.4.0
 **/
void
gdata_gd_phone_number_set_uri (GDataGDPhoneNumber *self, const gchar *uri)
{
	g_return_if_fail (GDATA_IS_GD_PHONE_NUMBER (self));

	g_free (self->priv->uri);
	self->priv->uri = g_strdup (uri);
	g_object_notify (G_OBJECT (self), "uri");
}

/**
 * gdata_gd_phone_number_get_relation_type:
 * @self: a #GDataGDPhoneNumber
 *
 * Gets the #GDataGDPhoneNumber:relation-type property.
 *
 * Return value: the phone number's relation type, or %NULL
 *
 * Since: 0.4.0
 **/
const gchar *
gdata_gd_phone_number_get_relation_type (GDataGDPhoneNumber *self)
{
	g_return_val_if_fail (GDATA_IS_GD_PHONE_NUMBER (self), NULL);
	return self->priv->relation_type;
}

/**
 * gdata_gd_phone_number_set_relation_type:
 * @self: a #GDataGDPhoneNumber
 * @relation_type: (allow-none): the new relation type for the phone number, or %NULL
 *
 * Sets the #GDataGDPhoneNumber:relation-type property to @relation_type.
 *
 * Set @relation_type to %NULL to unset the property in the phone number.
 *
 * Since: 0.4.0
 **/
void
gdata_gd_phone_number_set_relation_type (GDataGDPhoneNumber *self, const gchar *relation_type)
{
	g_return_if_fail (GDATA_IS_GD_PHONE_NUMBER (self));
	g_return_if_fail (relation_type == NULL || *relation_type != '\0');

	g_free (self->priv->relation_type);
	self->priv->relation_type = g_strdup (relation_type);
	g_object_notify (G_OBJECT (self), "relation-type");
}

/**
 * gdata_gd_phone_number_get_label:
 * @self: a #GDataGDPhoneNumber
 *
 * Gets the #GDataGDPhoneNumber:label property.
 *
 * Return value: the phone number's label, or %NULL
 *
 * Since: 0.4.0
 **/
const gchar *
gdata_gd_phone_number_get_label (GDataGDPhoneNumber *self)
{
	g_return_val_if_fail (GDATA_IS_GD_PHONE_NUMBER (self), NULL);
	return self->priv->label;
}

/**
 * gdata_gd_phone_number_set_label:
 * @self: a #GDataGDPhoneNumber
 * @label: (allow-none): the new label for the phone number, or %NULL
 *
 * Sets the #GDataGDPhoneNumber:label property to @label.
 *
 * Set @label to %NULL to unset the property in the phone number.
 *
 * Since: 0.4.0
 **/
void
gdata_gd_phone_number_set_label (GDataGDPhoneNumber *self, const gchar *label)
{
	g_return_if_fail (GDATA_IS_GD_PHONE_NUMBER (self));

	g_free (self->priv->label);
	self->priv->label = g_strdup (label);
	g_object_notify (G_OBJECT (self), "label");
}

/**
 * gdata_gd_phone_number_is_primary:
 * @self: a #GDataGDPhoneNumber
 *
 * Gets the #GDataGDPhoneNumber:is-primary property.
 *
 * Return value: %TRUE if this is the primary phone number, %FALSE otherwise
 *
 * Since: 0.4.0
 **/
gboolean
gdata_gd_phone_number_is_primary (GDataGDPhoneNumber *self)
{
	g_return_val_if_fail (GDATA_IS_GD_PHONE_NUMBER (self), FALSE);
	return self->priv->is_primary;
}

/**
 * gdata_gd_phone_number_set_is_primary:
 * @self: a #GDataGDPhoneNumber
 * @is_primary: %TRUE if this is the primary phone number, %FALSE otherwise
 *
 * Sets the #GDataGDPhoneNumber:is-primary property to @is_primary.
 *
 * Since: 0.4.0
 **/
void
gdata_gd_phone_number_set_is_primary (GDataGDPhoneNumber *self, gboolean is_primary)
{
	g_return_if_fail (GDATA_IS_GD_PHONE_NUMBER (self));

	self->priv->is_primary = is_primary;
	g_object_notify (G_OBJECT (self), "is-primary");
}
