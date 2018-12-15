/*
 * gui/widgets/timeline_arranger.c - The timeline containing regions
 *
 * Copyright (C) 2018 Alexandros Theodotou
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

#include "zrythm_app.h"
#include "project.h"
#include "settings_manager.h"
#include "gui/widgets/region.h"
#include "audio/automation_track.h"
#include "audio/channel.h"
#include "audio/instrument_track.h"
#include "audio/midi_region.h"
#include "audio/mixer.h"
#include "audio/track.h"
#include "audio/tracklist.h"
#include "audio/transport.h"
#include "gui/widgets/arranger.h"
#include "gui/widgets/automation_curve.h"
#include "gui/widgets/automation_point.h"
#include "gui/widgets/automation_track.h"
#include "gui/widgets/bot_dock_edge.h"
#include "gui/widgets/center_dock.h"
#include "gui/widgets/color_area.h"
#include "gui/widgets/inspector.h"
#include "gui/widgets/main_window.h"
#include "gui/widgets/midi_arranger.h"
#include "gui/widgets/midi_arranger_bg.h"
#include "gui/widgets/midi_region.h"
#include "gui/widgets/piano_roll.h"
#include "gui/widgets/midi_note.h"
#include "gui/widgets/piano_roll_labels.h"
#include "gui/widgets/region.h"
#include "gui/widgets/ruler.h"
#include "gui/widgets/timeline_arranger.h"
#include "gui/widgets/timeline_bg.h"
#include "gui/widgets/timeline_ruler.h"
#include "gui/widgets/track.h"
#include "gui/widgets/tracklist.h"
#include "utils/arrays.h"
#include "utils/ui.h"

#include <gtk/gtk.h>

G_DEFINE_TYPE (TimelineArrangerWidget,
               timeline_arranger_widget,
               ARRANGER_WIDGET_TYPE)

/**
 * To be called from get_child_position in parent widget.
 *
 * Used to allocate the overlay children.
 */
void
timeline_arranger_widget_set_allocation (
  TimelineArrangerWidget * self,
  GtkWidget *          widget,
  GdkRectangle *       allocation)
{
  if (IS_REGION_WIDGET (widget))
    {
      RegionWidget * rw = REGION_WIDGET (widget);
      REGION_WIDGET_GET_PRIVATE (rw);

      gint wx, wy;
      gtk_widget_translate_coordinates(
                GTK_WIDGET (rw_prv->region->track->widget->track_box),
                GTK_WIDGET (self),
                0,
                0,
                &wx,
                &wy);

      allocation->x = arranger_widget_pos_to_px (
        ARRANGER_WIDGET (self),
        &rw_prv->region->start_pos);
      allocation->y = wy;
      allocation->width = arranger_widget_pos_to_px (
        ARRANGER_WIDGET (self),
        &rw_prv->region->end_pos) -
          allocation->x;
      allocation->height =
        gtk_widget_get_allocated_height (
          GTK_WIDGET (rw_prv->region->track->widget->track_box));
    }
  else if (IS_AUTOMATION_POINT_WIDGET (widget))
    {
      AutomationPointWidget * ap_widget = AUTOMATION_POINT_WIDGET (widget);
      AutomationPoint * ap = ap_widget->ap;
      /*Automatable * a = ap->at->automatable;*/

      gint wx, wy;
      gtk_widget_translate_coordinates(
                GTK_WIDGET (ap->at->widget->at_grid),
                GTK_WIDGET (self),
                0,
                0,
                &wx,
                &wy);

      allocation->x = arranger_widget_pos_to_px (
        ARRANGER_WIDGET (self),
        &ap->pos) -
          AP_WIDGET_POINT_SIZE / 2;
      allocation->y = (wy + automation_point_get_y_in_px (ap)) -
        AP_WIDGET_POINT_SIZE / 2;
      allocation->width = AP_WIDGET_POINT_SIZE;
      allocation->height = AP_WIDGET_POINT_SIZE;
    }
  else if (IS_AUTOMATION_CURVE_WIDGET (widget))
    {
      AutomationCurveWidget * acw = AUTOMATION_CURVE_WIDGET (widget);
      AutomationCurve * ac = acw->ac;
      /*Automatable * a = ap->at->automatable;*/

      gint wx, wy;
      gtk_widget_translate_coordinates(
                GTK_WIDGET (ac->at->widget->at_grid),
                GTK_WIDGET (self),
                0,
                0,
                &wx,
                &wy);
      AutomationPoint * prev_ap =
        automation_track_get_ap_before_curve (ac->at, ac);
      AutomationPoint * next_ap =
        automation_track_get_ap_after_curve (ac->at, ac);

      allocation->x = arranger_widget_pos_to_px (
        ARRANGER_WIDGET (self),
        &prev_ap->pos);
      int prev_y = automation_point_get_y_in_px (prev_ap);
      int next_y = automation_point_get_y_in_px (next_ap);
      allocation->y = wy + (prev_y > next_y ? next_y : prev_y);
      allocation->width =
        arranger_widget_pos_to_px (
          ARRANGER_WIDGET (self),
          &next_ap->pos) - allocation->x;
      allocation->height = prev_y > next_y ? prev_y - next_y : next_y - prev_y;
    }
}

