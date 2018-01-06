#include <iostream>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include <unistd.h>

// Image code, for textures
struct Image {
  int sizeX, sizeY;
  char *data;
};

struct RGB {
  GLubyte r, g, b;
};

GLFWwindow *window; // Main application window

// Location of camera
float camLocation[] = {
  0.f, 0.f, 1.f
};

std::vector<float> posBuf;
std::vector<float> norBuf;
std::vector<float> texCoordBuf;
std::vector<unsigned int> eleBuf;

// Buffer IDs
unsigned vaoID;
unsigned posBufID;
unsigned eleBufID;
unsigned texCoordBufID;
unsigned texBufID;

// Shader program
GLuint pid;

// Shader attribs
GLint vertPosLoc;

// Shader uniforms
GLint perspectiveLoc;
GLint placementLoc;

// Texture testing
GLint texCoordLoc;
GLint texLoc;

// Height of window ???
int g_width, g_height;

// TESTING
float yRot = 0.f;

static const float posArr[] = {
  1.f, 1.f, 0.f,
  1.f, -1.f, 0.f,
  -1.f, 1.f, 0.f,
  -1.f, -1.f, 0.f
};

static const float texCoordArr[] = {
  1.f, 1.f,
  1.f, 0.f,
  0.f, 1.f,
  0.f, 0.f
};

static const unsigned int eleArr[] = {
  3, 0, 2,
  3, 1, 0
};

// Helper functions for image load
static unsigned int getint(FILE *fp) {
  int c, c1, c2, c3;
	
  // Get 4 bytes
  c = getc(fp);
  c1 = getc(fp);
  c2 = getc(fp);
  c3 = getc(fp);

	return ((unsigned int) c) + (((unsigned int) c1) << 8) +
	(((unsigned int) c2) << 16) + (((unsigned int) c3) << 24);
}

static unsigned int getshort(FILE *fp) {
	int c, c1;
	
	// Get 2 bytes
	c = getc(fp);
	c1 = getc(fp);
	
	return ((unsigned int) c) + (((unsigned int) c1) << 8);
}

int imageLoad(const char *filename, Image *image) {
	FILE *file;
	unsigned long size; // Size of the image in bytes
	unsigned long i; // Standard counter
	unsigned short int planes; // Number of planes in image (must be 1)
	unsigned short int bpp; // Number of bits per pixel (must be 24)
	char temp; // Used to convert bgr to rgb color
	
	// Make sure the file is there
	if((file = fopen(filename, "rb")) == NULL) {
		printf("File Not Found : %s\n", filename);
		return 0;
	}
	
	// Seek through the bmp header, up to the width height:
	fseek(file, 18, SEEK_CUR);
	
	// No 100% errorchecking anymore!!!
	
	// Read the width
  image->sizeX = getint(file);
	
	// Read the height
	image->sizeY = getint(file);
	
	// Calculate the size (assuming 24 bits or 3 bytes per pixel)
	size = image->sizeX * image->sizeY * 3;
	
	// Read the planes
	planes = getshort(file);
	if(planes != 1) {
		printf("Planes from %s is not 1: %u\n", filename, planes);
		return 0;
	}
	
	// Read the bpp
	bpp = getshort(file);
	if(bpp != 24) {
		printf("Bpp from %s is not 24: %u\n", filename, bpp);
		return 0;
	}
	
	// Seek past the rest of the bitmap header
	fseek(file, 24, SEEK_CUR);
	
	// Read the data
	image->data = (char *) malloc(size);
	if(image->data == NULL) {
		printf("Error allocating memory for color-corrected image data");
		return 0;
	}
	
	if((i = fread(image->data, size, 1, file)) != 1) {
		printf("Error reading image data from %s.\n", filename);
		return 0;
	}
	
	for(i = 0; i < size; i += 3) { // Reverse all of the colors (bgr -> rgb)
		temp = image->data[i];
		image->data[i] = image->data[i+2];
		image->data[i+2] = temp;
	}
	
	fclose(file); // Close the file and release the filedes
	
	// We're done
	return 1;
}

// For debugging
void printMatrix(glm::mat4 mat) {
  int i, j;
  for(i = 0; i < 4; i++) {
    for(j = 0; j < 4; j++) {
      printf("mat[%d][%d]: %f\n", i, j, mat[i][j]);
    }
  }
  printf("\n");
}

static void error_callback(int error, const char *description) {
  std::cerr << description << std::endl;
}

