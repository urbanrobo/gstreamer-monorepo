// This file was generated by the Gtk# code generator.
// Any changes made will be lost if regenerated.

namespace Gst.Audio {

	using System;
	using System.Runtime.InteropServices;

#region Autogenerated code
	[Flags]
	[GLib.GType (typeof (Gst.Audio.AudioResamplerFlagsGType))]
	public enum AudioResamplerFlags : uint {

		None = 0,
		NonInterleavedIn = 1,
		NonInterleavedOut = 2,
		VariableRate = 4,
	}

	internal class AudioResamplerFlagsGType {
		[DllImport ("gstaudio-1.0-0.dll", CallingConvention = CallingConvention.Cdecl)]
		static extern IntPtr gst_audio_resampler_flags_get_type ();

		public static GLib.GType GType {
			get {
				return new GLib.GType (gst_audio_resampler_flags_get_type ());
			}
		}
	}
#endregion
}