Track *
timeline_arranger_widget_get_track_at_y (double y)
{
  for (int i = 0; i < MIXER->num_channels; i++)
    {
      Track * track = MIXER->channels[i]->track;

      GtkAllocation allocation;
      gtk_widget_get_allocation (GTK_WIDGET (track->widget->track_box),
                                 &allocation);

      gint wx, wy;
      gtk_widget_translate_coordinates(
                GTK_WIDGET (MW_TIMELINE),
                GTK_WIDGET (track->widget->track_box),
                0,
                0,
                &wx,
                &wy);

      if (y > -wy && y < ((-wy) + allocation.height))
        {
          return track;
        }
    }
  return NULL;
}

AutomationTrack *
timeline_arranger_widget_get_automation_track_at_y (double y)
{
  for (int i = 0; i < MIXER->num_channels; i++)
    {
      Track * track = MIXER->channels[i]->track;
      InstrumentTrack * it;
      int num_automation_tracks;
      switch (track->type)
        {
        case TRACK_TYPE_INSTRUMENT:
          it = (InstrumentTrack *) track;
          num_automation_tracks = it->num_automation_tracks;
        case TRACK_TYPE_MASTER:
        case TRACK_TYPE_AUDIO:
        case TRACK_TYPE_CHORD:
        case TRACK_TYPE_BUS:
          break;
        }

      for (int j = 0; j < num_automation_tracks; j++)
        {
          /*g_message ("at %d of %d", j, i);*/
          AutomationTrack * at = NULL;
          if (it)
            at = it->automation_tracks[j];
          /* TODO check the rest */
          if (at->widget)
            {
              GtkAllocation allocation;
              gtk_widget_get_allocation (GTK_WIDGET (at->widget->at_grid),
                                         &allocation);

              gint wx, wy;
              gtk_widget_translate_coordinates(
                        GTK_WIDGET (MW_TIMELINE),
                        GTK_WIDGET (at->widget->at_grid),
                        0,
                        y,
                        &wx,
                        &wy);

              if (wy >= 0 && wy <= allocation.height)
                {
                  return at;
                }
            }
        }
    }

  return NULL;
}

RegionWidget *
timeline_arranger_widget_get_hit_region (TimelineArrangerWidget *  self,
                                  double            x,
                                  double            y)
{
  GtkWidget * widget =
    arranger_widget_get_hit_widget (
      ARRANGER_WIDGET (self),
      ARRANGER_CHILD_TYPE_REGION,
      x,
      y);
  if (widget)
    {
      return REGION_WIDGET (widget);
    }
  return NULL;
}

AutomationPointWidget *
timeline_arranger_widget_get_hit_ap (
  TimelineArrangerWidget *  self,
  double            x,
  double            y)
{
  GtkWidget * widget = arranger_widget_get_hit_widget (
    ARRANGER_WIDGET (self),
    ARRANGER_CHILD_TYPE_AP,
    x,
    y);
  if (widget)
    {
      return AUTOMATION_POINT_WIDGET (widget);
    }
  return NULL;
}

AutomationCurveWidget *
timeline_arranger_widget_get_hit_curve (
  TimelineArrangerWidget * self,
  double x,
  double y)
{
  GtkWidget * widget = arranger_widget_get_hit_widget (
    ARRANGER_WIDGET (self),
    ARRANGER_CHILD_TYPE_AC,
    x,
    y);
  if (widget)
    {
      return AUTOMATION_CURVE_WIDGET (widget);
    }
  return NULL;
}

