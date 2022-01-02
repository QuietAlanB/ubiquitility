#include "vulkan.h"

typedef struct {
	int					gfx, present;
} queue_fam_inds;

typedef struct {
	VkQueue					gfx, present;
	float					priority;
} vulkan_queues;

typedef struct {
	VkSurfaceCapabilitiesKHR		capabilities;
	uint32_t				format_cnt, present_mode_cnt;
	VkSurfaceFormatKHR			*formats;
	VkPresentModeKHR			*present_modes;
} swap_chain_details;

typedef struct {
	VkSurfaceFormatKHR			format;
	VkPresentModeKHR			present_mode;
	VkExtent2D				extent;
} swap_chain_settings;

typedef struct {
	VkImage					*imgs;
	VkImageView				*img_views;
	uint32_t				img_cnt;
} swap_chain_imgs;

typedef struct {
	VkShaderModule				vert, frag;
} shader_modules;

GLFWwindow					*wnd;

static VkInstance				inst;
static VkSurfaceKHR				surface;
static VkPhysicalDevice				phys_dev;
static VkDevice					dev;
static queue_fam_inds				qf_inds;
static vulkan_queues				queues;
static swap_chain_details			sc_dets;
static swap_chain_settings			sc_settings;
static VkSwapchainKHR				swap_chain;
static swap_chain_imgs				sc_imgs;
static shader_modules				shader_mods;
static VkRenderPass				render_pass;
static VkPipelineLayout				pipeline_layout;

const char *req_exts[] = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

static inline void query_swap_chain_details(void)
{
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys_dev, surface, &sc_dets.capabilities);

	vkGetPhysicalDeviceSurfaceFormatsKHR(phys_dev, surface, &sc_dets.format_cnt, NULL);

	if (sc_dets.format_cnt == 0)
		dbg_error("failed to find surface formats");

	sc_dets.formats = malloc(sc_dets.format_cnt * sizeof(VkSurfaceFormatKHR));

	vkGetPhysicalDeviceSurfaceFormatsKHR(phys_dev, surface, &sc_dets.format_cnt, sc_dets.formats);

	vkGetPhysicalDeviceSurfacePresentModesKHR(phys_dev, surface, &sc_dets.present_mode_cnt, NULL);

	if (sc_dets.present_mode_cnt == 0)
		dbg_error("failed to find present modes");

	sc_dets.present_modes = malloc(sc_dets.format_cnt * sizeof(VkPresentModeKHR));

	vkGetPhysicalDeviceSurfacePresentModesKHR(phys_dev, surface, &sc_dets.present_mode_cnt, sc_dets.present_modes);

	dbg_log("queried swap chain details successfully");
}

