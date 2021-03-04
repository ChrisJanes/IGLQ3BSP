#define GLEW_STATIC

#include "stb_image.h"

#include <GL\glew.h>
#include <GLFW\glfw3.h>
#include <glm\glm.hpp>
#include <glm\gtc\matrix_transform.hpp>
#include <glm\gtc\type_ptr.hpp>

#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <thread>

#include "physfs/physfs.h"

#include "BSPLoader.h"

#include "shaders.inc"

/*
126, 74
126, 73
127, 74
126, 74
126, 73
127, 73
*/

const bool AllowMouse = true;
const bool SingleDraw = false;

const int ScreenWidth = 1280;
const int ScreenHeight = 720;

glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 3.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

//const std::string file = "Data\\cube.bsp";
const std::string file = "Data\\q3dm0.bsp";

float yaw;
float pitch;

bool firstMouse = true;
float lastX = ScreenWidth / 2.0f;
float lastY = ScreenHeight / 2.0f;

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
	if (firstMouse) // initially set to true
	{
		lastX = xpos;
		lastY = ypos;
		firstMouse = false;
	}

	float xoffset = xpos - lastX;
	float yoffset = lastY - ypos; // reversed since y-coordinates range from bottom to top
	lastX = xpos;
	lastY = ypos;

	const float sensitivity = 0.1f;
	xoffset *= sensitivity;
	yoffset *= sensitivity;

	yaw += xoffset;
	pitch += yoffset;

	if (pitch > 89.0f)
		pitch = 89.0f;
	if (pitch < -89.0f)
		pitch = -89.0f;

	glm::vec3 direction;
	direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
	direction.y = sin(glm::radians(pitch));
	direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
	cameraFront = glm::normalize(direction);
}

void processInput(GLFWwindow* window)
{
	const float cameraSpeed = 5.f; // adjust accordingly
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		cameraPos += cameraSpeed * cameraFront;
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		cameraPos -= cameraSpeed * cameraFront;
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
		cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
}