static void key_callback(GLFWwindow *window, int key, int scancode, int action, 
  int mods) {
  if(key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
    glfwSetWindowShouldClose(window, GL_TRUE);
  }
}

static void resize_callback(GLFWwindow *window, int width, int height) {
  g_width = width;
  g_height = height;
  glViewport(0, 0, width, height);
}

char *textfileRead(const char *fn) {
  FILE *fp;
  char *content = NULL;
  int count = 0;
  if(fn != NULL) {
    fp = fopen(fn, "rt");
    if(fp != NULL) {
      fseek(fp, 0, SEEK_END);
      count = (int) ftell(fp);
      rewind(fp);
      if(count > 0) {
        content = (char *) malloc(sizeof(char) * (count + 1));
        count = (int) fread(content, sizeof(char), count, fp);
        content[count] = '\0';
      }
      fclose(fp);
    } else {
      printf("error loading %s\n", fn);
    }
  }
  return content;
}

static void resizeMesh(std::vector<float>& posBuf) {
  float minX, minY, minZ;
  float maxX, maxY, maxZ;
  float scaleX, scaleY, scaleZ;
  float shiftX, shiftY, shiftZ;
  float epsilon = 0.001;

  minX = minY = minZ = 1.1754E+38F;
  maxX = maxY = maxZ = -1.1754E+38F;

  //Go through all vertices to determine min and max of each dimension
  for(size_t v = 0; v < posBuf.size() / 3; v++) {
    if(posBuf[3*v+0] < minX) minX = posBuf[3*v+0];
    if(posBuf[3*v+0] > maxX) maxX = posBuf[3*v+0];

    if(posBuf[3*v+1] < minY) minY = posBuf[3*v+1];
    if(posBuf[3*v+1] > maxY) maxY = posBuf[3*v+1];

    if(posBuf[3*v+2] < minZ) minZ = posBuf[3*v+2];
    if(posBuf[3*v+2] > maxZ) maxZ = posBuf[3*v+2];
	}

	//From min and max compute necessary scale and shift for each dimension
  float maxExtent, xExtent, yExtent, zExtent;
  
  xExtent = maxX-minX;
  yExtent = maxY-minY;
  zExtent = maxZ-minZ;

  if(xExtent >= yExtent && xExtent >= zExtent) {
    maxExtent = xExtent;
  }

  if(yExtent >= xExtent && yExtent >= zExtent) {
    maxExtent = yExtent;
  }

  if(zExtent >= xExtent && zExtent >= yExtent) {
    maxExtent = zExtent;
  }

  scaleX = 2.0 /maxExtent;
  shiftX = minX + (xExtent/ 2.0);
  scaleY = 2.0 / maxExtent;
  shiftY = minY + (yExtent / 2.0);
  scaleZ = 2.0/ maxExtent;
  shiftZ = minZ + (zExtent)/2.0;

  //Go through all verticies shift and scale them
	for(size_t v = 0; v < posBuf.size() / 3; v++) {
    posBuf[3*v+0] = (posBuf[3*v+0] - shiftX) * scaleX;
    assert(posBuf[3*v+0] >= -1.0 - epsilon);
    assert(posBuf[3*v+0] <= 1.0 + epsilon);
    posBuf[3*v+1] = (posBuf[3*v+1] - shiftY) * scaleY;
    assert(posBuf[3*v+1] >= -1.0 - epsilon);
    assert(posBuf[3*v+1] <= 1.0 + epsilon);
    posBuf[3*v+2] = (posBuf[3*v+2] - shiftZ) * scaleZ;
    assert(posBuf[3*v+2] >= -1.0 - epsilon);
    assert(posBuf[3*v+2] <= 1.0 + epsilon);
  }
}

static void getMesh(const std::string &meshName) {
  /* std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> objMaterials;
	std::string errStr;
	bool rc = tinyobj::LoadObj(shapes, objMaterials, errStr, meshName.c_str());
	if(!rc) {
		std::cerr << errStr << std::endl;
    exit(0);
	} else {
		posBuf = shapes[0].mesh.positions;
    texCoordBuf = shapes[0].mesh.texcoords;
		eleBuf = shapes[0].mesh.indices;
	} */

  copy(&posArr[0], &posArr[12], back_inserter(posBuf));
  copy(&texCoordArr[0], &texCoordArr[8], back_inserter(texCoordBuf));
  copy(&eleArr[0], &eleArr[6], back_inserter(eleBuf));
}

