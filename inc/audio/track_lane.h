/*
 * Copyright (C) 2019 Alexandros Theodotou <alex at zrythm dot org>
 *
 * This file is part of Zrythm
 *
 * Zrythm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Zrythm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Zrythm.  If not, see <https://www.gnu.org/licenses/>.
 */

/**
 * \file
 *
 * Track lanes for each track.
 */

#ifndef __AUDIO_TRACK_LANE_H__
#define __AUDIO_TRACK_LANE_H__

#include "audio/region.h"
#include "utils/yaml.h"

/**
 * @addtogroup audio
 *
 * @{
 */
#define MAX_REGIONS 300

typedef struct _TrackLaneWidget TrackLaneWidget;

/**
 * A TrackLane belongs to a Track (can have many
 * TrackLanes in a Track) and contains Regions.
 *
 * Only Tracks that have Regions can have TrackLanes,
 * such as InstrumentTrack and AudioTrack.
 */
typedef struct TrackLane
{
  /** Position in the Track. */
  int                 pos;

  /** Name of lane, e.g. "Lane 1". */
  char *              name;

  /** TrackLaneWidget for this lane. */
  TrackLaneWidget *   widget;

  /** Position of multipane handle. */
  int                 handle_pos;

  /** Muted or not. */
  int                 mute;

  /** Soloed or not. */
  int                 solo;

  /** Regions in this track. */
  Region *            regions[MAX_REGIONS];
  int                 num_regions;

  /** Pointer back to the owner Track. */
  Track *             track;

  /** Position of owner Track in the Tracklist. */
  int                 track_pos;

} TrackLane;

static const cyaml_schema_field_t
track_lane_fields_schema[] =
{
	CYAML_FIELD_INT (
    "pos", CYAML_FLAG_DEFAULT,
    TrackLane, pos),
  CYAML_FIELD_STRING_PTR (
    "name", CYAML_FLAG_POINTER,
    TrackLane, name,
   	0, CYAML_UNLIMITED),
	CYAML_FIELD_INT (
    "handle_pos", CYAML_FLAG_DEFAULT,
    TrackLane, handle_pos),
	CYAML_FIELD_INT (
    "mute", CYAML_FLAG_DEFAULT,
    TrackLane, mute),
	CYAML_FIELD_INT (
    "solo", CYAML_FLAG_DEFAULT,
    TrackLane, solo),
  CYAML_FIELD_SEQUENCE_COUNT (
    "regions", CYAML_FLAG_DEFAULT,
    TrackLane, regions, num_regions,
    &region_schema, 0, CYAML_UNLIMITED),
	CYAML_FIELD_INT (
    "track_pos", CYAML_FLAG_DEFAULT,
    TrackLane, track_pos),

	CYAML_FIELD_END
};

static const cyaml_schema_value_t
track_lane_schema = {
	CYAML_VALUE_MAPPING (
    CYAML_FLAG_POINTER,
    TrackLane, track_lane_fields_schema),
};

/**
 * Creates a new TrackLane at the given pos in the
 * given Track.
 *
 * @param track The Track to create the TrackLane for.
 * @param pos The position (index) in the Track that
 *   this lane will be placed in.
 */
TrackLane *
track_lane_new (
  Track * track,
  int     pos);

/**
 * Adds a Region to the given TrackLane.
 */
void
track_lane_add_region (
  TrackLane * self,
  Region *    region);

/**
 * Clones the TrackLane.
 *
 * Mainly used when cloning Track's.
 */
TrackLane *
track_lane_clone (
  TrackLane * lane);

/**
 * Frees the TrackLane.
 */
void
track_lane_free (
  TrackLane * lane);

/**
 * @}
 */

#endif // __AUDIO_TRACK_LANE_H__