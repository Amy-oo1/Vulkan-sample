#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstring>
#include <cstdlib>
#include<memory>
#include<mutex>
#include<string>

#include "vulkan/vulkan.h"

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

using namespace std;

constexpr int WIDTH{ 800 };
constexpr int HEIGHT{ 600 };

const constexpr char* validationLayers{ "VK_LAYER_KHRONOS_validation" };

class VK_Application final {
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
			this->m_VK_Instance, &
			this->m_VkDebugUtilsMessengerCreateInfoEXT,
			nullptr,
			&this->m_Debug_Messenger)
			)
			throw runtime_error("Failed to set up debug messenger!");
	}
#endif // _DEBUG

	void Run() {
		this->Initialize();
		this->Main_Loop();
		this->Clean_Up();
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

	void Init_Vulkan() {
#ifdef _DEBUG
		this->Build_Debug_Messenger_Create_Info();
#endif // _DEBUG

		this->Create_Instance();
		this->Pick_Physical_Device();

#ifdef _DEBUG
		this->Set_Debug_Messenger();
#endif // _DEBUG
	}

	void Main_Loop() {
		while (!glfwWindowShouldClose(this->m_Window.get()))
			glfwPollEvents();
	}

	void Clean_Up() {

#ifdef _DEBUG

		VK_Application::Destroy_DebugUtils_Messenger_EXT(this->m_VK_Instance, this->m_Debug_Messenger, nullptr);

#endif // _DEBUG


		vkDestroyInstance(m_VK_Instance, nullptr);

		//glfwDestroyWindow(this->m_Window.get());

		glfwTerminate();
	}

	void Create_Instance() {
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

		if (VK_SUCCESS != vkCreateInstance(&VK_Instance_Info, nullptr, &this->m_VK_Instance))
			throw std::runtime_error("failed to create instance!");
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

	void Pick_Physical_Device() {
		uint32_t Device_Count{ 0 };
		vkEnumeratePhysicalDevices(this->m_VK_Instance, &Device_Count, nullptr);
		if (0 == Device_Count)
			throw runtime_error("Failed to find GPUs with Vulkan support!");

		vector<VkPhysicalDevice> Devices{};
		Devices.resize(Device_Count);
		vkEnumeratePhysicalDevices(this->m_VK_Instance, &Device_Count, Devices.data());
		for (const auto& Device : Devices)
			if (Is_Device_Suitable(Device)) {
				this->m_Physical_Device = Device;
				break;
			}
		if (VK_NULL_HANDLE == this->m_Physical_Device)
			throw runtime_error("Failed to find a suitable GPU!");
	}

private:
	static void Initialize_GLWF(void) {
		if (GLFW_FALSE == glfwInit())
			throw runtime_error("Failed to initialize GLFW");

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	}


	static bool Is_Device_Suitable(VkPhysicalDevice Device) {
		uint32_t Queue_Family_Count;
		vkGetPhysicalDeviceQueueFamilyProperties(Device, &Queue_Family_Count, nullptr);

		vector<VkQueueFamilyProperties> Queue_Families;
		Queue_Families.resize(Queue_Family_Count);
		vkGetPhysicalDeviceQueueFamilyProperties(Device, &Queue_Family_Count, Queue_Families.data());

		//NOTE : Pick Physical Device That Support Graphics Queue
		for (const auto& Properties : Queue_Families)
			if (Properties.queueFlags & VK_QUEUE_GRAPHICS_BIT)
				return true;

	}

private:
	unique_ptr<GLFWwindow, decltype(&glfwDestroyWindow)> m_Window{ nullptr, glfwDestroyWindow };

	VkInstance m_VK_Instance{ nullptr };

	VkPhysicalDevice m_Physical_Device{ nullptr };

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