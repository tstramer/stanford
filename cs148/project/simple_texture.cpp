#include "simple_texture.h"

static int texIdIndex;
static GLuint textures[MAX_TEXTURES];

SimpleTexture::SimpleTexture() {}
SimpleTexture::~SimpleTexture() {}

SimpleTexture::SimpleTexture(const SimpleImage* image, ImageOptions options) {
	loadImageData(nextTexId(), image, options);
}

SimpleTexture::SimpleTexture(const float* pixels, int width, int height, ImageOptions options) {
	loadImageData(nextTexId(), pixels, width, height, options);
}

void SimpleTexture::loadImageData(GLuint texId, const SimpleImage* image, ImageOptions options) {
	loadImageData(texId, (float*)image->data(), image->width(), image->height(), options);
}

void SimpleTexture::loadImageData(GLuint texId, const float* pixels, int width, int height, ImageOptions options) {
	mWidth = width;
	mHeight = height;
	mTexId = texId;

    glBindTexture(GL_TEXTURE_2D, mTexId);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

    if (options == kGenerateMipmaps) {
		gluBuild2DMipmaps(GL_TEXTURE_2D, 3, mWidth, mHeight, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    } else {
	    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, mWidth, mHeight, 0, GL_RGB, GL_FLOAT, pixels);
	}
}

void SimpleTexture::bind() {
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, mTexId);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

void SimpleTexture::unBind() {
	glDisable(GL_TEXTURE_2D);
}

void SimpleTexture::initialize() {}

GLuint SimpleTexture::nextTexId() {
	GLuint texId = textures[texIdIndex];
	texIdIndex++;
	return texId;
}

GLuint *SimpleTexture::getTextures() {
	return textures;
}