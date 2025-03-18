#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstring>
#include <cstdlib>
#include<memory>
#include<mutex>
#include<string>
#include<limits>
#include<functional>
#include<optional>
#include<unordered_set>
#include<algorithm>
#include<filesystem>
#include<fstream>
#include<sstream>
#include<array>
#include<cstddef>

#include "vulkan/vulkan.h"

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

#include "glm/glm.hpp"

using namespace std;

inline const char* vkResultToString(VkResult result) {
	switch (result) {
	case VK_SUCCESS: return "VK_SUCCESS";
	case VK_NOT_READY: return "VK_NOT_READY";
	case VK_TIMEOUT: return "VK_TIMEOUT";
	case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
	default: return "UNKNOWN_ERROR";
	}
}

#define THROW_IF_VK_FAILED(VK_CALL)                                       \
    do {                                                                      \
        VkResult __result = (VK_CALL);                                        \
        if (__result != VK_SUCCESS) {                                         \
            std::ostringstream __oss;                                         \
            __oss << "Vulkan error in " << __FILE__                           \
                  << "(" << __LINE__ << "): " << #VK_CALL                     \
                  << "\n  Code: " << vkResultToString(__result)				  \
                  << " (" << static_cast<int>(__result) << ")";               \
            throw std::runtime_error(__oss.str());                           \
        }                                                                     \
    } while (0)

constexpr int WIDTH{ 800 };
constexpr int HEIGHT{ 600 };

constexpr int MAX_FRAMES_IN_FLIGHT = 2;

const constexpr char* validationLayers{ "VK_LAYER_KHRONOS_validation" };

const constexpr char* Device_EXT_SwapChain{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };

const constexpr char* Vertex_Shader_File_Path{ "shaders/vshader.spv" };
const constexpr char* Fragment_Shader_File_Path{ "shaders/fshader.spv" };

struct Vertex final {
	glm::vec2 Pos;
	glm::vec3 Color;

	static const VkVertexInputBindingDescription Get_Binding_Description(void) {
		VkVertexInputBindingDescription Binding_Description{};
		{
			Binding_Description.binding = 0;
			Binding_Description.stride = sizeof(Vertex);
			Binding_Description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		}

		return Binding_Description;
	}

	static const std::array<VkVertexInputAttributeDescription, 2> Get_Attribute_Descriptions(void) {
		std::array<VkVertexInputAttributeDescription, 2> Attribute_Descriptions{};
		{
			Attribute_Descriptions[0].binding = 0;
			Attribute_Descriptions[0].location = 0;
			Attribute_Descriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
			Attribute_Descriptions[0].offset = offsetof(Vertex, Pos);
		}
		{
			Attribute_Descriptions[1].binding = 0;
			Attribute_Descriptions[1].location = 1;
			Attribute_Descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
			Attribute_Descriptions[1].offset = offsetof(Vertex, Color);
		}

		return Attribute_Descriptions;
	}
};

