#include "oily_png_ext.h"

void oily_png_check_size_constraints(long self_width, long self_height, long other_width, long other_height, long offset_x, long offset_y){
  // For now, these raise a standard runtime error. They should however raise custom exception classes (OutOfBounds)
  if(self_width  < other_width  + offset_x){
    rb_raise(rb_eRuntimeError, "Background image width is too small!");
  }
  if(self_height < other_height + offset_y){
    rb_raise(rb_eRuntimeError, "Background image height is too small!");
  }
}

VALUE oily_png_compose_bang(int argc, VALUE *argv, VALUE self) {
  // Corresponds to the other image(foreground) that we want to compose onto this one(background).
  VALUE other;
  
  // The offsets are optional arguments, so these may or may not be null pointers.
  // We'll prefix them with 'opt' to identify this.
  VALUE opt_offset_x;
  VALUE opt_offset_y;
  
  // Scan the passed in arguments, and populate the above-declared variables. Notice that '12'
  // specifies that oily_png_compose_bang takes in 1 required parameter, and 2 optional ones (the offsets)
  rb_scan_args(argc, argv, "12", &other,&opt_offset_x,&opt_offset_y);
  
  // Regardless of whether offsets were provided, we must specify a default value for them since they will
  // be used in calculating the position of the composed element.
  long offset_x = 0;
  long offset_y = 0;

  // If offsets were provided, then the opt_offset_* variables will not be null pointers. FIXNUM_P checks
  // whether they point to a fixnum object. If they do, then we can safely assign our offset_* variables to the values.
  if(FIXNUM_P(opt_offset_x)){
    offset_x = FIX2LONG(opt_offset_x);
  }
  if(FIXNUM_P(opt_offset_y)){
    offset_y = FIX2LONG(opt_offset_y);
  }
  
  // Get the dimension data for both foreground and background images.
  long self_width        = FIX2LONG(rb_funcall(self, rb_intern("width"), 0));
  long self_height       = FIX2LONG(rb_funcall(self, rb_intern("height"), 0));
  long other_width       = FIX2LONG(rb_funcall(other, rb_intern("width"), 0));
  long other_height      = FIX2LONG(rb_funcall(other, rb_intern("height"), 0));
  
  // Make sure that the 'other' image fits within the current image. If it doesn't, an exception is raised
  // and the operation should be aborted.
  oily_png_check_size_constraints( self_width, self_height, other_width, other_height, offset_x, offset_y );
  
  // Get the pixel data for both the foreground(other) and background(self) pixels. 
  VALUE* bg_pixels = RARRAY_PTR(rb_funcall(self, rb_intern("pixels"), 0));
  VALUE* fg_pixels = RARRAY_PTR(rb_funcall(other, rb_intern("pixels"), 0));

  long x = 0;
  long y = 0;
  long bg_index = 0; // corresponds to the current index in the bg_pixels array.
  for( y = 0; y < other_height; y++ ){
    for( x = 0; x < other_width; x++ ){
      // We need to find the value of bg_index twice, so we only calculate and store it once.
      bg_index = ( x + offset_x ) + ( y + offset_y ) * self_width;
      // Replace the background pixel with the composition of background + foreground
      bg_pixels[bg_index] = UINT2NUM( oily_png_compose_color( NUM2UINT( fg_pixels[x+ y * other_width] ), NUM2UINT( bg_pixels[bg_index] ) ) );
    }
  }
  return self;
}

