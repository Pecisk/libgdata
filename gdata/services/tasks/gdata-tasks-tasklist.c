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
 * SECTION:gdata-tasks-tasklist
 * @short_description: GData Tasks tasklist object
 * @stability: Unstable
 * @include: gdata/services/tasks/gdata-tasks-tasklist.h
 *
 * #GDataTasksTasklist is a subclass of #GDataEntry to represent a tasklist from Google Tasks.
 *
 * For more details of Google Tasks API, see the <ulink type="http" url="https://developers.google.com/google-apps/tasks/v1/reference/">
 * online documentation</ulink>.
 *
 **/

#include <config.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <libxml/parser.h>
#include <string.h>
#include <json-glib/json-glib.h>

#include "gdata-tasks-tasklist.h"
#include "gdata-private.h"
#include "gdata-service.h"
#include "gdata-parser.h"
#include "gdata-types.h"
#include "gdata-access-handler.h"

static GObject *gdata_tasks_tasklist_constructor (GType type, guint n_construct_params, GObjectConstructParam *construct_params);
static void gdata_tasks_tasklist_finalize (GObject *object);
static void get_json (GDataParsable *parsable, JsonBuilder *builder);
static gboolean parse_json (GDataParsable *parsable, JsonReader *node, gpointer user_data, GError **error);

G_DEFINE_TYPE (GDataTasksTasklist, gdata_tasks_tasklist, GDATA_TYPE_ENTRY)

static void
gdata_tasks_tasklist_class_init (GDataTasksTasklistClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GDataParsableClass *parsable_class = GDATA_PARSABLE_CLASS (klass);

	gobject_class->constructor = gdata_tasks_tasklist_constructor;
	gobject_class->finalize = gdata_tasks_tasklist_finalize;

	parsable_class->parse_json = parse_json;
	parsable_class->get_json = get_json;
}

static void
gdata_tasks_tasklist_init (GDataTasksTasklist *self)
{
	/* Empty */
}

static GObject *
gdata_tasks_tasklist_constructor (GType type, guint n_construct_params, GObjectConstructParam *construct_params)
{
	GObject *object;

	/* Chain up to the parent class */
	object = G_OBJECT_CLASS (gdata_tasks_tasklist_parent_class)->constructor (type, n_construct_params, construct_params);

	return object;
}

static void
gdata_tasks_tasklist_finalize (GObject *object)
{
	/* Chain up to the parent class */
	G_OBJECT_CLASS (gdata_tasks_tasklist_parent_class)->finalize (object);
}

static gboolean
parse_json (GDataParsable *parsable, JsonReader *reader, gpointer user_data, GError **error)
{
	/* well, there's nothing specific to parse from tasklist entries, all goes upstream */
	return GDATA_PARSABLE_CLASS (gdata_tasks_tasklist_parent_class)->parse_json (parsable, reader, user_data, error);
}

static void
get_json (GDataParsable *parsable, JsonBuilder *builder)
{
	/* Chain up to the parent class */
	GDATA_PARSABLE_CLASS (gdata_tasks_tasklist_parent_class)->get_json (parsable, builder);
}


/**
 * gdata_tasks_tasklist_new:
 * @id: (allow-none): the tasklist's ID, or %NULL
 *
 * Creates a new #GDataTasksTasklist with the given ID and default properties.
 *
 * Return value: a new #GDataTasksTasklist; unref with g_object_unref()
 **/
GDataTasksTasklist *
gdata_tasks_tasklist_new (const gchar *id)
{
	return GDATA_TASKS_TASKLIST (g_object_new (GDATA_TYPE_TASKS_TASKLIST, "id", id, NULL));
}
