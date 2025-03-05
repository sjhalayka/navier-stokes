// https://claude.ai/chat/150e7512-37b3-4a6d-aa2a-05072caface4

#include <GL/glew.h>
#include <GL/glut.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <iomanip>
#include <fstream>
#include <sstream>
using namespace std;

#pragma comment(lib, "freeglut")
#pragma comment(lib, "glew32")



// Simulation parameters
int WIDTH = 960;
int HEIGHT = 540;

const float DT = 1.0f / 60.0f;
const float VISCOSITY = 0.5f;     // Fluid viscosity
const float DIFFUSION = 0.5f;    //  diffusion rate
const float FORCE = 5000.0f;         // Force applied by mouse
const float OBSTACLE_RADIUS = 0.1f; // Radius of obstacle
const float COLLISION_THRESHOLD = 0.5f; // Threshold for color-obstacle collision
const int REPORT_INTERVAL = 60;   // Report collision locations every N frames

const float COLOR_DETECTION_THRESHOLD = 0.1f;  // How strict the color matching should be


bool red_mode = true;

bool lastRightMouseDown = false;



struct StampInfo {
	bool active;
	float posX, posY;  // Normalized position (0-1)
	float width, height;  // In pixels

	StampInfo() : active(false), posX(0), posY(0), width(0), height(0) {}
};

// Add this to the other global variables
std::vector<StampInfo> stamps;
//StampInfo currentStamp;



// OpenGL variables
GLuint velocityTexture[2];
GLuint pressureTexture[2];
GLuint divergenceTexture;
GLuint obstacleTexture;
GLuint collisionTexture;
GLuint colorTexture[2];  // Ping-pong buffers for color
int colorIndex = 0;      // Index for current color texture
GLuint friendlyColorTexture[2];  // Second set of color textures for friendly fire
int friendlyColorIndex = 0;      // Index for current friendly color texture

GLuint backgroundTexture;


GLuint advectProgram;
GLuint divergenceProgram;
GLuint pressureProgram;
GLuint gradientSubtractProgram;
GLuint addForceProgram;
GLuint addObstacleProgram;
GLuint detectCollisionProgram;
GLuint addColorProgram;
GLuint diffuseColorProgram;
GLuint diffuseVelocityProgram;

GLuint vao, vbo;
GLuint fbo;

// Mouse state
int mouseX = 0, mouseY = 0;
int prevMouseX = 0, prevMouseY = 0;
bool mouseDown = false;
bool rightMouseDown = false;

// Texture swap utilities
int velocityIndex = 0;
int pressureIndex = 0;

// Collision tracking
int frameCount = 0;
bool reportCollisions = true;

// Define a struct for collision data
struct CollisionPoint {
	int x, y;
	enum Type { RED, BLUE, BOTH, OTHER } type;

	CollisionPoint(int _x, int _y, Type _type) : x(_x), y(_y), type(_type) {}
};


GLuint loadTexture(const char* filename) {
	GLuint textureID;
	glGenTextures(1, &textureID);

	// Load image using stb_image
	int width, height, channels;
	unsigned char* data = stbi_load(filename, &width, &height, &channels, 0);

	if (!data) {
		std::cerr << "Failed to load texture: " << filename << std::endl;
		std::cerr << "STB Image error: " << stbi_failure_reason() << std::endl;

		//// Create a default checkerboard pattern as fallback
		//width = 256;
		//height = 256;
		//channels = 3;
		//unsigned char* fallback = new unsigned char[width * height * channels];
		//for (int y = 0; y < height; y++) {
		//	for (int x = 0; x < width; x++) {
		//		int idx = (y * width + x) * channels;
		//		bool isEven = ((x / 32) + (y / 32)) % 2 == 0;
		//		fallback[idx] = isEven ? 200 : 50;     // R
		//		fallback[idx + 1] = isEven ? 200 : 50; // G
		//		fallback[idx + 2] = isEven ? 200 : 50; // B
		//	}
		//}
		//data = fallback;
	}

	// Bind and set texture parameters
	glBindTexture(GL_TEXTURE_2D, textureID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// Determine format based on channels
	GLenum format;
	switch (channels) {
	case 1: format = GL_RED; break;
	case 3: format = GL_RGB; break;
	case 4: format = GL_RGBA; break;
	default:
		format = GL_RGB;
		std::cerr << "Unsupported number of channels: " << channels << std::endl;
	}

	// Load texture data to GPU
	glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);

	// Generate mipmaps
	glGenerateMipmap(GL_TEXTURE_2D);

	// Free image data
	if (stbi_failure_reason()) {
		delete[] data; // Free our fallback data
	}
	else {
		stbi_image_free(data); // Free stb_image data
	}

	return textureID;
}



std::vector<CollisionPoint> collisionPoints; //std::vector<std::pair<int, int>> collisionLocations;
//std::vector<std::pair<int, int>> collisionLocations;


GLuint stampObstacleProgram;
GLuint stampTexture = 0;
int stampWidth = 0;
int stampHeight = 0;
bool stampTextureLoaded = false;






const char* diffuseVelocityFragmentShader = R"(
#version 330 core
uniform sampler2D velocityTexture;
uniform sampler2D obstacleTexture;
uniform vec2 texelSize;
uniform float viscosity;
uniform float dt;
out vec4 FragColor;
const float fake_dispersion = 0.99;

in vec2 TexCoord;

void main() {
    // Check if we're in an obstacle
    float obstacle = texture(obstacleTexture, TexCoord).r;
    if (obstacle > 0.0) {
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    // Simple diffusion using 5-point stencil
    vec2 center = texture(velocityTexture, TexCoord).xy;
    vec2 left = texture(velocityTexture, TexCoord - vec2(texelSize.x, 0.0)).xy;
    vec2 right = texture(velocityTexture, TexCoord + vec2(texelSize.x, 0.0)).xy;
    vec2 bottom = texture(velocityTexture, TexCoord - vec2(0.0, texelSize.y)).xy;
    vec2 top = texture(velocityTexture, TexCoord + vec2(0.0, texelSize.y)).xy;
    
    // Check if sampling from obstacles
    float oLeft = texture(obstacleTexture, TexCoord - vec2(texelSize.x, 0.0)).r;
    float oRight = texture(obstacleTexture, TexCoord + vec2(texelSize.x, 0.0)).r;
    float oBottom = texture(obstacleTexture, TexCoord - vec2(0.0, texelSize.y)).r;
    float oTop = texture(obstacleTexture, TexCoord + vec2(0.0, texelSize.y)).r;
    
    if (oLeft > 0.0) left = center;
    if (oRight > 0.0) right = center;
    if (oBottom > 0.0) bottom = center;
    if (oTop > 0.0) top = center;
    
    // Compute Laplacian
    vec2 laplacian = (left + right + bottom + top - 4.0 * center);
    
    // Apply diffusion
    vec2 result = center + viscosity * dt * laplacian;
    
    FragColor = fake_dispersion*vec4(result, 0.0, 1.0);
}
)";



const char* stampObstacleFragmentShader = R"(
#version 330 core
uniform sampler2D obstacleTexture;
uniform sampler2D stampTexture;
uniform vec2 position;
uniform vec2 stampSize;
uniform float threshold;
out float FragColor;

in vec2 TexCoord;

void main() {
    // Get current obstacle value
    float obstacle = texture(obstacleTexture, TexCoord).r;
    
    // Calculate coordinates in stamp texture
    vec2 stamp_size = textureSize(stampTexture, 0)/2;

    vec2 stampCoord = (TexCoord - position) * textureSize(obstacleTexture, 0) / stamp_size + vec2(0.5);

    vec2 adjustedCoord = stampCoord;

    ivec2 obstacle_tex_size = textureSize(obstacleTexture, 0);    

    float aspect_ratio = float(obstacle_tex_size.x) / float(obstacle_tex_size.y);

    // For non-square textures, adjust sampling to prevent stretching
    if (aspect_ratio > 1.0) {
        adjustedCoord.x = (adjustedCoord.x - 0.5) / aspect_ratio + 0.5;
    } else if (aspect_ratio < 1.0) {
        adjustedCoord.y = (adjustedCoord.y - 0.5) * aspect_ratio + 0.5;
    }    

    stampCoord = adjustedCoord;

    // Check if we're within stamp bounds
    if (stampCoord.x >= 0.0 && stampCoord.x <= 1.0 && 
        stampCoord.y >= 0.0 && stampCoord.y <= 1.0) {
        
        float stampValue = texture(stampTexture, stampCoord).a;

        // Apply threshold to make it binary
        stampValue = stampValue > threshold ? 1.0 : 0.0;
        
        // CRITICAL: Combine with existing obstacle using max for union
        // This ensures we preserve all obstacles when adding a new one
        obstacle = max(obstacle, stampValue);
    }
    
    FragColor = obstacle;
}
)";