static void sendMesh() {
  // Send vertex position array to GPU
  glGenBuffers(1, &posBufID);
  glBindBuffer(GL_ARRAY_BUFFER, posBufID);
  glBufferData(GL_ARRAY_BUFFER, posBuf.size() * sizeof(float), &posBuf[0],
    GL_STATIC_DRAW);

  // Error if texture buffer is empty
  if(texCoordBuf.empty()) {
    fprintf(stderr, "Could not find texture coordinate buffer.\n");
    exit(0);
  }

  // Send texture coordinate array to GPU
  glGenBuffers(1, &texCoordBufID);
  glBindBuffer(GL_ARRAY_BUFFER, texCoordBufID);
  glBufferData(GL_ARRAY_BUFFER, texCoordBuf.size() * sizeof(float),
    &texCoordBuf[0], GL_STATIC_DRAW);

  // Send element array to GPU
  glGenBuffers(1, &eleBufID);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, eleBufID);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, eleBuf.size() * sizeof(unsigned),
    &eleBuf[0], GL_STATIC_DRAW);

  // Unbind arrays
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

static void init() {
  // Set background color
  glClearColor(.25f, .75f, 1.f, 0.f);
  // Enable z-buffer test ???
  glEnable(GL_DEPTH_TEST);

  // Something about blending ???
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_BLEND);

  // Get mesh
  // getMesh("../resources/sphere.obj");
  getMesh("../resources/cube.obj");
  resizeMesh(posBuf);

  // Send mesh to GPU
  sendMesh();

  // Read texture into CPU memory
  struct Image image;
  imageLoad("../resources/world.bmp", &image);

  // Load the texture into the GPU

  // Set the first texture unit as active
  glActiveTexture(GL_TEXTURE0);
  // Generate texture buffer object
  glGenTextures(1, &texBufID);
  // Bind current texture unit to texture buffer object as a GL_TEXTURE_2D
  glBindTexture(GL_TEXTURE_2D, texBufID);
  // Load texture data into texBufID
  // Base level is 0, number of channels is 3, and border is 0
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, image.sizeX, image.sizeY,
    0, GL_RGB, GL_UNSIGNED_BYTE, (GLubyte *) image.data);

  // Generate image pyramid
  glGenerateMipmap(GL_TEXTURE_2D);
  // Set texture wrap modes for S and T directions
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  // Set filtering mode for magnification and minification
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  // glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
    GL_LINEAR_MIPMAP_LINEAR);

  // Unbind from texture buffer object from current texture unit
  glBindTexture(GL_TEXTURE_2D, 0);

  // Initialize shader program
  GLint rc;

  // Create shader handles
  GLuint vsHandle = glCreateShader(GL_VERTEX_SHADER);
  GLuint fsHandle = glCreateShader(GL_FRAGMENT_SHADER);

  // Read shader source code
  const char *vsSource = textfileRead("../resources/vertexShader.glsl");
  const char *fsSource = textfileRead("../resources/fragmentShader.glsl");

  glShaderSource(vsHandle, 1, &vsSource, NULL);
  glShaderSource(fsHandle, 1, &fsSource, NULL);

  // Compile vertex shader
  glCompileShader(vsHandle);
  glGetShaderiv(vsHandle, GL_COMPILE_STATUS, &rc);
  
  if(!rc) {
    std::cout << "Error compiling vertex shader" << std::endl;
    exit(EXIT_FAILURE);
  }

  // Compile fragment shader
  glCompileShader(fsHandle);
  glGetShaderiv(fsHandle, GL_COMPILE_STATUS, &rc);

  if(!rc) {
    std::cout << "Error compiling fragment shader" << std::endl;
    exit(EXIT_FAILURE);
  }

  // Create program and link
  pid = glCreateProgram();
  glAttachShader(pid, vsHandle);
  glAttachShader(pid, fsHandle);
  glLinkProgram(pid);
  glGetProgramiv(pid, GL_LINK_STATUS, &rc);

  if(!rc) {
    std::cout << "Error linking shaders" << std::endl;
    exit(EXIT_FAILURE);
  }

  // Attribs
  vertPosLoc = glGetAttribLocation(pid, "vertPos");
  texCoordLoc = glGetAttribLocation(pid, "texCoord");

  // Create vertex array object
  glGenVertexArrays(1, &vaoID);
  glBindVertexArray(vaoID);

  // Bind position buffer
  glEnableVertexAttribArray(vertPosLoc);
  glBindBuffer(GL_ARRAY_BUFFER, posBufID);
  glVertexAttribPointer(vertPosLoc, 3, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT) * 3,
    (const void *) 0);

  // Bind texture coordinate buffer
  glEnableVertexAttribArray(texCoordLoc);
  glBindBuffer(GL_ARRAY_BUFFER, texCoordBufID);
  glVertexAttribPointer(texCoordLoc, 2, GL_FLOAT, GL_FALSE, 
    sizeof(GL_FLOAT) * 2, (const void *) 0);

  // Bind element buffer
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, eleBufID);

  // Unbind vertex array object
  glBindVertexArray(0);

  // Disable
  glDisableVertexAttribArray(vertPosLoc);
  glDisableVertexAttribArray(texCoordLoc);

  // Unbind GPU buffers
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

  // Matrices to pass to vertex shaders
  perspectiveLoc = glGetUniformLocation(pid, "perspective");
  placementLoc = glGetUniformLocation(pid, "placement");

  // Get the location of the sampler2D in fragment shader (???)
  texLoc = glGetUniformLocation(pid, "tex");
}

