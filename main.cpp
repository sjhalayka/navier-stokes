// https://claude.ai/chat/150e7512-37b3-4a6d-aa2a-05072caface4

#include <GL/glew.h>
#include <GL/glut.h>

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
int WIDTH = 1000;
int HEIGHT = 500;

const float DT = 0.1f;            // Time step
const float VISCOSITY = 10.0f;     // Fluid viscosity
const float DIFFUSION = 0.0f;    // Density diffusion rate
const float FORCE = 500.0f;         // Force applied by mouse
const float DENSITY_AMOUNT = 1.0f; // Density added with force
const float OBSTACLE_RADIUS = 0.1f; // Radius of obstacle
const float COLLISION_THRESHOLD = 0.5f; // Threshold for density-obstacle collision
const int REPORT_INTERVAL = 60;   // Report collision locations every N frames

// OpenGL variables
GLuint velocityTexture[2];
GLuint pressureTexture[2];
GLuint densityTexture[2];
GLuint divergenceTexture;
GLuint obstacleTexture;
GLuint collisionTexture;

GLuint advectProgram;
GLuint divergenceProgram;
GLuint pressureProgram;
GLuint gradientSubtractProgram;
GLuint addForceProgram;
GLuint addDensityProgram;
GLuint diffuseDensityProgram;
GLuint addObstacleProgram;
GLuint detectCollisionProgram;

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
int densityIndex = 0;

// Collision tracking
int frameCount = 0;
bool reportCollisions = false;
std::vector<std::pair<int, int>> collisionLocations;

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
float aspect_ratio = 1;//WIDTH/HEIGHT;

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

const char* addDensityFragmentShader = R"(
#version 330 core
uniform sampler2D densityTexture;
uniform sampler2D obstacleTexture;
uniform vec2 point;
uniform float radius;
uniform float amount;
out float FragColor;

in vec2 TexCoord;

void main() {
    // Check if we're in an obstacle
    float obstacle = texture(obstacleTexture, TexCoord).r;
    if (obstacle > 0.0) {
        FragColor = 0.0;
        return;
    }

    // Get current density
    float density = texture(densityTexture, TexCoord).r;
    
    // Calculate distance to density application point
    float distance = length(TexCoord - point);
    
    // Apply density based on radius
    if (distance < radius) {
        // Apply density with smooth falloff
        float falloff = 1.0 - (distance / radius);
        falloff = falloff * falloff;
        
        // Add density
        density += amount * falloff;
    }
    
    // Clamp density to [0, 1] range
    FragColor = clamp(density, 0.0, 1.0);
}
)";

const char* diffuseDensityFragmentShader = R"(
#version 330 core
uniform sampler2D densityTexture;
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
    float center = texture(densityTexture, TexCoord).r;
    float left = texture(densityTexture, TexCoord - vec2(texelSize.x, 0.0)).r;
    float right = texture(densityTexture, TexCoord + vec2(texelSize.x, 0.0)).r;
    float bottom = texture(densityTexture, TexCoord - vec2(0.0, texelSize.y)).r;
    float top = texture(densityTexture, TexCoord + vec2(0.0, texelSize.y)).r;
    
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
uniform sampler2D densityTexture;
uniform sampler2D obstacleTexture;
uniform float collisionThreshold;
out float FragColor;

in vec2 TexCoord;

