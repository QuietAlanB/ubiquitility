#include "util/debug.h"
#include "engine/game.h"

#include <stdio.h>
#include <stdlib.h>

extern GLFWwindow				*wnd;

int main(void)
{
	vk_init();

	while (!glfwWindowShouldClose(wnd))
		glfwPollEvents();

	vk_clean();

	dbg_info("ran successfully");

	return 0;
}