int main()
{
	// initialise physfs:
	PHYSFS_init("");
	PHYSFS_setSaneConfig("cjanes", "bsp_renderer", "pk3", false, true);
	PHYSFS_mount("F:/SteamLibrary/steamapps/common/Quake 3 Arena/baseq3/", "/data/", true);

	char** files = PHYSFS_enumerateFiles("/data");
	char** i;
	for (i = files; *i != NULL; i++)
	{
		std::string file{ *i };
		if (file.length() > 4 && file.substr(file.length() - 4) == ".pk3")
		{
			std::string fullfile = "/data/" + file;
			std::string path = PHYSFS_getRealDir(fullfile.c_str());
			path.append(file);
			PHYSFS_mount(path.c_str(), "/data/", 0);
		}
	}
	PHYSFS_freeList(files);

	glfwInit();

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

	glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

	// create windowed monitor
	GLFWwindow* window = glfwCreateWindow(ScreenWidth, ScreenHeight, "OG Q3 BSP Renderer", nullptr, nullptr);

	glfwMakeContextCurrent(window);

	glewExperimental = GL_TRUE;
	glewInit();

	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 150");

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glFrontFace(GL_CW);
	glEnable(GL_DEPTH_TEST);

	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

	// needs a valid Q3A BSP file.
	BSPLoader loader{ file, SingleDraw };

	std::vector<vertex> vertices = loader.get_vertex_data();
	
	// generate and bind array and buffer objects.
	GLuint vao;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	GLuint vbo;
	glGenBuffers(1, &vbo);

	GLuint ebo;
	glGenBuffers(1, &ebo);

	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(vertex), &vertices[0], GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	auto elements = loader.get_indices();
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, elements.size() * sizeof(unsigned int), &elements[0], GL_STATIC_DRAW);

	// load and compile vertex and frag shaders

	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertexSource, NULL);

	glCompileShader(vertexShader);

	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragmentSource, NULL);

	glCompileShader(fragmentShader);

	GLuint shaderProgram = glCreateProgram();
	glAttachShader(shaderProgram, vertexShader);
	glAttachShader(shaderProgram, fragmentShader);

	glBindFragDataLocation(shaderProgram, 0, "outColor");

	glLinkProgram(shaderProgram);

	GLint isLinked = 0;
	glGetProgramiv(shaderProgram, GL_LINK_STATUS, &isLinked);

	if (isLinked == GL_FALSE)
	{
		GLint maxLength = 0;
		glGetProgramiv(shaderProgram, GL_INFO_LOG_LENGTH, &maxLength);

		std::vector<GLchar> infoLog(maxLength);
		glGetProgramInfoLog(shaderProgram, maxLength, &maxLength, &infoLog[0]);

		// The program is useless now. So delete it.
		glDeleteProgram(shaderProgram);

		// Provide the infolog in whatever manner you deem best.
		// Exit with failure.
		return 0;
	}

	glUseProgram(shaderProgram);

	// vert shader attributes - see vertex struct in BSPLoader.h for specifics
	GLint posAttrib = glGetAttribLocation(shaderProgram, "position");
	glVertexAttribPointer(posAttrib, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), 0);

	GLint colAttrib = glGetAttribLocation(shaderProgram, "colour");
	glVertexAttribPointer(colAttrib, 4, GL_UNSIGNED_BYTE, GL_TRUE,
		sizeof(vertex), (void*)(10 * sizeof(float)));

	GLint uvAttrib = glGetAttribLocation(shaderProgram, "texcoord");
	glVertexAttribPointer(uvAttrib, 2, GL_FLOAT, GL_FALSE,
		sizeof(vertex), (void*)(3 * sizeof(float)));

	GLint lmAttrib = glGetAttribLocation(shaderProgram, "lmcoord");
	glVertexAttribPointer(lmAttrib, 2, GL_FLOAT, GL_FALSE,
		sizeof(vertex), (void*)(5 * sizeof(float)));

	glEnableVertexAttribArray(posAttrib);
	glEnableVertexAttribArray(colAttrib);
	glEnableVertexAttribArray(uvAttrib);
	glEnableVertexAttribArray(lmAttrib);

	int faceCount = loader.get_face_count();

	if (AllowMouse)
	{
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		glfwSetCursorPosCallback(window, mouse_callback);
	}

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		processInput(window);

		// build matrices for view, projection and model
		glm::mat4 view = glm::lookAt(
			cameraPos,
			cameraPos + cameraFront,
			cameraUp
		);

		GLint uniView = glGetUniformLocation(shaderProgram, "view");
		glUniformMatrix4fv(uniView, 1, GL_FALSE, glm::value_ptr(view));

		glm::mat4 proj = glm::perspective(glm::radians(60.0f), (float)ScreenWidth / ScreenHeight, 1.0f, 10000.0f);
		GLint uniProj = glGetUniformLocation(shaderProgram, "proj");
		glUniformMatrix4fv(uniProj, 1, GL_FALSE, glm::value_ptr(proj));

		// need to rotate the world by -90 in x to line everything up nicely.
		glm::mat4 model = glm::mat4(1.0);
		model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
		GLint modelProj = glGetUniformLocation(shaderProgram, "model");
		glUniformMatrix4fv(modelProj, 1, GL_FALSE, glm::value_ptr(model));

		ImGui::Render();
		glViewport(0, 0, ScreenWidth, ScreenHeight);
		glClearColor(clear_color.x, clear_color.y, clear_color.z, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		if (!SingleDraw)
		{
			// render each face individually - this currently leads to holes in the mesh
			// but is probably the necessary approach to correctly render lightmaps + textures.
			for (int i = 0; i < faceCount; ++i)
			{
				face _face = loader.get_face(i);
				if (_face.type == 1 || _face.type == 3)
				{
					int lm = _face.lm_index;
					shader _shader = loader.get_shader(_face.texture);
					if (!_shader.render || _shader.transparent) continue; // don't render transparent surfaces yet!
					if (lm < 0) lm = loader.get_default_lightmap(); // right now, we'll assign a "default" lightmap to a surface without a valid index.

					if (_face.effect >= 0)
						continue;

					GLuint texId = loader.get_lightmap_tex(lm);

					glActiveTexture(GL_TEXTURE0);
					glBindTexture(GL_TEXTURE_2D, _shader.id);

					glActiveTexture(GL_TEXTURE1);
					glBindTexture(GL_TEXTURE_2D, texId);

					glDrawElements(GL_TRIANGLES, _face.n_meshverts, GL_UNSIGNED_INT, (void*)(long)(_face.meshvert * sizeof(GLuint)));
				}

			}
		}
		else
		{
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, loader.get_lm_id());
			// just draw everything in one fell swoop - all renders correctly, but can't easily do lightmaps this way!
			glDrawElements(GL_TRIANGLES, elements.size(), GL_UNSIGNED_INT, 0);
		}

		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
			glfwSetWindowShouldClose(window, GL_TRUE);

		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(window);
	}

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwTerminate();
}