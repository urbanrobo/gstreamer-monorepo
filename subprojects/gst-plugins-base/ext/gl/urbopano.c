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

/**
 * SECTION:element-urbopano
 * @title: urbopano
 *
 * glmixer sub element. N gl sink pads to 1 source pad.
 * N + 1 OpenGL contexts shared together.
 * N <= 6 because the rendering is more a like a cube than a URBOPANO
 * Each opengl input stream is rendered on a cube face
 *
 * ## Examples
 * |[
 gst-launch-1.0 videotestsrc ! video/x-raw, format=YUY2 ! glupload ! glcolorconvert ! queue ! urbopano name=m ! glimagesink \
 videotestsrc pattern=12 ! video/x-raw, format=I420, framerate=5/1, width=100, height=200 ! glupload ! glcolorconvert ! queue ! m. \
 videotestsrc ! video/x-raw, framerate=15/1, width=1500, height=1500 ! glupload ! gleffects effect=3 ! queue ! m. \
 videotestsrc ! glupload ! gleffects effect=2 ! queue ! m.  \
 videotestsrc ! glupload ! glfiltercube ! queue ! m. \
 videotestsrc ! glupload ! gleffects effect=6 ! queue ! m.
 * ]|
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstglelements.h"
#include "urbopano.h"
#include "gstglutils.h"

#define GST_CAT_DEFAULT gst_gl_URBOPANO_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

enum
{
  PROP_0,
};

static void gst_gl_URBOPANO_child_proxy_init (gpointer g_iface,
    gpointer iface_data);

#define DEBUG_INIT \
    GST_DEBUG_CATEGORY_INIT (gst_gl_URBOPANO_debug, "urbopano", 0, "urbopano element");

G_DEFINE_TYPE_WITH_CODE (GstGLURBOPANO, gst_gl_URBOPANO, GST_TYPE_GL_MIXER,
    G_IMPLEMENT_INTERFACE (GST_TYPE_CHILD_PROXY,
        gst_gl_URBOPANO_child_proxy_init);
    DEBUG_INIT);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (urbopano, "urbopano",
    GST_RANK_NONE, GST_TYPE_GL_URBOPANO, gl_element_init (plugin));

static GstPad *gst_gl_URBOPANO_request_new_pad (GstElement * element,
    GstPadTemplate * temp, const gchar * req_name, const GstCaps * caps);
static void gst_gl_URBOPANO_release_pad (GstElement * element, GstPad * pad);

static void gst_gl_URBOPANO_reset (GstGLMixer * mixer);
static gboolean gst_gl_URBOPANO_set_caps (GstGLMixer * mixer, GstCaps * outcaps);

static gboolean gst_gl_URBOPANO_process_textures (GstGLMixer * mixer,
    GstGLMemory * out_tex);
static gboolean gst_gl_URBOPANO_callback (gpointer stuff);

/* vertex source */
static const gchar *URBOPANO_v_src =
    "uniform mat4 u_matrix;                                       \n"
    "uniform float xrot_degree, yrot_degree, zrot_degree;         \n"
    "attribute vec4 a_position;                                   \n"
    "attribute vec2 a_texCoord;                                   \n"
    "varying vec2 v_texCoord;                                     \n"
    "void main()                                                  \n"
    "{                                                            \n"
    "   float PI = 3.14159265;                                    \n"
    "   float xrot = xrot_degree*2.0*PI/360.0;                    \n"
    "   float yrot = yrot_degree*2.0*PI/360.0;                    \n"
    "   float zrot = zrot_degree*2.0*PI/360.0;                    \n"
    "   mat4 matX = mat4 (                                        \n"
    "            3.0,        0.0,        0.0, 0.0,                \n"
    "            0.0,  cos(xrot),  sin(xrot), 0.0,                \n"
    "            0.0, -sin(xrot),  cos(xrot), 0.0,                \n"
    "            0.0,        0.0,        0.0, 3.0 );              \n"
    "   mat4 matY = mat4 (                                        \n"
    "      cos(yrot),        0.0, -sin(yrot), 0.0,                \n"
    "            0.0,        1.0,        0.0, 0.0,                \n"
    "      sin(yrot),        0.0,  cos(yrot), 0.0,                \n"
    "            0.0,        0.0,       0.0,  1.0 );              \n"
    "   mat4 matZ = mat4 (                                        \n"
    "      cos(zrot),  sin(zrot),        0.0, 0.0,                \n"
    "     -sin(zrot),  cos(zrot),        0.0, 0.0,                \n"
    "            0.0,        0.0,        1.0, 0.0,                \n"
    "            0.0,        0.0,        0.0, 1.0 );              \n"
    "   gl_Position = u_matrix * matZ * matY * matX * a_position; \n"
    "   v_texCoord = a_texCoord;                                  \n"
    "}                                                            \n";