const char* diffuseColorFragmentShader = R"(
#version 330 core
uniform sampler2D colorTexture;
uniform sampler2D obstacleTexture;
uniform vec2 texelSize;
uniform float diffusionRate;
uniform float dt;
out float FragColor;

in vec2 TexCoord;
const float fake_dispersion = 0.99;

void main() {
    // Check if we're in an obstacle
    float obstacle = texture(obstacleTexture, TexCoord).r;
    if (obstacle > 0.0) {
        FragColor = 0.0;
        return;
    }



    // Simple diffusion using 5-point stencil
    float center = texture(colorTexture, TexCoord).r;
    float left = texture(colorTexture, TexCoord - vec2(texelSize.x, 0.0)).r;
    float right = texture(colorTexture, TexCoord + vec2(texelSize.x, 0.0)).r;
    float bottom = texture(colorTexture, TexCoord - vec2(0.0, texelSize.y)).r;
    float top = texture(colorTexture, TexCoord + vec2(0.0, texelSize.y)).r;
    
    // Check if sampling from obstacles
    float oLeft = texture(obstacleTexture, TexCoord - vec2(texelSize.x, 0.0)).r;
    float oRight = texture(obstacleTexture, TexCoord + vec2(texelSize.x, 0.0)).r;
    float oBottom = texture(obstacleTexture, TexCoord - vec2(0.0, texelSize.y)).r;
    float oTop = texture(obstacleTexture, TexCoord + vec2(0.0, texelSize.y)).r;
    
    if (oLeft > 0.0) left = center;
    if (oRight > 0.0) right = center;
    if (oBottom > 0.0) bottom = center;
    if (oTop > 0.0) top = center;
    
    // Compute Laplacian
    float laplacian = (left + right + bottom + top - 4.0 * center);
    
    // Apply diffusion
    float result = center + diffusionRate * dt * laplacian;
    
    // Clamp result to [0, 1]
    FragColor = fake_dispersion*clamp(result, 0.0, 1.0);
}
)";



// Add to the start of the file where other shaders are defined
const char* addColorFragmentShader = R"(
#version 330 core
uniform sampler2D colorTexture;
uniform sampler2D obstacleTexture;
uniform vec2 point;
uniform float radius;

out float FragColor;

in vec2 TexCoord;

void main() {
    // Check if we're in an obstacle
    float obstacle = texture(obstacleTexture, TexCoord).r;
    if (obstacle > 0.0) {
        FragColor = 0.0;
        return;
    }

    // Get current color intensity
    float currentValue = texture(colorTexture, TexCoord).r;
    
    // Calculate distance to application point
    float distance = length(TexCoord - point);
    
    // Apply color based on radius
    if (distance < radius) {
        // Apply with smooth falloff
        float falloff = 1.0 - (distance / radius);
        falloff = falloff * falloff;
        
        // Add intensity with falloff
        float newValue = falloff;
        FragColor = currentValue + newValue;
    } else {
        FragColor = currentValue;
    }
}
)";



// Inline GLSL shaders
const char* vertexShaderSource = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

void main() {
    gl_Position = vec4(aPos, 1.0);
    TexCoord = aTexCoord;
}
)";






const char* advectFragmentShader = R"(
#version 330 core
uniform sampler2D velocityTexture;
uniform sampler2D sourceTexture;
uniform sampler2D obstacleTexture;
uniform float dt;
uniform float gridScale;
uniform vec2 texelSize;

float WIDTH = texelSize.x;
float HEIGHT = texelSize.y;
float aspect_ratio = WIDTH/HEIGHT;

out vec4 FragColor;

in vec2 TexCoord;

void main() {
    // Check if we're in an obstacle
    float obstacle = texture(obstacleTexture, TexCoord).r;
    if (obstacle > 0.0) {
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    // Advection
    vec2 vel = texture(velocityTexture, TexCoord).xy;
    //vec2 pos = TexCoord - dt * vel * texelSize;
    vec2 pos = TexCoord - dt * vec2(vel.x * aspect_ratio, vel.y) * texelSize;

    // Sample the source texture at the back-traced position
    vec4 result = texture(sourceTexture, pos);
    
    // Boundary handling - don't advect from obstacles
    vec2 samplePos = pos;
    float obstacleSample = texture(obstacleTexture, samplePos).r;
    
    if (obstacleSample > 0.0) {
        // If we sampled from an obstacle, reflect the velocity
        result = vec4(-vel, 0.0, 1.0);
    }
    
    FragColor = result;
}
)";

const char* divergenceFragmentShader = R"(
#version 330 core
uniform sampler2D velocityTexture;
uniform sampler2D obstacleTexture;
uniform vec2 texelSize;
out float FragColor;

in vec2 TexCoord;

void main() {
    // Check if we're in an obstacle
    float obstacle = texture(obstacleTexture, TexCoord).r;
    if (obstacle > 0.0) {
        FragColor = 0.0;
        return;
    }

    // Calculate divergence using central differences
    vec2 right = texture(velocityTexture, TexCoord + vec2(texelSize.x, 0.0)).xy;
    vec2 left = texture(velocityTexture, TexCoord - vec2(texelSize.x, 0.0)).xy;
    vec2 top = texture(velocityTexture, TexCoord + vec2(0.0, texelSize.y)).xy;
    vec2 bottom = texture(velocityTexture, TexCoord - vec2(0.0, texelSize.y)).xy;
    
    // Check for obstacles in samples
    float oRight = texture(obstacleTexture, TexCoord + vec2(texelSize.x, 0.0)).r;
    float oLeft = texture(obstacleTexture, TexCoord - vec2(texelSize.x, 0.0)).r;
    float oTop = texture(obstacleTexture, TexCoord + vec2(0.0, texelSize.y)).r;
    float oBottom = texture(obstacleTexture, TexCoord - vec2(0.0, texelSize.y)).r;
    
    // Apply boundary conditions at obstacles
    if (oRight > 0.0) right = vec2(0.0, 0.0);
    if (oLeft > 0.0) left = vec2(0.0, 0.0);
    if (oTop > 0.0) top = vec2(0.0, 0.0);
    if (oBottom > 0.0) bottom = vec2(0.0, 0.0);
    
    float div = 0.5 * ((right.x - left.x) + (top.y - bottom.y));
    FragColor = div;
}
)";

const char* pressureFragmentShader = R"(
#version 330 core
uniform sampler2D pressureTexture;
uniform sampler2D divergenceTexture;
uniform sampler2D obstacleTexture;
uniform vec2 texelSize;
uniform float alpha;
uniform float rBeta;
out float FragColor;

in vec2 TexCoord;

void main() {
    // Check if we're in an obstacle
    float obstacle = texture(obstacleTexture, TexCoord).r;
    if (obstacle > 0.0) {
        FragColor = 0.0;
        return;
    }

    // Get pressure at neighboring cells
    float pRight = texture(pressureTexture, TexCoord + vec2(texelSize.x, 0.0)).r;
    float pLeft = texture(pressureTexture, TexCoord - vec2(texelSize.x, 0.0)).r;
    float pTop = texture(pressureTexture, TexCoord + vec2(0.0, texelSize.y)).r;
    float pBottom = texture(pressureTexture, TexCoord - vec2(0.0, texelSize.y)).r;
    
    // Check for obstacles in samples
    float oRight = texture(obstacleTexture, TexCoord + vec2(texelSize.x, 0.0)).r;
    float oLeft = texture(obstacleTexture, TexCoord - vec2(texelSize.x, 0.0)).r;
    float oTop = texture(obstacleTexture, TexCoord + vec2(0.0, texelSize.y)).r;
    float oBottom = texture(obstacleTexture, TexCoord - vec2(0.0, texelSize.y)).r;
    
    // Apply Neumann boundary conditions at obstacles
    if (oRight > 0.0) pRight = texture(pressureTexture, TexCoord).r;
    if (oLeft > 0.0) pLeft = texture(pressureTexture, TexCoord).r;
    if (oTop > 0.0) pTop = texture(pressureTexture, TexCoord).r;
    if (oBottom > 0.0) pBottom = texture(pressureTexture, TexCoord).r;
    
    // Divergence
    float div = texture(divergenceTexture, TexCoord).r;
    
    // Jacobi iteration step
    float pressure = (pLeft + pRight + pBottom + pTop + alpha * div) * rBeta;
    FragColor = pressure;
}
)";