void
timeline_arranger_widget_update_inspector (
  TimelineArrangerWidget *self)
{
  inspector_widget_show_selections (
    INSPECTOR_CHILD_REGION,
    (void **) self->regions,
    self->num_regions);
  inspector_widget_show_selections (
    INSPECTOR_CHILD_AP,
    (void **) self->automation_points,
    self->num_automation_points);
}

void
timeline_arranger_widget_select_all (
  TimelineArrangerWidget *  self,
  int                       select)
{
  self->num_regions = 0;
  self->num_automation_points = 0;

  for (int i = 0; i < MIXER->num_channels; i++)
    {
      Channel * chan = MIXER->channels[i];

      if (chan->visible)
        {
          int num_regions;
          InstrumentTrack * it;
          int num_automation_tracks;
          AutomationTrack ** automation_tracks;
          switch (chan->track->type)
            {
            case TRACK_TYPE_INSTRUMENT:
              it = (InstrumentTrack *) chan->track;
              num_regions = it->num_regions;
              num_automation_tracks = it->num_automation_tracks;
              automation_tracks = it->automation_tracks;
            case TRACK_TYPE_MASTER:
            case TRACK_TYPE_AUDIO:
            case TRACK_TYPE_CHORD:
            case TRACK_TYPE_BUS:
              break;
            }
          for (int j = 0; j < num_regions; j++)
            {
              Region * r = (Region *) it->regions[j];

              region_widget_select (r->widget, select);

              if (select)
                {
                  /* select  */
                  array_append ((void **)self->regions,
                                &self->num_regions,
                                r);
                }
            }
          if (chan->track->bot_paned_visible)
            {
              for (int j = 0; j < num_automation_tracks; j++)
                {
                  AutomationTrack * at = automation_tracks[j];
                  if (at->visible)
                    {
                      for (int k = 0; k < at->num_automation_points; k++)
                        {
                          /*AutomationPoint * ap = at->automation_points[k];*/

                          /*ap_widget_select (ap->widget, select);*/

                          /*if (select)*/
                            /*{*/
                              /*[> select  <]*/
                              /*array_append ((void **)self->tl_automation_points,*/
                                            /*&self->num_tl_automation_points,*/
                                            /*ap);*/
                            /*}*/
                        }
                    }
                }
            }
        }
    }
}

/**
 * Selects the region.
 */
void
timeline_arranger_widget_toggle_select_region (
  TimelineArrangerWidget * self,
  Region *                 region,
  int                      append)
{
  arranger_widget_toggle_select (
    ARRANGER_WIDGET (self),
    ARRANGER_CHILD_TYPE_REGION,
    (void *) region,
    append);
}

void
timeline_arranger_widget_toggle_select_automation_point (
  TimelineArrangerWidget *  self,
  AutomationPoint * ap,
  int               append)
{
  arranger_widget_toggle_select (
    ARRANGER_WIDGET (self),
    ARRANGER_CHILD_TYPE_AP,
    (void *) ap,
    append);
}

/**
 * Shows context menu.
 *
 * To be called from parent on right click.
 */
void
timeline_arranger_widget_show_context_menu (
  TimelineArrangerWidget * self)
{
  GtkWidget *menu, *menuitem;

  menu = gtk_menu_new();

  menuitem = gtk_menu_item_new_with_label("Do something");

  /*g_signal_connect(menuitem, "activate",*/
                   /*(GCallback) view_popup_menu_onDoSomething, treeview);*/

  gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

  gtk_widget_show_all(menu);

  gtk_menu_popup_at_pointer (GTK_MENU(menu), NULL);
}