static void render() {
  int width, height;
  float aspect;

  // Create matrices
  glm::mat4 matPlacement;
  glm::mat4 matPerspective;

  // Get current frame buffer size ???
  glfwGetFramebufferSize(window, &width, &height);
  glViewport(0, 0, width, height);

  // Clear framebuffer
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // Perspective matrix
  aspect = width / (float) height;
  matPerspective = glm::perspective(70.f, aspect, .1f, 100.f);

  // Placement matrix
  matPlacement = glm::mat4(1.f);
  matPlacement = glm::scale(glm::mat4(1.f),
    glm::vec3(1.f, 1.f, 1.f)) * 
    matPlacement;
  // TESTING
  matPlacement = glm::rotate(glm::mat4(1.f), yRot,
    glm::vec3(0.f, 1.f, 0.f)) * matPlacement;
  // yRot = yRot + 0.01;
  matPlacement = glm::translate(glm::mat4(1.f), 
    glm::vec3(-camLocation[0], -camLocation[1], -camLocation[2])) *
    matPlacement;
  camLocation[2] = camLocation[2] + 0.01;

  // Bind shader program
  glUseProgram(pid);

  // Fill in matrices
  glUniformMatrix4fv(perspectiveLoc, 1, GL_FALSE,
    glm::value_ptr(matPerspective));
  glUniformMatrix4fv(placementLoc, 1, GL_FALSE,
    glm::value_ptr(matPlacement));

  // Bind vertex array object
  glBindVertexArray(vaoID);

  // Bind texture to texture unit 0
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texBufID);
  glUniform1i(texLoc, 0);

  // Draw one object
  glDrawElements(GL_TRIANGLES, (int) eleBuf.size(), GL_UNSIGNED_INT,
    (const void *) 0);

  // Unbind texture
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, 0);

  // Unbind vertex array object
  glBindVertexArray(0);

  // Unbind shader program
  glUseProgram(0);
}

int main(int argc, char **argv) {
  // What function to call when there is an error
  glfwSetErrorCallback(error_callback);

  // Initialize GLFW
  if(glfwInit() == false) {
    return -1;
  }

  // ???
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);

  // Create a windowed mode window and (?) its OpenGL context. (?)
  window = glfwCreateWindow(640, 480, "Some title", NULL, NULL);
  if(window == false) {
    glfwTerminate();
    return -1;
  }
  glfwMakeContextCurrent(window);

  // Initialize GLEW
  glewExperimental = true;
  if(glewInit() != GLEW_OK) {
    std::cerr << "Failed to initialize GLEW" << std::endl;
  }

  // Bootstrap ???
  glGetError();
  std::cout << "OpenGL version: " << glGetString(GL_VERSION) << std::endl;
  std::cout << "GLSL version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) <<
    std::endl;

  // Set vsync ???
  glfwSwapInterval(1);
  // Set callback(s) for window
  glfwSetKeyCallback(window, key_callback);
  glfwSetFramebufferSizeCallback(window, resize_callback);

  // TESTING
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glFrontFace(GL_CCW);

  // Initialize scene
  init();

  // Loop until the user closes the window
  while(!glfwWindowShouldClose(window)) {
    // Render scene
    render();
    // Swap front and back buffers
    glfwSwapBuffers(window);

    // Poll for and process events
    glfwPollEvents();
  }

  // Quit program
  glfwDestroyWindow(window);
  glfwTerminate();

  return 0;
}

