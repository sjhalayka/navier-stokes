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




// to do: test bullet force-colour functionality
// to do: All kinds of stamps have age and lifespan and force/colour radius. cull after a certain lifespan length
// to do: Bullets and explosions use multiple sized force and velocity radii, detail on multiple scales
// to do: for example, a dead enemy disappears / fades, and is replaced by an explosion that consists of force and colour on multiple scales
// to do: make Bezier path for enemy ships. Along with the path is density along the curve; the denser the path, the slower the traveller is along that path
// to do: Give the player the option to use a shield for 30 seconds, for say, 1 unit of health.



// Simulation parameters
int WIDTH = 960;
int HEIGHT = 540;

const float FPS = 60;
const float DT = 1.0f / FPS;
const float VISCOSITY = 0.5f;     // Fluid viscosity
const float DIFFUSION = 0.5f;    //  diffusion rate
const float FORCE = 5000.0f;         // Force applied by mouse
const float OBSTACLE_RADIUS = 0.1f; // Radius of obstacle
const float COLLISION_THRESHOLD = 0.5f; // Threshold for color-obstacle collision
const int FLUID_STAMP_COLLISION_REPORT_INTERVAL = FPS / 6; // Every 10 frames update the collision data

const float COLOR_DETECTION_THRESHOLD = 0.01f;  // How strict the color matching should be

float global_time = 0.0f;

bool red_mode = true;

bool lastRightMouseDown = false;


vector<float> global_minXs;
vector<float> global_minYs;
vector<float> global_maxXs;
vector<float> global_maxYs;


//
//float global_minX, global_minY, global_maxX, global_maxY;



struct Stamp {
	// StampTexture properties
	std::vector<GLuint> textureIDs;         // Multiple texture IDs
	int width = 0; // pixels
	int height = 0; // pixels
	std::string baseFilename;               // Base filename without suffix
	std::vector<std::string> textureNames;  // Names of the specific textures
	std::vector<std::vector<unsigned char>> pixelData;  // Multiple pixel data arrays
	int channels = 0;                         // Store the number of channels

	bool to_be_culled = false;

	float health = 1;

	float birth_time = 0;
	// A negative death time means that the bullet is immortal 
	// (it is culled only when colliding with the ally/enemy or goes off screen)
	// A mortal bullet dies after a certain amount of time too
	float death_time = -1;

	float stamp_opacity = 1;

	float force_radius = 0.05;
	float colour_radius = force_radius;

	// StampInfo properties
	float posX = 0, posY = 0;                       // Normalized position (0-1)
	float velX = 0, velY = 0;

	int currentVariationIndex = 0;              // Which texture variation to use
};



std::vector<Stamp> allyShips;
std::vector<Stamp> enemyShips;
std::vector<Stamp> allyBullets;
std::vector<Stamp> enemyBullets;

std::vector<Stamp> stampTemplates;  // Stores template stamps (not active)
//std::vector<Stamp> activeStamps;    // Stores active stamp instances
int currentTemplateIndex = 0;       // Index for selecting template stamps








void calculateBoundingBox(const Stamp& stamp, float& minX, float& minY, float& maxX, float& maxY) {
	// Calculate aspect ratio
	float aspect = WIDTH / static_cast<float>(HEIGHT);

	// For the stamp positioning, we need to match how the stamp is rendered
	// The stamp is rendered with the same scaling as in the stampTextureFragmentShader
	float scale = 2.0f;

	// Calculate the actual width and height in normalized coordinates
	float halfWidthNorm = (stamp.width / 2.0f) / WIDTH;
	float halfHeightNorm = (stamp.height / 2.0f) / HEIGHT;

	// Apply the same aspect ratio adjustments as in the shader
	float stampY = (stamp.posY - 0.5f) * aspect + 0.5f;

	// Apply the scaling factor to match the shader
	halfWidthNorm /= scale;
	halfHeightNorm /= scale;

	// Set the bounding box coordinates
	minX = stamp.posX - halfWidthNorm;
	minY = stampY - halfHeightNorm;
	maxX = stamp.posX + halfWidthNorm * scale;
	maxY = stampY + halfHeightNorm * scale;
}






void drawBoundingBox(float minX, float minY, float maxX, float maxY) {
	// Convert normalized coordinates to NDC coordinates (-1 to 1)
	float ndcMinX = minX * 2.0f - 1.0f;
	float ndcMinY = minY * 2.0f - 1.0f;
	float ndcMaxX = maxX * 2.0f - 1.0f;
	float ndcMaxY = maxY * 2.0f - 1.0f;

	glColor3f(1.0f, 0.0f, 0.0f); // Red for bounding box

	glBegin(GL_LINE_LOOP);
	glVertex2f(ndcMinX, ndcMinY); // Bottom-left
	glVertex2f(ndcMaxX, ndcMinY); // Bottom-right
	glVertex2f(ndcMaxX, ndcMaxY); // Top-right
	glVertex2f(ndcMinX, ndcMaxY); // Top-left
	glEnd();
}

// 5. Modified drawBoundingBox for stamp to use our improved calculation
void drawBoundingBox(const Stamp& stamp) {
	//if (!stamp.active) return;

	float minX, minY, maxX, maxY;
	calculateBoundingBox(stamp, minX, minY, maxX, maxY);

	// Convert normalized coordinates to NDC coordinates (-1 to 1)
	float ndcMinX = minX * 2.0f - 1.0f;
	float ndcMinY = minY * 2.0f - 1.0f;
	float ndcMaxX = maxX * 2.0f - 1.0f;
	float ndcMaxY = maxY * 2.0f - 1.0f;

	glColor3f(1.0f, 0.0f, 0.0f); // Red for bounding box

	glBegin(GL_LINE_LOOP);
	glVertex2f(ndcMinX, ndcMinY); // Bottom-left
	glVertex2f(ndcMaxX, ndcMinY); // Bottom-right
	glVertex2f(ndcMaxX, ndcMaxY); // Top-right
	glVertex2f(ndcMinX, ndcMaxY); // Top-left
	glEnd();
}





bool isBoundingBoxOverlap(const Stamp& a, const Stamp& b) {
	float aMinX, aMinY, aMaxX, aMaxY;
	float bMinX, bMinY, bMaxX, bMaxY;

	calculateBoundingBox(a, aMinX, aMinY, aMaxX, aMaxY);
	calculateBoundingBox(b, bMinX, bMinY, bMaxX, bMaxY);

	return !(aMaxX < bMinX || aMinX > bMaxX ||
		aMaxY < bMinY || aMinY > bMaxY);
}







unsigned char getPixelValueFromStamp(const Stamp& stamp, int variationIndex, int x, int y, int channel) {
	// Make sure coordinates and indices are within bounds
	if (x < 0 || x >= stamp.width || y < 0 || y >= stamp.height ||
		channel < 0 || channel >= stamp.channels ||
		variationIndex < 0 || variationIndex >= stamp.pixelData.size() ||
		stamp.pixelData[variationIndex].empty()) {
		return 0;
	}

	// Calculate the index in the pixel data array
	int index = (y * stamp.width + x) * stamp.channels + channel;

	// Make sure the index is within bounds
	if (index < 0 || index >= stamp.pixelData[variationIndex].size()) {
		return 0;
	}

	return stamp.pixelData[variationIndex][index];
}