void
timeline_arranger_widget_on_drag_begin_region_hit (
  TimelineArrangerWidget * self,
  GdkModifierType          state_mask,
  double                   start_x,
  RegionWidget *           rw)
{
  ARRANGER_WIDGET_GET_PRIVATE (self);
  REGION_WIDGET_GET_PRIVATE (rw);

  /* open MIDI editor */
  if (prv->n_press > 0)
    {
      Track * track = rw_prv->region->track;
      Channel * chan;
      switch (track->type)
        {
        case TRACK_TYPE_INSTRUMENT:
          chan = ((InstrumentTrack *) track)->channel;
        case TRACK_TYPE_MASTER:
        case TRACK_TYPE_AUDIO:
        case TRACK_TYPE_CHORD:
        case TRACK_TYPE_BUS:
          break;
        }
      midi_arranger_widget_set_channel(
        MIDI_ARRANGER,
        chan);
    }

  Region * region = rw_prv->region;
  self->start_region = rw_prv->region;

  /* update arranger action */
  if (!rw_prv->hover)
    g_warning ("hitting region but region hover state is none, should be fixed");
  else if (region->type == REGION_TYPE_MIDI &&
           MIDI_REGION_WIDGET (rw)->cursor_state ==
             MIDI_REGION_CURSOR_RESIZE_L)
    prv->action = ARRANGER_ACTION_RESIZING_L;
  else if (region->type == REGION_TYPE_MIDI &&
           MIDI_REGION_WIDGET (rw)->cursor_state ==
             MIDI_REGION_CURSOR_RESIZE_R)
    prv->action = ARRANGER_ACTION_RESIZING_R;
  else if (rw_prv->hover)
    {
      prv->action = ARRANGER_ACTION_STARTING_MOVING;
      ui_set_cursor (GTK_WIDGET (rw), "grabbing");
    }

  /* select/ deselect regions */
  if (state_mask & GDK_SHIFT_MASK ||
      state_mask & GDK_CONTROL_MASK)
    {
      /* if ctrl pressed toggle on/off */
      timeline_arranger_widget_toggle_select_region (
        self, region, 1);
    }
  else if (!array_contains ((void **)self->regions,
                      self->num_regions,
                      region))
    {
      /* else if not already selected select only it */
      timeline_arranger_widget_select_all (self, 0);
      timeline_arranger_widget_toggle_select_region (self, region, 0);
    }

  /* find highest and lowest selected regions */
  self->top_region = self->regions[0];
  self->bot_region = self->regions[0];
  for (int i = 0; i < self->num_regions; i++)
    {
      Region * region = self->regions[i];
      if (tracklist_get_track_pos (PROJECT->tracklist,
                                   region->track) <
          tracklist_get_track_pos (PROJECT->tracklist,
                                   self->top_region->track))
        {
          self->top_region = region;
        }
      if (tracklist_get_track_pos (PROJECT->tracklist,
                                   region->track) >
          tracklist_get_track_pos (PROJECT->tracklist,
                                   self->bot_region->track))
        {
          self->bot_region = region;
        }
    }
}

void
timeline_arranger_widget_on_drag_begin_ap_hit (
  TimelineArrangerWidget * self,
  GdkModifierType          state_mask,
  double                   start_x,
  AutomationPointWidget *  ap_widget)
{
  ARRANGER_WIDGET_GET_PRIVATE (self);

  AutomationPoint * ap = ap_widget->ap;
  prv->start_pos_px = start_x;
  self->start_ap = ap;
  if (!array_contains ((void **) self->automation_points,
                       self->num_automation_points,
                       (void *) ap))
    {
      self->automation_points[0] = ap;
      self->num_automation_points = 1;
    }
  switch (ap_widget->state)
    {
    case APW_STATE_NONE:
      g_warning ("hitting AP but AP hover state is none, should be fixed");
      break;
    case APW_STATE_HOVER:
    case APW_STATE_SELECTED:
      prv->action = ARRANGER_ACTION_STARTING_MOVING;
      ui_set_cursor (GTK_WIDGET (ap_widget), "grabbing");
      break;
    }

  /* update selection */
  if (state_mask & GDK_SHIFT_MASK ||
      state_mask & GDK_CONTROL_MASK)
    {
      timeline_arranger_widget_toggle_select_automation_point (
                                                                    self, ap, 1);
    }
  else
    {
      timeline_arranger_widget_select_all (self, 0);
      timeline_arranger_widget_toggle_select_automation_point (self, ap, 0);
    }
}

void
timeline_arranger_widget_find_start_poses (
  TimelineArrangerWidget * self)
{
  ARRANGER_WIDGET_GET_PRIVATE (self);

  for (int i = 0; i < self->num_regions; i++)
    {
      Region * r = self->regions[i];
      if (position_compare (&r->start_pos,
                            &prv->start_pos) <= 0)
        {
          position_set_to_pos (&prv->start_pos,
                               &r->start_pos);
        }

      /* set start poses fo regions */
      position_set_to_pos (&self->region_start_poses[i],
                           &r->start_pos);
    }
  for (int i = 0; i < self->num_automation_points; i++)
    {
      AutomationPoint * ap = self->automation_points[i];
      if (position_compare (&ap->pos,
                            &prv->start_pos) <= 0)
        {
          position_set_to_pos (&prv->start_pos,
                               &ap->pos);
        }

      /* set start poses for APs */
      position_set_to_pos (&self->ap_poses[i],
                           &ap->pos);
    }
}

