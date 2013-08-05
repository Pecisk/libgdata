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

#include <config.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <string.h>

#include "gdata-tasks-query.h"
#include "gdata-query.h"
#include "gdata-parser.h"

static void gdata_tasks_query_finalize (GObject *object);
static void gdata_tasks_query_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void gdata_tasks_query_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void get_query_uri (GDataQuery *self, const gchar *feed_uri, GString *query_uri, gboolean *params_started);

struct _GDataTasksQueryPrivate {
	gint64 completed_max;
	gint64 completed_min;
	gint64 due_max;
	gint64 due_min;	
	gboolean show_completed;
	gboolean show_deleted;
	gboolean show_hidden;
	
	/* These params are here not in GDataQuery due of differently named query params for JSON protocols therefore need for different parse_uri */
	gint max_results;
	gchar *page_token;
	gint64 updated_min;
};

enum {
	PROP_COMPLETED_MAX = 1,
	PROP_COMPLETED_MIN,
	PROP_DUE_MAX,
	PROP_DUE_MIN,
	PROP_SHOW_COMPLETED,
	PROP_SHOW_DELETED,
	PROP_SHOW_HIDDEN,
	PROP_MAX_RESULTS,
	PROP_PAGE_TOKEN,
	PROP_UPDATED_MIN,
};

G_DEFINE_TYPE (GDataTasksQuery, gdata_tasks_query, GDATA_TYPE_QUERY)