bool isPixelPerfectCollision(const Stamp& a, const Stamp& b) {
	float aMinX, aMinY, aMaxX, aMaxY;
	float bMinX, bMinY, bMaxX, bMaxY;

	calculateBoundingBox(a, aMinX, aMinY, aMaxX, aMaxY);
	calculateBoundingBox(b, bMinX, bMinY, bMaxX, bMaxY);

	// Quick check if bounding boxes overlap
	if (!(aMaxX >= bMinX && aMinX <= bMaxX && aMaxY >= bMinY && aMinY <= bMaxY)) {
		return false;
	}

	// Calculate overlapping region in normalized coordinates
	float overlapMinX = std::max(aMinX, bMinX);
	float overlapMaxX = std::min(aMaxX, bMaxX);
	float overlapMinY = std::max(aMinY, bMinY);
	float overlapMaxY = std::min(aMaxY, bMaxY);

	// Convert to pixel coordinates for both stamps
	for (float y = overlapMinY; y < overlapMaxY; y += 1.0f / HEIGHT) {
		for (float x = overlapMinX; x < overlapMaxX; x += 1.0f / WIDTH) {
			// Map to texture space for both stamps
			int texAx = (x - aMinX) / (aMaxX - aMinX) * a.width;
			int texAy = (y - aMinY) / (aMaxY - aMinY) * a.height;
			int texBx = (x - bMinX) / (bMaxX - bMinX) * b.width;
			int texBy = (y - bMinY) / (bMaxY - bMinY) * b.height;

			// Get alpha values
			float alphaA = getPixelValueFromStamp(a, a.currentVariationIndex, texAx, texAy, 3) / 255.0f;
			float alphaB = getPixelValueFromStamp(b, b.currentVariationIndex, texBx, texBy, 3) / 255.0f;

			if (alphaA > 0.0f && alphaB > 0.0f) {
				return true; // Pixels overlap with sufficient alpha
			}
		}
	}

	return false;
}






void reportStampToStampCollisions() {
	std::cout << "\n===== Stamp-to-Stamp Collision Report =====" << std::endl;
	bool collisionDetected = false;

	// Check ally bullets with enemy ships (attack collision)
	std::cout << "** Ally Bullets vs Enemy Ships **" << std::endl;
	for (size_t i = 0; i < allyBullets.size(); ++i) {
		//if (!allyBullets[i].active) continue;

		for (size_t j = 0; j < enemyShips.size(); ++j) {
			//if (!enemyShips[j].active) continue;

			if (isPixelPerfectCollision(allyBullets[i], enemyShips[j])) {
				collisionDetected = true;
				std::cout << "Attack collision: Ally Bullet #" << i + 1
					<< " hit Enemy Ship #" << j + 1 << std::endl;
			}
		}
	}

	// Check enemy bullets with ally ships (attack collision)
	std::cout << "\n** Enemy Bullets vs Ally Ships **" << std::endl;
	for (size_t i = 0; i < enemyBullets.size(); ++i) {
		//if (!enemyBullets[i].active) continue;

		for (size_t j = 0; j < allyShips.size(); ++j) {
			//if (!allyShips[j].active) continue;

			if (isPixelPerfectCollision(enemyBullets[i], allyShips[j])) {
				collisionDetected = true;
				std::cout << "Attack collision: Enemy Bullet #" << i + 1
					<< " hit Ally Ship #" << j + 1 << std::endl;
			}
		}
	}

	// Check ally ships with enemy ships (ship-to-ship collision)
	//std::cout << "\n** Ally Ships vs Enemy Ships **" << std::endl;
	//for (size_t i = 0; i < allyShips.size(); ++i) {
	//	//if (!allyShips[i].active) continue;

	//	for (size_t j = 0; j < enemyShips.size(); ++j) {
	//		//if (!enemyShips[j].active) continue;

	//		if (isPixelPerfectCollision(allyShips[i], enemyShips[j])) {
	//			collisionDetected = true;
	//			std::cout << "Ship collision: Ally Ship #" << i + 1
	//				<< " collided with Enemy Ship #" << j + 1 << std::endl;
	//		}
	//	}
	//}

	//// Check ally ships with ally ships (friendly collision)
	//std::cout << "\n** Ally Ships vs Ally Ships **" << std::endl;
	//for (size_t i = 0; i < allyShips.size(); ++i) {
	//	//if (!allyShips[i].active) continue;

	//	for (size_t j = i + 1; j < allyShips.size(); ++j) {
	//		//if (!allyShips[j].active) continue;

	//		if (isPixelPerfectCollision(allyShips[i], allyShips[j])) {
	//			collisionDetected = true;
	//			std::cout << "Friendly collision: Ally Ship #" << i + 1
	//				<< " collided with Ally Ship #" << j + 1 << std::endl;
	//		}
	//	}
	//}

	//// Check enemy ships with enemy ships (enemy friendly collision)
	//std::cout << "\n** Enemy Ships vs Enemy Ships **" << std::endl;
	//for (size_t i = 0; i < enemyShips.size(); ++i) {
	//	//if (!enemyShips[i].active) continue;

	//	for (size_t j = i + 1; j < enemyShips.size(); ++j) {
	//		//if (!enemyShips[j].active) continue;

	//		if (isPixelPerfectCollision(enemyShips[i], enemyShips[j])) {
	//			collisionDetected = true;
	//			std::cout << "Enemy collision: Enemy Ship #" << i + 1
	//				<< " collided with Enemy Ship #" << j + 1 << std::endl;
	//		}
	//	}
	//}

	//// Check ally bullets with ally bullets (friendly fire crossover)
	//std::cout << "\n** Ally Bullets vs Ally Bullets **" << std::endl;
	//for (size_t i = 0; i < allyBullets.size(); ++i) {
	//	//if (!allyBullets[i].active) continue;

	//	for (size_t j = i + 1; j < allyBullets.size(); ++j) {
	//		//if (!allyBullets[j].active) continue;

	//		if (isPixelPerfectCollision(allyBullets[i], allyBullets[j])) {
	//			collisionDetected = true;
	//			std::cout << "Friendly fire crossover: Ally Bullet #" << i + 1
	//				<< " crossed with Ally Bullet #" << j + 1 << std::endl;
	//		}
	//	}
	//}

	//// Check enemy bullets with enemy bullets (enemy fire crossover)
	//std::cout << "\n** Enemy Bullets vs Enemy Bullets **" << std::endl;
	//for (size_t i = 0; i < enemyBullets.size(); ++i) {
	//	//if (!enemyBullets[i].active) continue;

	//	for (size_t j = i + 1; j < enemyBullets.size(); ++j) {
	//		//if (!enemyBullets[j].active) continue;

	//		if (isPixelPerfectCollision(enemyBullets[i], enemyBullets[j])) {
	//			collisionDetected = true;
	//			std::cout << "Enemy fire crossover: Enemy Bullet #" << i + 1
	//				<< " crossed with Enemy Bullet #" << j + 1 << std::endl;
	//		}
	//	}
	//}

	//// Check ally bullets with enemy bullets (opposing fire collision)
	//std::cout << "\n** Ally Bullets vs Enemy Bullets **" << std::endl;
	//for (size_t i = 0; i < allyBullets.size(); ++i) {
	//	//if (!allyBullets[i].active) continue;

	//	for (size_t j = 0; j < enemyBullets.size(); ++j) {
	//		//if (!enemyBullets[j].active) continue;

	//		if (isPixelPerfectCollision(allyBullets[i], enemyBullets[j])) {
	//			collisionDetected = true;
	//			std::cout << "Opposing fire collision: Ally Bullet #" << i + 1
	//				<< " collided with Enemy Bullet #" << j + 1 << std::endl;
	//		}
	//	}
	//}

	if (!collisionDetected) {
		std::cout << "No stamp-to-stamp collisions detected." << std::endl;
	}

	std::cout << "============================================" << std::endl;
}





bool upKeyPressed = false;
bool downKeyPressed = false;
bool rightKeyPressed = false;
bool leftKeyPressed = false;


int lastVariationIndex = 0; // Track last variation to detect changes





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
GLuint detectCollisionProgram;
GLuint addColorProgram;
GLuint diffuseColorProgram;
GLuint diffuseVelocityProgram;
GLuint stampObstacleProgram;
GLuint stampTextureProgram;
GLuint renderProgram;




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

	float r;
	float b;

	CollisionPoint(int _x, int _y, float _r, float _b) : x(_x), y(_y), r(_r), b(_b) {}
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



const char* stampTextureFragmentShader = R"(
#version 330 core
uniform sampler2D stampTexture;
uniform vec2 position;
uniform vec2 stampSize;
uniform float threshold;
uniform vec2 screenSize;
uniform float stamp_opacity;
in vec2 TexCoord;
out vec4 FragColor;