const char* gradientSubtractFragmentShader = R"(
#version 330 core
uniform sampler2D pressureTexture;
uniform sampler2D velocityTexture;
uniform sampler2D obstacleTexture;
uniform vec2 texelSize;
uniform float scale;
out vec4 FragColor;

in vec2 TexCoord;

void main() {
    // Check if we're in an obstacle
    float obstacle = texture(obstacleTexture, TexCoord).r;
    if (obstacle > 0.0) {
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    // Compute pressure gradient
    float pRight = texture(pressureTexture, TexCoord + vec2(texelSize.x, 0.0)).r;
    float pLeft = texture(pressureTexture, TexCoord - vec2(texelSize.x, 0.0)).r;
    float pTop = texture(pressureTexture, TexCoord + vec2(0.0, texelSize.y)).r;
    float pBottom = texture(pressureTexture, TexCoord - vec2(0.0, texelSize.y)).r;
    
    // Check for obstacles in samples
    float oRight = texture(obstacleTexture, TexCoord + vec2(texelSize.x, 0.0)).r;
    float oLeft = texture(obstacleTexture, TexCoord - vec2(texelSize.x, 0.0)).r;
    float oTop = texture(obstacleTexture, TexCoord + vec2(0.0, texelSize.y)).r;
    float oBottom = texture(obstacleTexture, TexCoord - vec2(0.0, texelSize.y)).r;
    
    // Apply boundary conditions at obstacles
    if (oRight > 0.0) pRight = texture(pressureTexture, TexCoord).r;
    if (oLeft > 0.0) pLeft = texture(pressureTexture, TexCoord).r;
    if (oTop > 0.0) pTop = texture(pressureTexture, TexCoord).r;
    if (oBottom > 0.0) pBottom = texture(pressureTexture, TexCoord).r;
    
    // Calculate gradient
    vec2 gradient = vec2(pRight - pLeft, pTop - pBottom) * 0.5;
    
    // Get current velocity
    vec2 velocity = texture(velocityTexture, TexCoord).xy;
    
    // Subtract gradient from velocity
    vec2 result = velocity - scale * gradient;
    
    FragColor = vec4(result, 0.0, 1.0);
}
)";

const char* addForceFragmentShader = R"(
#version 330 core
uniform sampler2D velocityTexture;
uniform sampler2D obstacleTexture;
uniform vec2 point;
uniform vec2 direction;
uniform float radius;
uniform float strength;
out vec4 FragColor;

in vec2 TexCoord;

void main() {
    // Check if we're in an obstacle
    float obstacle = texture(obstacleTexture, TexCoord).r;
    if (obstacle > 0.0) {
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    // Get current velocity
    vec2 velocity = texture(velocityTexture, TexCoord).xy;
    
    // Calculate distance to force application point
    float distance = length(TexCoord - point);
    
    // Apply force based on radius
    if (distance < radius) {
        // Apply force with smooth falloff
        float falloff = 1.0 - (distance / radius);
        falloff = falloff * falloff;
        
        // Add force to velocity
        velocity += direction * strength * falloff;
    }
    
    FragColor = vec4(velocity, 0.0, 1.0);
}
)";


const char* addObstacleFragmentShader = R"(
#version 330 core
uniform sampler2D obstacleTexture;
uniform vec2 point;
uniform float radius;
uniform bool addObstacle;
out float FragColor;

in vec2 TexCoord;

void main() {
    // Get current obstacle value
    float obstacle = texture(obstacleTexture, TexCoord).r;
    
    // Calculate distance to obstacle point
    float distance = length(TexCoord - point);
    
    // Apply obstacle based on radius
    if (distance < radius) {
        if (addObstacle) {
            obstacle = 1.0;
        } else {
            obstacle = 0.0;
        }
    }
    
    FragColor = obstacle;
}
)";



const char* detectCollisionFragmentShader = R"(
#version 330 core
uniform sampler2D obstacleTexture;
uniform sampler2D colorTexture;
uniform sampler2D friendlyColorTexture;
uniform float collisionThreshold;
uniform float colorThreshold;  // Threshold for color detection
out vec4 FragColor;

in vec2 TexCoord;

void main() {
    // Get current obstacle value
    float obstacle = texture(obstacleTexture, TexCoord).r;
    
    if (obstacle > 0.0) {
        // We're in an obstacle - check neighboring pixels
        vec2 texelSize = 1.0 / vec2(textureSize(colorTexture, 0));
        
        // Check neighboring cells for color values
        float leftRed = texture(colorTexture, TexCoord - vec2(texelSize.x, 0.0)).r;
        float rightRed = texture(colorTexture, TexCoord + vec2(texelSize.x, 0.0)).r;
        float bottomRed = texture(colorTexture, TexCoord - vec2(0.0, texelSize.y)).r;
        float topRed = texture(colorTexture, TexCoord + vec2(0.0, texelSize.y)).r;
        
        float leftBlue = texture(friendlyColorTexture, TexCoord - vec2(texelSize.x, 0.0)).r;
        float rightBlue = texture(friendlyColorTexture, TexCoord + vec2(texelSize.x, 0.0)).r;
        float bottomBlue = texture(friendlyColorTexture, TexCoord - vec2(0.0, texelSize.y)).r;
        float topBlue = texture(friendlyColorTexture, TexCoord + vec2(0.0, texelSize.y)).r;
        
        // Check for obstacles in neighboring cells
        float leftObstacle = texture(obstacleTexture, TexCoord - vec2(texelSize.x, 0.0)).r;
        float rightObstacle = texture(obstacleTexture, TexCoord + vec2(texelSize.x, 0.0)).r;
        float bottomObstacle = texture(obstacleTexture, TexCoord - vec2(0.0, texelSize.y)).r;
        float topObstacle = texture(obstacleTexture, TexCoord + vec2(0.0, texelSize.y)).r;
        
        // Only consider colors from non-obstacle cells
        if(leftObstacle > 0.0) { leftRed = 0.0; leftBlue = 0.0; }
        if(rightObstacle > 0.0) { rightRed = 0.0; rightBlue = 0.0; }
        if(bottomObstacle > 0.0) { bottomRed = 0.0; bottomBlue = 0.0; }
        if(topObstacle > 0.0) { topRed = 0.0; topBlue = 0.0; }
        
        // Check if any neighboring cell has significant color
        float maxRed = max(max(leftRed, rightRed), max(bottomRed, topRed));
        float maxBlue = max(max(leftBlue, rightBlue), max(bottomBlue, topBlue));
        
        bool redCollision = (maxRed > colorThreshold);
        bool blueCollision = (maxBlue > colorThreshold);
        
        // Set collision with specific type information
        if (redCollision && blueCollision) {
            // Both red and blue collided
            FragColor = vec4(1.0, 0.0, 1.0, 1.0); // Magenta for both
        } else if (redCollision) {
            // Only red collided
            FragColor = vec4(1.0, 0.0, 0.0, 1.0); // Red
        } else if (blueCollision) {
            // Only blue collided
            FragColor = vec4(0.0, 0.0, 1.0, 1.0); // Blue
        } else {
            // No collision
            FragColor = vec4(0.0, 0.0, 0.0, 0.0);
        }
    } else {
        // Not in an obstacle - no collision
        FragColor = vec4(0.0, 0.0, 0.0, 0.0);
    }
}
)";


const char* renderFragmentShader = R"(
#version 330 core
uniform sampler2D obstacleTexture;
uniform sampler2D collisionTexture;
uniform sampler2D colorTexture;
uniform sampler2D friendlyColorTexture;
uniform sampler2D backgroundTexture;
uniform vec2 texelSize;
uniform float time;



float WIDTH = texelSize.x;
float HEIGHT = texelSize.y;
float aspect_ratio = WIDTH/HEIGHT;





out vec4 FragColor;

in vec2 TexCoord;

