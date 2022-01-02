#ifndef VULKAN_H_INCLUDED
#define VULKAN_H_INCLUDED

#include "../../util/debug.h"
#include "../../util/util.h"
#include "../../util/dynarr.h"

#include <GL/gl.h>
#include <GL/freeglut.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

void vk_init(void);

void vk_clean(void);

#endif