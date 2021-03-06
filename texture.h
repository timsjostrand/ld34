#ifndef _TEXTURE_H
#define _TEXTURE_H

#include <stdint.h>
#include <GLFW/glfw3.h>

int  image_load(uint8_t **out, int *width, int *height, const uint8_t *data, size_t len);
void image_free(uint8_t *data);

int  texture_load(GLuint *tex, int *width, int *height, const uint8_t *data,
		size_t len);
int  texture_load_pixels(GLuint *tex, const uint8_t *data,
		const int width, const int height);
void texture_white(GLuint *tex);
void texture_free(const GLuint tex);

#endif