void
timeline_arranger_widget_create_ap (
  TimelineArrangerWidget * self,
  AutomationTrack *        at,
  Track *                  track,
  Position *               pos,
  double                   start_y)
{
  ARRANGER_WIDGET_GET_PRIVATE (self);

  position_snap (NULL,
                 pos,
                 track,
                 NULL,
                 prv->snap_grid);

  /* if the automatable is float in this automation track */
  if (automatable_is_float (at->automatable))
    {
      /* add automation point to automation track */
      float value = automation_track_widget_get_fvalue_at_y (
                                      at->widget,
                                      start_y);

      AutomationPoint * ap = automation_point_new_float (at,
                                             value,
                                             pos);
      automation_track_add_automation_point (at,
                                             ap,
                                             GENERATE_CURVE_POINTS);
      gtk_overlay_add_overlay (GTK_OVERLAY (self),
                               GTK_WIDGET (ap->widget));
      gtk_widget_show (GTK_WIDGET (ap->widget));
      self->automation_points[0] = ap;
      self->num_automation_points = 1;
    }
}

void
timeline_arranger_widget_create_region (
  TimelineArrangerWidget * self,
  Track *                  track,
  Position *               pos)
{
  ARRANGER_WIDGET_GET_PRIVATE (self);

  position_snap (NULL,
                 pos,
                 track,
                 NULL,
                 prv->snap_grid);
  Region * region;
  if (track->type == TRACK_TYPE_INSTRUMENT)
    {
      region = (Region *) midi_region_new (track,
                               pos,
                               pos);
    }
  else if (track->type == TRACK_TYPE_AUDIO)
    {
      /* TODO */
    }
  position_set_min_size (&region->start_pos,
                         &region->end_pos,
                         prv->snap_grid);
  if (track->type == TRACK_TYPE_INSTRUMENT)
    {
      instrument_track_add_region ((InstrumentTrack *)track,
                        (MidiRegion *) region);
    }
  else if (track->type == TRACK_TYPE_AUDIO)
    {
      /* TODO */
    }
  gtk_overlay_add_overlay (GTK_OVERLAY (self),
                           GTK_WIDGET (region->widget));
  gtk_widget_show (GTK_WIDGET (region->widget));
  prv->action = ARRANGER_ACTION_RESIZING_R;
  self->regions[0] = region;
  self->num_regions = 1;
}

void
timeline_arranger_widget_find_and_select_items (
  TimelineArrangerWidget * self,
  double                   offset_x,
  double                   offset_y)
{
  ARRANGER_WIDGET_GET_PRIVATE (self);

  /* find enclosed regions */
  GtkWidget *    region_widgets[800];
  int            num_region_widgets = 0;
  arranger_widget_get_hit_widgets_in_range (
    ARRANGER_WIDGET (self),
    ARRANGER_CHILD_TYPE_REGION,
    prv->start_x,
    prv->start_y,
    offset_x,
    offset_y,
    region_widgets,
    &num_region_widgets);


  /* select the enclosed regions */
  for (int i = 0; i < num_region_widgets; i++)
    {
      RegionWidget * rw = REGION_WIDGET (region_widgets[i]);
      REGION_WIDGET_GET_PRIVATE (rw);
      Region * region = rw_prv->region;
      timeline_arranger_widget_toggle_select_region (self,
                                            region,
                                            1);
    }

  /* find enclosed automation_points */
  GtkWidget *    ap_widgets[800];
  int            num_ap_widgets = 0;
  arranger_widget_get_hit_widgets_in_range (
    ARRANGER_WIDGET (self),
    ARRANGER_CHILD_TYPE_AP,
    prv->start_x,
    prv->start_y,
    offset_x,
    offset_y,
    ap_widgets,
    &num_ap_widgets);

  /* select the enclosed automation_points */
  for (int i = 0; i < num_ap_widgets; i++)
    {
      AutomationPointWidget * ap_widget =
        AUTOMATION_POINT_WIDGET (ap_widgets[i]);
      AutomationPoint * ap = ap_widget->ap;
      timeline_arranger_widget_toggle_select_automation_point (self,
                                                      ap,
                                                      1);
    }
}