static void
gdata_tasks_query_class_init (GDataTasksQueryClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GDataQueryClass *query_class = GDATA_QUERY_CLASS (klass);

	g_type_class_add_private (klass, sizeof (GDataTasksQueryPrivate));

	gobject_class->set_property = gdata_tasks_query_set_property;
	gobject_class->get_property = gdata_tasks_query_get_property;
	gobject_class->finalize = gdata_tasks_query_finalize;

	query_class->get_query_uri = get_query_uri;

	/**
	 * GDataTasksQuery:completed-max:
	 *
	 * Upper bound for a task's completion date (as a RFC 3339 timestamp) to filter by. Optional.
	 * The default is not to filter by completion date. 
	 */
	g_object_class_install_property (gobject_class, PROP_COMPLETED_MAX,
	                                 g_param_spec_int64 ("completed-max",
	                                                       "Max task completion date", "Upper bound for a task's completion date to filter by.",
	                                                       -1, G_MAXINT64, -1,
	                                                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * GDataTasksQuery:completed-min:
	 *
	 * Lower bound for a task's completion date (as a RFC 3339 timestamp) to filter by. Optional.
	 * The default is not to filter by completion date. 
	 */
	g_object_class_install_property (gobject_class, PROP_COMPLETED_MIN,
	                                 g_param_spec_int64 ("completed-min",
	                                                       "Min task completion date", "Lower bound for a task's completion date to filter by.",
	                                                       -1, G_MAXINT64, -1,
	                                                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * GDataTasksQuery:due-max:
	 *
	 * Upper bound for a task's due date (as a RFC 3339 timestamp) to filter by. Optional.
	 * The default is not to filter by completion date. 
	 */
	g_object_class_install_property (gobject_class, PROP_DUE_MAX,
	                                 g_param_spec_int64 ("due-max",
	                                                       "Max task completion date", "Upper bound for a task's completion date to filter by.",
	                                                       -1, G_MAXINT64, -1,
	                                                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * GDataTasksQuery:due-min:
	 *
	 * Lower bound for a task's due date (as a RFC 3339 timestamp) to filter by. Optional.
	 * The default is not to filter by completion date. 
	 */
	g_object_class_install_property (gobject_class, PROP_DUE_MIN,
	                                 g_param_spec_int64 ("due-min",
	                                                       "Min task completion date", "Lower bound for a task's completion date to filter by.",
	                                                       -1, G_MAXINT64, -1,
	                                                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * GDataTasksQuery:is-show-completed:
	 *
	 * Flag indicating whether completed tasks are returned in the result. Optional. The default is True. 
	 */
	g_object_class_install_property (gobject_class, PROP_SHOW_COMPLETED,
	                                 g_param_spec_boolean ("is-show-completed",
	                                                       "Show completed tasks?", "Indicated whatever completed tasks are returned in the result.",
	                                                       FALSE,
	                                                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * GDataTasksQuery:is-show-deleted:
	 *
	 * Flag indicating whether deleted tasks are returned in the result. Optional. The default is False. 
	 */
	g_object_class_install_property (gobject_class, PROP_SHOW_DELETED,
	                                 g_param_spec_boolean ("is-show-deleted",
	                                                       "Show deleted tasks?", "Indicated whatever deleted tasks are returned in the result.",
	                                                       FALSE,
	                                                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	/**
	 * GDataTasksQuery:is-show-hidden:
	 *
	 * Flag indicating whether hidden tasks are returned in the result. Optional. The default is False. 
	 */
	g_object_class_install_property (gobject_class, PROP_SHOW_HIDDEN,
	                                 g_param_spec_boolean ("is-show-hidden",
	                                                       "Show hidden tasks?", "Indicated whatever hidden tasks are returned in the result.",
	                                                       FALSE,
	                                                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gdata_tasks_query_init (GDataTasksQuery *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GDATA_TYPE_TASKS_QUERY, GDataTasksQueryPrivate);
	self->priv->completed_min = -1;
	self->priv->completed_max = -1;
	self->priv->due_min = -1;
	self->priv->due_max = -1;
}

static void
gdata_tasks_query_finalize (GObject *object)
{
	/* Chain up to the parent class */
	G_OBJECT_CLASS (gdata_tasks_query_parent_class)->finalize (object);
}

static void
gdata_tasks_query_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	GDataTasksQueryPrivate *priv = GDATA_TASKS_QUERY (object)->priv;

	switch (property_id) {
		case PROP_COMPLETED_MAX:
			g_value_set_int64 (value, priv->completed_max);
			break;
		case PROP_COMPLETED_MIN:
			g_value_set_int64 (value, priv->completed_min);
			break;
		case PROP_DUE_MAX:
			g_value_set_int64 (value, priv->due_max);
			break;
		case PROP_DUE_MIN:
			g_value_set_int64 (value, priv->due_min);
			break;
		case PROP_SHOW_COMPLETED:
			g_value_set_boolean (value, priv->show_completed);
			break;
		case PROP_SHOW_DELETED:
			g_value_set_boolean (value, priv->show_deleted);
			break;
		case PROP_SHOW_HIDDEN:
			g_value_set_boolean (value, priv->show_hidden);
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
gdata_tasks_query_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	GDataTasksQuery *self = GDATA_TASKS_QUERY (object);

	switch (property_id) {
		case PROP_COMPLETED_MAX:
			gdata_tasks_query_set_completed_max (self, g_value_get_int64 (value));
			break;
		case PROP_COMPLETED_MIN:
			gdata_tasks_query_set_completed_min (self, g_value_get_int64 (value));
			break;
		case PROP_DUE_MAX:
			gdata_tasks_query_set_due_max (self, g_value_get_int64 (value));
			break;
		case PROP_DUE_MIN:
			gdata_tasks_query_set_due_min (self, g_value_get_int64 (value));
			break;
		case PROP_SHOW_COMPLETED:
			gdata_tasks_query_set_is_show_completed (self, g_value_get_boolean (value));
			break;
		case PROP_SHOW_DELETED:
			gdata_tasks_query_set_is_show_deleted (self, g_value_get_boolean (value));
			break;
		case PROP_SHOW_HIDDEN:
			gdata_tasks_query_set_is_show_hidden (self, g_value_get_boolean (value));
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
get_query_uri (GDataQuery *self, const gchar *feed_uri, GString *query_uri, gboolean *params_started)
{
	GDataTasksQueryPrivate *priv = GDATA_TASKS_QUERY (self)->priv;

	#define APPEND_SEP g_string_append_c (query_uri, (*params_started == FALSE) ? '?' : '&'); *params_started = TRUE;

	if (gdata_query_get_max_results (GDATA_QUERY(self)) > 0) {
		APPEND_SEP
		g_string_append_printf (query_uri, "maxResults=%u", gdata_query_get_max_results (GDATA_QUERY(self)));
	}

	if (gdata_query_get_updated_min (GDATA_QUERY(self)) != -1) {
		gchar *updated_min;

		APPEND_SEP
		g_string_append (query_uri, "updatedMin=");
		updated_min = gdata_parser_int64_to_iso8601 (gdata_query_get_updated_min (GDATA_QUERY(self)));
		g_string_append (query_uri, updated_min);
		g_free (updated_min);
	}

	if (priv->completed_min != -1) {
		gchar *completed_min;

		APPEND_SEP
		g_string_append (query_uri, "completedMin=");
		completed_min = gdata_parser_int64_to_iso8601 (priv->completed_min);
		g_string_append (query_uri, completed_min);
		g_free (completed_min);
	}

	if (priv->completed_max != -1) {
		gchar *completed_max;

		APPEND_SEP
		g_string_append (query_uri, "completedMax=");
		completed_max = gdata_parser_int64_to_iso8601 (priv->completed_max);
		g_string_append (query_uri, completed_max);
		g_free (completed_max);
	}

	if (priv->due_min != -1) {
		gchar *due_min;

		APPEND_SEP
		g_string_append (query_uri, "dueMin=");
		due_min = gdata_parser_int64_to_iso8601 (priv->due_min);
		g_string_append (query_uri, due_min);
		g_free (due_min);
	}

	if (priv->due_max != -1) {
		gchar *due_max;

		APPEND_SEP
		g_string_append (query_uri, "dueMax=");
		due_max = gdata_parser_int64_to_iso8601 (priv->due_max);
		g_string_append (query_uri, due_max);
		g_free (due_max);
	}

	APPEND_SEP
	if (priv->show_completed == TRUE) {
		g_string_append (query_uri, "showCompleted=true");
	} else {
		g_string_append (query_uri, "showCompleted=false");
	}

	APPEND_SEP
	if (priv->show_deleted == TRUE) {
		g_string_append (query_uri, "showDeleted=true");
	} else {
		g_string_append (query_uri, "showDeleted=false");
	}

	APPEND_SEP
	if (priv->show_hidden == TRUE) {
		g_string_append (query_uri, "showHidden=true");
	} else {
		g_string_append (query_uri, "showHidden=false");
	}
	
	/* We don't chain up with parent class get_query_uri because it uses
	 *  GData protocol parameters and they aren't compatible with newest API family
	 */
}

/**
 * gdata_tasks_query_new:
 * @q: (allow-none): a query string, or %NULL
 *
 * Creates a new #GDataTasksQuery with its #GDataQuery:q property set to @q.
 *
 * Return value: a new #GDataTasksQuery
 * 
 * Since: 0.13.4
 */
GDataTasksQuery *
gdata_tasks_query_new (const gchar *q)
{
	return g_object_new (GDATA_TYPE_TASKS_QUERY, "q", q, NULL);
}

/**
 * gdata_tasks_query_get_completed_max:
 * @self: a #GDataTasksQuery
 *
 * Gets the #GDataTasksQuery:completed-max property. If the property is unset, <code class="literal">-1</code> will be returned.
 *
 * Return value: the UNIX timestamp for the completed-max property, or <code class="literal">-1</code>
 * 
 * Since: 0.13.4
 */
gint64
gdata_tasks_query_get_completed_max (GDataTasksQuery *self)
{
	g_return_val_if_fail (GDATA_IS_TASKS_QUERY (self), -1);
	return self->priv->completed_max;
}

/**
 * gdata_tasks_query_set_completed_max:
 * @self: a #GDataTasksQuery
 * @completed_max: upper bound for a task's completion date by UNIX timestamp, or  <code class="literal">-1</code>
 * 
 * Sets the #GDataTasksQuery:completed-max property of the #GDataTasksQuery
 * to the new time/date, @completed_max.
 *
 * Set @completed_max to <code class="literal">-1</code> to unset the property in the query URI.
 * 
 * Since: 0.13.4
 */
void
gdata_tasks_query_set_completed_max (GDataTasksQuery *self, gint64 completed_max)
{
	g_return_if_fail (GDATA_IS_TASKS_QUERY (self));
	g_return_if_fail (completed_max >= -1);
	
	self->priv->completed_max = completed_max;
	g_object_notify (G_OBJECT (self), "completed-max");
}

/**
 * gdata_tasks_query_get_completed_min:
 * @self: a #GDataTasksQuery
 *
 * Gets the #GDataTasksQuery:completed-min property. If the property is unset, <code class="literal">-1</code> will be returned.
 *
 * Return value: the UNIX timestamp for the completed-min property, or <code class="literal">-1</code>
 * 
 * Since: 0.13.4
 */
gint64
gdata_tasks_query_get_completed_min (GDataTasksQuery *self)
{
	g_return_val_if_fail (GDATA_IS_TASKS_QUERY (self), -1);
	return self->priv->completed_min;
}

/**
 * gdata_tasks_query_set_completed_min:
 * @self: a #GDataTasksQuery
 * @completed_max: lower bound for a task's completion date by UNIX timestamp, or  <code class="literal">-1</code>
 * 
 * Sets the #GDataTasksQuery:completed-min property of the #GDataTasksQuery
 * to the new time/date, @completed_min.
 * 
 * Set @completed_max to <code class="literal">-1</code> to unset the property in the query URI.
 * 
 * Since: 0.13.4
 */
void
gdata_tasks_query_set_completed_min (GDataTasksQuery *self, gint64 completed_min)
{
	g_return_if_fail (GDATA_IS_TASKS_QUERY (self));
	g_return_if_fail (completed_min >= -1);
	
	self->priv->completed_min = completed_min;
	g_object_notify (G_OBJECT (self), "completed-min");
}

/**
 * gdata_tasks_query_get_due_max:
 * @self: a #GDataTasksQuery
 *
 * Gets the #GDataTasksQuery:due-max property. If the property is unset, <code class="literal">-1</code> will be returned.
 *
 * Return value: the UNIX timestamp for the due-max property, or <code class="literal">-1</code>
 * 
 * Since: 0.13.4
 */
gint64
gdata_tasks_query_get_due_max (GDataTasksQuery *self)
{
	g_return_val_if_fail (GDATA_IS_TASKS_QUERY (self), -1);
	return self->priv->due_max;
}

/**
 * gdata_tasks_query_set_due_max:
 * @self: a #GDataTasksQuery
 * @due_max: upper bound for a task's due date by UNIX timestamp, or  <code class="literal">-1</code>
 * 
 * Sets the #GDataTasksQuery:due-max property of the #GDataTasksQuery
 * to the new time/date, @due_max.
 *
 * Set @due_max to <code class="literal">-1</code> to unset the property in the query URI.
 * 
 * Since: 0.13.4
 */
void
gdata_tasks_query_set_due_max (GDataTasksQuery *self, gint64 due_max)
{
	g_return_if_fail (GDATA_IS_TASKS_QUERY (self));
	g_return_if_fail (due_max >= -1);
	
	self->priv->due_max = due_max;
	g_object_notify (G_OBJECT (self), "due-max");
}


/**
 * gdata_tasks_query_get_due_min:
 * @self: a #GDataTasksQuery
 *
 * Gets the #GDataTasksQuery:due-min property. If the property is unset, <code class="literal">-1</code> will be returned.
 *
 * Return value: the UNIX timestamp for the due-min property, or <code class="literal">-1</code>
 * 
 * Since: 0.13.4
 */
gint64
gdata_tasks_query_get_due_min (GDataTasksQuery *self)
{
	g_return_val_if_fail (GDATA_IS_TASKS_QUERY (self), -1);
	return self->priv->due_min;
}

/**
 * gdata_tasks_query_set_due_min:
 * @self: a #GDataTasksQuery
 * @due_max: lower bound for a task's due date by UNIX timestamp, or  <code class="literal">-1</code>
 * 
 * Sets the #GDataTasksQuery:due-min property of the #GDataTasksQuery
 * to the new time/date, @due_min.
 * 
 * Set @due_max to <code class="literal">-1</code> to unset the property in the query URI.
 * 
 * Since: 0.13.4
 */
void
gdata_tasks_query_set_due_min (GDataTasksQuery *self, gint64 due_min)
{
	g_return_if_fail (GDATA_IS_TASKS_QUERY (self));
	g_return_if_fail (due_min >= -1);
	
	self->priv->due_min = due_min;
	g_object_notify (G_OBJECT (self), "d-min");
}

/**
 * gdata_tasks_query_is_show_completed:
 * @self: a #GDataTasksQuery
 *
 * Gets the #GDataTasksQuery:is-show-completed property.
 *
 * Return value: the is-show-completed property
 * 
 * Since: 0.13.4
 */
gboolean
gdata_tasks_query_is_show_completed (GDataTasksQuery *self)
{
	g_return_val_if_fail (GDATA_IS_TASKS_QUERY (self), FALSE);
	return self->priv->show_completed;
}

/**
 * gdata_tasks_query_set_is_show_completed:
 * @self: a #GDataTasksQuery
 * @show_completed: %TRUE to show completed tasks, %FALSE otherwise
 *
 * Sets the #GDataTasksQuery:is-show-completed property of the #GDataTasksQuery.
 *
 * Since: 0.13.4
 */
void
gdata_tasks_query_set_is_show_completed (GDataTasksQuery *self, gboolean show_completed)
{
	g_return_if_fail (GDATA_IS_TASKS_QUERY (self));

	self->priv->show_completed = show_completed;
	g_object_notify (G_OBJECT (self), "show-completed");
}

/**
 * gdata_tasks_query_is_show_deleted:
 * @self: a #GDataTasksQuery
 *
 * Gets the #GDataTasksQuery:is-show-deleted property.
 *
 * Return value: the is-show-deleted property
 * 
 * Since: 0.13.4
 */
gboolean
gdata_tasks_query_is_show_deleted (GDataTasksQuery *self)
{
	g_return_val_if_fail (GDATA_IS_TASKS_QUERY (self), FALSE);
	return self->priv->show_deleted;
}

/**
 * gdata_tasks_query_set_is_show_deleted:
 * @self: a #GDataTasksQuery
 * @show_deleted: %TRUE to show deleted tasks, %FALSE otherwise
 *
 * Sets the #GDataTasksQuery:is-show-deleted property of the #GDataTasksQuery.
 *
 * Since: 0.13.4
 */
void
gdata_tasks_query_set_is_show_deleted (GDataTasksQuery *self, gboolean show_deleted)
{
	g_return_if_fail (GDATA_IS_TASKS_QUERY (self));

	self->priv->show_deleted = show_deleted;
	g_object_notify (G_OBJECT (self), "show-deleted");
}

/**
 * gdata_tasks_query_is_show_hidden:
 * @self: a #GDataTasksQuery
 *
 * Gets the #GDataTasksQuery:is-show-hidden property.
 *
 * Return value: the is-show-hidden property
 * 
 * Since: 0.13.4
 */
gboolean
gdata_tasks_query_is_show_hidden (GDataTasksQuery *self)
{
	g_return_val_if_fail (GDATA_IS_TASKS_QUERY (self), FALSE);
	return self->priv->show_hidden;
}

/**
 * gdata_tasks_query_set_is_show_hidden:
 * @self: a #GDataTasksQuery
 * @show_hidden: %TRUE to show hidden tasks, %FALSE otherwise
 *
 * Sets the #GDataTasksQuery:is-show-hidden property of the #GDataTasksQuery.
 *
 * Since: 0.13.4
 */
void
gdata_tasks_query_set_is_show_hidden (GDataTasksQuery *self, gboolean show_hidden)
{
	g_return_if_fail (GDATA_IS_TASKS_QUERY (self));

	self->priv->show_hidden = show_hidden;
	g_object_notify (G_OBJECT (self), "show-hidden");
}