void main() 
{
    // Get dimensions
    vec2 stampTexSize = vec2(textureSize(stampTexture, 0));
    float windowAspect = screenSize.x / screenSize.y;
    
    // Calculate coordinates in stamp texture - use the same approach as the obstacle shader
    vec2 stampCoord = (TexCoord - position) * screenSize / (stampTexSize/2.0) + vec2(0.5);
    
    //// Apply aspect ratio correction
    //if (windowAspect > 1.0) {
    //    // Wide window - adjust x coordinate
    //    stampCoord.x = (stampCoord.x - 0.5) / windowAspect + 0.5;
    //} else if (windowAspect < 1.0) {
    //    // Tall window - adjust y coordinate
    //    stampCoord.y = (stampCoord.y - 0.5) * windowAspect + 0.5;
    //}
    
	// why is this necessary?
	stampCoord /= 1.5;//sqrt(2.0);

    // Check if we're within stamp bounds
    if (stampCoord.x >= 0.0 && stampCoord.x <= 1.0 && 
        stampCoord.y >= 0.0 && stampCoord.y <= 1.0) 
	{
        // Sample stamp texture (use all channels for RGBA output)
        vec4 stampColor = texture(stampTexture, stampCoord);

		stampColor.a *= stamp_opacity;      
        FragColor = stampColor;
    } 
	else
	{
        // Outside stamp bounds - transparent
        FragColor = vec4(0.0, 0.0, 0.0, 0.0);
    }
}
)";




const char* stampObstacleFragmentShader = R"(
#version 330 core
uniform sampler2D obstacleTexture;
uniform sampler2D stampTexture;
uniform vec2 position;
uniform vec2 stampSize;
uniform float threshold;
uniform vec2 screenSize; // Add this uniform to match texture shader

out float FragColor;

in vec2 TexCoord;

void main() 
{
    // Get current obstacle value
    float obstacle = texture(obstacleTexture, TexCoord).r;
    
    // Get dimensions
    vec2 stampTexSize = vec2(textureSize(stampTexture, 0));
    vec2 obstacleTexSize = vec2(textureSize(obstacleTexture, 0));
    float windowAspect = obstacleTexSize.x / obstacleTexSize.y; // Or use screenSize if passed
    
    // Calculate coordinates in stamp texture - use the same approach as the texture shader
    vec2 stampCoord = (TexCoord - position) * obstacleTexSize / (stampTexSize/2.0) + vec2(0.5);
    
	if(windowAspect > 1.0)
	stampCoord.y = (stampCoord.y - 0.5) * windowAspect + 0.5;

	// why is this necessary?
	stampCoord /= 1.5;//sqrt(2.0);


    // Check if we're within stamp bounds
    if (stampCoord.x >= 0.0 && stampCoord.x <= 1.0 && 
        stampCoord.y >= 0.0 && stampCoord.y <= 1.0) {
        
        // Sample stamp texture (use alpha channel for transparency)
        float stampValue = texture(stampTexture, stampCoord).a;
        
        // Apply threshold to make it binary
        stampValue = stampValue > threshold ? 1.0 : 0.0;
        
        // Combine with existing obstacle (using max for union)
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
const float fake_dispersion = 0.9;

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
//        result = vec4(-vel, 0.0, 1.0);


        // If we sampled from an obstacle, kill the velocity
		// We do this to avoid generating the opposite colour as a bug
		result = vec4(0.0, 0.0, 0.0, 1.0);

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
		// keep track of fire-smoke intensity. less intense, less damage per second
        if (redCollision && blueCollision) {
            // Both red and blue collided
            FragColor = vec4(maxRed, 0.0, maxBlue, 1.0); // Magenta for both
        } else if (redCollision) {
            // Only red collided
            FragColor = vec4(maxRed, 0.0, 0.0, 1.0); // Red
        } else if (blueCollision) {
            // Only blue collided
            FragColor = vec4(0.0, 0.0, maxBlue, 1.0); // Blue
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

    // Check for obstacle
    float obstacle = texture(obstacleTexture, adjustedCoord).r;

    if (obstacle > 0.0) 
	{
		//FragColor = vec4(1.0, 1.0, 1.0, 1.0);
		//return;

        // Render obstacles as background coloured
		FragColor = texture(backgroundTexture, scrolledCoord);
		return;
    }


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

void updateDynamicTexture(Stamp& stamp) {
	// For each valid texture in the stamp
	for (size_t i = 0; i < stamp.textureIDs.size(); i++) {
		if (stamp.textureIDs[i] != 0 && i < stamp.pixelData.size() && !stamp.pixelData[i].empty()) {
			glBindTexture(GL_TEXTURE_2D, stamp.textureIDs[i]);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, stamp.width, stamp.height,
				(stamp.channels == 1) ? GL_RED :
				(stamp.channels == 3) ? GL_RGB : GL_RGBA,
				GL_UNSIGNED_BYTE, stamp.pixelData[i].data());
		}
	}
}



void clearObstacleTexture() {
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, obstacleTexture, 0);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
}


void reapplyAllStamps() {
	auto processStamps = [&](const std::vector<Stamp>& stamps) {
		for (const auto& stamp : stamps) {

			// If the stamp is dead then don't use it for an obstacle
			// This is so that the stamp doesn't interfere with the colour / force of its explosion when it dies and fades away
			if (stamp.to_be_culled) continue;

			int variationIndex = stamp.currentVariationIndex;
			if (variationIndex < 0 || variationIndex >= stamp.textureIDs.size() ||
				stamp.textureIDs[variationIndex] == 0) {
				for (size_t i = 0; i < stamp.textureIDs.size(); i++) {
					if (stamp.textureIDs[i] != 0) {
						variationIndex = i;
						break;
					}
				}
				if (variationIndex < 0 || variationIndex >= stamp.textureIDs.size() ||
					stamp.textureIDs[variationIndex] == 0) {
					continue;
				}
			}

			glUniform2f(glGetUniformLocation(stampObstacleProgram, "position"), stamp.posX, stamp.posY);
			glUniform2f(glGetUniformLocation(stampObstacleProgram, "stampSize"), stamp.width, stamp.height);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, obstacleTexture);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, stamp.textureIDs[variationIndex]);

			glBindVertexArray(vao);
			glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
		}
	};

	if (allyShips.empty() && enemyShips.empty() && allyBullets.empty() && enemyBullets.empty()) return;

	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, obstacleTexture, 0);

	glUseProgram(stampObstacleProgram);

	glUniform1i(glGetUniformLocation(stampObstacleProgram, "obstacleTexture"), 0);
	glUniform1i(glGetUniformLocation(stampObstacleProgram, "stampTexture"), 1);
	glUniform1f(glGetUniformLocation(stampObstacleProgram, "threshold"), 0.5f);
	glUniform2f(glGetUniformLocation(stampObstacleProgram, "screenSize"), WIDTH, HEIGHT);

	processStamps(allyShips);
	processStamps(enemyShips);

	// Don't treat bullets as obstacles
	//processStamps(allyBullets);
	//processStamps(enemyBullets);
}







bool loadStampTextureFile(const char* filename, std::vector<unsigned char>& pixelData, GLuint& textureID, int& width, int& height, int& channels) {
	// Load image using stb_image - Don't flip vertically for our pixel data
	stbi_set_flip_vertically_on_load(true);
	unsigned char* data = stbi_load(filename, &width, &height, &channels, 0);

	if (!data) {
		std::cerr << "Failed to load stamp texture: " << filename << std::endl;
		std::cerr << "STB Image error: " << stbi_failure_reason() << std::endl;
		return false;
	}

	// Store the pixel data in the vector
	int dataSize = width * height * channels;
	pixelData.resize(dataSize);
	std::memcpy(pixelData.data(), data, dataSize);

	// Now set flip for OpenGL texture loading
	stbi_set_flip_vertically_on_load(true);
	unsigned char* glData = stbi_load(filename, &width, &height, &channels, 0);

	if (!glData) {
		std::cerr << "Failed to reload texture for OpenGL: " << filename << std::endl;
		// If we fail to reload, use the original data for OpenGL too
		glData = data;
	}

	// Create texture
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_2D, textureID);

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
	glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, glData);

	// Free image data - we've already copied what we need
	if (glData != data) {
		stbi_image_free(glData);
	}
	stbi_image_free(data);

	return true;
}










