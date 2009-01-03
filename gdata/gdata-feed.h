/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * GData Client
 * Copyright (C) Philip Withnall 2008 <philip@tecnocode.co.uk>
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

#ifndef GDATA_FEED_H
#define GDATA_FEED_H

#include <glib.h>
#include <glib-object.h>

#include "gdata-entry.h"

G_BEGIN_DECLS

#define GDATA_TYPE_FEED			(gdata_feed_get_type ())
#define GDATA_FEED(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), GDATA_TYPE_FEED, GDataFeed))
#define GDATA_FEED_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), GDATA_TYPE_FEED, GDataFeedClass))
#define GDATA_IS_FEED(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GDATA_TYPE_FEED))
#define GDATA_IS_FEED_CLASS(k)		(G_TYPE_CHECK_CLASS_TYPE ((k), GDATA_TYPE_FEED))
#define GDATA_FEED_GET_CLASS(o)		(G_TYPE_INSTANCE_GET_CLASS ((o), GDATA_TYPE_FEED, GDataFeedClass))

typedef struct _GDataFeedPrivate	GDataFeedPrivate;

typedef struct {
	GObject parent;
	GDataFeedPrivate *priv;
} GDataFeed;

typedef struct {
	GObjectClass parent;
} GDataFeedClass;

GType gdata_feed_get_type (void);

const GList *gdata_feed_get_entries (GDataFeed *self);
void gdata_feed_append_entry (GDataFeed *self, GDataEntry *entry);

const gchar *gdata_feed_get_title (GDataFeed *self);
void gdata_feed_set_title (GDataFeed *self, const gchar *title);
const gchar *gdata_feed_get_id (GDataFeed *self);
void gdata_feed_set_id (GDataFeed *self, const gchar *id);
void gdata_feed_get_updated (GDataFeed *self, GTimeVal *updated);
void gdata_feed_set_updated (GDataFeed *self, GTimeVal *updated);

G_END_DECLS

#endif /* !GDATA_FEED_H */