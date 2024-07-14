/* GStreamer NVENC plugin
 * Copyright (C) 2015 Centricular Ltd
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __GST_NV_H264_ENC_H_INCLUDED__
#define __GST_NV_H264_ENC_H_INCLUDED__

#include "gstnvbaseenc.h"

#define MAX_NUM_ROIS 32

typedef struct {
  GstNvBaseEnc base_nvenc;
  /* properties */
  gboolean aud;
  gint roi_start_x[MAX_NUM_ROIS];
  gint roi_start_y[MAX_NUM_ROIS];
  gint roi_width[MAX_NUM_ROIS];
  gint roi_height[MAX_NUM_ROIS];
  gint roi_inner_quality[MAX_NUM_ROIS];
  gint roi_outer_quality;
  guint qp_map_size;
  int8_t* qp_map;
  gboolean qp_map_changed;
  gboolean enable_roi;
  guint num_rois;
} GstNvH264Enc;

typedef struct {
  GstNvBaseEncClass video_encoder_class;
} GstNvH264EncClass;

void gst_nv_h264_enc_register (GstPlugin * plugin,
                               guint device_id,
                               guint rank,
                               GstCaps * sink_caps,
                               GstCaps * src_caps,
                               GstNvEncDeviceCaps * device_caps);


#endif /* __GST_NV_H264_ENC_H_INCLUDED__ */
