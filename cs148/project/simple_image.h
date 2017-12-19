#ifndef _SIMPLEIMAGE_H_
#define _SIMPLEIMAGE_H_

#include <string>
#include <iostream>
#include <assert.h>

/**
 * A simple RGB color struct with [0,1] floating point values for each color channel
 */
struct RGBColor {
  float r, g, b;

  RGBColor(float red, float green, float blue) : r(red), g(green), b(blue) { }

  RGBColor() : r(0), g(0), b(0) { }

  RGBColor operator+(RGBColor const o) {
    return RGBColor(r + o.r, g + o.g, b + o.b);
  }

  RGBColor operator-(RGBColor const o) {
    return RGBColor(r - o.r, g - o.g, b - o.b);
  }

  RGBColor operator*(float s) {
    return RGBColor(r * s, g * s, b * s);
  }
};

/**
 * An image class, adapted from libST's STImage.
 * The class stores colors in RGB form and can load and write PNG files.
 * Colors are stored as RGB floats with values between 0 and 1.
 *
 * Pixel values for a SimpleImage I can be accessed using I(x,y) and can be set to an RGBColor c through I.set(x, y, c).
 * All functions that operate on pixels clamp their indices to within the range of the image.
 */
class SimpleImage {
 public:
  SimpleImage();
  ~SimpleImage();

  // Constructors
  SimpleImage(const SimpleImage& img);
  SimpleImage(int width, int height, const RGBColor& bg);
  SimpleImage(int width, int height, unsigned char* data);
  SimpleImage(const std::string& filename);

  // Initialize SimpleImage to given width and height
  void initialize(int width, int height);

  // Return by reference pixel value at (x, y)
  RGBColor& operator() (int x, int y) {
    x = (x >= 0) ? ((x < _width) ? x : _width - 1) : 0;
    y = (y >= 0) ? ((y < _height) ? y : _height - 1) : 0;
    return _data[_width * y + x];
  }

  // Return pixel value at (x, y)
  RGBColor operator() (int x, int y) const {
    x = (x >= 0) ? ((x < _width) ? x : _width - 1) : 0;
    y = (y >= 0) ? ((y < _height) ? y : _height - 1) : 0;
    return _data[_width * y + x];
  }

  // Set value of pixel at (x, y) to c. NOTE: clamps x and y to be within image.
  void set(int x, int y, const RGBColor& c) {
    x = (x >= 0) ? ((x < _width) ? x : _width - 1) : 0;
    y = (y >= 0) ? ((y < _height) ? y : _height - 1) : 0;
    _data[_width * y + x] = c;
  }

  // Load image from filename
  void load(const std::string& filename);
  
  // Save image to filename
  bool save(const std::string& filename);

  // Return whether this SimpleImage is empty
  bool empty() const;

  // Return the width of this SimpleImage in pixels
  int width() const {
    return _width;
  }

  // Return the height of this SimpleImage in pixels
  int height() const {
    return _height;
  }

  // Return pointer to color data array
  RGBColor* data() const {
    return _data;
  }

 private:
  RGBColor* _data;
  int _width;
  int _height;

  bool inBounds(int x, int y) const { return x >= 0 && x < _width && y >= 0 && y < _height; }
};

#endif  // _SIMPLEIMAGE_H_