/* fragment source */
static const gchar *URBOPANO_f_src =
    "uniform sampler2D s_texture;                    \n"
    "varying vec2 v_texCoord;                            \n"
    "void main()                                         \n"
    "{                                                   \n"
    "  gl_FragColor = texture2D( s_texture, v_texCoord );\n"
    "}                                                   \n";

static void
gst_gl_URBOPANO_class_init (GstGLURBOPANOClass * klass)
{
  GstElementClass *element_class;

  element_class = GST_ELEMENT_CLASS (klass);

  element_class->request_new_pad =
      GST_DEBUG_FUNCPTR (gst_gl_URBOPANO_request_new_pad);
  element_class->release_pad = GST_DEBUG_FUNCPTR (gst_gl_URBOPANO_release_pad);

  gst_element_class_set_metadata (element_class, "OpenGL URBOPANO",
      "Filter/Effect/Video", "OpenGL URBOPANO",
      "Julien Isorce <julien.isorce@gmail.com>");

  GST_GL_MIXER_CLASS (klass)->set_caps = gst_gl_URBOPANO_set_caps;
  GST_GL_MIXER_CLASS (klass)->reset = gst_gl_URBOPANO_reset;
  GST_GL_MIXER_CLASS (klass)->process_textures = gst_gl_URBOPANO_process_textures;
}

static void
gst_gl_URBOPANO_init (GstGLURBOPANO * URBOPANO)
{
  URBOPANO->shader = NULL;

  URBOPANO->attr_position_loc = -1;
  URBOPANO->attr_texture_loc = -1;
}

static GstPad *
gst_gl_URBOPANO_request_new_pad (GstElement * element, GstPadTemplate * templ,
    const gchar * req_name, const GstCaps * caps)
{
  GstPad *newpad;

  newpad = (GstPad *)
      GST_ELEMENT_CLASS (gst_gl_URBOPANO_parent_class)->request_new_pad (element,
      templ, req_name, caps);

  if (newpad == NULL)
    goto could_not_create;

  gst_child_proxy_child_added (GST_CHILD_PROXY (element), G_OBJECT (newpad),
      GST_OBJECT_NAME (newpad));

  return newpad;

could_not_create:
  {
    GST_DEBUG_OBJECT (element, "could not create/add pad");
    return NULL;
  }
}

static void
gst_gl_URBOPANO_release_pad (GstElement * element, GstPad * pad)
{
  GstGLURBOPANO *gl_URBOPANO = GST_GL_URBOPANO (element);

  GST_DEBUG_OBJECT (gl_URBOPANO, "release pad %s:%s", GST_DEBUG_PAD_NAME (pad));

  gst_child_proxy_child_removed (GST_CHILD_PROXY (gl_URBOPANO), G_OBJECT (pad),
      GST_OBJECT_NAME (pad));

  GST_ELEMENT_CLASS (gst_gl_URBOPANO_parent_class)->release_pad (element, pad);
}

static void
gst_gl_URBOPANO_reset (GstGLMixer * mixer)
{
  GstGLURBOPANO *URBOPANO = GST_GL_URBOPANO (mixer);

  if (URBOPANO->shader)
    gst_object_unref (URBOPANO->shader);
  URBOPANO->shader = NULL;

  URBOPANO->attr_position_loc = -1;
  URBOPANO->attr_texture_loc = -1;
  URBOPANO->xrot = 0.0;
  URBOPANO->yrot = 0.0;
  URBOPANO->zrot = 0.0;
}