void main() {
    // Adjust texture coordinates based on aspect ratio
    vec2 adjustedCoord = TexCoord;
    
    // For non-square textures, adjust sampling to prevent stretching
    if (aspect_ratio > 1.0) {
        adjustedCoord.x = (adjustedCoord.x - 0.5) / aspect_ratio + 0.5;
    } else if (aspect_ratio < 1.0) {
        adjustedCoord.y = (adjustedCoord.y - 0.5) * aspect_ratio + 0.5;
    }
    
    // Check for collision at obstacle boundaries
    vec4 collision = texture(collisionTexture, adjustedCoord);
    if (collision.a > 0.0) {
        if (collision.r > 0.5 && collision.b > 0.5) {
            // Both red and blue collided - display as magenta
            FragColor = vec4(1.0, 0.0, 1.0, 1.0);
        } else if (collision.r > 0.5) {
            // Only red collided - display as orange (original color)
            FragColor = vec4(1.0, 0.6, 0.0, 1.0);
        } else if (collision.b > 0.5) {
            // Only blue collided - display as cyan
            FragColor = vec4(0.0, 0.8, 1.0, 1.0);
        } else {
            // Generic collision (shouldn't happen with your logic)
            FragColor = vec4(0.7, 0.7, 0.7, 1.0); // Gray
        }
        return;
    }
    
    // Check for obstacle
    float obstacle = texture(obstacleTexture, adjustedCoord).r;
    if (obstacle > 0.0) {
        // Render obstacles as white
        FragColor = vec4(1.0, 1.0, 1.0, 1.0);
        return;
    }
    
    // Get density and colors at adjusted position
	float redIntensity = texture(colorTexture, adjustedCoord).r;
	float blueIntensity = texture(friendlyColorTexture, adjustedCoord).r;

	float density = redIntensity + blueIntensity;

	// Create color vectors based on intensity
	vec4 redFluidColor = vec4(redIntensity, 0.0, 0.0, redIntensity);
	vec4 blueFluidColor = vec4(0.0, 0.0, blueIntensity, blueIntensity);

	// Combine both colors
	vec4 combinedColor = redFluidColor + blueFluidColor;
    

    vec2 adjustedCoord2 = TexCoord;
    
    // For non-square textures, adjust sampling to prevent stretching
    if (1.0/aspect_ratio > 1.0) {
        adjustedCoord2.x = (adjustedCoord2.x - 0.5) * aspect_ratio + 0.5;
    } else if (1.0/aspect_ratio < 1.0) {
        adjustedCoord2.y = (adjustedCoord2.y - 0.5) / aspect_ratio + 0.5;
    }


	vec2 scrolledCoord = adjustedCoord2;
	scrolledCoord.x += time * 0.01;




	vec4 color1 = texture(backgroundTexture, scrolledCoord);
    vec4 color2 = vec4(0.0, 0.125, 0.25, 1.0);
    vec4 color3 = combinedColor;
    vec4 color4 = vec4(0.0, 0.0, 0.0, 1.0);

	if(length(redFluidColor.r) > 0.5)
		color4 = vec4(0.0, 0.0, 0.0, 0.0);
    else
		color4 = vec4(1.0, 1.0, 1.0, 0.0);


///	FragColor = color3;

    if (density < 0.25) {
        FragColor = mix(color1, color2, density * 4.0);
    } else if (density < 0.5) {
        FragColor = mix(color2, color3, (density - 0.25) * 4.0);
    } else if (density < 0.75) {
        FragColor = mix(color3, color4, (density - 0.5) * 4.0);
    } else {
       FragColor = color4;
    }
}
)";




bool loadStampTexture(const char* filename) {
	// Clear previous texture if it exists
	if (stampTexture != 0) {
		glDeleteTextures(1, &stampTexture);
	}

	// Load image using stb_image
	int channels;

	stbi_set_flip_vertically_on_load(true);
	unsigned char* data = stbi_load(filename, &stampWidth, &stampHeight, &channels, 0);

	if (!data) {
		std::cerr << "Failed to load stamp texture: " << filename << std::endl;
		std::cerr << "STB Image error: " << stbi_failure_reason() << std::endl;
		return false;
	}

	// Create texture
	glGenTextures(1, &stampTexture);
	glBindTexture(GL_TEXTURE_2D, stampTexture);

	// Set texture parameters
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// Determine format based on channels
	GLenum format;
	switch (channels) {
	case 1: format = GL_RED; break;
	case 3: format = GL_RGB; break;
	case 4: format = GL_RGBA; break;
	default:
		format = GL_RGB;
		std::cerr << "Unsupported number of channels: " << channels << std::endl;
	}

	// Load texture data to GPU
	glTexImage2D(GL_TEXTURE_2D, 0, format, stampWidth, stampHeight, 0, format, GL_UNSIGNED_BYTE, data);

	// Free image data
	stbi_image_free(data);

	stampTextureLoaded = true;
	return true;
}


bool isCollisionInStamp(const CollisionPoint& point, const StampInfo& stamp) {
	if (!stamp.active) return false;

	float aspect = HEIGHT / float(WIDTH);

	// Convert pixel coordinates to normalized coordinates (0-1)
	float pointX = point.x / (float)WIDTH;
	float pointY = 1.0f - (point.y / (float)HEIGHT); // Invert Y for OpenGL

	// Apply aspect ratio correction to y-coordinate
	pointY = (pointY - 0.5f) * aspect + 0.5f;

	// Calculate stamp bounds in normalized coordinates
	float halfWidthNorm = (stamp.width / 2.0f) / WIDTH;
	float halfHeightNorm = ((stamp.height / 2.0f) / HEIGHT) * aspect;

	float stampMinX = stamp.posX - halfWidthNorm;
	float stampMaxX = stamp.posX + halfWidthNorm;
	float stampMinY = stamp.posY - halfHeightNorm;
	float stampMaxY = stamp.posY + halfHeightNorm;

	// Check if point is within stamp bounds (with a small margin)
	const float margin = 0.02f; // Add a small margin to detect collisions near the stamp
	return (pointX >= stampMinX - margin && pointX <= stampMaxX + margin &&
		pointY >= stampMinY - margin && pointY <= stampMaxY + margin);
}


void reportStampCollisions() {
	if (collisionPoints.empty()) {
		std::cout << "\n===== Stamp Collision Report =====" << std::endl;
		std::cout << "No collisions detected." << std::endl;
		std::cout << "=================================" << std::endl;
		return;
	}

	std::cout << "\n===== Stamp Collision Report =====" << std::endl;

	if (stamps.empty()) {
		std::cout << "No active stamps found. Use right mouse button to place stamps." << std::endl;
		std::cout << "=================================" << std::endl;
		return;
	}

	std::cout << "Number of active stamps: " << stamps.size() << std::endl;

	// Track overall collision statistics across all stamps
	int totalStampCollisions = 0;
	int totalRedStampCollisions = 0;
	int totalBlueStampCollisions = 0;
	int totalBothStampCollisions = 0;

	// Process each stamp
	for (size_t i = 0; i < stamps.size(); i++) {
		const auto& stamp = stamps[i];
		if (!stamp.active) continue;

		// Per-stamp collision counters
		int stampCollisions = 0;
		int redStampCollisions = 0;
		int blueStampCollisions = 0;
		int bothStampCollisions = 0;

		// Check all collision points against this stamp
		for (const auto& point : collisionPoints) {
			if (isCollisionInStamp(point, stamp)) {
				stampCollisions++;

				// Count by type
				switch (point.type) {
				case CollisionPoint::RED:
					redStampCollisions++;
					totalRedStampCollisions++;
					break;
				case CollisionPoint::BLUE:
					blueStampCollisions++;
					totalBlueStampCollisions++;
					break;
				case CollisionPoint::BOTH:
					bothStampCollisions++;
					totalBothStampCollisions++;
					break;
				default:
					break;
				}

				totalStampCollisions++;
			}
		}

		// Output the results for this stamp if it has any collisions
		if (stampCollisions > 0) {
			std::cout << "\nStamp #" << (i + 1) << ":" << std::endl;
			std::cout << "  Position: (" << stamp.posX << ", " << stamp.posY << ")" << std::endl;
			std::cout << "  Size: " << stamp.width << "x" << stamp.height << " pixels" << std::endl;
			std::cout << "  Collisions: " << stampCollisions << std::endl;
			std::cout << "  Red: " << redStampCollisions
				<< ", Blue: " << blueStampCollisions
				<< ", Both: " << bothStampCollisions << std::endl;
		}
	}

	// Output overall summary
	std::cout << "\nOverall Statistics:" << std::endl;
	std::cout << "  Total stamp collisions: " << totalStampCollisions << std::endl;
	std::cout << "  Red collisions: " << totalRedStampCollisions << std::endl;
	std::cout << "  Blue collisions: " << totalBlueStampCollisions << std::endl;
	std::cout << "  Both colors collisions: " << totalBothStampCollisions << std::endl;
	std::cout << "=================================" << std::endl;
}