bool loadStampTextures() {
	// Clear previous template textures
	for (auto& stamp : stampTemplates) {
		for (auto& texID : stamp.textureIDs) {
			if (texID != 0) {
				glDeleteTextures(1, &texID);
			}
		}
	}
	stampTemplates.clear();

	// Define the file prefixes for each type of game object
	const std::vector<std::string> prefixes = { "obstacle", "bullet", "enemy" };
	const std::vector<std::string> variations = { "_centre", "_up", "_down" };

	bool loadedAny = false;

	// For each prefix, attempt to load all indexed textures
	for (const auto& prefix : prefixes) {
		int index = 0;

		while (true) {
			std::string baseFilename = prefix + std::to_string(index);
			Stamp newStamp;
			newStamp.baseFilename = baseFilename;
			newStamp.textureNames = { "centre", "up", "down" };
			newStamp.currentVariationIndex = 0; // Default to center

			bool loadedAtLeastOne = false;

			for (size_t i = 0; i < variations.size(); i++) {
				std::string filename = baseFilename + variations[i] + ".png";
				GLuint textureID = 0;
				int width = 0, height = 0, channels = 0;
				std::vector<unsigned char> pixelData;

				if (loadStampTextureFile(filename.c_str(), pixelData, textureID, width, height, channels)) {
					if (newStamp.pixelData.empty()) {
						newStamp.width = width;
						newStamp.height = height;
						newStamp.channels = channels;
					}
					newStamp.textureIDs.push_back(textureID);
					newStamp.pixelData.push_back(std::move(pixelData));
					std::cout << "Loaded stamp texture: " << filename << " (" << width << "x" << height << ")" << std::endl;
					loadedAtLeastOne = true;
				}
				else {
					newStamp.textureIDs.push_back(0);
					newStamp.pixelData.push_back(std::vector<unsigned char>());
				}
			}

			if (loadedAtLeastOne) {
				stampTemplates.push_back(std::move(newStamp));
				loadedAny = true;
				index++;
			}
			else {
				break; // No more textures with this prefix
			}
		}
	}

	if (loadedAny) {
		currentTemplateIndex = 0;
	}

	return loadedAny;
}



bool isCollisionInStamp(const CollisionPoint& point, const Stamp& stamp) {
	//if (!stamp.active) return false;

	//// Validate variation index
	int variationIndex = stamp.currentVariationIndex;
	//if (variationIndex < 0 || variationIndex >= stamp.pixelData.size() ||
	//	stamp.pixelData[variationIndex].empty()) {
	//	// Fall back to first available texture
	//	for (size_t i = 0; i < stamp.pixelData.size(); i++) {
	//		if (!stamp.pixelData[i].empty()) {
	//			variationIndex = i;
	//			break;
	//		}
	//	}
	//	// If still no valid texture, return false
	//	if (variationIndex < 0 || variationIndex >= stamp.pixelData.size() ||
	//		stamp.pixelData[variationIndex].empty()) {
	//		return false;
	//	}
	//}

	//if (stamp.pixelData[variationIndex].empty()) {
	//	// No pixel data available, fall back to the bounding box check
	//	float minX, minY, maxX, maxY;
	//	calculateBoundingBox(stamp, minX, minY, maxX, maxY);

	//	float aspect = HEIGHT / float(WIDTH);

	//	// Convert pixel coordinates to normalized coordinates (0-1)
	//	float pointX = point.x / (float)WIDTH;
	//	float pointY = point.y / (float)HEIGHT; // Y is already in screen coordinates

	//	// Apply aspect ratio correction to y-coordinate
	//	pointY = (pointY - 0.5f) * aspect + 0.5f;

	//	// Simple bounding box check
	//	return (pointX >= minX && pointX <= maxX &&
	//		pointY >= minY && pointY <= maxY);
	//}

	// Get the normalized stamp position (0-1 in screen space)
	float stampX = stamp.posX;  // Normalized X position
	float stampY = stamp.posY;  // Normalized Y position

	// Calculate the screen-space dimensions of the stamp
	float aspect = HEIGHT / float(WIDTH);
	float stampWidth = stamp.width / float(WIDTH);  // In normalized screen units
	float stampHeight = (stamp.height / float(HEIGHT)) * aspect;  // Adjust for aspect ratio

	// Convert collision point to normalized screen coordinates
	float pointX = point.x / float(WIDTH);  // 0-1 range
	float pointY = point.y / float(HEIGHT);  // 0-1 range

	// SUPER IMPORTANT
	pointY = (pointY - 0.5f) / aspect + 0.5f;

	// Calculate bounding box
	float minX, minY, maxX, maxY;
	calculateBoundingBox(stamp, minX, minY, maxX, maxY);

	//global_minXs.push_back(minX);
	//global_minYs.push_back(minY);
	//global_maxXs.push_back(maxX);
	//global_maxYs.push_back(maxY);

	// Check if the collision point is within the stamp's bounding box
	if (pointX < minX || pointX > maxX || pointY < minY || pointY > maxY)
		return false;

	// Map the collision point to texture coordinates
	float texCoordX = (pointX - minX) / (maxX - minX);
	float texCoordY = (pointY - minY) / (maxY - minY);

	// Convert to pixel coordinates in the texture
	int pixelX = int(texCoordX * stamp.width);
	int pixelY = int(texCoordY * stamp.height);
	pixelX = std::max(0, std::min(pixelX, stamp.width - 1));
	pixelY = std::max(0, std::min(pixelY, stamp.height - 1));

	// Get the alpha/opacity at this pixel for the current variation
	float opacity = 0.0f;
	if (stamp.channels == 4) {
		// Use alpha channel for RGBA textures
		opacity = getPixelValueFromStamp(stamp, variationIndex, pixelX, pixelY, 3) / 255.0f;
	}
	else if (stamp.channels == 1) {
		// Use intensity for grayscale
		opacity = getPixelValueFromStamp(stamp, variationIndex, pixelX, pixelY, 0) / 255.0f;
	}
	else if (stamp.channels == 3) {
		// For RGB, use average intensity as opacity
		float r = getPixelValueFromStamp(stamp, variationIndex, pixelX, pixelY, 0) / 255.0f;
		float g = getPixelValueFromStamp(stamp, variationIndex, pixelX, pixelY, 1) / 255.0f;
		float b = getPixelValueFromStamp(stamp, variationIndex, pixelX, pixelY, 2) / 255.0f;
		opacity = (r + g + b) / 3.0f;
	}

	// Check if the pixel is opaque enough for a collision
	return opacity > COLOR_DETECTION_THRESHOLD;
}


void generateFluidStampCollisionsDamage()
{
	if (collisionPoints.empty())
		return;

	auto generateFluidCollisionsForStamps = [&](std::vector<Stamp>& stamps, const std::string& type)
	{
		int stampHitCount = 0;

		for (size_t i = 0; i < stamps.size(); i++)
		{
			float minX, minY, maxX, maxY;
			calculateBoundingBox(stamps[i], minX, minY, maxX, maxY);

			int stampCollisions = 0;
			int redStampCollisions = 0;
			int blueStampCollisions = 0;
			int bothStampCollisions = 0;

			float red_count = 0;
			float blue_count = 0;

			// Test each collision point against this stamp
			for (const auto& point : collisionPoints)
			{
				float normX = point.x / float(WIDTH);
				float normY = point.y / float(HEIGHT);

				// Perform the actual collision check
				bool collides = isCollisionInStamp(point, stamps[i]);

				if (collides) {
					stampCollisions++;

					if (point.r > 0) {
						red_count += point.r;
						redStampCollisions++;
					}

					if (point.b > 0) {
						blue_count += point.b;
						blueStampCollisions++;
					}

					if (point.r > 0 && point.b > 0) {
						red_count += point.r;
						blue_count += point.b;

						bothStampCollisions++;
					}
				}
			}

			// Report collisions for this stamp
			if (stampCollisions > 0)
			{
				stampHitCount++;

				std::string textureName = stamps[i].baseFilename;
				std::string variationName = "unknown";

				if (stamps[i].currentVariationIndex < stamps[i].textureNames.size())
					variationName = stamps[i].textureNames[stamps[i].currentVariationIndex];

				float damage = 0.0f;

				if (type == "Ally Ship")
					damage = blue_count;
				else
					damage = red_count;

				const float fps_coeff = float(FLUID_STAMP_COLLISION_REPORT_INTERVAL) / FPS;

				stamps[i].health -= damage * DT * fps_coeff;
				cout << stamps[i].health << endl;
			}
		}
	};

	generateFluidCollisionsForStamps(allyShips, "Ally Ship");
	generateFluidCollisionsForStamps(enemyShips, "Enemy Ship");
}









