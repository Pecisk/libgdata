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
 * SECTION:gdata-tasks-task
 * @short_description: GData Tasks task object
 * @stability: Unstable
 * @include: gdata/services/tasks/gdata-tasks-task.h
 *
 * #GDataTasksTask is a subclass of #GDataEntry to represent a task in a tasklist from Google Tasks.
 *
 * For more details of Google Tasks API, see the <ulink type="http" url="https://developers.google.com/google-apps/tasks/v1/reference/">
 * online documentation</ulink>.
 *
 */

#include <config.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <libxml/parser.h>
#include <string.h>

#include "gdata-tasks-task.h"
#include "gdata-private.h"
#include "gdata-service.h"
#include "gdata-parser.h"
#include "gdata-types.h"
#include "gdata-comparable.h"

static GObject *gdata_tasks_task_constructor (GType type, guint n_construct_params, GObjectConstructParam *construct_params);
static void gdata_tasks_task_dispose (GObject *object);
static void gdata_tasks_task_finalize (GObject *object);
static void gdata_tasks_task_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void gdata_tasks_task_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void get_json (GDataParsable *parsable, JsonBuilder *builder);
static gboolean parse_json (GDataParsable *parsable, JsonReader *reader, gpointer user_data, GError **error);
static const gchar *get_content_type (void);

struct _GDataTasksTaskPrivate {
	gchar *parent;
	gchar *position;
	gchar *notes;
	gchar *status;
	gint64 due;
	gint64 completed;
	gboolean deleted;
	gboolean hidden;
};

enum {
	PROP_PARENT = 1,
	PROP_POSITION,
	PROP_NOTES,
	PROP_STATUS,
	PROP_DUE,
	PROP_COMPLETED,
	PROP_DELETED,
	PROP_HIDDEN,
};

G_DEFINE_TYPE (GDataTasksTask, gdata_tasks_task, GDATA_TYPE_ENTRY)