void rebuildObstacleTexture() {
	// Skip if no stamps or texture not loaded
	if (stamps.empty() || !stampTextureLoaded) return;

	// Bind the FBO for rendering to obstacle texture
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, obstacleTexture, 0);

	// Clear the obstacle texture first
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	// Set up shader for rendering stamps
	glUseProgram(stampObstacleProgram);

	// Process each stamp
	for (size_t i = 0; i < stamps.size(); i++) {
		const auto& stamp = stamps[i];
		if (!stamp.active) continue;

		// Set uniforms for this stamp
		glUniform1i(glGetUniformLocation(stampObstacleProgram, "obstacleTexture"), 0);
		glUniform1i(glGetUniformLocation(stampObstacleProgram, "stampTexture"), 1);
		glUniform2f(glGetUniformLocation(stampObstacleProgram, "position"), stamp.posX, stamp.posY);
		glUniform2f(glGetUniformLocation(stampObstacleProgram, "stampSize"), stamp.width, stamp.height);
		glUniform1f(glGetUniformLocation(stampObstacleProgram, "threshold"), 0.5f);

		// Bind textures
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, obstacleTexture);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, stampTexture);

		// Render full-screen quad to add this stamp
		glBindVertexArray(vao);
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	}

	// Return to default framebuffer
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}






void applyBitmapObstacle() {
	if (!stampTextureLoaded) return;

	// Add a new stamp when right mouse button is first pressed
	if (rightMouseDown && !lastRightMouseDown) {
		float aspect = HEIGHT / float(WIDTH);

		// Get normalized mouse position (0 to 1 range)
		float mousePosX = mouseX / (float)WIDTH;
		float mousePosY = 1.0f - (mouseY / (float)HEIGHT); // Invert Y for OpenGL

		// Apply aspect ratio correction
		mousePosY = (mousePosY - 0.5f) * aspect + 0.5f;

		// Create a new stamp and add it to the vector
		StampInfo newStamp;
		newStamp.active = true;
		newStamp.posX = mousePosX;
		newStamp.posY = mousePosY;
		newStamp.width = stampWidth;
		newStamp.height = stampHeight;
		stamps.push_back(newStamp);

		std::cout << "Added new stamp #" << stamps.size() << " at position ("
			<< mousePosX << ", " << mousePosY << ")" << std::endl;

		// Ensure the obstacle texture is rebuilt entirely
		rebuildObstacleTexture();
	}

	lastRightMouseDown = rightMouseDown;
}




void diffuseVelocity() {
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, velocityTexture[1 - velocityIndex], 0);

	glUseProgram(diffuseVelocityProgram);

	// Set uniforms
	glUniform1i(glGetUniformLocation(diffuseVelocityProgram, "velocityTexture"), 0);
	glUniform1i(glGetUniformLocation(diffuseVelocityProgram, "obstacleTexture"), 1);
	glUniform2f(glGetUniformLocation(diffuseVelocityProgram, "texelSize"), 1.0f / WIDTH, 1.0f / HEIGHT);
	glUniform1f(glGetUniformLocation(diffuseVelocityProgram, "viscosity"), VISCOSITY);
	glUniform1f(glGetUniformLocation(diffuseVelocityProgram, "dt"), DT);

	// Bind textures
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, velocityTexture[velocityIndex]);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, obstacleTexture);

	// Render full-screen quad
	glBindVertexArray(vao);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	// Swap texture indices
	velocityIndex = 1 - velocityIndex;
}



void diffuseColor() {
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture[1 - colorIndex], 0);

	glUseProgram(diffuseColorProgram);

	// Set uniforms
	glUniform1i(glGetUniformLocation(diffuseColorProgram, "colorTexture"), 0);
	glUniform1i(glGetUniformLocation(diffuseColorProgram, "obstacleTexture"), 1);
	glUniform2f(glGetUniformLocation(diffuseColorProgram, "texelSize"), 1.0f / WIDTH, 1.0f / HEIGHT);
	glUniform1f(glGetUniformLocation(diffuseColorProgram, "diffusionRate"), DIFFUSION);
	glUniform1f(glGetUniformLocation(diffuseColorProgram, "dt"), DT);

	// Bind textures
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, colorTexture[colorIndex]);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, obstacleTexture);

	// Render full-screen quad
	glBindVertexArray(vao);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	// Swap texture indices
	colorIndex = 1 - colorIndex;
}

// Function to diffuse friendly color
void diffuseFriendlyColor() {
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, friendlyColorTexture[1 - friendlyColorIndex], 0);

	glUseProgram(diffuseColorProgram);

	// Set uniforms
	glUniform1i(glGetUniformLocation(diffuseColorProgram, "colorTexture"), 0);
	glUniform1i(glGetUniformLocation(diffuseColorProgram, "obstacleTexture"), 1);
	glUniform2f(glGetUniformLocation(diffuseColorProgram, "texelSize"), 1.0f / WIDTH, 1.0f / HEIGHT);
	glUniform1f(glGetUniformLocation(diffuseColorProgram, "diffusionRate"), DIFFUSION);
	glUniform1f(glGetUniformLocation(diffuseColorProgram, "dt"), DT);

	// Bind textures
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, friendlyColorTexture[friendlyColorIndex]);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, obstacleTexture);

	// Render full-screen quad
	glBindVertexArray(vao);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	// Swap texture indices
	friendlyColorIndex = 1 - friendlyColorIndex;
}



void detectCollisions() {
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, collisionTexture, 0);

	glUseProgram(detectCollisionProgram);

	// Set uniforms
	glUniform1i(glGetUniformLocation(detectCollisionProgram, "obstacleTexture"), 0);
	glUniform1i(glGetUniformLocation(detectCollisionProgram, "colorTexture"), 1);
	glUniform1i(glGetUniformLocation(detectCollisionProgram, "friendlyColorTexture"), 2);
	glUniform1f(glGetUniformLocation(detectCollisionProgram, "colorThreshold"), COLOR_DETECTION_THRESHOLD);

	// Bind textures
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, obstacleTexture);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, colorTexture[colorIndex]);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, friendlyColorTexture[friendlyColorIndex]);

	// Render full-screen quad
	glBindVertexArray(vao);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	// If reporting is enabled, read back collision data
	if (reportCollisions) {
		// Allocate buffer for collision data - RGBA
		std::vector<float> collisionData(WIDTH * HEIGHT * 4);

		// Read back collision texture data from GPU
		glReadPixels(0, 0, WIDTH, HEIGHT, GL_RGBA, GL_FLOAT, collisionData.data());

		// Clear previous collision locations
		collisionPoints.clear();

		// Find collision locations and categorize them
		for (int y = 0; y < HEIGHT; ++y) {
			for (int x = 0; x < WIDTH; ++x) {
				int index = (y * WIDTH + x) * 4;
				float r = collisionData[index];
				float b = collisionData[index + 2];
				float a = collisionData[index + 3];

				if (a > 0.0) {
					CollisionPoint::Type type;
					if (r > 0.5 && b > 0.5) type = CollisionPoint::BOTH;
					else if (r > 0.5) type = CollisionPoint::RED;
					else if (b > 0.5) type = CollisionPoint::BLUE;
					else type = CollisionPoint::OTHER;

					collisionPoints.push_back(CollisionPoint(x, y, type));
				}
			}
		}

		// Output collision report
		size_t red_count = 0;
		size_t blue_count = 0;
		size_t both_count = 0;

		for (int i = 0; i < collisionPoints.size(); ++i) {
			// Determine collision type
			switch (collisionPoints[i].type) {
			case CollisionPoint::RED:
				red_count++;
				break;
			case CollisionPoint::BLUE:
				blue_count++;
				break;
			case CollisionPoint::BOTH:
				both_count++;
				red_count++;
				blue_count++;
				break;
			default:
				break;
			}
		}

		if (collisionPoints.size() > 0)
		{
			// Output collision report
			std::cout << "===== Collision Report =====" << std::endl;
			std::cout << "Found " << collisionPoints.size() << " collision points" << std::endl;
			std::cout << "Red collisions: " << red_count << std::endl;
			std::cout << "Blue collisions: " << blue_count << std::endl;
			std::cout << "Both colors: " << both_count << std::endl;
			std::cout << "===========================" << std::endl;

			// Generate report for stamp-related collisions
			reportStampCollisions();
		}

		// Reset reporting flag
		reportCollisions = false;
	}
}