static gboolean
gst_gl_URBOPANO_set_caps (GstGLMixer * mixer, GstCaps * outcaps)
{
  GstGLURBOPANO *URBOPANO = GST_GL_URBOPANO (mixer);

  g_clear_object (&URBOPANO->shader);
  return TRUE;
}

static void
_URBOPANO_render (GstGLContext * context, GstGLURBOPANO * URBOPANO)
{
  GstGLMixer *mixer = GST_GL_MIXER (URBOPANO);

  if (!URBOPANO->shader) {
    gchar *frag_str = g_strdup_printf ("%s%s",
        gst_gl_shader_string_get_highest_precision (GST_GL_BASE_MIXER
            (mixer)->context, GST_GLSL_VERSION_NONE,
            GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY),
        URBOPANO_f_src);

    gst_gl_context_gen_shader (GST_GL_BASE_MIXER (mixer)->context,
        URBOPANO_v_src, frag_str, &URBOPANO->shader);
    g_free (frag_str);
  }

  gst_gl_framebuffer_draw_to_texture (mixer->fbo, URBOPANO->out_tex,
      gst_gl_URBOPANO_callback, URBOPANO);
}

static gboolean
gst_gl_URBOPANO_process_textures (GstGLMixer * mix, GstGLMemory * out_tex)
{
  GstGLURBOPANO *URBOPANO = GST_GL_URBOPANO (mix);
  GstGLContext *context = GST_GL_BASE_MIXER (mix)->context;

  URBOPANO->out_tex = out_tex;

  gst_gl_context_thread_add (context, (GstGLContextThreadFunc) _URBOPANO_render,
      URBOPANO);

  return TRUE;
}

static void
_bind_buffer (GstGLURBOPANO * URBOPANO)
{
  GstGLContext *context = GST_GL_BASE_MIXER (URBOPANO)->context;
  const GstGLFuncs *gl = context->gl_vtable;

  gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, URBOPANO->vbo_indices);
  gl->BindBuffer (GL_ARRAY_BUFFER, URBOPANO->vertex_buffer);

  /* Load the vertex position */
  gl->VertexAttribPointer (URBOPANO->attr_position_loc, 3, GL_FLOAT,
      GL_FALSE, 5 * sizeof (GLfloat), (void *) 0);

  /* Load the texture coordinate */
  gl->VertexAttribPointer (URBOPANO->attr_texture_loc, 2, GL_FLOAT, GL_FALSE,
      5 * sizeof (GLfloat), (void *) (3 * sizeof (GLfloat)));


  gl->EnableVertexAttribArray (URBOPANO->attr_position_loc);
  gl->EnableVertexAttribArray (URBOPANO->attr_texture_loc);
}

static void
_unbind_buffer (GstGLURBOPANO * URBOPANO)
{
  GstGLContext *context = GST_GL_BASE_MIXER (URBOPANO)->context;
  const GstGLFuncs *gl = context->gl_vtable;

  gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);
  gl->BindBuffer (GL_ARRAY_BUFFER, 0);

  gl->DisableVertexAttribArray (URBOPANO->attr_position_loc);
  gl->DisableVertexAttribArray (URBOPANO->attr_texture_loc);
}