void main() {
    // Get current density and obstacle values
    float density = texture(densityTexture, TexCoord).r;
    float obstacle = texture(obstacleTexture, TexCoord).r;
    
    // Check for collision on obstacle boundaries
    if (obstacle > 0.0) {
        // We're in an obstacle - check if there's high density nearby
        vec2 texelSize = 1.0 / vec2(textureSize(densityTexture, 0));
        
        // Check neighboring cells for high density
        float leftDensity = texture(densityTexture, TexCoord - vec2(texelSize.x, 0.0)).r;
        float rightDensity = texture(densityTexture, TexCoord + vec2(texelSize.x, 0.0)).r;
        float bottomDensity = texture(densityTexture, TexCoord - vec2(0.0, texelSize.y)).r;
        float topDensity = texture(densityTexture, TexCoord + vec2(0.0, texelSize.y)).r;
        
        // Check for obstacles in neighboring cells
        float leftObstacle = texture(obstacleTexture, TexCoord - vec2(texelSize.x, 0.0)).r;
        float rightObstacle = texture(obstacleTexture, TexCoord + vec2(texelSize.x, 0.0)).r;
        float bottomObstacle = texture(obstacleTexture, TexCoord - vec2(0.0, texelSize.y)).r;
        float topObstacle = texture(obstacleTexture, TexCoord + vec2(0.0, texelSize.y)).r;
        
        // Only consider densities from non-obstacle cells
        if (leftObstacle < 0.5) leftDensity = max(leftDensity, 0.0);
        else leftDensity = 0.0;
        
        if (rightObstacle < 0.5) rightDensity = max(rightDensity, 0.0);
        else rightDensity = 0.0;
        
        if (bottomObstacle < 0.5) bottomDensity = max(bottomDensity, 0.0);
        else bottomDensity = 0.0;
        
        if (topObstacle < 0.5) topDensity = max(topDensity, 0.0);
        else topDensity = 0.0;
        
        // Calculate max density at boundaries
        float maxBoundaryDensity = max(max(leftDensity, rightDensity), max(bottomDensity, topDensity));
        
        // Set collision value based on density threshold
        FragColor = (maxBoundaryDensity > collisionThreshold) ? 1.0 : 0.0;
    } else {
        // Not in an obstacle - no collision
        FragColor = 0.0;
    }
}
)";

const char* renderFragmentShader = R"(
#version 330 core
uniform sampler2D densityTexture;
uniform sampler2D obstacleTexture;
uniform sampler2D collisionTexture;

uniform vec2 texelSize;

float WIDTH = texelSize.x;
float HEIGHT = texelSize.y;
float aspect_ratio = WIDTH/HEIGHT;

out vec4 FragColor;

in vec2 TexCoord;

// Function to map density to color
vec3 densityToColor(float density) 
{
    vec3 gray = vec3(density, density, density);

    // Use a blue-to-red color map
    vec3 color1 = vec3(0.0, 0.0, 0.0); // Dark blue for low density
    vec3 color2 = vec3(0.25, 0.125, 0.0); // Cyan for medium-low density
    vec3 color3 = vec3(0.5, 0.25, 0.0); // Yellow for medium-high density
    vec3 color4 = vec3(1.0, 0.5, 0.0); // Red for high density
    
    if (density < 0.25) {
        return gray + mix(color1, color2, density * 4.0);
    } else if (density < 0.5) {
        return gray + mix(color2, color3, (density - 0.25) * 4.0);
    } else if (density < 0.75) {
        return gray + mix(color3, color4, (density - 0.5) * 4.0);
    } else {
        return gray + color4;
    }
}

void main() {
    // Adjust texture coordinates based on aspect ratio
    vec2 adjustedCoord = TexCoord;
    
    // For non-square textures, adjust sampling to prevent stretching
    // This keeps the simulation visually correct regardless of window shape
    if (aspect_ratio > 1.0) {
        // Wide screen - adjust x coordinate
        adjustedCoord.x = (adjustedCoord.x - 0.5) / aspect_ratio + 0.5;
    } else if (aspect_ratio < 1.0) {
        // Tall screen - adjust y coordinate
        adjustedCoord.y = (adjustedCoord.y - 0.5) * aspect_ratio + 0.5;
    }
    
    // Check for collision at obstacle boundaries using adjusted coordinates
    float collision = texture(collisionTexture, adjustedCoord).r;
    if (collision > 0.0) {
        // Show collision as bright orange
        FragColor = vec4(1.0, 0.6, 0.0, 1.0);
        return;
    }
    
    // Check for obstacle
    float obstacle = texture(obstacleTexture, adjustedCoord).r;
    if (obstacle > 0.0) {
        // Render obstacles as white
        FragColor = vec4(1.0, 1.0, 1.0, 1.0);
        return;
    }
    
    // Get density at adjusted position
    float density = texture(densityTexture, adjustedCoord).r;
    
    // Convert density to color
    vec3 color = densityToColor(density);
    
    FragColor = vec4(color, 1.0);
}
)";