// Utility function to create and compile shaders
GLuint compileShader(GLenum type, const char* source) {
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, nullptr);
	glCompileShader(shader);

	// Check for compilation errors
	GLint success;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		GLchar infoLog[512];
		glGetShaderInfoLog(shader, 512, nullptr, infoLog);
		std::cerr << "Shader compilation error: " << infoLog << std::endl;
	}

	return shader;
}

// Utility function to create and link a shader program
GLuint createShaderProgram(const char* vertexSource, const char* fragmentSource) {
	GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
	GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);

	GLuint program = glCreateProgram();
	glAttachShader(program, vertexShader);
	glAttachShader(program, fragmentShader);
	glLinkProgram(program);

	// Check for linking errors
	GLint success;
	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (!success) {
		GLchar infoLog[512];
		glGetProgramInfoLog(program, 512, nullptr, infoLog);
		std::cerr << "Shader program linking error: " << infoLog << std::endl;
	}

	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	return program;
}

// Create a texture for simulation
GLuint createTexture(GLint internalFormat, GLenum format, bool filtering) {
	GLuint texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);

	// Set texture parameters
	if (filtering)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	else
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// Allocate texture memory
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, WIDTH, HEIGHT, 0, format, GL_FLOAT, nullptr);

	return texture;
}

// Initialize OpenGL resources
void initGL() {
	// Initialize GLEW
	glewExperimental = GL_TRUE;
	GLenum err = glewInit();
	if (err != GLEW_OK)
	{
		std::cerr << "GLEW initialization failed: " << glewGetErrorString(err) << std::endl;
		exit(1);
	}

	stamps.clear();

	// Create shader programs
	advectProgram = createShaderProgram(vertexShaderSource, advectFragmentShader);
	divergenceProgram = createShaderProgram(vertexShaderSource, divergenceFragmentShader);
	pressureProgram = createShaderProgram(vertexShaderSource, pressureFragmentShader);
	gradientSubtractProgram = createShaderProgram(vertexShaderSource, gradientSubtractFragmentShader);
	addForceProgram = createShaderProgram(vertexShaderSource, addForceFragmentShader);
	addObstacleProgram = createShaderProgram(vertexShaderSource, addObstacleFragmentShader);
	detectCollisionProgram = createShaderProgram(vertexShaderSource, detectCollisionFragmentShader);
	addColorProgram = createShaderProgram(vertexShaderSource, addColorFragmentShader);
	diffuseColorProgram = createShaderProgram(vertexShaderSource, diffuseColorFragmentShader);
	stampObstacleProgram = createShaderProgram(vertexShaderSource, stampObstacleFragmentShader);
	diffuseVelocityProgram = createShaderProgram(vertexShaderSource, diffuseVelocityFragmentShader);


	for (int i = 0; i < 2; i++) {
		colorTexture[i] = createTexture(GL_R32F, GL_RED, true);
	}

	for (int i = 0; i < 2; i++) {
		friendlyColorTexture[i] = createTexture(GL_R32F, GL_RED, true);
	}


	// Create textures for simulation
	for (int i = 0; i < 2; i++)
	{
		velocityTexture[i] = createTexture(GL_RG32F, GL_RG, true);
		pressureTexture[i] = createTexture(GL_R32F, GL_RED, true);
	}

	divergenceTexture = createTexture(GL_R32F, GL_RED, true);
	obstacleTexture = createTexture(GL_R32F, GL_RED, false);
	collisionTexture = createTexture(GL_RGBA32F, GL_RGBA, false);
	backgroundTexture = loadTexture("grid_wide.png");

	// Create framebuffer object
	glGenFramebuffers(1, &fbo);

	// Create a full-screen quad for rendering
	float vertices[] = {
		// Position (x, y, z)    // Texture coords (u, v)
		-1.0f, -1.0f, 0.0f,      0.0f, 0.0f,
		 1.0f, -1.0f, 0.0f,      1.0f, 0.0f,
		 1.0f,  1.0f, 0.0f,      1.0f, 1.0f,
		-1.0f,  1.0f, 0.0f,      0.0f, 1.0f
	};

	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);

	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	// Position attribute
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	// Texture coordinate attribute
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	// Reset all textures to initial state

	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, obstacleTexture, 0);
	glClear(GL_COLOR_BUFFER_BIT);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, collisionTexture, 0);
	glClear(GL_COLOR_BUFFER_BIT);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, velocityTexture[0], 0);
	glClear(GL_COLOR_BUFFER_BIT);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, velocityTexture[1], 0);
	glClear(GL_COLOR_BUFFER_BIT);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pressureTexture[0], 0);
	glClear(GL_COLOR_BUFFER_BIT);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pressureTexture[1], 0);
	glClear(GL_COLOR_BUFFER_BIT);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, divergenceTexture, 0);
	glClear(GL_COLOR_BUFFER_BIT);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture[0], 0);
	glClear(GL_COLOR_BUFFER_BIT);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture[1], 0);
	glClear(GL_COLOR_BUFFER_BIT);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, friendlyColorTexture[0], 0);
	glClear(GL_COLOR_BUFFER_BIT);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, friendlyColorTexture[1], 0);
	glClear(GL_COLOR_BUFFER_BIT);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

}


void advectColor() {
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture[1 - colorIndex], 0);

	glUseProgram(advectProgram);

	// Set uniforms for the shader
	glUniform1i(glGetUniformLocation(advectProgram, "velocityTexture"), 0);
	glUniform1i(glGetUniformLocation(advectProgram, "sourceTexture"), 1);
	glUniform1i(glGetUniformLocation(advectProgram, "obstacleTexture"), 2);
	glUniform1f(glGetUniformLocation(advectProgram, "dt"), DT);
	glUniform1f(glGetUniformLocation(advectProgram, "gridScale"), 1.0f);
	glUniform2f(glGetUniformLocation(advectProgram, "texelSize"), 1.0f / WIDTH, 1.0f / HEIGHT);

	// Bind textures
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, velocityTexture[velocityIndex]);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, colorTexture[colorIndex]);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, obstacleTexture);

	// Render full-screen quad
	glBindVertexArray(vao);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	// Swap texture indices
	colorIndex = 1 - colorIndex;
}



void advectFriendlyColor() {
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, friendlyColorTexture[1 - friendlyColorIndex], 0);

	glUseProgram(advectProgram);

	// Set uniforms for the shader
	glUniform1i(glGetUniformLocation(advectProgram, "velocityTexture"), 0);
	glUniform1i(glGetUniformLocation(advectProgram, "sourceTexture"), 1);
	glUniform1i(glGetUniformLocation(advectProgram, "obstacleTexture"), 2);
	glUniform1f(glGetUniformLocation(advectProgram, "dt"), DT);
	glUniform1f(glGetUniformLocation(advectProgram, "gridScale"), 1.0f);
	glUniform2f(glGetUniformLocation(advectProgram, "texelSize"), 1.0f / WIDTH, 1.0f / HEIGHT);

	// Bind textures
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, velocityTexture[velocityIndex]);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, friendlyColorTexture[friendlyColorIndex]);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, obstacleTexture);

	// Render full-screen quad
	glBindVertexArray(vao);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	// Swap texture indices
	friendlyColorIndex = 1 - friendlyColorIndex;
}



// Apply the advection step
void advectVelocity() {
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, velocityTexture[1 - velocityIndex], 0);

	glUseProgram(advectProgram);

	// Set uniforms
	glUniform1i(glGetUniformLocation(advectProgram, "velocityTexture"), 0);
	glUniform1i(glGetUniformLocation(advectProgram, "sourceTexture"), 1);
	glUniform1i(glGetUniformLocation(advectProgram, "obstacleTexture"), 2);
	glUniform1f(glGetUniformLocation(advectProgram, "dt"), DT);
	glUniform1f(glGetUniformLocation(advectProgram, "gridScale"), 1.0f);
	glUniform2f(glGetUniformLocation(advectProgram, "texelSize"), 1.0f / WIDTH, 1.0f / HEIGHT);

	// Bind textures
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, velocityTexture[velocityIndex]);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, velocityTexture[velocityIndex]);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, obstacleTexture);

	// Render full-screen quad
	glBindVertexArray(vao);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	// Swap texture indices
	velocityIndex = 1 - velocityIndex;
}