static inline void select_swap_chain_settings(void)
{
	uint32_t				width, height;

	sc_settings.format = sc_dets.formats[0];

	for (uint32_t i = 0; i < sc_dets.format_cnt; i++) {
		if (sc_dets.formats[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
			sc_dets.formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			sc_settings.format = sc_dets.formats[i];
	}

	sc_settings.present_mode = VK_PRESENT_MODE_FIFO_KHR;

	for (uint32_t i = 0; i < sc_dets.present_mode_cnt; i++) {
		if (sc_dets.present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
			sc_settings.present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
	}
	
	if (sc_dets.capabilities.currentExtent.width != UINT32_MAX)
		sc_settings.extent = sc_dets.capabilities.currentExtent;
	else {
		glfwGetFramebufferSize(wnd, &width, &height);
		
		sc_settings.extent = (VkExtent2D){
			clamp_uint(width, sc_dets.capabilities.minImageExtent.width, sc_dets.capabilities.maxImageExtent.width),
			clamp_uint(height, sc_dets.capabilities.minImageExtent.height, sc_dets.capabilities.maxImageExtent.height)
		};
	}

	dbg_log("selected swap chain settings successfully");
}

static inline bool qf_inds_complete(queue_fam_inds *qf_inds)
{
	return qf_inds->gfx != -1 && qf_inds->present != -1;
}

static inline void define_queue_infos(VkDeviceQueueCreateInfo *queue_infos, float *priority)
{
	queue_infos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queue_infos[0].queueFamilyIndex = qf_inds.gfx;
	queue_infos[0].queueCount = 1;
	queue_infos[0].pQueuePriorities = priority;

	queue_infos[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queue_infos[1].queueFamilyIndex = qf_inds.present;
	queue_infos[1].queueCount = 1;
	queue_infos[1].pQueuePriorities = priority;
}

static inline bool phys_dev_ext_support(VkPhysicalDevice phys_dev)
{
	uint32_t				ext_cnt, supported_ext_cnt;
	VkExtensionProperties			*avl_exts;
	
	vkEnumerateDeviceExtensionProperties(phys_dev, NULL, &ext_cnt, NULL);
		
	avl_exts = malloc(ext_cnt * sizeof(VkExtensionProperties));
	
	vkEnumerateDeviceExtensionProperties(phys_dev, NULL, &ext_cnt, avl_exts);

	supported_ext_cnt = 0;

	for (uint32_t i = 0; i < ext_cnt; i++) {
		for (uint32_t j = 0; j < ARRAY_SIZE(req_exts); j++) {
			if (strcmp(avl_exts[i].extensionName, req_exts[j]) == 0)
				supported_ext_cnt++;
		}
	}

	free(avl_exts);

	return supported_ext_cnt == ARRAY_SIZE(req_exts);
}

static inline uint32_t phys_dev_rate(VkPhysicalDevice phys_dev)
{
	uint32_t				score;

	VkPhysicalDeviceProperties phys_dev_props;
	vkGetPhysicalDeviceProperties(phys_dev, &phys_dev_props);

	VkPhysicalDeviceFeatures phys_dev_feats;
	vkGetPhysicalDeviceFeatures(phys_dev, &phys_dev_feats);

	if (!phys_dev_ext_support(phys_dev))
		return 0;

	if (!phys_dev_feats.geometryShader)
		return 0;

	score = 0;

	if (phys_dev_props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		score += 1000;

	score += phys_dev_props.limits.maxImageDimension2D;

	return score;
}

static inline void init_glfw(uint32_t wnd_width, uint32_t wnd_height)
{
	glfwInit();
	
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	wnd = glfwCreateWindow(wnd_width, wnd_height, "come up with a name later", NULL, NULL);
}

static inline void create_inst(void)
{
	uint32_t				glfw_ext_cnt;
	char					**glfw_exts;
	VkApplicationInfo			app_info;
	VkInstanceCreateInfo			inst_info;

	memset(&app_info, '\0', sizeof(app_info));

	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pApplicationName = "tirimids engine game";
	app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.pEngineName = "tirimids engine";
	app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.apiVersion = VK_API_VERSION_1_0;
	
	glfw_exts = (char **)glfwGetRequiredInstanceExtensions(&glfw_ext_cnt);

	memset(&inst_info, '\0', sizeof(inst_info));

	inst_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	inst_info.pApplicationInfo = &app_info;
	inst_info.enabledExtensionCount = glfw_ext_cnt;
	inst_info.ppEnabledExtensionNames = (const char **)glfw_exts;
	inst_info.enabledLayerCount = 0;

	if (vkCreateInstance(&inst_info, NULL, &inst) != VK_SUCCESS)
		dbg_error("failed to create instance");

	dbg_log("created instance successfully");
}

static inline void create_surface(GLFWwindow *wnd)
{
	if (glfwCreateWindowSurface(inst, wnd, NULL, &surface) != VK_SUCCESS)
		dbg_error("failed to create surface");

	dbg_log("created surface successfully");
}

static inline void pick_phys_dev(void)
{
	uint32_t				phys_dev_cnt, cur_best_score;
	VkPhysicalDevice			*phys_devs;

	vkEnumeratePhysicalDevices(inst, &phys_dev_cnt, NULL);

	phys_devs = malloc(phys_dev_cnt * sizeof(VkPhysicalDevice));

	vkEnumeratePhysicalDevices(inst, &phys_dev_cnt, phys_devs);

	cur_best_score = 0;

	for (uint32_t i = 0; i < phys_dev_cnt; i++) {
		if (phys_dev_rate(phys_devs[i]) > cur_best_score) {
			cur_best_score = phys_dev_rate(phys_devs[i]);
			phys_dev = phys_devs[i];
		}
	}

	free(phys_devs);

	if (cur_best_score == 0)
		dbg_error("no suitable physical devices found");

	dbg_log("picked a physical device successfully");
}

static inline void find_queue_fams(void)
{
	uint32_t				queue_fam_cnt;
	VkQueueFamilyProperties			*queue_fams;
	VkBool32				present_support;

	qf_inds.gfx = -1;
	qf_inds.present = -1;

	queue_fam_cnt = 0;

	vkGetPhysicalDeviceQueueFamilyProperties(phys_dev, &queue_fam_cnt, NULL);
	
	queue_fams = malloc(queue_fam_cnt * sizeof(VkQueueFamilyProperties));

	vkGetPhysicalDeviceQueueFamilyProperties(phys_dev, &queue_fam_cnt, queue_fams);

	for (uint32_t i = 0; i < queue_fam_cnt; i++) {
		if (queue_fams[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			qf_inds.gfx = i;

		present_support = false;

		vkGetPhysicalDeviceSurfaceSupportKHR(phys_dev, i, surface, &present_support);

		if (present_support)
			qf_inds.present = i;

		if (qf_inds_complete(&qf_inds))
			break;
	}

	free(queue_fams);

	if (!qf_inds_complete(&qf_inds))
		dbg_error("failed to find queue families");

	dbg_log("found queue families successfully");
}

static inline void create_dev(void)
{
	VkPhysicalDeviceFeatures		dev_feats;
	VkDeviceQueueCreateInfo			queue_infos[2];
	VkDeviceCreateInfo			dev_info;

	memset(&dev_feats, '\0', sizeof(dev_feats));
	memset(queue_infos, '\0', sizeof(queue_infos));

	queues.priority = 1.0f;

	define_queue_infos(queue_infos, &queues.priority);

	memset(&dev_info, '\0', sizeof(dev_info));

	dev_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	dev_info.pQueueCreateInfos = queue_infos;
	dev_info.queueCreateInfoCount = ARRAY_SIZE(queue_infos);
	dev_info.pEnabledFeatures = &dev_feats;
	dev_info.enabledExtensionCount = ARRAY_SIZE(req_exts);
	dev_info.ppEnabledExtensionNames = req_exts;
	dev_info.enabledLayerCount = 0;

	if (vkCreateDevice(phys_dev, &dev_info, NULL, &dev) != VK_SUCCESS)
		dbg_error("failed to create device");

	vkGetDeviceQueue(dev, qf_inds.gfx, 0, &queues.gfx);
	vkGetDeviceQueue(dev, qf_inds.present, 0, &queues.present);

	dbg_log("created device successfully");
}

static inline void create_swap_chain(void)
{
	VkSwapchainCreateInfoKHR		sc_info;
	uint32_t				img_cnt, qf_ind_arr[2];

	img_cnt = sc_dets.capabilities.minImageCount + 1;

	if (sc_dets.capabilities.maxImageCount > 0 && img_cnt > sc_dets.capabilities.maxImageCount)
		img_cnt = sc_dets.capabilities.maxImageCount;

	memset(&sc_info, '\0', sizeof(sc_info));

	sc_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	sc_info.surface = surface;
	sc_info.minImageCount = img_cnt;
	sc_info.imageFormat = sc_settings.format.format;
	sc_info.imageColorSpace = sc_settings.format.colorSpace;
	sc_info.imageExtent = sc_settings.extent;
	sc_info.imageArrayLayers = 1;
	sc_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	qf_ind_arr[0] = qf_inds.gfx;
	qf_ind_arr[1] = qf_inds.present;

	if (qf_inds.gfx != qf_inds.present) {
		sc_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		sc_info.queueFamilyIndexCount = 2;
		sc_info.pQueueFamilyIndices = qf_ind_arr;
	} else {
		sc_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		sc_info.queueFamilyIndexCount = 0;
		sc_info.pQueueFamilyIndices = NULL;
	}

	sc_info.preTransform = sc_dets.capabilities.currentTransform;
	sc_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	sc_info.presentMode = sc_settings.present_mode;
	sc_info.clipped = VK_TRUE;
	sc_info.oldSwapchain = VK_NULL_HANDLE;

	if (vkCreateSwapchainKHR(dev, &sc_info, NULL, &swap_chain) != VK_SUCCESS)
		dbg_error("failed to create swap chain");

	vkGetSwapchainImagesKHR(dev, swap_chain, &sc_imgs.img_cnt, NULL);

	sc_imgs.imgs = malloc(sc_imgs.img_cnt * sizeof(VkImage));

	vkGetSwapchainImagesKHR(dev, swap_chain, &sc_imgs.img_cnt, sc_imgs.imgs);

	dbg_log("created swap chain successfully");
}

static inline void create_img_views(void)
{
	VkImageViewCreateInfo			img_view_info;

	sc_imgs.img_views = malloc(sc_imgs.img_cnt * sizeof(VkImage));

	for (uint32_t i = 0; i < sc_imgs.img_cnt; i++) {
		memset(&img_view_info, '\0', sizeof(img_view_info));

		img_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		img_view_info.image = sc_imgs.imgs[i];
		img_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		img_view_info.format = sc_settings.format.format;
		img_view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		img_view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		img_view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		img_view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		img_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		img_view_info.subresourceRange.baseMipLevel = 0;
		img_view_info.subresourceRange.levelCount = 1;
		img_view_info.subresourceRange.baseArrayLayer = 0;
		img_view_info.subresourceRange.layerCount = 1;

		if (vkCreateImageView(dev, &img_view_info, NULL, &sc_imgs.img_views[i]) != VK_SUCCESS)
			dbg_error("failed to create image views");
	}

	dbg_log("created image views successfully");
}

static inline void create_render_pass(void)
{
	VkAttachmentDescription			color_att;
	VkAttachmentReference			color_att_ref;
	VkSubpassDescription			subpass;
	
	memset(&color_att, '\0', sizeof(color_att));

	color_att.format = sc_settings.format.format;
	color_att.samples = VK_SAMPLE_COUNT_1_BIT;
	color_att.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	color_att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	color_att.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	color_att.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	color_att.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	memset(&color_att_ref, '\0', sizeof(color_att_ref));

	color_att_ref.attachment = 0;
	color_att_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	memset(&subpass, '\0', sizeof(subpass));

	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_att_ref;
}

static inline void create_shader_mods(void)
{
	char					*vert_shader_buf, *frag_shader_buf;
	size_t					vert_shader_len, frag_shader_len;
	VkShaderModuleCreateInfo		vert_info, frag_info;

	vert_shader_len = get_file_len("build/shaders/vert.spv");
	vert_shader_buf = malloc(vert_shader_len);

	vert_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	vert_info.codeSize = vert_shader_len;
	vert_info.pCode = (uint32_t *)vert_shader_buf;

	if (vkCreateShaderModule(dev, &vert_info, NULL, &shader_mods.vert) != VK_SUCCESS)
		dbg_error("failed to create vertex shader module");

	frag_shader_len = get_file_len("build/shaders/frag.spv");
	frag_shader_buf = malloc(frag_shader_len);

	frag_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	frag_info.codeSize = frag_shader_len;
	frag_info.pCode = (uint32_t *)frag_shader_buf;

	if (vkCreateShaderModule(dev, &frag_info, NULL, &shader_mods.frag) != VK_SUCCESS)
		dbg_error("failed to create fragment shader module");

	free(vert_shader_buf);
	free(frag_shader_buf);

	dbg_log("created shader modules successfully");
}

static inline void create_pipeline(void)
{
	VkPipelineShaderStageCreateInfo		vert_stage_info, frag_stage_info, shader_stage_infos[2];
	VkPipelineVertexInputStateCreateInfo	vert_input_info;
	VkPipelineInputAssemblyStateCreateInfo	ia_info;
	VkViewport				viewport;
	VkRect2D				vp_scissor;
	VkPipelineViewportStateCreateInfo	viewport_info;
	VkPipelineRasterizationStateCreateInfo	rast_info;
	VkPipelineMultisampleStateCreateInfo	ms_info;
	VkPipelineColorBlendAttachmentState	cba_info;
	VkPipelineColorBlendStateCreateInfo	blend_info;
	VkPipelineDynamicStateCreateInfo	ds_info;
	VkDynamicState				dynam_states[2];
	VkPipelineLayoutCreateInfo		pl_info;

	create_shader_mods();

	memset(&vert_stage_info, '\0', sizeof(vert_stage_info));

	vert_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vert_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vert_stage_info.module = shader_mods.vert;
	vert_stage_info.pName = "main";

	memset(&frag_stage_info, '\0', sizeof(frag_stage_info));

	frag_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	frag_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	frag_stage_info.module = shader_mods.frag;
	frag_stage_info.pName = "main";

	shader_stage_infos[0] = vert_stage_info;
	shader_stage_infos[1] = frag_stage_info;

	memset(&vert_input_info, '\0', sizeof(vert_input_info));

	vert_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vert_input_info.vertexBindingDescriptionCount = 0;
	vert_input_info.pVertexBindingDescriptions = NULL;
	vert_input_info.vertexAttributeDescriptionCount = 0;
	vert_input_info.pVertexAttributeDescriptions = NULL;

	memset(&ia_info, '\0', sizeof(ia_info));

	ia_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	ia_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	ia_info.primitiveRestartEnable = VK_FALSE;

	memset(&viewport, '\0', sizeof(viewport));

	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)sc_settings.extent.width;
	viewport.height = (float)sc_settings.extent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	memset(&vp_scissor, '\0', sizeof(vp_scissor));

	vp_scissor.offset = (VkOffset2D){
		0,
		0
	};

	vp_scissor.extent = sc_settings.extent;

	memset(&viewport_info, '\0', sizeof(viewport_info));

	viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_info.viewportCount = 1;
	viewport_info.pViewports = &viewport;
	viewport_info.scissorCount = 1;
	viewport_info.pScissors = &vp_scissor;

	memset(&rast_info, '\0', sizeof(rast_info));

	rast_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rast_info.depthClampEnable = VK_FALSE;
	rast_info.rasterizerDiscardEnable = VK_FALSE;
	rast_info.polygonMode = VK_POLYGON_MODE_FILL;
	rast_info.lineWidth = 1.0f;
	rast_info.cullMode = VK_CULL_MODE_BACK_BIT;
	rast_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rast_info.depthBiasEnable = VK_FALSE;
	rast_info.depthBiasConstantFactor = 0.0f;
	rast_info.depthBiasClamp = 0.0f;
	rast_info.depthBiasSlopeFactor = 0.0f;

	memset(&ms_info, '\0', sizeof(ms_info));

	ms_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	ms_info.sampleShadingEnable = VK_FALSE;
	ms_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	ms_info.minSampleShading = 1.0f;
	ms_info.pSampleMask = NULL;
	ms_info.alphaToCoverageEnable = VK_FALSE;
	ms_info.alphaToOneEnable = VK_FALSE;

	memset(&cba_info, '\0', sizeof(cba_info));

	cba_info.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT |
		VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT;

	cba_info.blendEnable = VK_FALSE;
	cba_info.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	cba_info.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	cba_info.colorBlendOp = VK_BLEND_OP_ADD;
	cba_info.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	cba_info.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	cba_info.alphaBlendOp = VK_BLEND_OP_ADD;

	memset(&blend_info, '\0', sizeof(blend_info));

	blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend_info.logicOpEnable = VK_FALSE;
	blend_info.logicOp = VK_LOGIC_OP_COPY;
	blend_info.attachmentCount = 1;
	blend_info.pAttachments = &cba_info;
	blend_info.blendConstants[0] = 0.0f;
	blend_info.blendConstants[1] = 0.0f;
	blend_info.blendConstants[2] = 0.0f;
	blend_info.blendConstants[3] = 0.0f;

	dynam_states[0] = VK_DYNAMIC_STATE_VIEWPORT;
	dynam_states[1] = VK_DYNAMIC_STATE_LINE_WIDTH;

	memset(&ds_info, '\0', sizeof(ds_info));

	ds_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	ds_info.dynamicStateCount = 2;
	ds_info.pDynamicStates = dynam_states;

	memset(&pl_info, '\0', sizeof(pipeline_layout));
	
	pl_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pl_info.setLayoutCount = 0;
	pl_info.pSetLayouts = NULL;
	pl_info.pushConstantRangeCount = 0;
	pl_info.pPushConstantRanges = NULL;

	if (vkCreatePipelineLayout(dev, &pl_info, NULL, &pipeline_layout) != VK_SUCCESS)
		dbg_error("failed to create pipeline");

	dbg_log("created pipeline successfully");
}

void vk_init(void)
{
	init_glfw(800, 600);
	create_inst();
	create_surface(wnd);
	pick_phys_dev();
	find_queue_fams();
	create_dev();
	query_swap_chain_details();
	select_swap_chain_settings();
	create_swap_chain();
	create_img_views();
	create_pipeline();

	dbg_log("initialized vulkan successfully");
}

void vk_clean(void)
{
	vkDestroyPipelineLayout(dev, pipeline_layout, NULL);

	vkDestroyShaderModule(dev, shader_mods.vert, NULL);
	vkDestroyShaderModule(dev, shader_mods.frag, NULL);

	for (uint32_t i = 0; i < sc_imgs.img_cnt; i++)
		vkDestroyImageView(dev, sc_imgs.img_views[i], NULL);

	free(sc_imgs.img_views);
	free(sc_imgs.imgs);

	free(sc_dets.formats);
	free(sc_dets.present_modes);

	vkDestroySwapchainKHR(dev, swap_chain, NULL);
	vkDestroyDevice(dev, NULL);
	vkDestroySurfaceKHR(inst, surface, NULL);
	vkDestroyInstance(inst, NULL);

	glfwDestroyWindow(wnd);
	glfwTerminate();

	dbg_log("cleaned vulkan successfully");
}