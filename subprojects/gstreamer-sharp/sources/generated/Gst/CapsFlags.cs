// This file was generated by the Gtk# code generator.
// Any changes made will be lost if regenerated.

namespace Gst {

	using System;
	using System.Runtime.InteropServices;

#region Autogenerated code
	[Flags]
	[GLib.GType (typeof (Gst.CapsFlagsGType))]
	public enum CapsFlags : uint {

		Any = 16,
	}

	internal class CapsFlagsGType {
		[DllImport ("gstreamer-1.0-0.dll", CallingConvention = CallingConvention.Cdecl)]
		static extern IntPtr gst_caps_flags_get_type ();

		public static GLib.GType GType {
			get {
				return new GLib.GType (gst_caps_flags_get_type ());
			}
		}
	}
#endregion
}