void detectCollisions() {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, collisionTexture, 0);

    glUseProgram(detectCollisionProgram);

    // Set uniforms
    glUniform1i(glGetUniformLocation(detectCollisionProgram, "densityTexture"), 0);
    glUniform1i(glGetUniformLocation(detectCollisionProgram, "obstacleTexture"), 1);
    glUniform1f(glGetUniformLocation(detectCollisionProgram, "collisionThreshold"), COLLISION_THRESHOLD);

    // Bind textures
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, densityTexture[densityIndex]);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, obstacleTexture);

    // Render full-screen quad
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    // If reporting is enabled, read back collision data
    if (reportCollisions) {
        // Allocate buffer for collision data
        std::vector<float> collisionData(WIDTH * HEIGHT);

        // Read back collision texture data from GPU
        glReadPixels(0, 0, WIDTH, HEIGHT, GL_RED, GL_FLOAT, collisionData.data());

        // Clear previous collision locations
        collisionLocations.clear();

        // Find collision locations
        for (int y = 0; y < HEIGHT; ++y) {
            for (int x = 0; x < WIDTH; ++x) {
                int index = y * WIDTH + x;
                if (collisionData[index] > 0.5f) {
                    collisionLocations.push_back(std::make_pair(x, y));
                }
            }
        }

        // Output collision report
        std::cout << "===== Collision Report =====" << std::endl;
        std::cout << "Found " << collisionLocations.size() << " collision points" << std::endl;

        // Output first few collision locations
        const int MAX_REPORT_LOCATIONS = 10;
        int locationsToReport = std::min((int)collisionLocations.size(), MAX_REPORT_LOCATIONS);

        for (int i = 0; i < locationsToReport; ++i) {
            std::cout << "Collision at (" << collisionLocations[i].first << ", "
                << collisionLocations[i].second << ")" << std::endl;
        }

        if (collisionLocations.size() > MAX_REPORT_LOCATIONS) {
            std::cout << "... and " << (collisionLocations.size() - MAX_REPORT_LOCATIONS)
                << " more locations" << std::endl;
        }

        // Write complete collision data to file if there are any collisions    
        //if (!collisionLocations.empty()) {
        //    std::stringstream filename;
        //    filename << "collision_data_" << frameCount << ".csv";
        //    std::ofstream outFile(filename.str());

        //    if (outFile.is_open()) {
        //        outFile << "x,y" << std::endl;
        //        for (const auto& loc : collisionLocations) {
        //            outFile << loc.first << "," << loc.second << std::endl;
        //        }
        //        outFile.close();
        //        std::cout << "Complete collision data written to " << filename.str() << std::endl;
        //    }
        //    else {
        //        std::cerr << "Could not open file for writing collision data" << std::endl;
        //    }
        //}

        std::cout << "===========================" << std::endl;

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
    if (filtering) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    else {
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
    if (err != GLEW_OK) {
        std::cerr << "GLEW initialization failed: " << glewGetErrorString(err) << std::endl;
        exit(1);
    }

    // Create shader programs
    advectProgram = createShaderProgram(vertexShaderSource, advectFragmentShader);
    divergenceProgram = createShaderProgram(vertexShaderSource, divergenceFragmentShader);
    pressureProgram = createShaderProgram(vertexShaderSource, pressureFragmentShader);
    gradientSubtractProgram = createShaderProgram(vertexShaderSource, gradientSubtractFragmentShader);
    addForceProgram = createShaderProgram(vertexShaderSource, addForceFragmentShader);
    addDensityProgram = createShaderProgram(vertexShaderSource, addDensityFragmentShader);
    diffuseDensityProgram = createShaderProgram(vertexShaderSource, diffuseDensityFragmentShader);
    addObstacleProgram = createShaderProgram(vertexShaderSource, addObstacleFragmentShader);
    detectCollisionProgram = createShaderProgram(vertexShaderSource, detectCollisionFragmentShader);

    // Create textures for simulation
    for (int i = 0; i < 2; i++) {
        velocityTexture[i] = createTexture(GL_RG32F, GL_RG, true);
        pressureTexture[i] = createTexture(GL_R32F, GL_RED, true);
        densityTexture[i] = createTexture(GL_R32F, GL_RED, true);
    }

    divergenceTexture = createTexture(GL_R32F, GL_RED, true);
    obstacleTexture = createTexture(GL_R32F, GL_RED, false);
    collisionTexture = createTexture(GL_R32F, GL_RED, false);

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

    // Clear the obstacle and collision textures
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, obstacleTexture, 0);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, collisionTexture, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    // Reset all textures to initial state
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, velocityTexture[0], 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, velocityTexture[1], 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pressureTexture[0], 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pressureTexture[1], 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, densityTexture[0], 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, densityTexture[1], 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, divergenceTexture, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
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

    // Get normalized mouse position
    float mousePosX = mouseX / (float)WIDTH;
    float mousePosY = 1.0f - (mouseY / (float)HEIGHT);  // Invert Y for OpenGL coordinates

    mousePosY *= aspect;
    mousePosY += 0.25;

    float mouseVelX = (mouseX - prevMouseX) * 0.01f / (HEIGHT/(float(WIDTH)));
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

// Add density to the fluid
void addDensity() {
    if (!mouseDown) return;

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, densityTexture[1 - densityIndex], 0);

    glUseProgram(addDensityProgram);

    float aspect = HEIGHT / float(WIDTH);

    // Get normalized mouse position
    float mousePosX = mouseX / (float)WIDTH;
    float mousePosY = 1.0f - (mouseY / (float)HEIGHT);  // Invert Y for OpenGL coordinates

    mousePosY *= aspect;
    mousePosY += 0.25;

    // Set uniforms
    glUniform1i(glGetUniformLocation(addDensityProgram, "densityTexture"), 0);
    glUniform1i(glGetUniformLocation(addDensityProgram, "obstacleTexture"), 1);
    glUniform2f(glGetUniformLocation(addDensityProgram, "point"), mousePosX, mousePosY);
    glUniform1f(glGetUniformLocation(addDensityProgram, "radius"), 0.05f);
    glUniform1f(glGetUniformLocation(addDensityProgram, "amount"), DENSITY_AMOUNT);

    // Bind textures
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, densityTexture[densityIndex]);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, obstacleTexture);

    // Render full-screen quad
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    // Swap texture indices
    densityIndex = 1 - densityIndex;
}