VALUE oily_png_fast_compose_bang(int argc, VALUE *argv, VALUE self) {
  // Parse arguments
  VALUE other, opt_offset_x, opt_offset_y;
  rb_scan_args(argc, argv, "12", &other, &opt_offset_x, &opt_offset_y);
  long offset_x = 0, offset_y = 0;
  if (FIXNUM_P(opt_offset_x)) offset_x = FIX2LONG(opt_offset_x);
  if (FIXNUM_P(opt_offset_y)) offset_y = FIX2LONG(opt_offset_y);

  // Check dimensions
  long self_width   = FIX2LONG(rb_funcall(self,  rb_intern("width"),  0));
  long self_height  = FIX2LONG(rb_funcall(self,  rb_intern("height"), 0));
  long other_width  = FIX2LONG(rb_funcall(other, rb_intern("width"),  0));
  long other_height = FIX2LONG(rb_funcall(other, rb_intern("height"), 0));
  oily_png_check_size_constraints(self_width, self_height, other_width, other_height, offset_x, offset_y);
  
  // Get pixel data
  VALUE* bg_pixels = RARRAY_PTR(rb_funcall(self,  rb_intern("pixels"), 0));
  VALUE* fg_pixels = RARRAY_PTR(rb_funcall(other, rb_intern("pixels"), 0));

  // Replace pixel data accordingly
  long w = other_width;
  long o = self_width - w + 1;
  long i = offset_y * self_width + offset_x - o;
  long c = other_width * other_height;
  for (long p = 0; p < c; p++) {
    i += p % w == 0 ? o : 1;
    if (FIX2UINT(fg_pixels[p]) == 0) continue;
    bg_pixels[i] = fg_pixels[p];
  }
  return self;
}

VALUE oily_png_fast_rect(int argc, VALUE *argv, VALUE self) {
  // Parse arguments
  VALUE opt_x0, opt_y0, opt_x1, opt_y1, opt_stroke_color, opt_fill_color;
  rb_scan_args(argc, argv, "42", &opt_x0, &opt_y0, &opt_x1, &opt_y1, &opt_stroke_color, &opt_fill_color);
  long x0 = FIX2LONG(opt_x0), y0 = FIX2LONG(opt_y0), x1 = FIX2LONG(opt_x1), y1 = FIX2LONG(opt_y1);
  VALUE stroke_color = LONG2FIX(0), fill_color = LONG2FIX(0);
  if (FIXNUM_P(opt_stroke_color)) stroke_color = opt_stroke_color;
  if (FIXNUM_P(opt_fill_color)) fill_color = opt_fill_color;

  // Retrieve values
  long width = FIX2LONG(rb_funcall(self, rb_intern("width"), 0));
  VALUE* pixels = RARRAY_PTR(rb_funcall(self, rb_intern("pixels"), 0));
  
  // Draw rectangle filling, unless it's transparent
  if (FIX2UINT(fill_color) != 0) {
    long x, y;
    for (x = x0; x <= x1; x++) {
      for (y = y0; y <= y1; y++) {
        pixels[y * width + x] = fill_color;
      }
    }
  }

  // Draw rectangle frame, unless it's transparent
  if (FIX2UINT(stroke_color) != 0) {
    long x, y;
    for (x = x0; x <= x1; x++) pixels[y0 * width + x] = stroke_color;
    for (x = x0; x <= x1; x++) pixels[y1 * width + x] = stroke_color;
    for (y = y0; y <= y1; y++) pixels[y * width + x0] = stroke_color;
    for (y = y0; y <= y1; y++) pixels[y * width + x1] = stroke_color; 
  }

  return self;
}