// Compute the velocity divergence
void computeDivergence() {
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, divergenceTexture, 0);

	glUseProgram(divergenceProgram);

	// Set uniforms
	glUniform1i(glGetUniformLocation(divergenceProgram, "velocityTexture"), 0);
	glUniform1i(glGetUniformLocation(divergenceProgram, "obstacleTexture"), 1);
	glUniform2f(glGetUniformLocation(divergenceProgram, "texelSize"), 1.0f / WIDTH, 1.0f / HEIGHT);

	// Bind textures
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, velocityTexture[velocityIndex]);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, obstacleTexture);

	// Render full-screen quad
	glBindVertexArray(vao);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

// Solve for pressure using Jacobi iteration
void solvePressure(int iterations) {
	// Clear pressure textures
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pressureTexture[0], 0);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pressureTexture[1], 0);
	glClear(GL_COLOR_BUFFER_BIT);

	pressureIndex = 0;

	// Jacobi iteration
	for (int i = 0; i < iterations; i++) {
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pressureTexture[1 - pressureIndex], 0);

		glUseProgram(pressureProgram);

		// Set uniforms
		float alpha = -1.0f;  // Central coefficient
		float rBeta = 0.25f;  // Reciprocal of sum of neighboring coefficients

		glUniform1i(glGetUniformLocation(pressureProgram, "pressureTexture"), 0);
		glUniform1i(glGetUniformLocation(pressureProgram, "divergenceTexture"), 1);
		glUniform1i(glGetUniformLocation(pressureProgram, "obstacleTexture"), 2);
		glUniform2f(glGetUniformLocation(pressureProgram, "texelSize"), 1.0f / WIDTH, 1.0f / HEIGHT);
		glUniform1f(glGetUniformLocation(pressureProgram, "alpha"), alpha);
		glUniform1f(glGetUniformLocation(pressureProgram, "rBeta"), rBeta);

		// Bind textures
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, pressureTexture[pressureIndex]);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, divergenceTexture);
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, obstacleTexture);

		// Render full-screen quad
		glBindVertexArray(vao);
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

		// Swap texture indices
		pressureIndex = 1 - pressureIndex;
	}
}

// Subtract pressure gradient from velocity
void subtractPressureGradient() {
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, velocityTexture[1 - velocityIndex], 0);

	glUseProgram(gradientSubtractProgram);

	// Set uniforms
	glUniform1i(glGetUniformLocation(gradientSubtractProgram, "pressureTexture"), 0);
	glUniform1i(glGetUniformLocation(gradientSubtractProgram, "velocityTexture"), 1);
	glUniform1i(glGetUniformLocation(gradientSubtractProgram, "obstacleTexture"), 2);
	glUniform2f(glGetUniformLocation(gradientSubtractProgram, "texelSize"), 1.0f / WIDTH, 1.0f / HEIGHT);
	glUniform1f(glGetUniformLocation(gradientSubtractProgram, "scale"), 1.0f);

	// Bind textures
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, pressureTexture[pressureIndex]);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, velocityTexture[velocityIndex]);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, obstacleTexture);

	// Render full-screen quad
	glBindVertexArray(vao);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	// Swap texture indices
	velocityIndex = 1 - velocityIndex;
}

// Add force to the velocity field
void addForce() {
	if (!mouseDown) return;

	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, velocityTexture[1 - velocityIndex], 0);

	glUseProgram(addForceProgram);

	float aspect = HEIGHT / float(WIDTH);

	// Get normalized mouse position (0 to 1 range)
	float mousePosX = mouseX / (float)WIDTH;
	float mousePosY = 1.0f - (mouseY / (float)HEIGHT);  // Invert Y for OpenGL coordinates

	// Center the Y coordinate, apply aspect ratio, then un-center
	mousePosY = (mousePosY - 0.5f) * aspect + 0.5f;

	float mouseVelX = (mouseX - prevMouseX) * 0.01f / (HEIGHT / (float(WIDTH)));
	float mouseVelY = -(mouseY - prevMouseY) * 0.01f;


	// Set uniforms
	glUniform1i(glGetUniformLocation(addForceProgram, "velocityTexture"), 0);
	glUniform1i(glGetUniformLocation(addForceProgram, "obstacleTexture"), 1);
	glUniform2f(glGetUniformLocation(addForceProgram, "point"), mousePosX, mousePosY);
	glUniform2f(glGetUniformLocation(addForceProgram, "direction"), mouseVelX, mouseVelY);
	glUniform1f(glGetUniformLocation(addForceProgram, "radius"), 0.05f);
	glUniform1f(glGetUniformLocation(addForceProgram, "strength"), FORCE);

	// Bind textures
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, velocityTexture[velocityIndex]);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, obstacleTexture);

	// Render full-screen quad
	glBindVertexArray(vao);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	// Swap texture indices
	velocityIndex = 1 - velocityIndex;

	// Update previous mouse position
	prevMouseX = mouseX;
	prevMouseY = mouseY;
}





void updateObstacle() {
	if (!rightMouseDown) return;

	// Choose between circle obstacles or bitmap stamp
	bool useStamp = stampTextureLoaded;

	if (useStamp) {
		applyBitmapObstacle();
	}
	else {
		// Original circle obstacle code
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, obstacleTexture, 0);

		glUseProgram(addObstacleProgram);

		float aspect = HEIGHT / float(WIDTH);

		// Get normalized mouse position (0 to 1 range)
		float mousePosX = mouseX / (float)WIDTH;
		float mousePosY = 1.0f - (mouseY / (float)HEIGHT);  // Invert Y for OpenGL coordinates

		// Center the Y coordinate, apply aspect ratio, then un-center
		mousePosY = (mousePosY - 0.5f) * aspect + 0.5f;

		// Set uniforms
		glUniform1i(glGetUniformLocation(addObstacleProgram, "obstacleTexture"), 0);
		glUniform2f(glGetUniformLocation(addObstacleProgram, "point"), mousePosX, mousePosY);
		glUniform1f(glGetUniformLocation(addObstacleProgram, "radius"), OBSTACLE_RADIUS);
		glUniform1i(glGetUniformLocation(addObstacleProgram, "addObstacle"), true);

		// Bind textures
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, obstacleTexture);

		// Render full-screen quad
		glBindVertexArray(vao);
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	}
}






void addColor()
{
	if (!mouseDown) return;

	// Determine which color texture to modify based on the active mode
	GLuint targetTexture;
	int* targetIndex;

	if (red_mode)
	{
		targetTexture = colorTexture[1 - colorIndex];
		targetIndex = &colorIndex;
	}
	else
	{
		targetTexture = friendlyColorTexture[1 - friendlyColorIndex];
		targetIndex = &friendlyColorIndex;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, targetTexture, 0);

	glUseProgram(addColorProgram);

	float aspect = HEIGHT / float(WIDTH);

	// Get normalized mouse position (0 to 1 range)
	float mousePosX = mouseX / (float)WIDTH;
	float mousePosY = 1.0f - (mouseY / (float)HEIGHT);  // Invert Y for OpenGL coordinates

	// Center the Y coordinate, apply aspect ratio, then un-center
	mousePosY = (mousePosY - 0.5f) * aspect + 0.5f;

	// Set uniforms
	glUniform1i(glGetUniformLocation(addColorProgram, "colorTexture"), 0);
	glUniform1i(glGetUniformLocation(addColorProgram, "obstacleTexture"), 1);
	glUniform2f(glGetUniformLocation(addColorProgram, "point"), mousePosX, mousePosY);
	glUniform1f(glGetUniformLocation(addColorProgram, "radius"), 0.05f);

	// Bind the appropriate texture based on mode
	glActiveTexture(GL_TEXTURE0);

	if (red_mode)
	{
		glBindTexture(GL_TEXTURE_2D, colorTexture[colorIndex]);
	}
	else
	{
		glBindTexture(GL_TEXTURE_2D, friendlyColorTexture[friendlyColorIndex]);
	}

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, obstacleTexture);

	// Render full-screen quad
	glBindVertexArray(vao);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	// Swap the appropriate texture index
	if (red_mode)
	{
		colorIndex = 1 - colorIndex;
	}
	else
	{
		friendlyColorIndex = 1 - friendlyColorIndex;
	}
}