// Diffuse density
void diffuseDensity() {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, densityTexture[1 - densityIndex], 0);

    glUseProgram(diffuseDensityProgram);

    // Set uniforms
    glUniform1i(glGetUniformLocation(diffuseDensityProgram, "densityTexture"), 0);
    glUniform1i(glGetUniformLocation(diffuseDensityProgram, "obstacleTexture"), 1);
    glUniform2f(glGetUniformLocation(diffuseDensityProgram, "texelSize"), 1.0f / WIDTH, 1.0f / HEIGHT);
    glUniform1f(glGetUniformLocation(diffuseDensityProgram, "diffusionRate"), DIFFUSION);
    glUniform1f(glGetUniformLocation(diffuseDensityProgram, "dt"), DT);

    // Bind textures
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, densityTexture[densityIndex]);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, obstacleTexture);

    // Render full-screen quad
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    // Swap texture indices
    densityIndex = 1 - densityIndex;
}

// Add or remove obstacles
void updateObstacle() 
{
    if (!rightMouseDown) return;

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, obstacleTexture, 0);

    glUseProgram(addObstacleProgram);

    float aspect = HEIGHT / float(WIDTH);

    // Get normalized mouse position
    float mousePosX = mouseX / (float)WIDTH;
    float mousePosY = 1.0f - (mouseY / (float)HEIGHT);  // Invert Y for OpenGL coordinates

    mousePosY *= aspect;
    mousePosY += 0.25;

    // Set uniforms
    glUniform1i(glGetUniformLocation(addObstacleProgram, "obstacleTexture"), 0);
    glUniform2f(glGetUniformLocation(addObstacleProgram, "point"), mousePosX, mousePosY);
    glUniform1f(glGetUniformLocation(addObstacleProgram, "radius"), OBSTACLE_RADIUS);
    glUniform1i(glGetUniformLocation(addObstacleProgram, "addObstacle"), true);  // Always add obstacles for simplicity

    // Bind textures
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, obstacleTexture);

    // Render full-screen quad
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void advectDensity()
{
    // Target the next density texture (ping-pong buffers)
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, densityTexture[1 - densityIndex], 0);

    // Use the advection shader program
    glUseProgram(advectProgram);

    // Set uniforms for the shader
    glUniform1i(glGetUniformLocation(advectProgram, "velocityTexture"), 0);
    glUniform1i(glGetUniformLocation(advectProgram, "sourceTexture"), 1);
    glUniform1i(glGetUniformLocation(advectProgram, "obstacleTexture"), 2);
    glUniform1f(glGetUniformLocation(advectProgram, "dt"), DT);
    glUniform1f(glGetUniformLocation(advectProgram, "gridScale"), 1.0f);
    glUniform2f(glGetUniformLocation(advectProgram, "texelSize"), 1.0f / WIDTH, 1.0f / HEIGHT);

    // Bind all required textures
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, velocityTexture[velocityIndex]);  // Current velocity field
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, densityTexture[densityIndex]);    // Current density field
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, obstacleTexture);                 // Obstacle field

    // Execute the advection shader on a full-screen quad
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    // Swap to use the newly computed density field in the next step
    densityIndex = 1 - densityIndex;
}

