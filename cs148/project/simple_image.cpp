#include "simple_image.h"

#include <cstdlib>
#include <stdexcept>

#include "stb_image.h"
#include "stb_image_write.h"

using std::string;

SimpleImage::SimpleImage() :
  _width(0),
  _height(0) {
  _data = new RGBColor[1];  // just initialize the pointer to something
}

SimpleImage::SimpleImage(int width, int height, const RGBColor& bg):
  _width(width),
  _height(height) {
  _data = new RGBColor[_width * _height];

  for (int i = 0; i < _width; i++) {
    for (int j = 0; j < _height; j++) {
      _data[j * _width + i] = bg;
    }
  }
}

SimpleImage::SimpleImage(const SimpleImage& img) {
  _width = img.width();
  _height = img.height();
  _data = new RGBColor[_width * _height];

  for (int i = 0; i < _width; i++) {
    for (int j = 0; j < _height; j++) {
      _data[j * _width + i] = img(i, j);
    }
  }
}

SimpleImage::SimpleImage(const string& filename) {
  //just set rgb color to something
  _data = new RGBColor[1];

  load(filename);
}

SimpleImage::SimpleImage(int width, int height, unsigned char* data) {
  if (width <= 0)
  { throw std::runtime_error("SimpleImage width must be positive"); }
  if (height <= 0)
  { throw std::runtime_error("SimpleImage height must be positive"); }

  _width = width;
  _height = height;
  _data = new RGBColor[width * height];
  for (int i = 0; i < width; i++) {
    for (int j = 0; j < height; j++) {
      int idx = 4 * (width * j + i);
      int idbx2 = width * j + i;
      _data[idbx2] = RGBColor(data[idx] / 255.f, data[idx + 1] / 255.f, data[idx + 2] / 255.f);
    }
  }
}


SimpleImage::~SimpleImage() {
  if (_data) { delete[] _data; }
}

bool SimpleImage::empty() const {
  return _width == 0 || _height == 0;
}

void SimpleImage::initialize(int width, int height) {
  if (width <= 0) { throw std::runtime_error("SimpleImage width must be positive"); }
  if (height <= 0) { throw std::runtime_error("SimpleImage height must be positive"); }

  _width = width;
  _height = height;

  int numPixels = _width * _height;

  //delete previous data
  if (_data) { delete[] _data; }

  _data = new RGBColor[numPixels];
}

void SimpleImage::load(const string& filename) {
  int width, height, n;
  unsigned char* charData = stbi_load(filename.c_str(), &width, &height, &n, 3);

  if (!charData) {
    fprintf(stderr, "SimpleImage::load() - Could not open '%s'.\n",
            filename.c_str());
    throw std::runtime_error("Error in load");
  }
  initialize(width, height);

  // Transform the data into floating point on the range [0,1]
  RGBColor* pixels = _data;
  for (int i = 0; i < width * height; ++i) {
    pixels[i].r = charData[i * 3 + 0] / 255.0f;
    pixels[i].g = charData[i * 3 + 1] / 255.0f;
    pixels[i].b = charData[i * 3 + 2] / 255.0f;
  }
  stbi_image_free(charData);
}

bool SimpleImage::save(const string& filename) {
  int channels = 3;
  // Transform the data into unsigned chars
  unsigned char* charData = (unsigned char*)std::malloc(_width * _height * channels);
  RGBColor* pixels = _data;
  for (int i = 0; i < _width * _height; ++i) {
    charData[i * 3 + 0] = (unsigned char)(pixels[i].r * 255.0f);
    charData[i * 3 + 1] = (unsigned char)(pixels[i].g * 255.0f);
    charData[i * 3 + 2] = (unsigned char)(pixels[i].b * 255.0f);
  }

  int success = stbi_write_png(filename.c_str(), _width, _height, channels, charData, _width * channels);
  std::free(charData);
  if (!success) {
    fprintf(stderr, "SimpleImage::save() - Could not save '%s'.\n",
            filename.c_str());
    return false;
  }
  return true;
}