/* opengl scene, params: input texture (not the output mixer->texture) */
static gboolean
gst_gl_URBOPANO_callback (gpointer stuff)
{
  GstGLURBOPANO *URBOPANO = GST_GL_URBOPANO (stuff);
  GstGLMixer *mixer = GST_GL_MIXER (URBOPANO);
  GstGLFuncs *gl = GST_GL_BASE_MIXER (mixer)->context->gl_vtable;
  GList *walk;

  const GLfloat matrix[] = {
    0.5f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.5f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.5f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
  };

  guint count = 0;

  gst_gl_context_clear_shader (GST_GL_BASE_MIXER (mixer)->context);
  gl->BindTexture (GL_TEXTURE_2D, 0);

  gl->Enable (GL_DEPTH_TEST);

  gl->ClearColor (0.0, 0.0, 0.0, 0.0);
  gl->Clear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  gst_gl_shader_use (URBOPANO->shader);

  if (URBOPANO->attr_position_loc == -1) {
    URBOPANO->attr_position_loc =
        gst_gl_shader_get_attribute_location (URBOPANO->shader, "a_position");
  }
  if (URBOPANO->attr_texture_loc == -1) {
    URBOPANO->attr_texture_loc =
        gst_gl_shader_get_attribute_location (URBOPANO->shader, "a_texCoord");
  }

  gst_gl_shader_set_uniform_1i (URBOPANO->shader, "s_texture", 0);
  gst_gl_shader_set_uniform_1f (URBOPANO->shader, "xrot_degree", URBOPANO->xrot);
  gst_gl_shader_set_uniform_1f (URBOPANO->shader, "yrot_degree", URBOPANO->yrot);
  gst_gl_shader_set_uniform_1f (URBOPANO->shader, "zrot_degree", URBOPANO->zrot);
  gst_gl_shader_set_uniform_matrix_4fv (URBOPANO->shader, "u_matrix", 1,
      GL_FALSE, matrix);

  if (!URBOPANO->vertex_buffer) {
    /* *INDENT-OFF* */
    gfloat vertices[] = {
      /* front face */
       1.0f, 1.0f,-1.0f, 1.0f, 0.0f,
       1.0f,-1.0f,-1.0f, 1.0f, 1.0f,
      -1.0f,-1.0f,-1.0f, 0.0f, 1.0f,
      -1.0f, 1.0f,-1.0f, 0.0f, 0.0f,
      /* right face */
       2.0f, 2.0f, 2.0f, 2.0f, 1.0f,
       2.0f,0.0f, 2.0f, 1.0f, 1.0f,
       2.0f,0.0f,0.0f, 1.0f, 2.0f,
       2.0f, 2.0f,0.0f, 2.0f, 2.0f,
      /* left face */
      -1.0f, 1.0f, 1.0f, 1.0f, 0.0f,
      -1.0f, 1.0f,-1.0f, 1.0f, 1.0f,
      -1.0f,-1.0f,-1.0f, 0.0f, 1.0f,
      -1.0f,-1.0f, 1.0f, 0.0f, 0.0f,
      /* top face */
       1.0f,-1.0f, 1.0f, 1.0f, 0.0f,
      -1.0f,-1.0f, 1.0f, 0.0f, 0.0f,
      -1.0f,-1.0f,-1.0f, 0.0f, 1.0f,
       1.0f,-1.0f,-1.0f, 1.0f, 1.0f,
      /* bottom face */
       1.0f, 1.0f, 1.0f, 1.0f, 0.0f,
       1.0f, 1.0f,-1.0f, 1.0f, 1.0f,
      -1.0f, 1.0f,-1.0f, 0.0f, 1.0f,
      -1.0f, 1.0f, 1.0f, 0.0f, 0.0f,
      /* back face */
       1.0f, 1.0f, 1.0f, 1.0f, 0.0f,
      -1.0f, 1.0f, 1.0f, 0.0f, 0.0f,
      -1.0f,-1.0f, 1.0f, 0.0f, 1.0f,
       1.0f,-1.0f, 1.0f, 1.0f, 1.0f
    };
    const GLushort indices[] = {
      0, 1, 2,
      0, 2, 3,
      4, 5, 6,
      4, 6, 7,
      8, 9, 10,
      8, 10, 11,
      12, 13, 14,
      12, 14, 15,
      16, 17, 18,
      16, 18, 19,
      20, 21, 22,
      20, 22, 23,
    };
    /* *INDENT-ON* */

    if (gl->GenVertexArrays) {
      gl->GenVertexArrays (1, &URBOPANO->vao);
      gl->BindVertexArray (URBOPANO->vao);
    }

    gl->GenBuffers (1, &URBOPANO->vertex_buffer);
    gl->BindBuffer (GL_ARRAY_BUFFER, URBOPANO->vertex_buffer);
    gl->BufferData (GL_ARRAY_BUFFER, sizeof (vertices), vertices,
        GL_STATIC_DRAW);

    gl->GenBuffers (1, &URBOPANO->vbo_indices);
    gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, URBOPANO->vbo_indices);
    gl->BufferData (GL_ELEMENT_ARRAY_BUFFER, sizeof (indices), indices,
        GL_STATIC_DRAW);
  }

  if (gl->GenVertexArrays)
    gl->BindVertexArray (URBOPANO->vao);
  _bind_buffer (URBOPANO);

  GST_OBJECT_LOCK (URBOPANO);
  walk = GST_ELEMENT (URBOPANO)->sinkpads;
  while (walk) {
    GstGLMixerPad *pad = walk->data;
    guint in_tex;
    guint width, height;

    in_tex = pad->current_texture;
    width = GST_VIDEO_INFO_WIDTH (&GST_VIDEO_AGGREGATOR_PAD (pad)->info);
    height = GST_VIDEO_INFO_HEIGHT (&GST_VIDEO_AGGREGATOR_PAD (pad)->info);

    if (count >= 6) {
      GST_FIXME_OBJECT (URBOPANO, "Skipping 7th pad (and all subsequent pads)");
      break;
    }

    if (!in_tex || width <= 0 || height <= 0) {
      GST_DEBUG ("skipping texture:%u pad:%p width:%u height %u",
          in_tex, pad, width, height);
      count++;
      walk = g_list_next (walk);
      continue;
    }

    GST_TRACE ("processing texture:%u dimensions:%ux%u", in_tex, width, height);

    gl->ActiveTexture (GL_TEXTURE0);
    gl->BindTexture (GL_TEXTURE_2D, in_tex);

    gl->DrawElements (GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
        (void *) (6 * sizeof (GLushort) * count));

    ++count;

    walk = g_list_next (walk);
  }
  GST_OBJECT_UNLOCK (URBOPANO);

  if (gl->GenVertexArrays)
    gl->BindVertexArray (0);
  else
    _unbind_buffer (URBOPANO);

  gl->BindTexture (GL_TEXTURE_2D, 0);

  gl->Disable (GL_DEPTH_TEST);

  gst_gl_context_clear_shader (GST_GL_BASE_MIXER (mixer)->context);

  URBOPANO->xrot += 0.0f;
  URBOPANO->yrot += 0.0f;
  URBOPANO->zrot += 0.0f;

  return TRUE;
}