void applyBitmapObstacle()
{
	if (!rightMouseDown || stampTemplates.empty()) return;

	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, obstacleTexture, 0);

	glUseProgram(stampObstacleProgram);

	float mousePosX = mouseX / (float)WIDTH;
	float mousePosY = 1.0f - (mouseY / (float)HEIGHT);

	lastRightMouseDown = rightMouseDown;

	// Preview the current template stamp at mouse position
	const Stamp& currentStamp = stampTemplates[currentTemplateIndex];
	int currentVariation = 0;
	if (upKeyPressed) {
		currentVariation = 1;
	}
	else if (downKeyPressed) {
		currentVariation = 2;
	}
	if (currentVariation >= currentStamp.textureIDs.size() ||
		currentStamp.textureIDs[currentVariation] == 0) {
		for (size_t i = 0; i < currentStamp.textureIDs.size(); i++) {
			if (currentStamp.textureIDs[i] != 0) {
				currentVariation = i;
				break;
			}
		}
	}

	glUniform1i(glGetUniformLocation(stampObstacleProgram, "obstacleTexture"), 0);
	glUniform1i(glGetUniformLocation(stampObstacleProgram, "stampTexture"), 1);
	glUniform2f(glGetUniformLocation(stampObstacleProgram, "position"), mousePosX, mousePosY);
	glUniform2f(glGetUniformLocation(stampObstacleProgram, "stampSize"),
		currentStamp.width, currentStamp.height);
	glUniform1f(glGetUniformLocation(stampObstacleProgram, "threshold"), 0.5f);
	glUniform2f(glGetUniformLocation(stampObstacleProgram, "screenSize"), WIDTH, HEIGHT);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, obstacleTexture);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, currentStamp.textureIDs[currentVariation]);

	glBindVertexArray(vao);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
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





void detectCollisions()
{
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
				collisionPoints.push_back(CollisionPoint(x, y, r, b));
			}
		}
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

	loadStampTextures();

	// Create shader programs
	advectProgram = createShaderProgram(vertexShaderSource, advectFragmentShader);
	divergenceProgram = createShaderProgram(vertexShaderSource, divergenceFragmentShader);
	pressureProgram = createShaderProgram(vertexShaderSource, pressureFragmentShader);
	gradientSubtractProgram = createShaderProgram(vertexShaderSource, gradientSubtractFragmentShader);
	addForceProgram = createShaderProgram(vertexShaderSource, addForceFragmentShader);
	detectCollisionProgram = createShaderProgram(vertexShaderSource, detectCollisionFragmentShader);
	addColorProgram = createShaderProgram(vertexShaderSource, addColorFragmentShader);
	diffuseColorProgram = createShaderProgram(vertexShaderSource, diffuseColorFragmentShader);
	stampObstacleProgram = createShaderProgram(vertexShaderSource, stampObstacleFragmentShader);
	diffuseVelocityProgram = createShaderProgram(vertexShaderSource, diffuseVelocityFragmentShader);
	stampTextureProgram = createShaderProgram(vertexShaderSource, stampTextureFragmentShader);
	renderProgram = createShaderProgram(vertexShaderSource, renderFragmentShader);



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








void applyForceCore(float posX, float posY, float velX, float velY, float radius, float strength) {
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, velocityTexture[1 - velocityIndex], 0);

	glUseProgram(addForceProgram);

	float aspect = HEIGHT / float(WIDTH);

	// Convert normalized position (0-1) to the appropriate coordinates for shader
	float shaderPosX = posX;
	float shaderPosY = posY;

	// Apply aspect ratio adjustment to position - uncomment this for consistency
	shaderPosY = (shaderPosY - 0.5f) * aspect + 0.5f;

	float x_shift = 0.05 * rand() / float(RAND_MAX);
	float y_shift = 0.05 * rand() / float(RAND_MAX);

	shaderPosX += x_shift;
	shaderPosY += y_shift;

	// Scale velocity to account for aspect ratio
	float shaderVelX = velX * aspect;
	float shaderVelY = velY;

	// Set uniforms
	glUniform1i(glGetUniformLocation(addForceProgram, "velocityTexture"), 0);
	glUniform1i(glGetUniformLocation(addForceProgram, "obstacleTexture"), 1);
	glUniform2f(glGetUniformLocation(addForceProgram, "point"), shaderPosX, shaderPosY);
	glUniform2f(glGetUniformLocation(addForceProgram, "direction"), shaderVelX, shaderVelY);
	glUniform1f(glGetUniformLocation(addForceProgram, "radius"), radius);
	glUniform1f(glGetUniformLocation(addForceProgram, "strength"), strength);

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










void addMouseForce(float radius, float strength) {
	// Get normalized mouse position (0 to 1 range)
	float mousePosX = mouseX / (float)WIDTH;
	float mousePosY = 1.0f - (mouseY / (float)HEIGHT);  // Invert Y for OpenGL coordinates

	// Calculate velocity from mouse movement
	float mouseVelX = (mouseX - prevMouseX) / (float)WIDTH;  // Normalize to texture space
	float mouseVelY = -(mouseY - prevMouseY) / (float)HEIGHT; // Normalize to texture space

	// Apply the force using the core implementation
	applyForceCore(mousePosX, mousePosY, mouseVelX, mouseVelY, radius, strength);

	// Update previous mouse position
	prevMouseX = mouseX;
	prevMouseY = mouseY;
}



void addForce(float posX, float posY, float velX, float velY, float radius, float strength) {
	applyForceCore(posX, posY, velX, velY, radius, strength);
}