void simulationStep() {
	// Add force from mouse interaction
	addForce();

	// Add color from mouse interaction
	addColor();

	// Update obstacles from mouse interaction
	updateObstacle();

	// Advect velocity
	advectVelocity();

	// Diffuse velocity - add this line
	diffuseVelocity();

	// Advect both color sets
	advectColor();
	advectFriendlyColor();

	// Diffuse both color sets
	diffuseColor();
	diffuseFriendlyColor();

	// Project velocity to be divergence-free
	computeDivergence();
	solvePressure(20);  // 20 iterations of Jacobi method
	subtractPressureGradient();

	// Detect collisions between density and obstacles
	detectCollisions();
}


void renderToScreen() {
	// Bind default framebuffer (the screen)
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Clear the screen
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	// Use a render shader program
	GLuint renderProgram = createShaderProgram(vertexShaderSource, renderFragmentShader);
	glUseProgram(renderProgram);

	static float time = 0.0f;
	time += 0.016f; // Approximate time for 60fps, adjust as needed

	// Set uniforms
	glUniform1i(glGetUniformLocation(renderProgram, "obstacleTexture"), 1);
	glUniform1i(glGetUniformLocation(renderProgram, "collisionTexture"), 2);
	glUniform1i(glGetUniformLocation(renderProgram, "colorTexture"), 3);
	glUniform1i(glGetUniformLocation(renderProgram, "friendlyColorTexture"), 4);
	glUniform1i(glGetUniformLocation(renderProgram, "backgroundTexture"), 5);
	glUniform2f(glGetUniformLocation(renderProgram, "texelSize"), 1.0f / WIDTH, 1.0f / HEIGHT);
	glUniform1f(glGetUniformLocation(renderProgram, "time"), time);



	// Bind textures
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, obstacleTexture);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, collisionTexture);
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, colorTexture[colorIndex]);
	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_2D, friendlyColorTexture[friendlyColorIndex]);
	glActiveTexture(GL_TEXTURE5);
	glBindTexture(GL_TEXTURE_2D, backgroundTexture);

	// Render full-screen quad
	glBindVertexArray(vao);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	// Cleanup
	glDeleteProgram(renderProgram);
}



// GLUT display callback
void display() {
	// Increment frame counter
	frameCount++;

	// Check if it's time to report collisions
	if (frameCount % REPORT_INTERVAL == 0) {
		reportCollisions = true;
	}

	// Perform simulation step
	simulationStep();

	// Render to screen
	renderToScreen();

	// Swap buffers
	glutSwapBuffers();
}

// GLUT idle callback
void idle() {
	glutPostRedisplay();
}

// GLUT mouse button callback
void mouseButton(int button, int state, int x, int y) {
	mouseX = x;
	mouseY = y;
	prevMouseX = x;
	prevMouseY = y;

	if (button == GLUT_LEFT_BUTTON)
	{
		mouseDown = (state == GLUT_DOWN);
	}
	else if (button == GLUT_RIGHT_BUTTON)
	{
		if (state == GLUT_UP)
			lastRightMouseDown = false;

		rightMouseDown = (state == GLUT_DOWN);
	}
}

// GLUT mouse motion callback
void mouseMotion(int x, int y) {
	mouseX = x;
	mouseY = y;
}



// GLUT keyboard callback
void keyboard(unsigned char key, int x, int y) {
	switch (key) {
	case 'r':
	case 'R':
		red_mode = !red_mode;
		std::cout << "Switched to " << (red_mode ? "RED" : "BLUE") << " color mode" << std::endl;
		break;

	case 'c':  // Report collisions immediately
	case 'C':
		reportCollisions = true;
		std::cout << "Generating collision report on next frame..." << std::endl;
		break;

	case 'l':  // Load bitmap as stamp
	case 'L':
		if (loadStampTexture("obstacle.png")) {
			std::cout << "Loaded bitmap as obstacle stamp" << std::endl;
		}
		else {
			std::cout << "Failed to load bitmap stamp" << std::endl;
		}
		break;

	case 's':  // Clear all stamps
	case 'S':
		if (!stamps.empty()) {
			stamps.clear();
			// Clear obstacle texture
			glBindFramebuffer(GL_FRAMEBUFFER, fbo);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, obstacleTexture, 0);
			glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			std::cout << "Cleared all stamps" << std::endl;
		}
		else {
			std::cout << "No stamps to clear" << std::endl;
		}
		break;

	case 'x':  // Clear only the most recently added stamp
	case 'X':
		if (!stamps.empty()) {
			stamps.pop_back();
			std::cout << "Removed the most recent stamp. "
				<< stamps.size() << " stamps remaining." << std::endl;
			// Rebuild obstacle texture with remaining stamps
			rebuildObstacleTexture();
		}
		else {
			std::cout << "No stamps to remove" << std::endl;
		}
		break;

	case 'p':  // Print debug info about stamps
	case 'P':
		std::cout << "Debug: Current stamps (" << stamps.size() << " total):" << std::endl;
		for (size_t i = 0; i < stamps.size(); i++) {
			const auto& stamp = stamps[i];
			std::cout << "  Stamp #" << (i + 1) << ": active=" << (stamp.active ? "yes" : "no")
				<< ", pos=(" << stamp.posX << "," << stamp.posY << ")"
				<< ", size=" << stamp.width << "x" << stamp.height << std::endl;
		}
		break;
	}
}






// GLUT reshape callback
void reshape(int w, int h) {

	glViewport(0, 0, w, h);

	WIDTH = w;
	HEIGHT = h;

	glDeleteProgram(advectProgram);
	glDeleteProgram(divergenceProgram);
	glDeleteProgram(pressureProgram);
	glDeleteProgram(gradientSubtractProgram);
	glDeleteProgram(addForceProgram);
	glDeleteProgram(addObstacleProgram);
	glDeleteProgram(detectCollisionProgram);
	glDeleteProgram(diffuseColorProgram);
	glDeleteProgram(addColorProgram);
	glDeleteProgram(stampObstacleProgram);
	glDeleteProgram(diffuseVelocityProgram);

	glDeleteFramebuffers(1, &fbo);
	glDeleteVertexArrays(1, &vao);
	glDeleteBuffers(1, &vbo);

	glDeleteTextures(2, velocityTexture);
	glDeleteTextures(2, pressureTexture);
	glDeleteTextures(1, &divergenceTexture);
	glDeleteTextures(1, &obstacleTexture);
	glDeleteTextures(1, &collisionTexture);
	glDeleteTextures(2, colorTexture);
	glDeleteTextures(2, friendlyColorTexture);
	glDeleteTextures(1, &backgroundTexture);

	initGL();

	rebuildObstacleTexture();
}
void printInstructions() {
	std::cout << "GPU-Accelerated Navier-Stokes Solver" << std::endl;
	std::cout << "-----------------------------------" << std::endl;
	std::cout << "Left Mouse Button: Add velocity and density" << std::endl;
	std::cout << "Right Mouse Button: Add obstacles/stamps" << std::endl;
	std::cout << "F1: Reset simulation" << std::endl;
	std::cout << "R: Toggle between red and blue color modes" << std::endl;
	std::cout << "C: Generate collision report immediately" << std::endl;
	std::cout << "L: Load bitmap as obstacle stamp" << std::endl;
	std::cout << "S: Clear all stamps" << std::endl;
	std::cout << "X: Remove the most recent stamp" << std::endl;
	std::cout << "ESC: Exit" << std::endl;
	std::cout << "Highlights show colour-obstacle collisions" << std::endl;
	std::cout << "Collision reports are generated every " << REPORT_INTERVAL << " frames" << std::endl;
}

// Then update the main function to call this instead of printing directly
int main(int argc, char** argv) {
	// Initialize GLUT
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE);
	glutInitWindowSize(WIDTH, HEIGHT);
	glutCreateWindow("GPU-Accelerated Navier-Stokes Solver");

	// Initialize OpenGL
	initGL();

	// Register callbacks
	glutDisplayFunc(display);
	glutIdleFunc(idle);
	glutMouseFunc(mouseButton);
	glutMotionFunc(mouseMotion);
	glutKeyboardFunc(keyboard);
	glutReshapeFunc(reshape);

	// Print instructions
	printInstructions();

	// Main loop
	glutMainLoop();

	return 0;
} 