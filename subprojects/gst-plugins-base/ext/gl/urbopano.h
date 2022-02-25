/*
 * GStreamer
 * Copyright (C) 2009 Julien Isorce <julien.isorce@gmail.com>
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

#ifndef _GST_GL_URBOPANO_H_
#define _GST_GL_URBOPANO_H_

#include "gstglmixer.h"

G_BEGIN_DECLS

#define GST_TYPE_GL_URBOPANO            (gst_gl_URBOPANO_get_type())
#define GST_GL_URBOPANO(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GL_URBOPANO,GstGLURBOPANO))
#define GST_IS_GL_URBOPANO(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GL_URBOPANO))
#define GST_GL_URBOPANO_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass) ,GST_TYPE_GL_URBOPANO,GstGLURBOPANOClass))
#define GST_IS_GL_URBOPANO_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass) ,GST_TYPE_GL_URBOPANO))
#define GST_GL_URBOPANO_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj) ,GST_TYPE_GL_URBOPANO,GstGLURBOPANOClass))

typedef struct _GstGLURBOPANO GstGLURBOPANO;
typedef struct _GstGLURBOPANOClass GstGLURBOPANOClass;

struct _GstGLURBOPANO
{
    GstGLMixer mixer;

    GstGLShader *shader;
    guint vao;
    guint vertex_buffer;
    guint vbo_indices;
    gint attr_position_loc;
    gint attr_texture_loc;
    GstGLMemory *out_tex;
    GLfloat xrot, yrot, zrot;
};

struct _GstGLURBOPANOClass
{
    GstGLMixerClass mixer_class;
};

GType gst_gl_URBOPANO_get_type (void);

G_END_DECLS

#endif /* _GST_GLFILTERCUBE_H_ */