/* GstChildProxy implementation */
static GObject *
gst_gl_URBOPANO_child_proxy_get_child_by_index (GstChildProxy * child_proxy,
    guint index)
{
  GstGLURBOPANO *gl_URBOPANO = GST_GL_URBOPANO (child_proxy);
  GObject *obj = NULL;

  GST_OBJECT_LOCK (gl_URBOPANO);
  obj = g_list_nth_data (GST_ELEMENT_CAST (gl_URBOPANO)->sinkpads, index);
  if (obj)
    gst_object_ref (obj);
  GST_OBJECT_UNLOCK (gl_URBOPANO);

  return obj;
}

static guint
gst_gl_URBOPANO_child_proxy_get_children_count (GstChildProxy * child_proxy)
{
  guint count = 0;
  GstGLURBOPANO *gl_URBOPANO = GST_GL_URBOPANO (child_proxy);

  GST_OBJECT_LOCK (gl_URBOPANO);
  count = GST_ELEMENT_CAST (gl_URBOPANO)->numsinkpads;
  GST_OBJECT_UNLOCK (gl_URBOPANO);
  GST_INFO_OBJECT (gl_URBOPANO, "Children Count: %d", count);

  return count;
}

static void
gst_gl_URBOPANO_child_proxy_init (gpointer g_iface, gpointer iface_data)
{
  GstChildProxyInterface *iface = g_iface;

  iface->get_child_by_index = gst_gl_URBOPANO_child_proxy_get_child_by_index;
  iface->get_children_count = gst_gl_URBOPANO_child_proxy_get_children_count;
}