void
timeline_arranger_widget_snap_regions_l (
  TimelineArrangerWidget * self,
  Position *               pos)
{
  ARRANGER_WIDGET_GET_PRIVATE (self);

  for (int i = 0; i < self->num_regions; i++)
    {
      Region * region = self->regions[i];
      position_snap (NULL,
                     pos,
                     region->track,
                     NULL,
                     prv->snap_grid);
      region_set_start_pos (region,
                            pos,
                            0);
    }
}

void
timeline_arranger_widget_snap_regions_r (
  TimelineArrangerWidget * self,
  Position *               pos)
{
  ARRANGER_WIDGET_GET_PRIVATE (self);
  for (int i = 0; i < self->num_regions; i++)
    {
      Region * region = self->regions[i];
      position_snap (NULL,
                     pos,
                     region->track,
                     NULL,
                     prv->snap_grid);
      if (position_compare (pos, &region->start_pos) > 0)
        {
          region_set_end_pos (region,
                              pos);
        }
    }
}

void
timeline_arranger_widget_move_items_x (
  TimelineArrangerWidget * self,
  int                      frames_diff)
{
  /* update region positions */
  for (int i = 0; i < self->num_regions; i++)
    {
      Region * r = self->regions[i];
      Position * prev_start_pos = &self->region_start_poses[i];
      int length_frames = position_to_frames (&r->end_pos) -
        position_to_frames (&r->start_pos);
      Position tmp;
      position_set_to_pos (&tmp, prev_start_pos);
      position_add_frames (&tmp, frames_diff + length_frames);
      region_set_end_pos (r, &tmp);
      position_set_to_pos (&tmp, prev_start_pos);
      position_add_frames (&tmp, frames_diff);
      region_set_start_pos (r, &tmp, 1);
    }

  /* update ap positions */
  for (int i = 0; i < self->num_automation_points; i++)
    {
      AutomationPoint * ap = self->automation_points[i];

      /* get prev and next value APs */
      AutomationPoint * prev_ap = automation_track_get_prev_ap (ap->at,
                                                                ap);
      AutomationPoint * next_ap = automation_track_get_next_ap (ap->at,
                                                                ap);
      /* get adjusted pos for this automation point */
      Position ap_pos;
      Position * prev_pos = &self->ap_poses[i];
      position_set_to_pos (&ap_pos,
                           prev_pos);
      position_add_frames (&ap_pos, frames_diff);

      Position mid_pos;
      AutomationCurve * ac;

      /* update midway points */
      if (prev_ap && position_compare (&ap_pos, &prev_ap->pos) >= 0)
        {
          /* set prev curve point to new midway pos */
          position_get_midway_pos (&prev_ap->pos,
                                   &ap_pos,
                                   &mid_pos);
          ac = automation_track_get_next_curve_ac (ap->at,
                                                   prev_ap);
          position_set_to_pos (&ac->pos, &mid_pos);

          /* set pos for ap */
          if (!next_ap)
            {
              position_set_to_pos (&ap->pos, &ap_pos);
            }
        }
      if (next_ap && position_compare (&ap_pos, &next_ap->pos) <= 0)
        {
          /* set next curve point to new midway pos */
          position_get_midway_pos (&ap_pos,
                                   &next_ap->pos,
                                   &mid_pos);
          ac = automation_track_get_next_curve_ac (ap->at,
                                                   ap);
          position_set_to_pos (&ac->pos, &mid_pos);

          /* set pos for ap - if no prev ap exists or if the position
           * is also after the prev ap */
          if ((prev_ap &&
                position_compare (&ap_pos, &prev_ap->pos) >= 0) ||
              (!prev_ap))
            {
              position_set_to_pos (&ap->pos, &ap_pos);
            }
        }
      else if (!prev_ap && !next_ap)
        {
          /* set pos for ap */
          position_set_to_pos (&ap->pos, &ap_pos);
        }
    }
}

void
timeline_arranger_widget_setup (
  TimelineArrangerWidget * self)
{
  // set the size
  int ww, hh;
  TracklistWidget * tracklist = MW_TRACKLIST;
  gtk_widget_get_size_request (
    GTK_WIDGET (tracklist),
    &ww,
    &hh);
  RULER_WIDGET_GET_PRIVATE (MW_RULER);
  gtk_widget_set_size_request (
    GTK_WIDGET (self),
    prv->total_px,
    hh);
}