void addColor(float posX, float posY, float velX, float velY, float radius)
{
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

	float x_shift = 0.025 * rand() / float(RAND_MAX);
	float y_shift = 0.025 * rand() / float(RAND_MAX);

	float mousePosX = posX +x_shift;
	float mousePosY = posY +y_shift;

	// Set uniforms
	glUniform1i(glGetUniformLocation(addColorProgram, "colorTexture"), 0);
	glUniform1i(glGetUniformLocation(addColorProgram, "obstacleTexture"), 1);
	glUniform2f(glGetUniformLocation(addColorProgram, "point"), mousePosX, mousePosY);
	glUniform1f(glGetUniformLocation(addColorProgram, "radius"), radius);

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


void addMouseColor()
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












void updateObstacle()
{
	if (!rightMouseDown || stampTemplates.empty()) return;

	if (rightMouseDown && !lastRightMouseDown) {
		float aspect = HEIGHT / float(WIDTH);
		float mousePosX = mouseX / (float)WIDTH;
		float mousePosY = 1.0f - (mouseY / (float)HEIGHT);
		mousePosY = (mousePosY - 0.5f) * aspect + 0.5f;

		// Create new stamp from the current template
		Stamp newStamp = stampTemplates[currentTemplateIndex];
		newStamp.posX = mousePosX;
		newStamp.posY = mousePosY;

		// Set variation based on arrow key state
		if (upKeyPressed) {
			if (newStamp.textureIDs.size() > 1 && newStamp.textureIDs[1] != 0) {
				newStamp.currentVariationIndex = 1;
			}
			else {
				newStamp.currentVariationIndex = 0;
			}
		}
		else if (downKeyPressed) {
			if (newStamp.textureIDs.size() > 2 && newStamp.textureIDs[2] != 0) {
				newStamp.currentVariationIndex = 2;
			}
			else {
				newStamp.currentVariationIndex = 0;
			}
		}
		else {
			newStamp.currentVariationIndex = 0;
		}

		// Fall back to first available texture if necessary
		if (newStamp.currentVariationIndex >= newStamp.textureIDs.size() ||
			newStamp.textureIDs[newStamp.currentVariationIndex] == 0) {
			for (size_t i = 0; i < newStamp.textureIDs.size(); i++) {
				if (newStamp.textureIDs[i] != 0) {
					newStamp.currentVariationIndex = i;
					break;
				}
			}
		}

		// Add the stamp to the appropriate vector based on the file prefix in baseFilename
		std::string prefix = newStamp.baseFilename.substr(0, newStamp.baseFilename.find_first_of("0123456789"));

		if (prefix == "obstacle") {
			allyShips.push_back(newStamp);
			std::cout << "Added new ally ship";
		}
		else if (prefix == "bullet")
		{
			newStamp.velX = rand() / float(RAND_MAX) * 0.01;
			newStamp.velY = rand() / float(RAND_MAX) * 0.01;

			if (rand() % 2)
				newStamp.velX = -newStamp.velX;

			if (rand() % 2)
				newStamp.velY = -newStamp.velY;



			newStamp.birth_time = global_time;
			newStamp.death_time = -1;// global_time + 3.0 * rand() / float(RAND_MAX);

			allyBullets.push_back(newStamp);
			std::cout << "Added new ally bullet";
		}
		else if (prefix == "enemy")
		{
			//newStamp.velX = rand() / float(RAND_MAX) * 0.001;
			//newStamp.velY = rand() / float(RAND_MAX) * 0.001;

			//if (rand() % 2)
			//	newStamp.velX = -newStamp.velX;

			//if (rand() % 2)
			//	newStamp.velY = -newStamp.velY;

			enemyShips.push_back(newStamp);
			std::cout << "Added new enemy ship";
		}

		std::string variationName = "unknown";
		if (newStamp.currentVariationIndex < newStamp.textureNames.size()) {
			variationName = newStamp.textureNames[newStamp.currentVariationIndex];
		}

		std::cout << " at position (" << mousePosX << ", " << mousePosY << ") with texture: "
			<< newStamp.baseFilename << " (variation: " << variationName << ")" << std::endl;
	}

	lastRightMouseDown = rightMouseDown;
}


void move_bullets(void) 
{
	auto update_bullets = [&](std::vector<Stamp>& stamps) 
	{
		float aspect = HEIGHT / float(WIDTH);
		for (auto& stamp : stamps) 
		{
			// Scale X velocity by aspect ratio for consistency
			stamp.posX += stamp.velX * aspect;
			stamp.posY += stamp.velY;
		}
	};

	update_bullets(allyBullets);
	update_bullets(enemyBullets);
}


void mark_colliding_bullets(void)
{
	for (size_t i = 0; i < allyBullets.size(); ++i)
		for (size_t j = 0; j < enemyShips.size(); ++j)
			if (isPixelPerfectCollision(allyBullets[i], enemyShips[j]))
				allyBullets[i].to_be_culled = true;

	for (size_t i = 0; i < enemyBullets.size(); ++i)
		for (size_t j = 0; j < allyShips.size(); ++j)
			if (isPixelPerfectCollision(enemyBullets[i], allyShips[j]))
				enemyBullets[i].to_be_culled = true;
}

void mark_old_bullets(void)
{
	for (size_t i = 0; i < allyBullets.size(); ++i)
	{
		if (allyBullets[i].death_time < 0.0)
			continue;

		if (allyBullets[i].death_time <= global_time)
			allyBullets[i].to_be_culled = true;
	}

	//for (size_t i = 0; i < allyBullets.size(); ++i)
	//{
	//	if (allyBullets[i].death_time < 0.0)
	//		continue;

	//	if (allyBullets[i].death_time <= global_time)
	//		allyBullets[i].to_be_culled = true;
	//}
}

void mark_offscreen_bullets(void)
{
	auto update_bullets = [&](std::vector<Stamp>& stamps)
	{
		for (auto& stamp : stamps)
		{
			float aspect = WIDTH / float(HEIGHT);

			// Calculate adjusted Y coordinate that accounts for aspect ratio
			float adjustedPosY = (stamp.posY - 0.5f) * aspect + 0.5f;

			// Check if the stamp is outside the visible area
			if (stamp.posX < -0.1 || stamp.posX > 1.1 ||
				adjustedPosY < -0.1 || adjustedPosY > 1.1)
			{
				stamp.to_be_culled = true;
			}
		}
	};

	update_bullets(allyBullets);
	update_bullets(enemyBullets);
}


void cull_marked_bullets(void)
{
	auto update_bullets = [&](std::vector<Stamp>& stamps, string type)
	{
		for (size_t i = 0; i < stamps.size(); i++)
		{
			if (stamps[i].to_be_culled)
			{
				cout << "culling " << type << " bullet" << endl;
				stamps.erase(stamps.begin() + i);
				i = 0;
			}
		}
	};

	update_bullets(allyBullets, "Ally");
	update_bullets(enemyBullets, "Enemy");
}









void move_ships(void)
{
	auto update_ships = [&](std::vector<Stamp>& stamps, bool keep_within_screen_bounds)
	{
		for (auto& stamp : stamps)
		{
			stamp.posX += stamp.velX;
			stamp.posY += stamp.velY;

			const float aspect = WIDTH / float(HEIGHT);

			if (keep_within_screen_bounds)
			{
				// Calculate adjusted Y coordinate that accounts for aspect ratio
				float adjustedPosY = (stamp.posY - 0.5f) * aspect + 0.5f;

				// Constrain X position
				if (stamp.posX < 0)
					stamp.posX = 0;
				if (stamp.posX > 1)
					stamp.posX = 1;

				// Constrain Y position, accounting for aspect ratio
				if (adjustedPosY < 0)
					stamp.posY = 0.5f - 0.5f / aspect; // Convert back from adjusted to original
				if (adjustedPosY > 1)
					stamp.posY = 0.5f + 0.5f / aspect; // Convert back from adjusted to original
			}
		}
	};

	update_ships(allyShips, true);
	update_ships(enemyShips, false);
}


void mark_dying_ships(void)
{
	for (size_t i = 0; i < allyShips.size(); ++i)
		if (allyShips[i].health <= 0)
			allyShips[i].to_be_culled = true;

	for (size_t i = 0; i < enemyShips.size(); ++i)
		if (enemyShips[i].health <= 0)
			enemyShips[i].to_be_culled = true;
}

void mark_colliding_ships(void)
{
	for (size_t i = 0; i < allyShips.size(); ++i)
	{
		for (size_t j = 0; j < enemyShips.size(); ++j)
		{
			if (isPixelPerfectCollision(allyShips[i], enemyShips[j]))
			{
				allyShips[i].health = 0;
				allyShips[i].to_be_culled = true;
			}
		}
	}
}

void mark_offscreen_ships(void)
{
	auto update_ships = [&](std::vector<Stamp>& stamps)
	{
		for (auto& stamp : stamps)
		{
			float aspect = WIDTH / float(HEIGHT);

			// Calculate adjusted Y coordinate that accounts for aspect ratio
			float adjustedPosY = (stamp.posY - 0.5f) * aspect + 0.5f;

			// Check if the stamp is outside the visible area
			if (stamp.posX < -0.1 || stamp.posX > 1.1 ||
				adjustedPosY < -0.1 || adjustedPosY > 1.1)
			{
				stamp.to_be_culled = true;
			}
		}
	};

	//update_ships(allyShips);
	update_ships(enemyShips);
}



void proceed_stamp_opacity(void)
{
	auto update_ships = [&](std::vector<Stamp>& stamps, string type)
	{
		for (size_t i = 0; i < stamps.size(); i++)
		{
			if (stamps[i].to_be_culled)
			{
				stamps[i].stamp_opacity -= DT;
			}
		}
	};

	update_ships(allyShips, "Ally");
	update_ships(enemyShips, "Enemy");
}


void cull_marked_ships(void)
{
	auto update_ships = [&](std::vector<Stamp>& stamps, string type)
	{
		for (size_t i = 0; i < stamps.size(); i++)
		{
			if (stamps[i].to_be_culled && stamps[i].stamp_opacity <= 0)
			{
				cout << "culling " << type << " ship" << endl;
				stamps.erase(stamps.begin() + i);
				i = 0;
			}
		}
	};

	update_ships(allyShips, "Ally");
	update_ships(enemyShips, "Enemy");
}








void simulationStep() {
	clearObstacleTexture();
	reapplyAllStamps();

	auto updateDynamicTextures = [&](std::vector<Stamp>& stamps)
	{
		for (auto& stamp : stamps)
			updateDynamicTexture(stamp);
	};

	updateDynamicTextures(allyShips);
	updateDynamicTextures(enemyShips);
	updateDynamicTextures(allyBullets);
	updateDynamicTextures(enemyBullets);


	bool old_red_mode = red_mode;

	red_mode = true;

	for (size_t i = 0; i < allyBullets.size(); i++)
	{
		addForce(allyBullets[i].posX, allyBullets[i].posY, allyBullets[i].velX, allyBullets[i].velY, allyBullets[i].force_radius, 5000);
		addColor(allyBullets[i].posX, allyBullets[i].posY, allyBullets[i].velX, allyBullets[i].velY, allyBullets[i].colour_radius);
	}

	red_mode = false;

	for (size_t i = 0; i < enemyBullets.size(); i++)
	{
		addForce(enemyBullets[i].posX, enemyBullets[i].posY, enemyBullets[i].velX, enemyBullets[i].velY, enemyBullets[i].force_radius, 5000);
		addColor(enemyBullets[i].posX, enemyBullets[i].posY, enemyBullets[i].velX, enemyBullets[i].velY, enemyBullets[i].colour_radius);
	}

	red_mode = old_red_mode;



	addMouseForce(0.05, 5000);
	addMouseColor();


	updateObstacle();
	advectVelocity();
	diffuseVelocity();
	advectColor();
	advectFriendlyColor();
	diffuseColor();
	diffuseFriendlyColor();
	computeDivergence();
	solvePressure(10);
	subtractPressureGradient();

	move_bullets();
	mark_colliding_bullets();
	mark_old_bullets();
	mark_offscreen_bullets();
	cull_marked_bullets();

	move_ships();
	mark_colliding_ships();
	mark_offscreen_ships();
	mark_dying_ships();
	proceed_stamp_opacity();
	cull_marked_ships();

	if (frameCount % FLUID_STAMP_COLLISION_REPORT_INTERVAL == 0)
	{
		detectCollisions();
		generateFluidStampCollisionsDamage();
	}
}








void renderToScreen() {
	// Bind default framebuffer (the screen)
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Clear the screen
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	// Use a render shader program

	glUseProgram(renderProgram);


	global_time += DT; // Approximate time for 60fps, adjust as needed

	// Set uniforms
	glUniform1i(glGetUniformLocation(renderProgram, "obstacleTexture"), 1);
	glUniform1i(glGetUniformLocation(renderProgram, "collisionTexture"), 2);
	glUniform1i(glGetUniformLocation(renderProgram, "colorTexture"), 3);
	glUniform1i(glGetUniformLocation(renderProgram, "friendlyColorTexture"), 4);
	glUniform1i(glGetUniformLocation(renderProgram, "backgroundTexture"), 5);
	glUniform2f(glGetUniformLocation(renderProgram, "texelSize"), 1.0f / WIDTH, 1.0f / HEIGHT);
	glUniform1f(glGetUniformLocation(renderProgram, "time"), global_time);

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

	// Render full-screen quad with the fluid simulation
	glBindVertexArray(vao);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);


	// Now render all the stamps with textures using the new program
	// Enable blending for transparent textures
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glUseProgram(stampTextureProgram);

	auto renderStamps = [&](const std::vector<Stamp>& stamps) {
		for (const auto& stamp : stamps) {
			//if (!stamp.active) continue;

			int variationIndex = stamp.currentVariationIndex;
			if (variationIndex < 0 || variationIndex >= stamp.textureIDs.size() ||
				stamp.textureIDs[variationIndex] == 0) {
				for (size_t i = 0; i < stamp.textureIDs.size(); i++) {
					if (stamp.textureIDs[i] != 0) {
						variationIndex = i;
						break;
					}
				}
				if (variationIndex < 0 || variationIndex >= stamp.textureIDs.size() ||
					stamp.textureIDs[variationIndex] == 0) {
					continue;
				}
			}

			float aspect = WIDTH / float(HEIGHT);
			float stamp_y = (stamp.posY - 0.5f) * aspect + 0.5f;

			glUniform1i(glGetUniformLocation(stampTextureProgram, "stampTexture"), 0);
			glUniform2f(glGetUniformLocation(stampTextureProgram, "position"), stamp.posX, stamp_y);
			glUniform2f(glGetUniformLocation(stampTextureProgram, "stampSize"), stamp.width, stamp.height);
			glUniform1f(glGetUniformLocation(stampTextureProgram, "threshold"), 0.1f);
			glUniform2f(glGetUniformLocation(stampTextureProgram, "screenSize"), WIDTH, HEIGHT);

			// added in opacity as a uniform, so that the stamp can fade away over time upon death
			glUniform1f(glGetUniformLocation(stampTextureProgram, "stamp_opacity"), stamp.stamp_opacity);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, stamp.textureIDs[variationIndex]);

			glBindVertexArray(vao);
			glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
		}
	};

	renderStamps(allyShips);
	renderStamps(enemyShips);
	renderStamps(allyBullets);
	renderStamps(enemyBullets);

	glDisable(GL_BLEND);

	// Draw bounding boxes for all stamps to debug
	//for (const auto& stamp : allyShips) {
	//	if (stamp.active) {
	//		drawBoundingBox(stamp);
	//	}
	//}

	//for (const auto& stamp : enemyShips) {
	//	if (stamp.active) {
	//		drawBoundingBox(stamp);
	//	}
	//}

	//for (const auto& stamp : allyBullets) {
	//	if (stamp.active) {
	//		drawBoundingBox(stamp);
	//	}
	//}

	//for (const auto& stamp : enemyBullets) {
	//	if (stamp.active) {
	//		drawBoundingBox(stamp);
	//	}
	//}

	// Debug visualization of global bounding box
	for (size_t i = 0; i < global_minXs.size(); i++)
	{
		drawBoundingBox(global_minXs[i], global_minYs[i], global_maxXs[i], global_maxYs[i]);
	}


	global_minXs.clear();
	global_minYs.clear();
	global_maxXs.clear();
	global_maxYs.clear();


}