static void
gdata_tasks_task_class_init (GDataTasksTaskClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GDataParsableClass *parsable_class = GDATA_PARSABLE_CLASS (klass);

	g_type_class_add_private (klass, sizeof (GDataTasksTaskPrivate));

	gobject_class->constructor = gdata_tasks_task_constructor;
	gobject_class->get_property = gdata_tasks_task_get_property;
	gobject_class->set_property = gdata_tasks_task_set_property;
	gobject_class->dispose = gdata_tasks_task_dispose;
	gobject_class->finalize = gdata_tasks_task_finalize;

	parsable_class->parse_json = parse_json;
	parsable_class->get_json = get_json;
	parsable_class->get_content_type = get_content_type;

	/**
	 * GDataTasksTask:parent:
	 *
	 * Parent task identifier. This field is omitted if it is a top-level task. This field is read-only.
	 *
	 */
	g_object_class_install_property (gobject_class, PROP_PARENT,
	                                 g_param_spec_string ("parent",
	                                                      "Parent of task", "Identifier of parent task.",
	                                                      NULL,
	                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * GDataTasksTask:position:
	 *
	 * String indicating the position of the task among its sibling tasks under the same parent task
	 * or at the top level. If this string is greater than another task's corresponding position string
	 * according to lexicographical ordering, the task is positioned after the other task under the same
	 * parent task (or at the top level). This field is read-only.
	 * 
	 */
	g_object_class_install_property (gobject_class, PROP_STATUS,
	                                 g_param_spec_string ("position",
	                                                      "Position of task", "Position of the task among sibling tasks using lexicographical order.",
	                                                      NULL,
	                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * GDataTasksTask:notes:
	 *
	 * Notes describing the task
	 */
	g_object_class_install_property (gobject_class, PROP_NOTES,
	                                 g_param_spec_string ("notes",
	                                                      "Notes of task", "Notes describing the task.",
	                                                      NULL,
	                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * GDataTasksTask:status:
	 *
	 * Status of the task. This is either "needsAction" or "completed".
	 *
	 */
	g_object_class_install_property (gobject_class, PROP_STATUS,
	                                 g_param_spec_string ("status",
	                                                      "Status of task", "Status of the task. This is either \"needsAction\" or \"completed\".",
	                                                      NULL,
	                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * GDataTasksTask:due:
	 *
	 * Due date of the task (as a RFC 3339 timestamp). Optional.
	 * 
	 */
	g_object_class_install_property (gobject_class, PROP_DUE,
	                                 g_param_spec_int64 ("due",
	                                                       "Due date of the task", "Due date of the task.",
	                                                       -1, G_MAXINT64, -1,
	                                                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * GDataTasksTask:completed:
	 *
	 * Completion date of the task (as a RFC 3339 timestamp).
	 * This field is omitted if the task has not been completed.
	 * 
	 */
	g_object_class_install_property (gobject_class, PROP_COMPLETED,
	                                 g_param_spec_int64 ("completed",
	                                                       "Completion date of task", "Completion date of the task.",
	                                                       -1, G_MAXINT64, -1,
	                                                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * GDataTasksTask:is-deleted:
	 *
	 * Flag indicating whether the task has been deleted. The default if False.
	 *  
	 */
	g_object_class_install_property (gobject_class, PROP_DELETED,
	                                 g_param_spec_boolean ("is-deleted",
	                                                       "Deleted?", "Indicated whatever task is deleted.",
	                                                       FALSE,
	                                                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * GDataTasksTask:is-hidden:
	 *
	 * Flag indicating whether completed tasks are returned in the result. Optional. The default is True. 
	 */
	g_object_class_install_property (gobject_class, PROP_HIDDEN,
	                                 g_param_spec_boolean ("is-hidden",
	                                                       "Hidden?", "Indicated whatever task is hidden.",
	                                                       FALSE,
	                                                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gdata_tasks_task_init (GDataTasksTask *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GDATA_TYPE_TASKS_TASK, GDataTasksTaskPrivate);
	self->priv->due = -1;
	self->priv->completed = -1;
	/* FIXME make sure we don't have to initialise anything else */
}

static GObject *
gdata_tasks_task_constructor (GType type, guint n_construct_params, GObjectConstructParam *construct_params)
{
	GObject *object;
	/* Chain up to the parent class */
	object = G_OBJECT_CLASS (gdata_tasks_task_parent_class)->constructor (type, n_construct_params, construct_params);
	
	return object;
}

static void
gdata_tasks_task_dispose (GObject *object)
{

	/* Chain up to the parent class */
	G_OBJECT_CLASS (gdata_tasks_task_parent_class)->dispose (object);
}

static void
gdata_tasks_task_finalize (GObject *object)
{
	GDataTasksTaskPrivate *priv = GDATA_TASKS_TASK (object)->priv;

	g_free (priv->parent);
	g_free (priv->position);
	g_free (priv->notes);
	g_free (priv->status);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (gdata_tasks_task_parent_class)->finalize (object);
}

static void
gdata_tasks_task_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	GDataTasksTaskPrivate *priv = GDATA_TASKS_TASK (object)->priv;

	switch (property_id) {
		case PROP_PARENT:
			g_value_set_string (value, priv->parent);
			break;
		case PROP_POSITION:
			g_value_set_string (value, priv->position);
			break;
		case PROP_NOTES:
			g_value_set_string (value, priv->notes);
			break;
		case PROP_STATUS:
			g_value_set_string (value, priv->status);
			break;
		case PROP_DUE:
			g_value_set_int64 (value, priv->due);
			break;
		case PROP_COMPLETED:
			g_value_set_int64 (value, priv->completed);
			break;
		case PROP_DELETED:
			g_value_set_boolean (value, priv->deleted);
			break;
		case PROP_HIDDEN:
			g_value_set_boolean (value, priv->hidden);
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
gdata_tasks_task_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	GDataTasksTask *self = GDATA_TASKS_TASK (object);

	switch (property_id) {
		case PROP_PARENT:
			gdata_tasks_task_set_parent (self, g_value_get_string (value));
			break;
		case PROP_POSITION:
			gdata_tasks_task_set_position (self, g_value_get_string (value));
			break;
		case PROP_NOTES:
			gdata_tasks_task_set_notes (self, g_value_get_string (value));
			break;
		case PROP_STATUS:
			gdata_tasks_task_set_status (self, g_value_get_string (value));
			break;
		case PROP_DUE:
			gdata_tasks_task_set_due (self, g_value_get_int64 (value));
			break;
		case PROP_COMPLETED:
			gdata_tasks_task_set_completed (self, g_value_get_int64 (value));
			break;
		case PROP_DELETED:
			gdata_tasks_task_set_is_deleted (self, g_value_get_boolean (value));
			break;
		case PROP_HIDDEN:
			gdata_tasks_task_set_is_hidden (self, g_value_get_boolean (value));
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static gboolean
parse_json (GDataParsable *parsable, JsonReader *reader, gpointer user_data, GError **error)
{
	gboolean success;
	GDataTasksTask *self = GDATA_TASKS_TASK (parsable);
	
	if (gdata_parser_string_from_json_member (reader, "parent", P_DEFAULT, &(self->priv->parent), &success, error) == TRUE ||
        gdata_parser_string_from_json_member (reader, "position", P_DEFAULT, &(self->priv->position), &success, error) == TRUE ||
        gdata_parser_string_from_json_member (reader, "notes", P_DEFAULT, &(self->priv->notes), &success, error) == TRUE ||
        gdata_parser_string_from_json_member (reader, "status", P_DEFAULT, &(self->priv->status), &success, error) == TRUE ||
        gdata_parser_int64_time_from_json_member (reader, "due", P_DEFAULT, &(self->priv->due), &success, error) == TRUE ||
        gdata_parser_int64_time_from_json_member (reader, "completed", P_DEFAULT, &(self->priv->completed), &success, error) == TRUE ||
        gdata_parser_boolean_from_json_member (reader, "deleted", P_DEFAULT, &(self->priv->deleted), &success, error) == TRUE ||
        gdata_parser_boolean_from_json_member (reader, "hidden", P_DEFAULT, &(self->priv->hidden), &success, error) == TRUE) {
		return success;
	} else {
		return GDATA_PARSABLE_CLASS (gdata_tasks_task_parent_class)->parse_json (parsable, reader, user_data, error);
	}
	
	return TRUE;
}

static void
get_json (GDataParsable *parsable, JsonBuilder *builder)
{
	GDataTasksTaskPrivate *priv = GDATA_TASKS_TASK (parsable)->priv;

	/* Chain up to the parent class */
	GDATA_PARSABLE_CLASS (gdata_tasks_task_parent_class)->get_json (parsable, builder);

	/* Add all the task specific JSON */

	if (priv->parent != NULL) {
		json_builder_set_member_name (builder, "parent");
		json_builder_add_string_value (builder, priv->parent);
	}
	if (priv->position != NULL) {
		json_builder_set_member_name (builder, "position");
		json_builder_add_string_value (builder, priv->position);
	}
	if (priv->notes != NULL) {
		json_builder_set_member_name (builder, "notes");
		json_builder_add_string_value (builder, priv->notes);
	}
	if (priv->status != NULL) {
		json_builder_set_member_name (builder, "status");
		json_builder_add_string_value (builder, priv->status);
	}
	if (priv->due != -1) {
		json_builder_set_member_name (builder, "due");
		json_builder_add_string_value (builder, gdata_parser_int64_to_iso8601 (priv->due));
	}
	if (priv->completed != -1) {
		json_builder_set_member_name (builder, "completed");
		json_builder_add_string_value (builder, gdata_parser_int64_to_iso8601 (priv->completed));
	}

	if(priv->deleted == TRUE) {
		json_builder_set_member_name (builder, "deleted");
		json_builder_add_boolean_value (builder, TRUE);
	}
	else {
		json_builder_set_member_name (builder, "deleted");
		json_builder_add_boolean_value (builder, FALSE);
	}
}

static const gchar *
get_content_type (void) {
	return "application/json";	
}

/**
 * gdata_tasks_task_new:
 * @id: (allow-none): the event's ID, or %NULL
 *
 * Creates a new #GDataTasksTask with the given ID and default properties.
 *
 * Return value: a new #GDataTasksTask; unref with g_object_unref()
 * 
 * Since: 0.13.4
 */
GDataTasksTask *
gdata_tasks_task_new (const gchar *id)
{
	return GDATA_TASKS_TASK (g_object_new (GDATA_TYPE_TASKS_TASK, "id", id, NULL));
}

/**
 * gdata_tasks_task_get_parent:
 * @self: a #GDataTasksTask
 *
 * Gets the #GDataTasksTask:parent property.
 *
 * Return value: the parent of the task, or %NULL
 *
 * Since: 0.13.4
 */
const gchar *
gdata_tasks_task_get_parent (GDataTasksTask *self)
{
	g_return_val_if_fail (GDATA_IS_TASKS_TASK (self), NULL);
	return self->priv->parent;
}

/**
 * gdata_tasks_task_set_parent:
 * @self: a #GDataTasksTask
 * @parent: (allow-none): a new parent of the task, or %NULL
 *
 * Sets the #GDataTasksTask:parent property to the new parent, @parent.
 *
 * Set @parent to %NULL to unset the property in the task.
 *
 * Since: 0.13.4
 */
void
gdata_tasks_task_set_parent (GDataTasksTask *self, const gchar *parent)
{
	g_return_if_fail (GDATA_IS_TASKS_TASK (self));

	g_free (self->priv->parent);
	self->priv->parent = g_strdup (parent);
	g_object_notify (G_OBJECT (self), "parent");
}

/**
 * gdata_tasks_task_get_position:
 * @self: a #GDataTasksTask
 *
 * Gets the #GDataTasksTask:position property.
 *
 * Return value: the position of the task, or %NULL
 *
 * Since: 0.13.4
 */
const gchar *
gdata_tasks_task_get_position (GDataTasksTask *self)
{
	g_return_val_if_fail (GDATA_IS_TASKS_TASK (self), NULL);
	return self->priv->position;
}

/**
 * gdata_tasks_task_set_position:
 * @self: a #GDataTasksTask
 * @position: (allow-none): a new position of the task, or %NULL
 *
 * Sets the #GDataTasksTask:position property to the new position, @position.
 *
 * Set @position to %NULL to unset the property in the task.
 *
 * Since: 0.13.4
 */
void
gdata_tasks_task_set_position (GDataTasksTask *self, const gchar *position)
{
	g_return_if_fail (GDATA_IS_TASKS_TASK (self));

	g_free (self->priv->position);
	self->priv->position = g_strdup (position);
	g_object_notify (G_OBJECT (self), "position");
}

/**
 * gdata_tasks_task_get_notes:
 * @self: a #GDataTasksTask
 *
 * Gets the #GDataTasksTask:notes property.
 *
 * Return value: notes of the task, or %NULL
 *
 * Since: 0.13.4
 */
const gchar *
gdata_tasks_task_get_notes (GDataTasksTask *self)
{
	g_return_val_if_fail (GDATA_IS_TASKS_TASK (self), NULL);
	return self->priv->notes;
}

/**
 * gdata_tasks_task_set_notes:
 * @self: a #GDataTasksTask
 * @notes: (allow-none): a new notes of the task, or %NULL
 *
 * Sets the #GDataTasksTask:notes property to the new notes, @notes.
 *
 * Set @notes to %NULL to unset the property in the task.
 *
 * Since: 0.13.4
 */
void
gdata_tasks_task_set_notes (GDataTasksTask *self, const gchar *notes)
{
	g_return_if_fail (GDATA_IS_TASKS_TASK (self));

	g_free (self->priv->notes);
	self->priv->notes = g_strdup (notes);
	g_object_notify (G_OBJECT (self), "notes");
}

/**
 * gdata_tasks_task_get_status:
 * @self: a #GDataTasksTask
 *
 * Gets the #GDataTasksTask:status property.
 *
 * Return value: the status of the task, or %NULL
 *
 * Since: 0.13.4
 */
const gchar *
gdata_tasks_task_get_status (GDataTasksTask *self)
{
	g_return_val_if_fail (GDATA_IS_TASKS_TASK (self), NULL);
	return self->priv->status;
}

/**
 * gdata_tasks_task_set_status:
 * @self: a #GDataTasksTask
 * @status: (allow-none): a new status of the task, or %NULL
 *
 * Sets the #GDataTasksTask:status property to the new status, @status.
 *
 * Set @status to %NULL to unset the property in the task.
 *
 * Since: 0.13.4
 */
void
gdata_tasks_task_set_status (GDataTasksTask *self, const gchar *status)
{
	g_return_if_fail (GDATA_IS_TASKS_TASK (self));

	g_free (self->priv->status);
	self->priv->status = g_strdup (status);
	g_object_notify (G_OBJECT (self), "status");
}

/**
 * gdata_tasks_task_get_due:
 * @self: a #GDataTasksTask
 *
 * Gets the #GDataTasksTask:due property. If the property is unset, <code class="literal">-1</code> will be returned.
 *
 * Return value: the due property, or <code class="literal">-1</code>
 * 
 * Since: 0.13.4
 */
gint64
gdata_tasks_task_get_due (GDataTasksTask *self)
{
	g_return_val_if_fail (GDATA_IS_TASKS_TASK (self), -1);
	return self->priv->due;
}

/**
 * gdata_tasks_task_set_due:
 * @self: a #GDataTasksTask
 * @due: due time of the task, or <code class="literal">-1</code>
 *
 * Sets the #GDataTasksTask:due property of the #GDataTasksTask to the new due time of the task, @due.
 *
 * Set @due to <code class="literal">-1</code> to unset the property in the due time of the task
 * 
 * Since: 0.13.4
 */
void
gdata_tasks_task_set_due (GDataTasksTask *self, gint64 due)
{
	g_return_if_fail (GDATA_IS_TASKS_TASK (self));
	g_return_if_fail (due >= -1);

	self->priv->due = due;
	g_object_notify (G_OBJECT (self), "due");
}

/**
 * gdata_tasks_task_get_completed:
 * @self: a #GDataTasksTask
 *
 * Gets the #GDataTasksTask:completed property. If the property is unset, <code class="literal">-1</code> will be returned.
 *
 * Return value: the completed property, or <code class="literal">-1</code>
 * 
 * Since: 0.13.4
 */
gint64
gdata_tasks_task_get_completed (GDataTasksTask *self)
{
	g_return_val_if_fail (GDATA_IS_TASKS_TASK (self), -1);
	return self->priv->completed;
}

/**
 * gdata_tasks_task_set_completed:
 * @self: a #GDataTasksTask
 * @completed: completion time of the task, or <code class="literal">-1</code>
 *
 * Sets the #GDataTasksTask:completed property of the #GDataTasksTask to the new completion time of the task, @due.
 *
 * Set @completed to <code class="literal">-1</code> to unset the property in the completion time of the task
 * 
 * Since: 0.13.4
 */
void
gdata_tasks_task_set_completed (GDataTasksTask *self, gint64 completed)
{
	g_return_if_fail (GDATA_IS_TASKS_TASK (self));
	g_return_if_fail (completed >= -1);

	self->priv->completed = completed;
	g_object_notify (G_OBJECT (self), "completed");
}

/**
 * gdata_tasks_task_is_deleted:
 * @self: a #GDataTasksTask
 *
 * Gets the #GDataTasksTask:is-deleted property.
 *
 * Return value: %TRUE if event is deleted, %FALSE otherwise
 * 
 * Since: 0.13.4
 */
gboolean
gdata_tasks_task_is_deleted (GDataTasksTask *self)
{
	g_return_val_if_fail (GDATA_IS_TASKS_TASK (self), FALSE);
	return self->priv->deleted;
}

/**
 * gdata_tasks_task_set_is_deleted:
 * @self: a #GDataTasksTask
 * @deleted: %TRUE if task is deleted, %FALSE otherwise
 *
 * Sets the #GDataTasksTask:is-deleted property to @deleted.
 * 
 * Since: 0.13.4
 */
void
gdata_tasks_task_set_is_deleted (GDataTasksTask *self, gboolean deleted)
{
	g_return_if_fail (GDATA_IS_TASKS_TASK (self));
	self->priv->deleted = deleted;
	g_object_notify (G_OBJECT (self), "deleted");
}

/**
 * gdata_tasks_task_is_hidden:
 * @self: a #GDataTasksTask
 *
 * Gets the #GDataTasksTask:is-hidden property.
 *
 * Return value: %TRUE if event is hidden, %FALSE otherwise
 * 
 * Since: 0.13.4
 */
gboolean
gdata_tasks_task_is_hidden (GDataTasksTask *self)
{
	g_return_val_if_fail (GDATA_IS_TASKS_TASK (self), FALSE);
	return self->priv->hidden;
}

/**
 * gdata_tasks_task_set_is_hidden:
 * @self: a #GDataTasksTask
 * @hidden: %TRUE if task is hidden, %FALSE otherwise
 *
 * Sets the #GDataTasksTask:is-hidden property to @hidden.
 * 
 * Since: 0.13.4
 */
void
gdata_tasks_task_set_is_hidden (GDataTasksTask *self, gboolean hidden)
{
	g_return_if_fail (GDATA_IS_TASKS_TASK (self));
	self->priv->hidden = hidden;
	g_object_notify (G_OBJECT (self), "hidden");
}
