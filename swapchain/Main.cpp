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

#include "vulkan/vulkan.h"

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

using namespace std;

constexpr int WIDTH{ 800 };
constexpr int HEIGHT{ 600 };

const constexpr char* validationLayers{ "VK_LAYER_KHRONOS_validation" };

const constexpr char* Device_EXT_SwapChain{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };

class VK_Application final {
private:
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

	bool Check_Vaildation_Layer_Support(void) {
		uint32_t Layer_Count;
		vkEnumerateInstanceLayerProperties(&Layer_Count, nullptr);

		vector<VkLayerProperties> Available_Layers;
		Available_Layers.resize(Layer_Count);
		vkEnumerateInstanceLayerProperties(&Layer_Count, Available_Layers.data());

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

	static VkResult Create_DebugUtils_Messenger_EXT(VkInstance Instance, const VkDebugUtilsMessengerCreateInfoEXT* Create_Info, const VkAllocationCallbacks* Allocator, VkDebugUtilsMessengerEXT* m_Debug_Messenger) {
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
		if (VK_SUCCESS != VK_Application::Create_DebugUtils_Messenger_EXT(
			this->m_VK_Instance.get(), &
			this->m_VkDebugUtilsMessengerCreateInfoEXT,
			nullptr,
			&this->m_Debug_Messenger)
			)
			throw runtime_error("Failed to set up debug messenger!");
	}
#endif // _DEBUG

public:
	VK_Application(void) = default;

	~VK_Application(void) {
		this->Clean_Up();
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
	}

	void Main_Loop(void) {
		while (!glfwWindowShouldClose(this->m_Window.get()))
			glfwPollEvents();
	}

	void Clean_Up(void) {
		//NOTE : Clean Up Logical Device Before Instance and ,First Clean Command Queue Before Logical Device

		//vkDestroySwapchainKHR(this->m_Logical_Device.get(), this->m_Swap_Chain.get(), nullptr);
		this->m_Swap_Chain.reset();

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

	void Create_Instance(void) {
		VkApplicationInfo VK_Application_Info{};
		{
			VK_Application_Info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
			VK_Application_Info.pNext = nullptr;
			VK_Application_Info.pApplicationName = "Hello Triangle";
			VK_Application_Info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
			VK_Application_Info.pEngineName = "No Engine";
			VK_Application_Info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
			VK_Application_Info.apiVersion = VK_API_VERSION_1_0;
		}

		const auto& Extensions = VK_Application::Get_Require_Extensions();
		VkInstanceCreateInfo VK_Instance_Info = {};
		{
			VK_Instance_Info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

			VK_Instance_Info.pApplicationInfo = &VK_Application_Info;

#ifdef _DEBUG
			VK_Instance_Info.pNext = reinterpret_cast<const void*>(&this->m_VkDebugUtilsMessengerCreateInfoEXT);

			VK_Instance_Info.enabledLayerCount = static_cast<uint32_t>(this->m_Validation_Layer_List.size());
			VK_Instance_Info.ppEnabledLayerNames = this->m_Validation_Layer_List.data();
#else
			VK_Instance_Info.enabledLayerCount = 0;
			VK_Instance_Info.ppEnabledLayerNames = nullptr;
#endif // _DEBUG

			VK_Instance_Info.enabledExtensionCount = static_cast<uint32_t>(Extensions.size());
			VK_Instance_Info.ppEnabledExtensionNames = Extensions.data();
		}

		VkInstance VK_Instance{ nullptr };
		if (VK_SUCCESS != vkCreateInstance(&VK_Instance_Info, nullptr, &VK_Instance))
			throw std::runtime_error("failed to create instance!");
		this->m_VK_Instance.reset(VK_Instance);
	}

private:
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

	void Create_Surface(void) {
		VkSurfaceKHR Surface{ nullptr };
		if (VK_SUCCESS != glfwCreateWindowSurface(this->m_VK_Instance.get(), this->m_Window.get(), nullptr, &Surface))
			throw runtime_error("Failed to create window surface!");

		this->m_Surface.get_deleter() = [Instance = this->m_VK_Instance.get()](VkSurfaceKHR Surface) {vkDestroySurfaceKHR(Instance, Surface, nullptr); };
		this->m_Surface.reset(Surface);
	}

	void Pick_Physical_Device(void) {
		uint32_t Device_Count{ 0 };
		vkEnumeratePhysicalDevices(this->m_VK_Instance.get(), &Device_Count, nullptr);
		if (0 == Device_Count)
			throw runtime_error("Failed to find GPUs with Vulkan support!");

		vector<VkPhysicalDevice> Devices{};
		Devices.resize(Device_Count);
		vkEnumeratePhysicalDevices(this->m_VK_Instance.get(), &Device_Count, Devices.data());
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
			Device_Create_Info.enabledExtensionCount = 0;
			Device_Create_Info.ppEnabledExtensionNames = nullptr;
			Device_Create_Info.pEnabledFeatures = &Device_Features;
		}

		VkDevice Logical_Device{ nullptr };
		if (VK_SUCCESS != vkCreateDevice(this->m_Physical_Device, &Device_Create_Info, nullptr, &Logical_Device))
			throw runtime_error("Failed to create logical device!");
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

		vector<uint32_t> Queue_Family_Indices{ this->m_Queue_Family_Indices.Graphics_Family };
		VkSharingMode Sharing_Mode{ VK_SHARING_MODE_EXCLUSIVE };

		if (this->m_Queue_Family_Indices.Graphics_Family != this->m_Queue_Family_Indices.Present_Family) {
			Queue_Family_Indices.emplace_back(this->m_Queue_Family_Indices.Present_Family);
			Sharing_Mode = VK_SHARING_MODE_CONCURRENT;
		}

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
		if (VK_SUCCESS != vkCreateSwapchainKHR(this->m_Logical_Device.get(), &Swap_Chain_Create_Info, nullptr, &Swap_Chain))
			throw runtime_error("Failed to create swap chain!");

		this->m_Swap_Chain.get_deleter() = [Device = this->m_Logical_Device.get()](VkSwapchainKHR Swap_Chain) {vkDestroySwapchainKHR(Device, Swap_Chain, nullptr); };
		this->m_Swap_Chain.reset(Swap_Chain);

		vkGetSwapchainImagesKHR(this->m_Logical_Device.get(), this->m_Swap_Chain.get(), &Image_Count, nullptr);
		this->m_Swap_Chain_Images.resize(Image_Count);
		vkGetSwapchainImagesKHR(this->m_Logical_Device.get(), this->m_Swap_Chain.get(), &Image_Count, this->m_Swap_Chain_Images.data());

		this->m_Swap_Chain_Image_Format = Surface_Format.format;
		this->m_Swap_Chain_Extent = Swap_Chain_Extent;
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
				return  CIt - Queue_Families.cbegin();

		throw runtime_error("Failed to find a suitable GPU!");

		return numeric_limits<uint32_t>::max();
	}

	void Query_Swap_Chain_Support_Details(void) {
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(this->m_Physical_Device, this->m_Surface.get(), &this->m_Swap_Chain_Support_Details.Capabilities);

		uint32_t Format_Count;
		vkGetPhysicalDeviceSurfaceFormatsKHR(this->m_Physical_Device, this->m_Surface.get(), &Format_Count, nullptr);

		if (0 != Format_Count) {
			this->m_Swap_Chain_Support_Details.Formats.resize(Format_Count);
			vkGetPhysicalDeviceSurfaceFormatsKHR(this->m_Physical_Device, this->m_Surface.get(), &Format_Count, this->m_Swap_Chain_Support_Details.Formats.data());
		}
		else
			throw runtime_error("Failed to find a suitable GPU!");

		uint32_t Present_Mode_Count;
		vkGetPhysicalDeviceSurfacePresentModesKHR(this->m_Physical_Device, this->m_Surface.get(), &Present_Mode_Count, nullptr);
		if (0 != Present_Mode_Count) {
			this->m_Swap_Chain_Support_Details.Present_Modes.resize(Present_Mode_Count);
			vkGetPhysicalDeviceSurfacePresentModesKHR(this->m_Physical_Device, this->m_Surface.get(), &Present_Mode_Count, this->m_Swap_Chain_Support_Details.Present_Modes.data());
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

private:
	static void Initialize_GLWF(void) {
		if (GLFW_FALSE == glfwInit())
			throw runtime_error("Failed to initialize GLFW");

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	}

	static bool Is_Device_Suitable(VkPhysicalDevice Device) {
		VkPhysicalDeviceProperties Device_Properties;
		vkGetPhysicalDeviceProperties(Device, &Device_Properties);

		VkPhysicalDeviceFeatures Device_Features;
		vkGetPhysicalDeviceFeatures(Device, &Device_Features);

		//NOTE : Use This For Check Device Type
		return Device_Properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
			Device_Features.geometryShader;
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
			vkGetPhysicalDeviceSurfaceSupportKHR(this->m_Physical_Device, CIt - Queue_Families.cbegin(), this->m_Surface.get(), &Present_Support);

			if (Present_Support) {
				Queue_Family_Index = static_cast<uint32_t>(CIt - Queue_Families.cbegin());

				//NOTE : First Chose Queue Family With Graphics Bit
				if (Queue_Family_Index == this->m_Queue_Family_Indices.Graphics_Family)
					return Queue_Family_Index;
			}
		}

		return Queue_Family_Index;
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

	static bool Check_Device_Extension_Support(VkPhysicalDevice Device) {
		uint32_t Extension_Count;
		vkEnumerateDeviceExtensionProperties(Device, nullptr, &Extension_Count, nullptr);
		vector<VkExtensionProperties> Available_Extensions;
		Available_Extensions.resize(Extension_Count);
		vkEnumerateDeviceExtensionProperties(Device, nullptr, &Extension_Count, Available_Extensions.data());

		unordered_set<string> Required_Extensions{ Device_EXT_SwapChain };
		for (const auto& Available_Extension : Available_Extensions)
			Required_Extensions.erase(Available_Extension.extensionName);

		return Required_Extensions.empty();
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