// GLUT display callback
void display() {
	// Increment frame counter
	frameCount++;

	// Check if it's time to report collisions
	if (frameCount % FLUID_STAMP_COLLISION_REPORT_INTERVAL == 0)
	{
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

	case 'l':  // Load all stamp textures
	case 'L':
		if (loadStampTextures()) {
			std::cout << "Loaded " << stampTemplates.size() << " stamp templates" << std::endl;
			if (!stampTemplates.empty()) {
				std::cout << "Currently using: " << stampTemplates[currentTemplateIndex].baseFilename << std::endl;
				std::cout << "Available variations:";
				for (size_t i = 0; i < stampTemplates[currentTemplateIndex].textureNames.size(); i++) {
					if (stampTemplates[currentTemplateIndex].textureIDs[i] != 0) {
						std::cout << " " << stampTemplates[currentTemplateIndex].textureNames[i];
					}
				}
				std::cout << std::endl;
			}
		}
		else {
			std::cout << "Failed to load any stamp textures" << std::endl;
		}
		break;

	case 't':  // Cycle to the next stamp texture
	case 'T':
		if (!stampTemplates.empty()) {
			currentTemplateIndex = (currentTemplateIndex + 1) % stampTemplates.size();
			std::cout << "Switched to stamp texture: "
				<< stampTemplates[currentTemplateIndex].baseFilename
				<< " (" << (currentTemplateIndex + 1) << " of "
				<< stampTemplates.size() << ")" << std::endl;

			// Identify the stamp type based on the prefix
			std::string prefix = stampTemplates[currentTemplateIndex].baseFilename.substr(
				0, stampTemplates[currentTemplateIndex].baseFilename.find_first_of("0123456789"));

			if (prefix == "obstacle") {
				std::cout << "Type: Ally Ship" << std::endl;
			}
			else if (prefix == "bullet") {
				std::cout << "Type: Bullet" << std::endl;
			}
			else if (prefix == "enemy") {
				std::cout << "Type: Enemy Ship" << std::endl;
			}
		}
		else {
			std::cout << "No stamp textures loaded. Press 'L' to load textures." << std::endl;
		}
		break;
	}
}





