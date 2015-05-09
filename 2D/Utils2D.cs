﻿using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Text;
using ClassicalSharp.GraphicsAPI;

namespace ClassicalSharp {
	
	public static class Utils2D {
		
		public static StringFormat format;
		static Bitmap measuringBmp;
		static Graphics measuringGraphics;
		static Utils2D() {
			format = StringFormat.GenericTypographic;
			format.FormatFlags |= StringFormatFlags.MeasureTrailingSpaces;
			format.Trimming = StringTrimming.None;
			//format.FormatFlags |= StringFormatFlags.NoWrap;
			//format.FormatFlags |= StringFormatFlags.NoClip;
			measuringBmp = new Bitmap( 1, 1 );
			measuringGraphics = Graphics.FromImage( measuringBmp );
		}
		
		const float shadowOffset = 1.3f;		
		public static Size MeasureSize( string text, Font font, bool shadow ) {
			SizeF size = measuringGraphics.MeasureString( text, font, Int32.MaxValue, format );
			if( shadow ) {
				size.Width += shadowOffset;
				size.Height += shadowOffset;
			}
			return Size.Ceiling( size );
		}
		
		public static Size MeasureSize( List<DrawTextArgs> parts, Font font, bool shadow ) {
			SizeF total = SizeF.Empty;
			for( int i = 0; i < parts.Count; i++ ) {
				SizeF size = measuringGraphics.MeasureString( parts[i].Text, font, Int32.MaxValue, format );
				total.Height = Math.Max( total.Height, size.Height );
				total.Width += size.Width;
			}
			if( shadow ) {
				total.Width += shadowOffset;
				total.Height += shadowOffset;
			}
			return Size.Ceiling( total );
		}
		
		public static void DrawText( Graphics g, Font font, DrawTextArgs args, float x, float y ) {
			using( Brush textBrush = new SolidBrush( args.TextColour ),
			      shadowBrush = new SolidBrush( args.ShadowColour ) ) {
				g.TextRenderingHint = TextRenderingHint.AntiAlias;
				if( args.UseShadow ) {
					g.DrawString( args.Text, font, shadowBrush, x + shadowOffset, y + shadowOffset, format );
				}
				g.DrawString( args.Text, font, textBrush, x, y, format );
			}
		}
		
		public static void DrawText( Graphics g, List<DrawTextArgs> parts, Font font, float x, float y ) {
			for( int i = 0; i < parts.Count; i++ ) {
				DrawTextArgs part = parts[i];
				DrawText( g, font, part, x, y );
				SizeF partSize = g.MeasureString( part.Text, font, Int32.MaxValue, format );
				x += partSize.Width;
			}
		}
		
		public static void DrawRect( Graphics g, Color colour, int x, int y, int width, int height ) {
			using( Brush brush = new SolidBrush( colour ) ) {
				g.FillRectangle( brush, x, y, width, height );
			}
		}
		
		public static void DrawRectBounds( Graphics g, Color colour, float lineWidth, int x, int y, int width, int height ) {
			using( Pen pen = new Pen( colour, lineWidth ) ) {
				g.DrawRectangle( pen, x, y, width, height );
			}
		}
		
		public static Texture MakeTextTexture( Font font, int x1, int y1, DrawTextArgs args ) {
			Size size = MeasureSize( args.Text, font, args.UseShadow );
			using( Bitmap bmp = new Bitmap( size.Width, size.Height ) ) {
				using( Graphics g = Graphics.FromImage( bmp ) ) {
					DrawText( g, font, args, 0, 0 );
				}
				return Make2DTexture( args.Graphics, bmp, x1, y1 );
			}
		}
		
		public static Texture MakeTextTexture( List<DrawTextArgs> parts, Font font, Size size, int x1, int y1 ) {
			if( parts.Count == 0 ) return new Texture( -1, x1, y1, 0, 0, 1, 1 );
			using( Bitmap bmp = new Bitmap( size.Width, size.Height ) ) {
				using( Graphics g = Graphics.FromImage( bmp ) ) {
					DrawText( g, parts, font, 0, 0 );
				}
				return Make2DTexture( parts[0].Graphics, bmp, x1, y1 );
			}
		}
		
		public static Texture Make2DTexture( IGraphicsApi graphics, Bitmap bmp, int x1, int y1 ) {
			if( graphics.SupportsNonPowerOf2Textures ) {
				int textureID = graphics.LoadTexture( bmp );
				return new Texture( textureID, x1, y1, bmp.Width, bmp.Height, 1f, 1f );
			} else {
				using( Bitmap adjBmp = ResizeToPower2( bmp ) )  {
					int textureID = graphics.LoadTexture( adjBmp );
					return new Texture( textureID, x1, y1, bmp.Width, bmp.Height,
					                   (float)bmp.Width / adjBmp.Width, (float)bmp.Height / adjBmp.Height );
				}
			}
		}
		
		public static Bitmap ResizeToPower2( Bitmap bmp ) {
			int adjWidth = Utils.NextPowerOf2( bmp.Width );
			int adjHeight = Utils.NextPowerOf2( bmp.Height );
			Bitmap adjBmp = new Bitmap( adjWidth, adjHeight );
			using( Graphics g = Graphics.FromImage( adjBmp ) ) {
				g.DrawImage( bmp, 0, 0 );
			}
			return adjBmp;
		}
	}
}