void
timeline_arranger_widget_move_items_y (
  TimelineArrangerWidget * self,
  double                   offset_y)
{
  ARRANGER_WIDGET_GET_PRIVATE (self);

  if (self->start_region)
    {
      /* check if should be moved to new track */
      Track * track = timeline_arranger_widget_get_track_at_y (prv->start_y + offset_y);
      Track * old_track = self->start_region->track;
      if (track)
        {
          Track * pt =
            tracklist_get_prev_visible_track (
              PROJECT->tracklist,
              old_track);
          Track * nt =
            tracklist_get_next_visible_track (
              PROJECT->tracklist,
              old_track);
          Track * tt =
            tracklist_get_first_visible_track (
              PROJECT->tracklist);
          Track * bt =
            tracklist_get_last_visible_track (
              PROJECT->tracklist);
          if (self->start_region->track != track)
            {
              /* if new track is lower and bot region is not at the lowest track */
              if (track == nt &&
                  self->bot_region->track != bt)
                {
                  /* shift all selected regions to their next track */
                  for (int i = 0; i < self->num_regions; i++)
                    {
                      Region * region = self->regions[i];
                      nt =
                        tracklist_get_next_visible_track (
                          PROJECT->tracklist,
                          region->track);
                      old_track = region->track;
                      if (old_track->type == nt->type)
                        {
                          if (nt->type ==
                                TRACK_TYPE_INSTRUMENT)
                            {
                              instrument_track_remove_region ((InstrumentTrack *) old_track, (MidiRegion *) region);
                              instrument_track_add_region ((InstrumentTrack *) nt, (MidiRegion *) region);
                            }
                          else if (nt->type ==
                                     TRACK_TYPE_AUDIO)
                            {
                              /* TODO */

                            }
                        }
                    }
                }
              else if (track == pt &&
                       self->top_region->track != tt)
                {
                  /*g_message ("track %s top region track %s tt %s",*/
                             /*track->channel->name,*/
                             /*self->top_region->track->channel->name,*/
                             /*tt->channel->name);*/
                  /* shift all selected regions to their prev track */
                  for (int i = 0; i < self->num_regions; i++)
                    {
                      Region * region = self->regions[i];
                      pt =
                        tracklist_get_prev_visible_track (
                          PROJECT->tracklist,
                          region->track);
                      old_track = region->track;
                      if (old_track->type == pt->type)
                        {
                          if (pt->type ==
                                TRACK_TYPE_INSTRUMENT)
                            {
                              instrument_track_remove_region ((InstrumentTrack *) old_track, (MidiRegion *) region);
                              instrument_track_add_region ((InstrumentTrack *) pt, (MidiRegion *) region);
                            }
                          else if (pt->type ==
                                     TRACK_TYPE_AUDIO)
                            {
                              /* TODO */

                            }
                        }
                    }
                }
            }
        }
    }
  else if (self->start_ap)
    {
      for (int i = 0; i < self->num_automation_points; i++)
        {
          AutomationPoint * ap = self->automation_points[i];

          /* get adjusted y for this ap */
          /*Position region_pos;*/
          /*position_set_to_pos (&region_pos,*/
                               /*&pos);*/
          /*int diff = position_to_frames (&region->start_pos) -*/
            /*position_to_frames (&self->tl_start_region->start_pos);*/
          /*position_add_frames (&region_pos, diff);*/
          int this_y =
            automation_track_widget_get_y (ap->at->widget,
                                           ap->widget);
          int start_ap_y =
            automation_track_widget_get_y (self->start_ap->at->widget,
                                           self->start_ap->widget);
          int diff = this_y - start_ap_y;

          float fval =
            automation_track_widget_get_fvalue_at_y (ap->at->widget,
                                                     prv->start_y + offset_y + diff);
          automation_point_update_fvalue (ap, fval);
        }
    }
}

void
timeline_arranger_widget_on_drag_end (
  TimelineArrangerWidget * self)
{
  ARRANGER_WIDGET_GET_PRIVATE (self);

  for (int i = 0; i < self->num_regions; i++)
    {
      Region * region = self->regions[i];
      ui_set_cursor (GTK_WIDGET (region->widget), "default");
    }

  /* if didn't click on something */
  if (prv->action != ARRANGER_ACTION_STARTING_MOVING)
    {
      self->start_region = NULL;
      self->start_ap = NULL;
    }
}

static void
timeline_arranger_widget_class_init (TimelineArrangerWidgetClass * klass)
{
}

static void
timeline_arranger_widget_init (TimelineArrangerWidget *self )
{
}