// Modified specialKeyboard function to handle diagonal movement
void specialKeyboard(int key, int x, int y) {
	switch (key) {
	case GLUT_KEY_UP:
		upKeyPressed = true;

		for (auto& stamp : allyShips) {
			if (stamp.textureIDs[0] != 0) {
				stamp.currentVariationIndex = 1; // up variation
			}
		}

		break;
	case GLUT_KEY_DOWN:
		downKeyPressed = true;

		for (auto& stamp : allyShips) {
			if (stamp.textureIDs[0] != 0) {
				stamp.currentVariationIndex = 2; // down variation
			}
		}

		break;
	case GLUT_KEY_LEFT:
		leftKeyPressed = true;
		break;
	case GLUT_KEY_RIGHT:
		rightKeyPressed = true;
		break;
	}

	if (allyShips.size() > 0) {
		// Reset velocity
		allyShips[0].velX = 0.0;
		allyShips[0].velY = 0.0;

		// Combine key states to allow diagonal movement
		if (upKeyPressed) {
			allyShips[0].velY = 1;
		}
		if (downKeyPressed) {
			allyShips[0].velY = -1;
		}
		if (leftKeyPressed) {
			allyShips[0].velX = -1;
		}
		if (rightKeyPressed) {
			allyShips[0].velX = 1;
		}

		float vel_length = sqrt(allyShips[0].velX * allyShips[0].velX + allyShips[0].velY * allyShips[0].velY);

		if (vel_length > 0)
		{
			allyShips[0].velX /= vel_length;
			allyShips[0].velY /= vel_length;

			allyShips[0].velX *= 0.0025;
			allyShips[0].velY *= 0.0025;
		}
	}
}

// Modified specialKeyboardUp function to reset key states
void specialKeyboardUp(int key, int x, int y) {
	switch (key) {
	case GLUT_KEY_UP:
		upKeyPressed = false;
		break;
	case GLUT_KEY_DOWN:
		downKeyPressed = false;
		break;
	case GLUT_KEY_LEFT:
		leftKeyPressed = false;
		break;
	case GLUT_KEY_RIGHT:
		rightKeyPressed = false;
		break;
	}


	for (auto& stamp : allyShips) {
		if (stamp.textureIDs[0] != 0) {
			stamp.currentVariationIndex = 0; // center variation
		}
	}

	if (allyShips.size() > 0) {
		// Reset velocity if no keys are pressed
		allyShips[0].velX = 0.0;
		allyShips[0].velY = 0.0;

		if (upKeyPressed) {
			allyShips[0].velY = 1;
		}
		if (downKeyPressed) {
			allyShips[0].velY = -1;
		}
		if (leftKeyPressed) {
			allyShips[0].velX = -1;
		}
		if (rightKeyPressed) {
			allyShips[0].velX = 1;
		}

		float vel_length = sqrt(allyShips[0].velX * allyShips[0].velX + allyShips[0].velY * allyShips[0].velY);

		if (vel_length > 0)
		{
			allyShips[0].velX /= vel_length;
			allyShips[0].velY /= vel_length;

			allyShips[0].velX *= 0.0025;
			allyShips[0].velY *= 0.0025;
		}
	}
}




// GLUT reshape callback// GLUT reshape callback

void reshape(int w, int h) {
	glViewport(0, 0, w, h);
	WIDTH = w;
	HEIGHT = h;

	// Delete existing shader programs
	glDeleteProgram(advectProgram);
	glDeleteProgram(divergenceProgram);
	glDeleteProgram(pressureProgram);
	glDeleteProgram(gradientSubtractProgram);
	glDeleteProgram(addForceProgram);
	glDeleteProgram(detectCollisionProgram);
	glDeleteProgram(diffuseColorProgram);
	glDeleteProgram(addColorProgram);
	glDeleteProgram(stampObstacleProgram);
	glDeleteProgram(diffuseVelocityProgram);
	glDeleteProgram(stampTextureProgram);
	glDeleteProgram(renderProgram);

	// Delete OpenGL resources
	glDeleteFramebuffers(1, &fbo);
	glDeleteVertexArrays(1, &vao);
	glDeleteBuffers(1, &vbo);

	// Delete textures
	glDeleteTextures(2, velocityTexture);
	glDeleteTextures(2, pressureTexture);
	glDeleteTextures(1, &divergenceTexture);
	glDeleteTextures(1, &obstacleTexture);
	glDeleteTextures(1, &collisionTexture);
	glDeleteTextures(2, colorTexture);
	glDeleteTextures(2, friendlyColorTexture);
	glDeleteTextures(1, &backgroundTexture);

	// Delete stamp textures from templates
	for (auto& stamp : stampTemplates) {
		for (auto& textureID : stamp.textureIDs) {
			if (textureID != 0) {
				glDeleteTextures(1, &textureID);
			}
		}
	}

	stampTemplates.clear();

	// Delete stamp textures from active stamps
	for (auto& stamp : allyShips) {
		for (auto& textureID : stamp.textureIDs) {
			if (textureID != 0) {
				glDeleteTextures(1, &textureID);
			}
		}
	}

	// Delete stamp textures from active stamps
	for (auto& stamp : enemyShips) {
		for (auto& textureID : stamp.textureIDs) {
			if (textureID != 0) {
				glDeleteTextures(1, &textureID);
			}
		}
	}

	for (auto& stamp : allyBullets) {
		for (auto& textureID : stamp.textureIDs) {
			if (textureID != 0) {
				glDeleteTextures(1, &textureID);
			}
		}
	}

	for (auto& stamp : enemyBullets) {
		for (auto& textureID : stamp.textureIDs) {
			if (textureID != 0) {
				glDeleteTextures(1, &textureID);
			}
		}
	}

	// Reinitialize OpenGL
	initGL();
}








void printInstructions() {
	std::cout << "GPU-Accelerated Navier-Stokes Solver" << std::endl;
	std::cout << "-----------------------------------" << std::endl;
	std::cout << "Left Mouse Button: Add velocity and density" << std::endl;
	std::cout << "Right Mouse Button: Add game objects using current template" << std::endl;
	std::cout << "R: Toggle between red and blue color modes" << std::endl;
	std::cout << "C: Generate collision report immediately" << std::endl;
	std::cout << "L: Load all available game object textures" << std::endl;
	std::cout << "T: Cycle through loaded textures (obstacles=ally ships, bullets, enemy)" << std::endl;
	std::cout << "UP/DOWN Arrow Keys: Change ship orientation when placing" << std::endl;
	std::cout << "Highlights show colour-obstacle collisions" << std::endl;
	std::cout << "Collision reports are generated every " << FLUID_STAMP_COLLISION_REPORT_INTERVAL << " frames" << std::endl;
	std::cout << "-----------------------------------" << std::endl;
	std::cout << "File Naming Convention:" << std::endl;
	std::cout << "  obstacle*.png - Ally ships" << std::endl;
	std::cout << "  bullet*.png - Bullets" << std::endl;
	std::cout << "  enemy*.png - Enemy ships" << std::endl;
	std::cout << "Each can have _centre, _up, and _down variations" << std::endl;
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

	glutSpecialFunc(specialKeyboard);
	glutSpecialUpFunc(specialKeyboardUp);

	// Print instructions
	printInstructions();

	// Main loop
	glutMainLoop();

	return 0;
}