// Perform one simulation step
void simulationStep() {
    // Add force from mouse interaction
    addForce();

    // Add density from mouse interaction
    addDensity();

    // Update obstacles from mouse interaction
    updateObstacle();

    // Advect velocity
    advectVelocity();

    // Advect density
    advectDensity();

    // Diffuse density
    diffuseDensity();

    // Project velocity to be divergence-free
    computeDivergence();
    solvePressure(20);  // 20 iterations of Jacobi method
    subtractPressureGradient();

    // Detect collisions between density and obstacles
    detectCollisions();
}

// Render the simulation to the screen
void renderToScreen() {
    // Bind default framebuffer (the screen)
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Clear the screen
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Use a render shader program
    GLuint renderProgram = createShaderProgram(vertexShaderSource, renderFragmentShader);
    glUseProgram(renderProgram);

    // Set uniforms
    glUniform1i(glGetUniformLocation(renderProgram, "densityTexture"), 0);
    glUniform1i(glGetUniformLocation(renderProgram, "obstacleTexture"), 1);
    glUniform1i(glGetUniformLocation(renderProgram, "collisionTexture"), 2);
    glUniform2f(glGetUniformLocation(renderProgram, "texelSize"), 1.0f / WIDTH, 1.0f / HEIGHT);

    // Bind textures
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, densityTexture[densityIndex]);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, obstacleTexture);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, collisionTexture);

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

    if (button == GLUT_LEFT_BUTTON) {
        mouseDown = (state == GLUT_DOWN);
    }
    else if (button == GLUT_RIGHT_BUTTON) {
        rightMouseDown = (state == GLUT_DOWN);
    }
}

// GLUT mouse motion callback
void mouseMotion(int x, int y) {
    mouseX = x;
    mouseY = y;
}