VALUE oily_png_replace_bang(int argc, VALUE *argv, VALUE self) {
  // Corresponds to the other image(foreground) that we want to compose onto this one(background).
  VALUE other;
  
  // The offsets are optional arguments, so these may or may not be null pointers.
  // We'll prefix them with 'opt' to identify this.
  VALUE opt_offset_x;
  VALUE opt_offset_y;
  
  // Scan the passed in arguments, and populate the above-declared variables. Notice that '12'
  // specifies that oily_png_compose_bang takes in 1 required parameter, and 2 optional ones (the offsets)
  rb_scan_args(argc, argv, "12", &other,&opt_offset_x,&opt_offset_y);
  
  // Regardless of whether offsets were provided, we must specify a default value for them since they will
  // be used in calculating the position of the composed element.
  long offset_x = 0;
  long offset_y = 0;

  // If offsets were provided, then the opt_offset_* variables will not be null pointers. FIXNUM_P checks
  // whether they point to a fixnum object. If they do, then we can safely assign our offset_* variables to the values.
  if(FIXNUM_P(opt_offset_x)){
    offset_x = FIX2LONG(opt_offset_x);
  }
  if(FIXNUM_P(opt_offset_y)){
    offset_y = FIX2LONG(opt_offset_y);
  }
  
  // Get the dimension data for both foreground and background images.
  long self_width        = FIX2LONG(rb_funcall(self, rb_intern("width"), 0));
  long self_height       = FIX2LONG(rb_funcall(self, rb_intern("height"), 0));
  long other_width       = FIX2LONG(rb_funcall(other, rb_intern("width"), 0));
  long other_height      = FIX2LONG(rb_funcall(other, rb_intern("height"), 0));
  
  // Make sure that the 'other' image fits within the current image. If it doesn't, an exception is raised
  // and the operation should be aborted.
  oily_png_check_size_constraints( self_width, self_height, other_width, other_height, offset_x, offset_y );
  
  // Get the pixel data for both the foreground(other) and background(self) pixels. 
  VALUE* bg_pixels = RARRAY_PTR(rb_funcall(self, rb_intern("pixels"), 0));
  VALUE* fg_pixels = RARRAY_PTR(rb_funcall(other, rb_intern("pixels"), 0));

  long x = 0;
  long y = 0;
  long bg_index = 0; // corresponds to the current index in the bg_pixels array.
  for( y = 0; y < other_height; y++ ){
    for( x = 0; x < other_width; x++ ){
      // We need to find the value of bg_index twice, so we only calculate and store it once.
      bg_index = ( x + offset_x ) + ( y + offset_y ) * self_width;
      // Replace the background pixel with the composition of background + foreground
      bg_pixels[bg_index] = fg_pixels[x+ y * other_width];
    }
  }
  return self;
}

VALUE oily_png_rotate_left_bang(VALUE self){
  int store_at;
  VALUE pixel_value;
  int canvas_width = NUM2INT(rb_funcall(self, rb_intern("width"), 0));
  int canvas_height = NUM2INT(rb_funcall(self, rb_intern("height"), 0));
  VALUE original_pixels = rb_funcall(self, rb_intern("pixels"), 0);
  VALUE new_pixels = rb_ary_dup(original_pixels);
  int i, j;
  for( j = 0 ; j < canvas_width; j++ ){
    for( i = 0 ; i < canvas_height; i++ ){
      store_at = (canvas_width - 1 - j)*canvas_height + i;
      pixel_value = rb_ary_entry(original_pixels, i*canvas_width + j);
      rb_ary_store(new_pixels, store_at, pixel_value );
    }
  }
  rb_funcall(self, rb_intern("replace_canvas!"), 3, INT2NUM(canvas_height), INT2NUM(canvas_width), new_pixels);
  return self;
}

VALUE oily_png_rotate_right_bang(VALUE self){
  int store_at;
  VALUE pixel_value;
  int canvas_width = NUM2INT(rb_funcall(self, rb_intern("width"), 0));
  int canvas_height = NUM2INT(rb_funcall(self, rb_intern("height"), 0));
  VALUE original_pixels = rb_funcall(self, rb_intern("pixels"), 0);
  VALUE new_pixels = rb_ary_dup(original_pixels);
  int i, j;
  for( j = 0; j < canvas_width; j++ ){
    for( i = 0; i < canvas_height; i++ ){
      store_at = j * canvas_height + (canvas_height - i - 1);
      pixel_value = rb_ary_entry(original_pixels, i*canvas_width + j);
      rb_ary_store(new_pixels, store_at, pixel_value );
    }
  }
  rb_funcall(self, rb_intern("replace_canvas!"), 3, INT2NUM(canvas_height), INT2NUM(canvas_width), new_pixels);
  return self;
}