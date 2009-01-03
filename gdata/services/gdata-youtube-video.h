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

#ifndef GDATA_YOUTUBE_VIDEO_H
#define GDATA_YOUTUBE_VIDEO_H

#include <glib.h>
#include <glib-object.h>

#include "gdata-entry.h"

G_BEGIN_DECLS

#define GDATA_TYPE_YOUTUBE_VIDEO		(gdata_youtube_video_get_type ())
#define GDATA_YOUTUBE_VIDEO(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), GDATA_TYPE_YOUTUBE_VIDEO, GDataYouTubeVideo))
#define GDATA_YOUTUBE_VIDEO_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), GDATA_TYPE_YOUTUBE_VIDEO, GDataYouTubeVideoClass))
#define GDATA_IS_YOUTUBE_VIDEO(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GDATA_TYPE_YOUTUBE_VIDEO))
#define GDATA_IS_YOUTUBE_VIDEO_CLASS(k)		(G_TYPE_CHECK_CLASS_TYPE ((k), GDATA_TYPE_YOUTUBE_VIDEO))
#define GDATA_YOUTUBE_VIDEO_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GDATA_TYPE_YOUTUBE_VIDEO, GDataYouTubeVideoClass))

typedef struct _GDataYouTubeVideoPrivate	GDataYouTubeVideoPrivate;

typedef struct {
	GDataEntry parent;
	GDataYouTubeVideoPrivate *priv;
} GDataYouTubeVideo;

typedef struct {
	GDataEntryClass parent;
} GDataYouTubeVideoClass;

GType gdata_youtube_video_get_type (void);

GDataYouTubeVideo *gdata_youtube_video_new (void);

G_END_DECLS

#endif /* !GDATA_YOUTUBE_VIDEO_H */