// GLUT special key callback
void keyboardSpecial(int key, int x, int y) {
    switch (key) {
    case GLUT_KEY_F1:
        // Reset simulation
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        // Clear velocity textures
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, velocityTexture[0], 0);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, velocityTexture[1], 0);
        glClear(GL_COLOR_BUFFER_BIT);

        // Clear density textures
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, densityTexture[0], 0);
        glClear(GL_COLOR_BUFFER_BIT);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, densityTexture[1], 0);
        glClear(GL_COLOR_BUFFER_BIT);

        // Clear obstacle texture
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, obstacleTexture, 0);
        glClear(GL_COLOR_BUFFER_BIT);

        // Clear collision texture
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, collisionTexture, 0);
        glClear(GL_COLOR_BUFFER_BIT);

        break;
    }
}

// GLUT keyboard callback
void keyboard(unsigned char key, int x, int y) {
    switch (key) {
    case 27:  // ESC key
        // Clean up and exit
        glDeleteTextures(2, velocityTexture);
        glDeleteTextures(2, pressureTexture);
        glDeleteTextures(2, densityTexture);
        glDeleteTextures(1, &divergenceTexture);
        glDeleteTextures(1, &obstacleTexture);
        glDeleteTextures(1, &collisionTexture);
        glDeleteFramebuffers(1, &fbo);
        glDeleteVertexArrays(1, &vao);
        glDeleteBuffers(1, &vbo);
        glDeleteProgram(advectProgram);
        glDeleteProgram(divergenceProgram);
        glDeleteProgram(pressureProgram);
        glDeleteProgram(gradientSubtractProgram);
        glDeleteProgram(addForceProgram);
        glDeleteProgram(addDensityProgram);
        glDeleteProgram(diffuseDensityProgram);
        glDeleteProgram(addObstacleProgram);
        glDeleteProgram(detectCollisionProgram);
        exit(0);
        break;

    case 'c':  // Report collisions immediately
    case 'C':
        reportCollisions = true;
        std::cout << "Generating collision report on next frame..." << std::endl;
        break;
    }
}

// GLUT reshape callback
void reshape(int w, int h) {

    glViewport(0, 0, w, h);

    WIDTH = w;
    HEIGHT = h;

    glDeleteTextures(2, velocityTexture);
    glDeleteTextures(2, pressureTexture);
    glDeleteTextures(2, densityTexture);
    glDeleteTextures(1, &divergenceTexture);
    glDeleteTextures(1, &obstacleTexture);
    glDeleteTextures(1, &collisionTexture);
    glDeleteFramebuffers(1, &fbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(advectProgram);
    glDeleteProgram(divergenceProgram);
    glDeleteProgram(pressureProgram);
    glDeleteProgram(gradientSubtractProgram);
    glDeleteProgram(addForceProgram);
    glDeleteProgram(addDensityProgram);
    glDeleteProgram(diffuseDensityProgram);
    glDeleteProgram(addObstacleProgram);
    glDeleteProgram(detectCollisionProgram);

    initGL();


}

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
    glutSpecialFunc(keyboardSpecial);
    glutKeyboardFunc(keyboard);
    glutReshapeFunc(reshape);

    // Print instructions
    std::cout << "GPU-Accelerated Navier-Stokes Solver" << std::endl;
    std::cout << "-----------------------------------" << std::endl;
    std::cout << "Left Mouse Button: Add velocity and density" << std::endl;
    std::cout << "Right Mouse Button: Add obstacles" << std::endl;
    std::cout << "F1: Reset simulation" << std::endl;
    std::cout << "C: Generate collision report immediately" << std::endl;
    std::cout << "ESC: Exit" << std::endl;
    std::cout << "Orange highlights show density-obstacle collisions" << std::endl;
    std::cout << "Collision reports are generated every " << REPORT_INTERVAL << " frames" << std::endl;

    // Main loop
    glutMainLoop();

    return 0;
}