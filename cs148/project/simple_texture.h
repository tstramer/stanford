// simple_texture.h
#ifndef __SIMPLE_TEXTURE_H__
#define __SIMPLE_TEXTURE_H__

#include "findGLUT.h"
#include "simple_image.h"

#define MAX_TEXTURES 100

/**
* The SimpleTexture class allows use of an SimpleImage as an OpenGL texture.
* In the simplest case, just construct a new SimpleTexture with an image:
*
*   SimpleTexture* texture = new SimpleTexture(image);
*
* You can also change the image used by an SimpleTexture at any time by
* using the loadImageData() method:
*
*   texture->loadImageData(someOtherImage);
*
* To use an SimpleTexture for OpenGL rendering, you should call Bind()
* before rendering with the texture, and unBind() after you are done.
*
*   texture->bind();
*   // do OpenGL rendering
*   texture->unBind();
*
* You must remember not to create any SimpleTextures until you have
* initialized OpenGL.
*/
class SimpleTexture
{
public:
    //
    // Options when loading an image to an SimpleTexture. Use the
    // kGenerateMipmaps option to use OpenGL to generate mipmaps -
    // downsampled images used to improve the quality of texture
    // filtering.
    //
    enum ImageOptions {
        kNone = 0,
        kGenerateMipmaps = 0x1,
    };

    //
    // Create a new SimpleTexture using the pixel data from the given
    // image (which will be copied in). Use the options to specify
    // whether mipmaps should be generated.
    //
    SimpleTexture(const SimpleImage* image, ImageOptions options = kNone);
	SimpleTexture(const float* pixels, int width, int height, ImageOptions options = kNone);

    //
    // Create an "empty" SimpleTexture with no image data. You will need
    // to load an image before you can use this texture for
    // rendering.
    //
    SimpleTexture();

    //
    // Delete an existing SimpleTexture.
    //
    ~SimpleTexture();

    //
    // Load image data into the SimpleTexture. The texture will be
    // resized to match the image as needed. Use the options
    // to specify whether mipmaps should be generated.
    //
    void loadImageData(GLuint texId, const SimpleImage* image,
                       ImageOptions options = kNone);
	void loadImageData(GLuint texId, const float* pixels, int width, int height,
		ImageOptions options = kNone);

    //
    // Bind this texture for use in subsequent OpenGL drawing.
    //
    void bind();

    //
    // Un-bind this texture and return to untextured drawing.
    //
    void unBind();

    //
    // Get the width (in pixels) of the image.
    //
    int getWidth() const { return mWidth; }

    //
    // Get the height (in pixels) of the image.
    //
	int getHeight() const { return mHeight; }

	// Common initialization code, used by all constructors.
	void initialize();

    static GLuint nextTexId();
    static GLuint *getTextures();
private:

    // The OpenGL texture id.
    GLuint mTexId;

    // The width and height of the image data.
    int mWidth;
    int mHeight;
};

#endif // __SIMPLE_TEXTURE_H__