const std::vector<Vertex> Vertices = {
	{{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
	{{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
	{{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}
};

static const std::vector<char> Read_File(const std::filesystem::path& File_Path) {
	ifstream File{ File_Path, ios::ate | ios::binary };

	if (!File.is_open())
		throw runtime_error("Failed to open file!");

	const size_t File_Size{ static_cast<size_t>(File.tellg()) };
	vector<char> Buffer(File_Size);

	File.seekg(0);
	File.read(Buffer.data(), File_Size);

	File.close();

	return Buffer;
}

class VK_Application final {
private:
	struct Queue_Family_Indices final {
		uint32_t Graphics_Family;
		uint32_t Present_Family;
	};

	struct Swap_Chain_Support_Details final {
		VkSurfaceCapabilitiesKHR Capabilities{};
		vector<VkSurfaceFormatKHR> Formats{};
		vector<VkPresentModeKHR> Present_Modes{};
	};

public:
#ifdef _DEBUG
	vector<const char*> m_Validation_Layer_List{};

	VkDebugUtilsMessengerEXT m_Debug_Messenger{};

	VkDebugUtilsMessengerCreateInfoEXT m_VkDebugUtilsMessengerCreateInfoEXT{};

	//TODO : TO Static Function
	bool Check_Vaildation_Layer_Support(void) {
		uint32_t Layer_Count;
		THROW_IF_VK_FAILED(vkEnumerateInstanceLayerProperties(&Layer_Count, nullptr));

		vector<VkLayerProperties> Available_Layers;
		Available_Layers.resize(Layer_Count);
		THROW_IF_VK_FAILED(vkEnumerateInstanceLayerProperties(&Layer_Count, Available_Layers.data()));

		for (const auto& Layer_Name : this->m_Validation_Layer_List) {
			bool layer_Is_Found = false;

			for (const auto& Layer_Propery : Available_Layers)
				if (string{ Layer_Name } == string{ Layer_Propery.layerName }) {
					layer_Is_Found = true;
					break;
				}
			if (!layer_Is_Found)
				return false;
		}

		return true;
	}

	static VKAPI_ATTR VkBool32 VKAPI_CALL Debug_Callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
		cerr << "Validation layer: " << pCallbackData->pMessage << endl;

		//NOTE : This Return Common Is VK_FALSE, We Should Not  Debug Failedtaion Layer Just it's Warning
		return VK_FALSE;
	}

	const void Build_Debug_Messenger_Create_Info(void) {
		this->m_VkDebugUtilsMessengerCreateInfoEXT.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

		this->m_VkDebugUtilsMessengerCreateInfoEXT.messageSeverity =
			//VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

		this->m_VkDebugUtilsMessengerCreateInfoEXT.messageType =
			VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

		this->m_VkDebugUtilsMessengerCreateInfoEXT.pfnUserCallback = VK_Application::Debug_Callback;

		this->m_VkDebugUtilsMessengerCreateInfoEXT.pUserData = nullptr;
	}

	static const VkResult Create_DebugUtils_Messenger_EXT(VkInstance Instance, const VkDebugUtilsMessengerCreateInfoEXT* Create_Info, const VkAllocationCallbacks* Allocator, VkDebugUtilsMessengerEXT* m_Debug_Messenger) {
		auto Func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(Instance, "vkCreateDebugUtilsMessengerEXT"));
		if (nullptr != Func)
			return Func(Instance, Create_Info, Allocator, m_Debug_Messenger);
		else
			return VK_ERROR_EXTENSION_NOT_PRESENT;
	}

	static void Destroy_DebugUtils_Messenger_EXT(VkInstance Instance, VkDebugUtilsMessengerEXT m_Debug_Messenger, const VkAllocationCallbacks* Allocator) {
		auto Func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(Instance, "vkDestroyDebugUtilsMessengerEXT"));
		if (nullptr != Func)
			Func(Instance, m_Debug_Messenger, Allocator);
	}

	void Set_Debug_Messenger() {
		THROW_IF_VK_FAILED(VK_Application::Create_DebugUtils_Messenger_EXT(
			this->m_VK_Instance.get(),
			&this->m_VkDebugUtilsMessengerCreateInfoEXT,
			nullptr,
			&this->m_Debug_Messenger)
		);
	}
#endif // _DEBUG

public:
	VK_Application(void) = default;

	~VK_Application(void) {
		this->CleanUp();
	}

	void Run(void) {
		this->Initialize();
		this->Main_Loop();
	}

private:
	void Initialize(void) {
		Init_Window();
		Init_Vulkan();
	}

	void Init_Window(void) {
		static std::once_flag Initialized_GLWF_Flag{};

		std::call_once(Initialized_GLWF_Flag, VK_Application::Initialize_GLWF);

		GLFWwindow* window{ glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr) };
		if (nullptr == window)
			throw std::runtime_error("Failed to create GLFW window");
		this->m_Window.reset(window);

		glfwSetWindowUserPointer(this->m_Window.get(), this);
		glfwSetFramebufferSizeCallback(this->m_Window.get(), VK_Application::Frame_Buffer_Resize_CallBack);
	}

	void Init_Vulkan(void) {
#ifdef _DEBUG
		this->Build_Debug_Messenger_Create_Info();
#endif // _DEBUG

		this->Create_Instance();

#ifdef _DEBUG
		this->Set_Debug_Messenger();
#endif // _DEBUG

		this->Create_Surface();
		this->Pick_Physical_Device();
		this->Create_Logical_Device();
		this->Create_SwapChain();
		this->Create_SwapChhain_Image_Views();
		this->Create_Render_Pass();
		this->Create_GraphicsPipeline();
		this->Create_Frame_Buffers();
		this->Create_Command_Pool();
		this->Create_Vertex_Buffer();
		this->Create_Command_Buffers();
		this->Create_Sync_Objects();
	}

	void Main_Loop(void) {
		while (!glfwWindowShouldClose(this->m_Window.get())) {
			glfwPollEvents();

			this->Draw_Frame();
		}
		THROW_IF_VK_FAILED(vkDeviceWaitIdle(this->m_Logical_Device.get()));
	}

	void CleanUp_SwapChain(void) {
		for (auto& Framebuffer : this->m_Swap_Chain_Frame_buffers) {
			//vkDestroyFramebuffer(this->m_Logical_Device.get(), Framebuffer.get(), nullptr);
			Framebuffer.reset();
		}

		for (auto& Image_View : this->m_Swap_Chain_Image_Views) {
			//vkDestroyImageView(this->m_Logical_Device.get(), Image_View.get(), nullptr);

			Image_View.reset();
		}

		this->m_Swap_Chain.reset();
	}

	void CleanUp(void) {
		//NOTE : Clean Up Logical Device Before Instance and ,First Clean Command Queue Before Logical Device

		this->CleanUp_SwapChain();


		for (size_t Index = 0; Index < MAX_FRAMES_IN_FLIGHT; ++Index) {
			//vkDestroySemaphore(this->m_Logical_Device.get(), this->m_Render_Finished_Semaphores[Index].get(), nullptr);
			this->m_Render_Finished_Semaphores[Index].reset();

			//vkDestroySemaphore(this->m_Logical_Device.get(), this->m_Image_Available_Semaphores[Index].get(), nullptr);
			this->m_Image_Available_Semaphores[Index].reset();

			//vkDestroyFence(this->m_Logical_Device.get(), this->m_InFlight_Fences[Index].get(), nullptr);
			this->m_InFlight_Fences[Index].reset();
		}

		//vkDestroyBuffer(this->m_Logical_Device.get(), this->m_Vertex_Buffer.get(), nullptr);
		this->m_Vertex_Buffer.reset();

		//vkFreeMemory(this->m_Logical_Device.get(), this->m_Vertex_Buffer_Memory.get(), nullptr);
		this->m_Vertex_Buffer_Memory.reset();

		//vkDestroyCommandPool(this->m_Logical_Device.get(), this->m_Command_Pool.get(), nullptr);
		this->m_Command_Pool.reset();

		//NOTE : In Clean Up Swap Chain Func
		//for (auto& Framebuffer : this->m_Swap_Chain_Frame_buffers) {
		//	//vkDestroyFramebuffer(this->m_Logical_Device.get(), Framebuffer.get(), nullptr);
		//	Framebuffer.reset();
		//}

		//vkDestroyPipeline(this->m_Logical_Device.get(), this->m_Graphics_Pipeline.get(), nullptr);
		this->m_Graphics_Pipeline.reset();


		//vkDestroyPipelineLayout(this->m_Logical_Device.get(), this->m_Pipeline_Layout.get(), nullptr);
		this->m_Pipeline_Layout.reset();

		//vkDestroyRenderPass(this->m_Logical_Device.get(), this->m_Render_Pass.get(), nullptr);
		this->m_Render_Pass.reset();

		//NOTE : In Clean Up Swap Chain Func
		//for (auto& Image_View : this->m_Swap_Chain_Image_Views) {
		//	//vkDestroyImageView(this->m_Logical_Device.get(), Image_View.get(), nullptr);

		//	Image_View.reset();
		//}

		// NOTE : In Clean Up Swap Chain Func
		////vkDestroySwapchainKHR(this->m_Logical_Device.get(), this->m_Swap_Chain.get(), nullptr);
		//this->m_Swap_Chain.reset();

		//vkDestroyDevice(this->m_Logical_Device.get(), nullptr);
		this->m_Logical_Device.reset();

#ifdef _DEBUG
		VK_Application::Destroy_DebugUtils_Messenger_EXT(this->m_VK_Instance.get(), this->m_Debug_Messenger, nullptr);
#endif // _DEBUG

		//vkDestroySurfaceKHR(this->m_VK_Instance.get(), this->m_Surface.get(), nullptr);
		this->m_Surface.reset();

		//vkDestroyInstance(m_VK_Instance, nullptr);
		this->m_VK_Instance.reset();
		//glfwDestroyWindow(this->m_Window.get());

		//glfwDestroyWindow(this->m_Window.get());
		this->m_Window.reset();

		glfwTerminate();
	}

	void Re_Create_SwapChain(void) {
		int Width{ 0 }, Height{ 0 };
		glfwGetFramebufferSize(this->m_Window.get(), &Width, &Height);
		while (Width == 0 || Height == 0) {
			glfwGetFramebufferSize(this->m_Window.get(), &Width, &Height);
			glfwWaitEvents();
		}

		THROW_IF_VK_FAILED(vkDeviceWaitIdle(this->m_Logical_Device.get()));

		this->CleanUp_SwapChain();

		this->Create_SwapChain();
		this->Create_SwapChhain_Image_Views();
		this->Create_Frame_Buffers();
	}

private:
	void Create_Instance(void) {
		VkApplicationInfo VK_Application_Info{};
		{
			VK_Application_Info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
			VK_Application_Info.pApplicationName = "Hello Triangle";
			VK_Application_Info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
			VK_Application_Info.pEngineName = "No Engine";
			VK_Application_Info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
			VK_Application_Info.apiVersion = VK_API_VERSION_1_0;
		}

		const auto& Extensions = VK_Application::Get_Require_Extensions();
		VkInstanceCreateInfo Instance_Create_Info = {};
		{
			Instance_Create_Info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

			Instance_Create_Info.pApplicationInfo = &VK_Application_Info;

#ifdef _DEBUG
			Instance_Create_Info.pNext = reinterpret_cast<const void*>(&this->m_VkDebugUtilsMessengerCreateInfoEXT);
			Instance_Create_Info.enabledLayerCount = static_cast<uint32_t>(this->m_Validation_Layer_List.size());
			Instance_Create_Info.ppEnabledLayerNames = this->m_Validation_Layer_List.data();
#else
			Instance_Create_Info.pNext = nullptr;
			Instance_Create_Info.enabledLayerCount = 0;
			Instance_Create_Info.ppEnabledLayerNames = nullptr;
#endif // _DEBUG
			Instance_Create_Info.enabledExtensionCount = static_cast<uint32_t>(Extensions.size());
			Instance_Create_Info.ppEnabledExtensionNames = Extensions.data();
		}

		VkInstance VK_Instance{ nullptr };
		THROW_IF_VK_FAILED(vkCreateInstance(&Instance_Create_Info, nullptr, &VK_Instance));
		this->m_VK_Instance.reset(VK_Instance);
	}

	void Create_Surface(void) {
		VkSurfaceKHR Surface{ nullptr };
		THROW_IF_VK_FAILED(glfwCreateWindowSurface(this->m_VK_Instance.get(), this->m_Window.get(), nullptr, &Surface));


		this->m_Surface.get_deleter() = [Instance = this->m_VK_Instance.get()](VkSurfaceKHR Surface) {vkDestroySurfaceKHR(Instance, Surface, nullptr); };
		this->m_Surface.reset(Surface);
	}

	void Pick_Physical_Device(void) {
		uint32_t Device_Count{ 0 };
		THROW_IF_VK_FAILED(vkEnumeratePhysicalDevices(this->m_VK_Instance.get(), &Device_Count, nullptr));

		if (0 == Device_Count)
			throw runtime_error("Failed to find GPUs with Vulkan support!");

		vector<VkPhysicalDevice> Devices{};
		Devices.resize(Device_Count);
		THROW_IF_VK_FAILED(vkEnumeratePhysicalDevices(this->m_VK_Instance.get(), &Device_Count, Devices.data()));
		for (const auto& Device : Devices)
			if (Is_Device_Suitable(Device)) {
				this->m_Physical_Device = Device;
				break;
			}

		if (nullptr == this->m_Physical_Device)
			throw runtime_error("Failed to find a suitable GPU!");
	}

	void Create_Logical_Device(void) {
		this->m_Queue_Family_Indices.Graphics_Family =
			this->Find_Queue_Families(VK_QUEUE_GRAPHICS_BIT);

		if (numeric_limits<uint32_t>::max() == this->m_Queue_Family_Indices.Graphics_Family)
			throw runtime_error("Failed to find a queue family with graphics bit!");

		this->m_Queue_Family_Indices.Present_Family = this->Get_Physical_Device_Queue_Present_Family();

		if (numeric_limits<uint32_t>::max() == this->m_Queue_Family_Indices.Present_Family)
			throw runtime_error("Failed to find a queue family with present bit!");

		//NOTE : Queue Family Should Be Unique,Because It Maybe Same
		unordered_set<uint32_t> Unique_Queue_Families{
			this->m_Queue_Family_Indices.Graphics_Family,
			this->m_Queue_Family_Indices.Present_Family
		};

		//NOTE : Refence Continue From Create_Instance
		constexpr float Queue_Priority{ 1.0f };

		vector<VkDeviceQueueCreateInfo> Queue_Create_Info_List;
		Queue_Create_Info_List.reserve(Unique_Queue_Families.size());
		{
			for (const auto& Queue_Family : Unique_Queue_Families) {
				VkDeviceQueueCreateInfo Queue_Create_Info{};
				Queue_Create_Info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
				Queue_Create_Info.queueFamilyIndex = Queue_Family;
				Queue_Create_Info.queueCount = 1;
				Queue_Create_Info.pQueuePriorities = &Queue_Priority;
				Queue_Create_Info_List.emplace_back(Queue_Create_Info);
			}
		}

		VkPhysicalDeviceFeatures Device_Features{};
		VkDeviceCreateInfo Device_Create_Info{};
		{
			Device_Create_Info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
			Device_Create_Info.queueCreateInfoCount = static_cast<uint32_t>(Queue_Create_Info_List.size());
			Device_Create_Info.pQueueCreateInfos = Queue_Create_Info_List.data();
			Device_Create_Info.enabledExtensionCount = 1;
			Device_Create_Info.ppEnabledExtensionNames = &Device_EXT_SwapChain;
			Device_Create_Info.pEnabledFeatures = &Device_Features;
		}

		VkDevice Logical_Device{ nullptr };
		THROW_IF_VK_FAILED(vkCreateDevice(this->m_Physical_Device, &Device_Create_Info, nullptr, &Logical_Device));
		this->m_Logical_Device.reset(Logical_Device);

		vkGetDeviceQueue(this->m_Logical_Device.get(), this->m_Queue_Family_Indices.Graphics_Family, 0, &this->m_Graphics_Queue);

		vkGetDeviceQueue(this->m_Logical_Device.get(), this->m_Queue_Family_Indices.Present_Family, 0, &this->m_Present_Queue);

	}

	void Create_SwapChain(void) {
		this->Query_Swap_Chain_Support_Details();

		const VkSurfaceFormatKHR Surface_Format{ Choose_SwapChain_Surface_Format(this->m_Swap_Chain_Support_Details.Formats) };
		const VkPresentModeKHR Present_Mode{ Choose_SwapChain_Present_Mode(this->m_Swap_Chain_Support_Details.Present_Modes) };
		const VkExtent2D Swap_Chain_Extent{ Choose_SwapChain_Extent(this->m_Swap_Chain_Support_Details.Capabilities) };

		//NOTE : Choose Image Count ,We Want One More Image Than Min Image Count
		uint32_t Image_Count{ this->m_Swap_Chain_Support_Details.Capabilities.minImageCount + 1 };
		Image_Count = std::clamp(Image_Count, this->m_Swap_Chain_Support_Details.Capabilities.minImageCount, this->m_Swap_Chain_Support_Details.Capabilities.maxImageCount);

		//TODO : Why We Need Graphics And Present Family
		vector<uint32_t> Queue_Family_Indices{ this->m_Queue_Family_Indices.Graphics_Family,this->m_Queue_Family_Indices.Present_Family };
		VkSharingMode Sharing_Mode{ VK_SHARING_MODE_EXCLUSIVE };

		if (this->m_Queue_Family_Indices.Graphics_Family != this->m_Queue_Family_Indices.Present_Family)
			Sharing_Mode = VK_SHARING_MODE_CONCURRENT;

		VkSwapchainCreateInfoKHR Swap_Chain_Create_Info{};
		{
			Swap_Chain_Create_Info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
			Swap_Chain_Create_Info.surface = this->m_Surface.get();
			Swap_Chain_Create_Info.minImageCount = Image_Count;
			Swap_Chain_Create_Info.imageFormat = Surface_Format.format;
			Swap_Chain_Create_Info.imageColorSpace = Surface_Format.colorSpace;
			Swap_Chain_Create_Info.imageExtent = Swap_Chain_Extent;
			Swap_Chain_Create_Info.imageArrayLayers = 1;
			Swap_Chain_Create_Info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
			Swap_Chain_Create_Info.imageSharingMode = Sharing_Mode;
			Swap_Chain_Create_Info.queueFamilyIndexCount = static_cast<uint32_t>(Queue_Family_Indices.size());
			Swap_Chain_Create_Info.pQueueFamilyIndices = Queue_Family_Indices.data();
			Swap_Chain_Create_Info.preTransform = this->m_Swap_Chain_Support_Details.Capabilities.currentTransform;
			Swap_Chain_Create_Info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
			Swap_Chain_Create_Info.presentMode = Present_Mode;
			//NOTE : SHould Not Read Back Buffer
			Swap_Chain_Create_Info.clipped = VK_TRUE;
			Swap_Chain_Create_Info.oldSwapchain = VK_NULL_HANDLE;
		}

		VkSwapchainKHR Swap_Chain{ nullptr };
		THROW_IF_VK_FAILED(vkCreateSwapchainKHR(this->m_Logical_Device.get(), &Swap_Chain_Create_Info, nullptr, &Swap_Chain));

		this->m_Swap_Chain.get_deleter() = [Device = this->m_Logical_Device.get()](VkSwapchainKHR Swap_Chain) {vkDestroySwapchainKHR(Device, Swap_Chain, nullptr); };
		this->m_Swap_Chain.reset(Swap_Chain);

		THROW_IF_VK_FAILED(vkGetSwapchainImagesKHR(this->m_Logical_Device.get(), this->m_Swap_Chain.get(), &Image_Count, nullptr));
		this->m_Swap_Chain_Images.resize(Image_Count);
		THROW_IF_VK_FAILED(vkGetSwapchainImagesKHR(this->m_Logical_Device.get(), this->m_Swap_Chain.get(), &Image_Count, this->m_Swap_Chain_Images.data()));

		this->m_Swap_Chain_Image_Format = Surface_Format.format;
		this->m_Swap_Chain_Extent = Swap_Chain_Extent;
	}

	void Create_SwapChhain_Image_Views(void) {

		VkComponentMapping Component_Mapping{};
		{
			Component_Mapping.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			Component_Mapping.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			Component_Mapping.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			Component_Mapping.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		}

		VkImageSubresourceRange Subresource_Range{};
		{
			Subresource_Range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			Subresource_Range.baseMipLevel = 0;
			Subresource_Range.levelCount = 1;
			Subresource_Range.baseArrayLayer = 0;
			Subresource_Range.layerCount = 1;
		}

		this->m_Swap_Chain_Image_Views.resize(this->m_Swap_Chain_Images.size());
		for (size_t Index = 0; Index < this->m_Swap_Chain_Images.size(); ++Index) {
			VkImageViewCreateInfo Image_View_Create_Info{};
			{
				Image_View_Create_Info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
				Image_View_Create_Info.image = this->m_Swap_Chain_Images[Index];
				Image_View_Create_Info.viewType = VK_IMAGE_VIEW_TYPE_2D;
				Image_View_Create_Info.format = this->m_Swap_Chain_Image_Format;
				Image_View_Create_Info.components = Component_Mapping;
				Image_View_Create_Info.subresourceRange = Subresource_Range;
			}

			VkImageView Image_View{ nullptr };
			THROW_IF_VK_FAILED(vkCreateImageView(this->m_Logical_Device.get(), &Image_View_Create_Info, nullptr, &Image_View));

			this->m_Swap_Chain_Image_Views[Index].get_deleter() = [Device = this->m_Logical_Device.get()](VkImageView Image_View) {if (nullptr != Image_View) vkDestroyImageView(Device, Image_View, nullptr); };
			this->m_Swap_Chain_Image_Views[Index].reset(Image_View);
		}

	}

	void Create_Render_Pass(void) {
		VkAttachmentDescription Color_Attachment{};
		{
			Color_Attachment.format = this->m_Swap_Chain_Image_Format;
			Color_Attachment.samples = VK_SAMPLE_COUNT_1_BIT;
			Color_Attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			Color_Attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			Color_Attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			Color_Attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			Color_Attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			Color_Attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		}

		VkAttachmentReference Color_Attachment_Ref{};
		{
			Color_Attachment_Ref.attachment = 0;
			Color_Attachment_Ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}

		VkSubpassDescription Subpass{};
		{
			Subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			Subpass.colorAttachmentCount = 1;
			Subpass.pColorAttachments = &Color_Attachment_Ref;
		}

		VkSubpassDependency Dependency{};
		{
			Dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
			Dependency.dstSubpass = 0;
			Dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			Dependency.srcAccessMask = 0;
			Dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			Dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		}

		VkRenderPassCreateInfo Render_Pass_Create_Info{};
		{
			Render_Pass_Create_Info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			Render_Pass_Create_Info.attachmentCount = 1;
			Render_Pass_Create_Info.pAttachments = &Color_Attachment;
			Render_Pass_Create_Info.subpassCount = 1;
			Render_Pass_Create_Info.pSubpasses = &Subpass;
			Render_Pass_Create_Info.dependencyCount = 1;
			Render_Pass_Create_Info.pDependencies = &Dependency;
		}

		VkRenderPass Render_Pass{ nullptr };
		THROW_IF_VK_FAILED(vkCreateRenderPass(this->m_Logical_Device.get(), &Render_Pass_Create_Info, nullptr, &Render_Pass));

		this->m_Render_Pass.get_deleter() = [Device = this->m_Logical_Device.get()](VkRenderPass Render_Pass) {if (nullptr != Render_Pass) vkDestroyRenderPass(Device, Render_Pass, nullptr); };
		this->m_Render_Pass.reset(Render_Pass);
	}

	void Create_GraphicsPipeline(void) {
		const auto& Vertex_Shader_Code = Read_File(std::filesystem::path(Vertex_Shader_File_Path, std::filesystem::path::generic_format));
		const auto& Fragment_Shader_Code = Read_File(std::filesystem::path(Fragment_Shader_File_Path, std::filesystem::path::generic_format));

		const auto& Vertex_Shader_Module = Create_Shader_Module(Vertex_Shader_Code);
		const auto& Fragment_Shader_Module = Create_Shader_Module(Fragment_Shader_Code);

		VkPipelineShaderStageCreateInfo Vertex_Shader_Stage_Info{};
		{
			Vertex_Shader_Stage_Info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			Vertex_Shader_Stage_Info.stage = VK_SHADER_STAGE_VERTEX_BIT;
			Vertex_Shader_Stage_Info.module = Vertex_Shader_Module;
			Vertex_Shader_Stage_Info.pName = "main";
			Vertex_Shader_Stage_Info.pSpecializationInfo = nullptr;
		}

		VkPipelineShaderStageCreateInfo Fragment_Shader_Stage_Info{};
		{
			Fragment_Shader_Stage_Info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			Fragment_Shader_Stage_Info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
			Fragment_Shader_Stage_Info.module = Fragment_Shader_Module;
			Fragment_Shader_Stage_Info.pName = "main";
			Fragment_Shader_Stage_Info.pSpecializationInfo = nullptr;
		}

		const auto& Binding_Description = Vertex::Get_Binding_Description();

		const auto& Attribute_Descriptions = Vertex::Get_Attribute_Descriptions();

		VkPipelineVertexInputStateCreateInfo Vertex_Input_Info{};
		{
			Vertex_Input_Info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
			Vertex_Input_Info.vertexBindingDescriptionCount = 1;
			Vertex_Input_Info.pVertexBindingDescriptions = &Binding_Description;
			Vertex_Input_Info.vertexAttributeDescriptionCount = static_cast<uint32_t>(Attribute_Descriptions.size());
			Vertex_Input_Info.pVertexAttributeDescriptions = Attribute_Descriptions.data();
		}

		VkPipelineInputAssemblyStateCreateInfo Input_Assembly_Info{};
		{
			Input_Assembly_Info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
			Input_Assembly_Info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			Input_Assembly_Info.primitiveRestartEnable = VK_FALSE;
		}

		VkViewport Viewport{};
		{
			Viewport.x = 0.0f;
			Viewport.y = 0.0f;
			Viewport.width = static_cast<float>(this->m_Swap_Chain_Extent.width);
			Viewport.height = static_cast<float>(this->m_Swap_Chain_Extent.height);
			Viewport.minDepth = 0.0f;
			Viewport.maxDepth = 1.0f;
		}

		VkRect2D Scissor{};
		{
			Scissor.offset = { 0, 0 };
			Scissor.extent = this->m_Swap_Chain_Extent;
		}

		VkPipelineViewportStateCreateInfo Viewport_State_Info{};
		{
			//TODO : Ignore Some Field
			Viewport_State_Info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
			Viewport_State_Info.viewportCount = 1;
			//Viewport_State_Info.pViewports = &Viewport;
			Viewport_State_Info.scissorCount = 1;
			//Viewport_State_Info.pScissors = &Scissor;
		}

		VkPipelineRasterizationStateCreateInfo Rasterizer{};
		{
			Rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
			Rasterizer.depthClampEnable = VK_FALSE;
			Rasterizer.rasterizerDiscardEnable = VK_FALSE;
			Rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
			Rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
			Rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
			Rasterizer.depthBiasEnable = VK_FALSE;
			Rasterizer.depthBiasConstantFactor = 0.0f;
			Rasterizer.depthBiasClamp = 0.0f;
			Rasterizer.depthBiasSlopeFactor = 0.0f;
			Rasterizer.lineWidth = 1.0f;
		}

		VkPipelineMultisampleStateCreateInfo Multisampling{};
		{
			Multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
			Multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
			Multisampling.sampleShadingEnable = VK_FALSE;
			Multisampling.minSampleShading = 0.f;
			Multisampling.pSampleMask = nullptr;
			Multisampling.alphaToCoverageEnable = VK_FALSE;
			Multisampling.alphaToOneEnable = VK_FALSE;

		}

		VkPipelineColorBlendAttachmentState Color_Blend_Attachment{};
		{
			Color_Blend_Attachment.blendEnable = VK_FALSE;
			Color_Blend_Attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
			Color_Blend_Attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
			Color_Blend_Attachment.colorBlendOp = VK_BLEND_OP_ADD;
			Color_Blend_Attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
			Color_Blend_Attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
			Color_Blend_Attachment.alphaBlendOp = VK_BLEND_OP_ADD;
			Color_Blend_Attachment.colorWriteMask =
				VK_COLOR_COMPONENT_R_BIT |
				VK_COLOR_COMPONENT_G_BIT |
				VK_COLOR_COMPONENT_B_BIT |
				VK_COLOR_COMPONENT_A_BIT;
		}

		VkPipelineColorBlendStateCreateInfo Color_Blending{};
		{
			Color_Blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
			//NOTE : If Attend This Object, It Will Be Disable Color Blend
			Color_Blending.logicOpEnable = VK_FALSE;
			Color_Blending.logicOp = VK_LOGIC_OP_COPY;
			Color_Blending.attachmentCount = 1;
			Color_Blending.pAttachments = &Color_Blend_Attachment;
			Color_Blending.blendConstants[0] = 0.0f;
			Color_Blending.blendConstants[1] = 0.0f;
			Color_Blending.blendConstants[2] = 0.0f;
			Color_Blending.blendConstants[3] = 0.0f;
		}

		vector<VkDynamicState> Dynamic_States{
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};

		//NOTE : If Set Dynamic Field ,Old Static Field Will Be Disable, So We Should Set All Field In Feature 
		VkPipelineDynamicStateCreateInfo Dynamic_State{};
		{
			Dynamic_State.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
			Dynamic_State.dynamicStateCount = static_cast<uint32_t>(Dynamic_States.size());
			Dynamic_State.pDynamicStates = Dynamic_States.data();
		}

		VkPipelineLayoutCreateInfo Pipeline_Layout_Info{};
		{
			Pipeline_Layout_Info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			Pipeline_Layout_Info.setLayoutCount = 0;
			Pipeline_Layout_Info.pSetLayouts = nullptr;
			Pipeline_Layout_Info.pushConstantRangeCount = 0;
			Pipeline_Layout_Info.pPushConstantRanges = nullptr;
		}

		VkPipelineLayout Pipeline_Layout{ nullptr };
		THROW_IF_VK_FAILED(vkCreatePipelineLayout(this->m_Logical_Device.get(), &Pipeline_Layout_Info, nullptr, &Pipeline_Layout));

		this->m_Pipeline_Layout.get_deleter() = [Device = this->m_Logical_Device.get()](VkPipelineLayout Pipeline_Layout) {if (nullptr != Pipeline_Layout) vkDestroyPipelineLayout(Device, Pipeline_Layout, nullptr); };
		this->m_Pipeline_Layout.reset(Pipeline_Layout);

		const VkPipelineShaderStageCreateInfo Shader_Stages[] = { Vertex_Shader_Stage_Info,Fragment_Shader_Stage_Info };

		VkGraphicsPipelineCreateInfo Pipeline_Info{};
		{
			Pipeline_Info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
			Pipeline_Info.stageCount = 2;
			Pipeline_Info.pStages = Shader_Stages;
			Pipeline_Info.pVertexInputState = &Vertex_Input_Info;
			Pipeline_Info.pInputAssemblyState = &Input_Assembly_Info;
			Pipeline_Info.pTessellationState = nullptr;
			Pipeline_Info.pViewportState = &Viewport_State_Info;
			Pipeline_Info.pRasterizationState = &Rasterizer;
			Pipeline_Info.pMultisampleState = &Multisampling;
			Pipeline_Info.pDepthStencilState = nullptr;
			Pipeline_Info.pColorBlendState = &Color_Blending;
			Pipeline_Info.pDynamicState = &Dynamic_State;
			Pipeline_Info.layout = this->m_Pipeline_Layout.get();
			Pipeline_Info.renderPass = this->m_Render_Pass.get();
			Pipeline_Info.subpass = 0;
			//NOTE : Only Flag Use VK PIPELINE CREATE DERIVATIVE BIT Can Be Usefual 
			Pipeline_Info.basePipelineHandle = VK_NULL_HANDLE;
			//Pipeline_Info.basePipelineIndex = -1;
		}

		VkPipeline Graphics_Pipeline{ nullptr };
		THROW_IF_VK_FAILED(vkCreateGraphicsPipelines(this->m_Logical_Device.get(), VK_NULL_HANDLE, 1, &Pipeline_Info, nullptr, &Graphics_Pipeline));

		this->m_Graphics_Pipeline.get_deleter() = [Device = this->m_Logical_Device.get()](VkPipeline Graphics_Pipeline) {if (nullptr != Graphics_Pipeline) vkDestroyPipeline(Device, Graphics_Pipeline, nullptr); };
		this->m_Graphics_Pipeline.reset(Graphics_Pipeline);

		vkDestroyShaderModule(this->m_Logical_Device.get(), Fragment_Shader_Module, nullptr);
		vkDestroyShaderModule(this->m_Logical_Device.get(), Vertex_Shader_Module, nullptr);
	}

	void Create_Frame_Buffers(void) {
		this->m_Swap_Chain_Frame_buffers.resize(this->m_Swap_Chain_Image_Views.size());

		for (size_t Index = 0; Index < this->m_Swap_Chain_Image_Views.size(); ++Index) {
			VkImageView Attachments[] = { this->m_Swap_Chain_Image_Views[Index].get() };

			VkFramebufferCreateInfo Framebuffer_Info{};
			{
				Framebuffer_Info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
				Framebuffer_Info.renderPass = this->m_Render_Pass.get();
				Framebuffer_Info.attachmentCount = 1;
				Framebuffer_Info.pAttachments = Attachments;
				Framebuffer_Info.width = this->m_Swap_Chain_Extent.width;
				Framebuffer_Info.height = this->m_Swap_Chain_Extent.height;
				Framebuffer_Info.layers = 1;
			}

			VkFramebuffer Framebuffer{ nullptr };
			THROW_IF_VK_FAILED(vkCreateFramebuffer(this->m_Logical_Device.get(), &Framebuffer_Info, nullptr, &Framebuffer));

			this->m_Swap_Chain_Frame_buffers[Index].get_deleter() = [Device = this->m_Logical_Device.get()](VkFramebuffer Framebuffer) {if (nullptr != Framebuffer) vkDestroyFramebuffer(Device, Framebuffer, nullptr); };
			this->m_Swap_Chain_Frame_buffers[Index].reset(Framebuffer);
		}
	}

	void Create_Command_Pool(void) {
		VkCommandPoolCreateInfo Command_Pool_Info{};
		{
			Command_Pool_Info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			Command_Pool_Info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
			Command_Pool_Info.queueFamilyIndex = this->m_Queue_Family_Indices.Graphics_Family;
		}

		VkCommandPool Command_Pool{ nullptr };
		THROW_IF_VK_FAILED(vkCreateCommandPool(this->m_Logical_Device.get(), &Command_Pool_Info, nullptr, &Command_Pool));

		this->m_Command_Pool.get_deleter() = [Device = this->m_Logical_Device.get()](VkCommandPool Command_Pool) {if (nullptr != Command_Pool) vkDestroyCommandPool(Device, Command_Pool, nullptr); };
		this->m_Command_Pool.reset(Command_Pool);
	}

	void Create_Vertex_Buffer(void) {
		VkBufferCreateInfo Buffer_Info{};
		{
			Buffer_Info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			Buffer_Info.size = static_cast<VkDeviceSize>(sizeof(Vertex) * Vertices.size());
			Buffer_Info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
			Buffer_Info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		}

		VkBuffer Vertex_Buffer{ nullptr };
		THROW_IF_VK_FAILED(vkCreateBuffer(this->m_Logical_Device.get(), &Buffer_Info, nullptr, &Vertex_Buffer));

		const auto Delete_Buffer = [Device = this->m_Logical_Device.get()](VkBuffer Buffer) {if (nullptr != Buffer) vkDestroyBuffer(Device, Buffer, nullptr); };
		this->m_Vertex_Buffer.get_deleter() = Delete_Buffer;
		this->m_Vertex_Buffer.reset(Vertex_Buffer);

		VkMemoryRequirements Memory_Requirements{};
		vkGetBufferMemoryRequirements(this->m_Logical_Device.get(), this->m_Vertex_Buffer.get(), &Memory_Requirements);

		VkMemoryAllocateInfo Memory_Allocate_Info{};
		{
			Memory_Allocate_Info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			Memory_Allocate_Info.allocationSize = Memory_Requirements.size;
			Memory_Allocate_Info.memoryTypeIndex = this->Find_Memory_Type(Memory_Requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		}

		VkDeviceMemory Vertex_Buffer_Memory{ nullptr };
		THROW_IF_VK_FAILED(vkAllocateMemory(this->m_Logical_Device.get(), &Memory_Allocate_Info, nullptr, &Vertex_Buffer_Memory));

		this->m_Vertex_Buffer_Memory.get_deleter() = [Device = this->m_Logical_Device.get()](VkDeviceMemory Memory) {if (nullptr != Memory) vkFreeMemory(Device, Memory, nullptr); };
		this->m_Vertex_Buffer_Memory.reset(Vertex_Buffer_Memory);

		THROW_IF_VK_FAILED(vkBindBufferMemory(this->m_Logical_Device.get(), this->m_Vertex_Buffer.get(), this->m_Vertex_Buffer_Memory.get(), 0));

		//NOTE : Buffer_Info.size Requal Vertices Size
		void* Data{ nullptr };
		THROW_IF_VK_FAILED(vkMapMemory(this->m_Logical_Device.get(), this->m_Vertex_Buffer_Memory.get(), 0, Buffer_Info.size, 0, &Data));
		memcpy(Data, Vertices.data(), static_cast<size_t>(Buffer_Info.size));
		vkUnmapMemory(this->m_Logical_Device.get(), this->m_Vertex_Buffer_Memory.get());
	}

	void Create_Command_Buffers(void) {
		this->m_Command_Buffers.resize(MAX_FRAMES_IN_FLIGHT);

		VkCommandBufferAllocateInfo Command_Buffer_Allocate_Info{};
		{
			Command_Buffer_Allocate_Info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			Command_Buffer_Allocate_Info.commandPool = this->m_Command_Pool.get();
			Command_Buffer_Allocate_Info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			Command_Buffer_Allocate_Info.commandBufferCount = static_cast<uint32_t>(m_Command_Buffers.size());
		}

		THROW_IF_VK_FAILED(vkAllocateCommandBuffers(this->m_Logical_Device.get(), &Command_Buffer_Allocate_Info, this->m_Command_Buffers.data()));
	}

	void Record_Command_Buffer(VkCommandBuffer Command_Buffer, uint32_t Image_Index) {

		VkCommandBufferBeginInfo Command_Buffer_Begin_Info{};
		{
			Command_Buffer_Begin_Info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			Command_Buffer_Begin_Info.pInheritanceInfo = nullptr;
		}

		if (VK_SUCCESS != vkBeginCommandBuffer(this->m_Command_Buffers[this->m_Current_Frame], &Command_Buffer_Begin_Info))
			throw runtime_error("Failed to begin recording command buffer!");

		VkRect2D Render_Area{};
		{
			Render_Area.offset = { 0, 0 };
			Render_Area.extent = this->m_Swap_Chain_Extent;
		}

		VkClearValue Clear_Color{ 0.0f, 0.0f, 0.0f, 1.0f };

		VkViewport Viewport{};
		{
			Viewport.x = 0.0f;
			Viewport.y = 0.0f;
			Viewport.width = static_cast<float>(this->m_Swap_Chain_Extent.width);
			Viewport.height = static_cast<float>(this->m_Swap_Chain_Extent.height);
			Viewport.minDepth = 0.0f;
			Viewport.maxDepth = 1.0f;
		}

		VkRect2D Scissor{};
		{
			Scissor.offset = { 0, 0 };
			Scissor.extent = this->m_Swap_Chain_Extent;
		}

		VkRenderPassBeginInfo Render_Pass_Begin_Info{};
		{
			Render_Pass_Begin_Info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			Render_Pass_Begin_Info.renderPass = this->m_Render_Pass.get();
			Render_Pass_Begin_Info.framebuffer = this->m_Swap_Chain_Frame_buffers[Image_Index].get();
			Render_Pass_Begin_Info.renderArea = Render_Area;
			Render_Pass_Begin_Info.clearValueCount = 1;
			Render_Pass_Begin_Info.pClearValues = &Clear_Color;
		}

		vkCmdBeginRenderPass(Command_Buffer, &Render_Pass_Begin_Info, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(Command_Buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, this->m_Graphics_Pipeline.get());

		vkCmdSetViewport(Command_Buffer, 0, 1, &Viewport);
		vkCmdSetScissor(Command_Buffer, 0, 1, &Scissor);

		VkBuffer Vertex_Buffers[] = { this->m_Vertex_Buffer.get() };
		VkDeviceSize Offsets[] = { 0 };
		vkCmdBindVertexBuffers(Command_Buffer, 0, 1, Vertex_Buffers, Offsets);

		vkCmdDraw(Command_Buffer, static_cast<uint32_t>(Vertices.size()), 1, 0, 0);

		vkCmdEndRenderPass(Command_Buffer);

		if (VK_SUCCESS != vkEndCommandBuffer(Command_Buffer))
			throw runtime_error("Failed to record command buffer!");
	}

	void Create_Sync_Objects(void) {
		VkSemaphoreCreateInfo Semaphore_Info{};
		{
			Semaphore_Info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		}

		VkFenceCreateInfo Fence_Info{};
		{
			Fence_Info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			Fence_Info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		}

		VkSemaphore Semaphore{ nullptr };
		const auto  Deleter_Semaphore = [Device = this->m_Logical_Device.get()](VkSemaphore Semaphore) {if (nullptr != Semaphore) vkDestroySemaphore(Device, Semaphore, nullptr); };

		VkFence InFlight_Fence{ nullptr };
		const auto Delete_Fence = [Device = this->m_Logical_Device.get()](VkFence Fence) {if (nullptr != Fence) vkDestroyFence(Device, Fence, nullptr); };

		this->m_Image_Available_Semaphores.resize(MAX_FRAMES_IN_FLIGHT);
		this->m_Render_Finished_Semaphores.resize(MAX_FRAMES_IN_FLIGHT);

		this->m_InFlight_Fences.resize(MAX_FRAMES_IN_FLIGHT);

		for (size_t Index = 0; Index < MAX_FRAMES_IN_FLIGHT; ++Index) {
			THROW_IF_VK_FAILED(vkCreateSemaphore(this->m_Logical_Device.get(), &Semaphore_Info, nullptr, &Semaphore));

			this->m_Image_Available_Semaphores[Index].get_deleter() = Deleter_Semaphore;
			this->m_Image_Available_Semaphores[Index].reset(Semaphore);

			THROW_IF_VK_FAILED(vkCreateSemaphore(this->m_Logical_Device.get(), &Semaphore_Info, nullptr, &Semaphore));

			this->m_Render_Finished_Semaphores[Index].get_deleter() = Deleter_Semaphore;
			this->m_Render_Finished_Semaphores[Index].reset(Semaphore);

			THROW_IF_VK_FAILED(vkCreateFence(this->m_Logical_Device.get(), &Fence_Info, nullptr, &InFlight_Fence));

			this->m_InFlight_Fences[Index].get_deleter() = Delete_Fence;
			this->m_InFlight_Fences[Index].reset(InFlight_Fence);
		}
	}

	void Draw_Frame(void) {
		const auto  Temp_INFlightFence = this->m_InFlight_Fences[this->m_Current_Frame].get();

		THROW_IF_VK_FAILED(vkWaitForFences(this->m_Logical_Device.get(), 1, &Temp_INFlightFence, VK_TRUE, std::numeric_limits<uint64_t>::max()));
		THROW_IF_VK_FAILED(vkResetFences(this->m_Logical_Device.get(), 1, &Temp_INFlightFence));

		uint32_t Image_Index{};
		const VkResult Acquire_Flag{ vkAcquireNextImageKHR(this->m_Logical_Device.get(), this->m_Swap_Chain.get(), std::numeric_limits<uint64_t>::max(), this->m_Image_Available_Semaphores[this->m_Current_Frame].get(), VK_NULL_HANDLE, &Image_Index) };

		if (VK_ERROR_OUT_OF_DATE_KHR == Acquire_Flag)
			this->Re_Create_SwapChain();
		else if (VK_SUCCESS != Acquire_Flag && VK_SUBOPTIMAL_KHR != Acquire_Flag)
			throw runtime_error("Failed to acquire swap chain image!");

		THROW_IF_VK_FAILED(vkResetCommandBuffer(this->m_Command_Buffers[this->m_Current_Frame], 0));
		this->Record_Command_Buffer(this->m_Command_Buffers[this->m_Current_Frame], Image_Index);

		VkSemaphore Wait_Semaphores[] = { this->m_Image_Available_Semaphores[this->m_Current_Frame].get() };
		VkPipelineStageFlags Wait_Stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		VkSemaphore Signal_Semaphores[] = { this->m_Render_Finished_Semaphores[this->m_Current_Frame].get() };

		VkSubmitInfo Submit_Info{};
		{
			Submit_Info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			Submit_Info.waitSemaphoreCount = 1;
			Submit_Info.pWaitSemaphores = Wait_Semaphores;
			Submit_Info.pWaitDstStageMask = Wait_Stages;
			Submit_Info.pCommandBuffers = &this->m_Command_Buffers[this->m_Current_Frame];
			Submit_Info.commandBufferCount = 1;
			Submit_Info.signalSemaphoreCount = 1;
			Submit_Info.pSignalSemaphores = Signal_Semaphores;
		}

		THROW_IF_VK_FAILED(vkQueueSubmit(this->m_Graphics_Queue, 1, &Submit_Info, this->m_InFlight_Fences[this->m_Current_Frame].get()));

		VkSwapchainKHR Swap_Chains[] = { this->m_Swap_Chain.get() };

		VkPresentInfoKHR Present_Info{};
		{
			Present_Info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
			Present_Info.waitSemaphoreCount = 1;
			Present_Info.pWaitSemaphores = Signal_Semaphores;
			Present_Info.swapchainCount = 1;
			Present_Info.pSwapchains = Swap_Chains;
			Present_Info.pImageIndices = &Image_Index;
			Present_Info.pResults = nullptr;
		}

		const VkResult Present_Flag{ vkQueuePresentKHR(this->m_Present_Queue, &Present_Info) };

		if (VK_ERROR_OUT_OF_DATE_KHR == Present_Flag ||
			VK_SUBOPTIMAL_KHR == Present_Flag ||
			this->m_Frame_Buffer_Resized) {
			this->m_Frame_Buffer_Resized = false;
			this->Re_Create_SwapChain();
		}
		else if (VK_SUCCESS != Present_Flag && VK_SUBOPTIMAL_KHR != Present_Flag)
			throw runtime_error("Failed to present swap chain image!");

		this->m_Current_Frame = (this->m_Current_Frame + 1) % MAX_FRAMES_IN_FLIGHT;
	}

private:
	uint32_t Find_Queue_Families(VkQueueFlagBits Vk_Queue_FlagBit) const {
		uint32_t Queue_Family_Count;
		vkGetPhysicalDeviceQueueFamilyProperties(this->m_Physical_Device, &Queue_Family_Count, nullptr);

		vector<VkQueueFamilyProperties> Queue_Families;
		Queue_Families.resize(Queue_Family_Count);
		vkGetPhysicalDeviceQueueFamilyProperties(this->m_Physical_Device, &Queue_Family_Count, Queue_Families.data());

		for (auto CIt = Queue_Families.cbegin(); CIt != Queue_Families.cend(); ++CIt)
			if (CIt->queueFlags & Vk_Queue_FlagBit)
				return  static_cast<uint32_t>(CIt - Queue_Families.cbegin());

		throw runtime_error("Failed to find a suitable GPU!");

		return numeric_limits<uint32_t>::max();
	}

	uint32_t Get_Physical_Device_Queue_Present_Family(void) const {
		uint32_t Queue_Family_Count;
		vkGetPhysicalDeviceQueueFamilyProperties(this->m_Physical_Device, &Queue_Family_Count, nullptr);

		vector<VkQueueFamilyProperties> Queue_Families;
		Queue_Families.resize(Queue_Family_Count);
		vkGetPhysicalDeviceQueueFamilyProperties(this->m_Physical_Device, &Queue_Family_Count, Queue_Families.data());

		uint32_t Queue_Family_Index{ numeric_limits<uint32_t>::max() };
		for (auto CIt = Queue_Families.cbegin(); CIt != Queue_Families.cend(); ++CIt) {
			VkBool32 Present_Support{ false };
			THROW_IF_VK_FAILED(vkGetPhysicalDeviceSurfaceSupportKHR(this->m_Physical_Device, static_cast<uint32_t>(CIt - Queue_Families.cbegin()), this->m_Surface.get(), &Present_Support));

			if (Present_Support) {
				Queue_Family_Index = static_cast<uint32_t>(CIt - Queue_Families.cbegin());

				//NOTE : First Chose Queue Family With Graphics Bit
				if (Queue_Family_Index == this->m_Queue_Family_Indices.Graphics_Family)
					return Queue_Family_Index;
			}
		}

		return Queue_Family_Index;
	}

	void Query_Swap_Chain_Support_Details(void) {
		THROW_IF_VK_FAILED(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(this->m_Physical_Device, this->m_Surface.get(), &this->m_Swap_Chain_Support_Details.Capabilities));

		uint32_t Format_Count{};
		THROW_IF_VK_FAILED(vkGetPhysicalDeviceSurfaceFormatsKHR(this->m_Physical_Device, this->m_Surface.get(), &Format_Count, nullptr));

		if (0 != Format_Count) {
			this->m_Swap_Chain_Support_Details.Formats.resize(Format_Count);
			THROW_IF_VK_FAILED(vkGetPhysicalDeviceSurfaceFormatsKHR(this->m_Physical_Device, this->m_Surface.get(), &Format_Count, this->m_Swap_Chain_Support_Details.Formats.data()));
		}
		else
			throw runtime_error("Failed to find a suitable GPU!");

		uint32_t Present_Mode_Count{};
		(vkGetPhysicalDeviceSurfacePresentModesKHR(this->m_Physical_Device, this->m_Surface.get(), &Present_Mode_Count, nullptr));
		if (0 != Present_Mode_Count) {
			this->m_Swap_Chain_Support_Details.Present_Modes.resize(Present_Mode_Count);
			THROW_IF_VK_FAILED(vkGetPhysicalDeviceSurfacePresentModesKHR(this->m_Physical_Device, this->m_Surface.get(), &Present_Mode_Count, this->m_Swap_Chain_Support_Details.Present_Modes.data()));
		}
		else
			throw runtime_error("Failed to find a suitable GPU!");
	}

	const VkExtent2D Choose_SwapChain_Extent(const VkSurfaceCapabilitiesKHR& Capabilities) {
		if (numeric_limits<uint32_t>::max() != Capabilities.currentExtent.width)
			return Capabilities.currentExtent;
		else {
			int Width, Height;
			glfwGetFramebufferSize(this->m_Window.get(), &Width, &Height);
			VkExtent2D Actual_Extent{ static_cast<uint32_t>(Width),static_cast<uint32_t>(Height) };

			Actual_Extent.width = std::clamp(Actual_Extent.width, Capabilities.minImageExtent.width, Capabilities.maxImageExtent.width);
			Actual_Extent.height = std::clamp(Actual_Extent.height, Capabilities.minImageExtent.height, Capabilities.maxImageExtent.height);

			return Actual_Extent;
		}
	}

	const VkShaderModule Create_Shader_Module(const vector<char>& Code) {
		VkShaderModuleCreateInfo Create_Info{};
		{
			Create_Info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			Create_Info.codeSize = Code.size();
			Create_Info.pCode = reinterpret_cast<const uint32_t*>(Code.data());
		}

		VkShaderModule Shader_Module{ nullptr };
		THROW_IF_VK_FAILED(vkCreateShaderModule(this->m_Logical_Device.get(), &Create_Info, nullptr, &Shader_Module));
		return Shader_Module;
	}

	uint32_t Find_Memory_Type(uint32_t Type_Filter, VkMemoryPropertyFlags Property_Flags) const {
		VkPhysicalDeviceMemoryProperties Memory_Properties{};
		vkGetPhysicalDeviceMemoryProperties(this->m_Physical_Device, &Memory_Properties);

		for (uint32_t Index = 0; Index < Memory_Properties.memoryTypeCount; ++Index)
			if (Type_Filter & (1 << Index) && (Memory_Properties.memoryTypes[Index].propertyFlags & Property_Flags) == Property_Flags)
				return Index;

		throw runtime_error("Failed to find suitable memory type!");

		return numeric_limits<uint32_t>::max();
	}

private:
	static void Initialize_GLWF(void) {
		if (GLFW_FALSE == glfwInit())
			throw runtime_error("Failed to initialize GLFW");

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		//glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	}

	static vector<const char*> Get_Require_Extensions(void) {
		uint32_t GLFW_Extension_Count;
		const char** GLFW_Extensions;
		GLFW_Extensions = glfwGetRequiredInstanceExtensions(&GLFW_Extension_Count);

		vector<const char*> Extensions{ GLFW_Extensions, GLFW_Extensions + GLFW_Extension_Count };

#ifdef _DEBUG
		Extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif // _DEBUG

		return Extensions;
	}

	static bool Check_Device_Extension_Support(VkPhysicalDevice Device) {
		uint32_t Extension_Count;
		THROW_IF_VK_FAILED(vkEnumerateDeviceExtensionProperties(Device, nullptr, &Extension_Count, nullptr));
		vector<VkExtensionProperties> Available_Extensions;
		Available_Extensions.resize(Extension_Count);
		THROW_IF_VK_FAILED(vkEnumerateDeviceExtensionProperties(Device, nullptr, &Extension_Count, Available_Extensions.data()));

		unordered_set<string> Required_Extensions{ Device_EXT_SwapChain };
		for (const auto& Available_Extension : Available_Extensions)
			Required_Extensions.erase(Available_Extension.extensionName);

		return Required_Extensions.empty();
	}

	static bool Is_Device_Suitable(VkPhysicalDevice Device) {
		if (!VK_Application::Check_Device_Extension_Support(Device))
			return false;

		VkPhysicalDeviceProperties Device_Properties{};
		vkGetPhysicalDeviceProperties(Device, &Device_Properties);

		VkPhysicalDeviceFeatures Device_Features{};
		vkGetPhysicalDeviceFeatures(Device, &Device_Features);

		//NOTE : Use This For Check Device Type
		return
			Device_Properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
			Device_Features.geometryShader;
	}

	static const VkSurfaceFormatKHR Choose_SwapChain_Surface_Format(const vector<VkSurfaceFormatKHR>& Available_Formats) {
		for (const auto& Available_Format : Available_Formats)
			if (VK_FORMAT_B8G8R8A8_SRGB == Available_Format.format && VK_COLOR_SPACE_SRGB_NONLINEAR_KHR == Available_Format.colorSpace)
				return Available_Format;

		//NOTE : Default Return First Format
		return Available_Formats.front();
	}

	static const VkPresentModeKHR Choose_SwapChain_Present_Mode(const vector<VkPresentModeKHR>& Available_Present_Modes) {
		for (const auto& Available_Present_Mode : Available_Present_Modes)
			if (VK_PRESENT_MODE_MAILBOX_KHR == Available_Present_Mode)
				return Available_Present_Mode;

		//NOTE : Default Return FIFO ,If Device Suppert Present Queue ,IT Must Support FIFO
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	static void Frame_Buffer_Resize_CallBack(GLFWwindow* Window, int Width, int Height) {
		auto App = reinterpret_cast<VK_Application*>(glfwGetWindowUserPointer(Window));
		App->m_Frame_Buffer_Resized = true;
	}

private:
	unique_ptr<GLFWwindow, decltype(&glfwDestroyWindow)> m_Window{ nullptr, glfwDestroyWindow };

	static constexpr auto Delete_VK_Instance = [](VkInstance Instance) {if (nullptr != Instance)vkDestroyInstance(Instance, nullptr); };
	unique_ptr<VkInstance_T, decltype(Delete_VK_Instance)> m_VK_Instance{ nullptr ,Delete_VK_Instance };

	unique_ptr<VkSurfaceKHR_T, function<void(VkSurfaceKHR)>> m_Surface{ nullptr };

	VkPhysicalDevice m_Physical_Device{ nullptr };

	static constexpr auto Delete_VK_Device = [](VkDevice Device) {if (nullptr != Device)vkDestroyDevice(Device, nullptr); };
	unique_ptr<VkDevice_T, decltype(Delete_VK_Device)> m_Logical_Device{ nullptr, Delete_VK_Device };

	Queue_Family_Indices m_Queue_Family_Indices{};

	VkQueue m_Graphics_Queue{ nullptr };
	VkQueue m_Present_Queue{ nullptr };

	unique_ptr<VkSwapchainKHR_T, function<void(VkSwapchainKHR)>> m_Swap_Chain{ nullptr };

	Swap_Chain_Support_Details m_Swap_Chain_Support_Details{};

	vector<VkImage> m_Swap_Chain_Images{};
	VkFormat m_Swap_Chain_Image_Format{};
	VkExtent2D m_Swap_Chain_Extent{};
	vector<unique_ptr<VkImageView_T, function<void(VkImageView)>>> m_Swap_Chain_Image_Views{};

	unique_ptr< VkRenderPass_T, function<void(VkRenderPass)>> m_Render_Pass{ nullptr };

	unique_ptr<VkPipelineLayout_T, function<void(VkPipelineLayout)>> m_Pipeline_Layout{ nullptr };

	unique_ptr<VkPipeline_T, function<void(VkPipeline)>> m_Graphics_Pipeline{ nullptr };

	vector<unique_ptr<VkFramebuffer_T, function<void(VkFramebuffer)>>> m_Swap_Chain_Frame_buffers{};

	unique_ptr<VkCommandPool_T, function<void(VkCommandPool)>> m_Command_Pool{ nullptr };

	unique_ptr<VkBuffer_T, function<void(VkBuffer)>> m_Vertex_Buffer{ nullptr };
	unique_ptr<VkDeviceMemory_T, function<void(VkDeviceMemory)>> m_Vertex_Buffer_Memory{ nullptr };

	vector<VkCommandBuffer> m_Command_Buffers{};

	vector<unique_ptr<VkSemaphore_T, function<void(VkSemaphore)>>> m_Image_Available_Semaphores{};
	vector<unique_ptr<VkSemaphore_T, function<void(VkSemaphore)>>> m_Render_Finished_Semaphores{};

	vector<unique_ptr<VkFence_T, function<void(VkFence)>>> m_InFlight_Fences{};

	uint32_t m_Current_Frame{ 0 };

	bool m_Frame_Buffer_Resized{ false };

};

int main() {

	try {
		VK_Application App{};

#ifdef _DEBUG
		App.m_Validation_Layer_List.emplace_back(validationLayers);

		if (!App.Check_Vaildation_Layer_Support())
			throw runtime_error("Validation layers requested, but not available!");
#endif // _DEBUG

		App.Run();
	}
	catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}