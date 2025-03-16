#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstring>
#include <cstdlib>
#include<memory>

#include "vulkan/vulkan.h"

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

using namespace std;

constexpr int WIDTH{ 800 };
constexpr int HEIGHT{ 600 };

class HelloTriangleApplication final {
public:
	void Run() {
		this->Init_Window();
		this->Init_Vulkan();
		this->Main_Loop();
		this->Clean_Up();
	}

private:
	void Init_Window() {
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
		if (nullptr == window) {
			throw std::runtime_error("Failed to create GLFW window");
		}
		this->m_Window.reset(window);
	}
	void Init_Vulkan() {
		Create_Instance();
	}

	void Main_Loop() {
		while (!glfwWindowShouldClose(this->m_Window.get()))
			glfwPollEvents();
	}

	void Clean_Up() {
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

		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		VkInstanceCreateInfo VK_Instance_Info = {};
		{
			VK_Instance_Info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

			VK_Instance_Info.pApplicationInfo = &VK_Application_Info;

			VK_Instance_Info.flags = 0;

			VK_Instance_Info.enabledLayerCount = 0;
			VK_Instance_Info.ppEnabledLayerNames = nullptr;

			VK_Instance_Info.enabledExtensionCount = glfwExtensionCount;
			VK_Instance_Info.ppEnabledExtensionNames = glfwExtensions;
		}

		if (VK_SUCCESS != vkCreateInstance(&VK_Instance_Info, nullptr, &this->m_VK_Instance))
			throw std::runtime_error("failed to create instance!");
	}

private:
	unique_ptr<GLFWwindow, decltype(&glfwDestroyWindow)> m_Window{ nullptr, glfwDestroyWindow };

	VkInstance m_VK_Instance{ nullptr };

};

int main() {
	HelloTriangleApplication app{};

	try {
		app.Run();
	}
	catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
