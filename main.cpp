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
#include <chrono>
#include <set>
using namespace std;

#pragma comment(lib, "freeglut")
#pragma comment(lib, "glew32")



// to do: get ship-powerup collisions and determine which type of powerup it is

// to do: At the beginning of level, generate all enemies and powerups for that level, using a particular seed. That way the users can share seed numbers.

// to do: Give the player the option to use a shield for 30 seconds, for say, 1 unit of health.

// to do: The key is to let the user choose the fire type once they have got it. User loses the fire type when they continue 

// to do: if the event ship takes a hot on the wing, the wing falls off. If it takes the shot on the cockpit the shop blows up... or see.thing like that


class vec2
{
public:
	float x, y;
};


class ivec2
{
public:
	size_t x, y;

	bool operator<(const ivec2& right) const
	{
		if (right.x > x)
			return true;
		else if (right.x < x)
			return false;

		if (right.y > y)
			return true;
		else if (right.y < y)
			return false;

		return false;
	}

	bool operator==(const ivec2& right)
	{
		if (x == right.x && y == right.y)
			return true;

		return false;
	}


	size_t stamp_index = 0;
	string stamp_type = "";
};




vec2 get_straight_point(vector<vec2> points, float t)
{
	if (points.empty()) return { 0, 0 };
	if (points.size() == 1) return points[0];

	// Scale t to the total path length
	float scaledT = t * (points.size() - 1);

	// Determine which segment we're in
	int segment = static_cast<int>(scaledT);
	if (segment >= points.size() - 1) {
		return points.back();
	}

	// Calculate the interpolation parameter for this segment
	float segmentT = scaledT - segment;

	// Linear interpolation between the two points
	vec2 p1 = points[segment];
	vec2 p2 = points[segment + 1];

	vec2 ret;
	ret.x = p1.x + (p2.x - p1.x) * segmentT;
	ret.y = p1.y + (p2.y - p1.y) * segmentT;

	return ret;
}








vec2 get_curve_point(vector<vec2> points, float t)
{
	if (points.size() == 0)
	{
		vec2 vd;
		vd.x = vd.y = 0;
		return vd;
	}

	size_t i = points.size() - 1;

	while (i > 0)
	{
		for (int k = 0; k < i; k++)
		{
			points[k].x += t * (points[k + 1].x - points[k].x);
			points[k].y += t * (points[k + 1].y - points[k].y);
		}

		i--;
	}

	return points[0];
}





// Simulation parameters
int WIDTH = 960;
int HEIGHT = 540;

const float FPS = 60;
const float DT = 1.0f / FPS;
const float VISCOSITY = 0.5f;     // Fluid viscosity
const float DIFFUSION = 0.5f;    //  diffusion rate
const float COLLISION_THRESHOLD = 0.5f; // Threshold for color-obstacle collision
const float COLOR_DETECTION_THRESHOLD = 0.01f;  // How strict the color matching should be

std::chrono::high_resolution_clock::time_point app_start_time = std::chrono::high_resolution_clock::now();





bool red_mode = true;

bool lastRightMouseDown = false;


vector<float> global_minXs;
vector<float> global_minYs;
vector<float> global_maxXs;
vector<float> global_maxYs;


//
//float global_minX, global_minY, global_maxX, global_maxY;


enum fire_type { STRAIGHT, SINUSOIDAL, RANDOM, HOMING };

enum fire_type ally_fire = STRAIGHT;


enum powerup_type { SINUSOIDAL_POWERUP, RANDOM_POWERUP, HOMING_POWERUP, X3_POWERUP, X5_POWERUP};


bool has_sinusoidal_fire = false;
bool has_random_fire = false;
bool has_homing_fire = false;
bool x3_fire = false;
bool x5_fire = false;


float eddyIntensity = 1.0;
float eddyDensity = 10;






struct Stamp {
	// StampTexture properties
	std::vector<GLuint> textureIDs;         // Multiple texture IDs
	int width = 0; // pixels
	int height = 0; // pixels
	std::string baseFilename;               // Base filename without suf fix
	std::vector<std::string> textureNames;  // Names of the specific textures
	std::vector<std::vector<unsigned char>> pixelData;

	std::vector<std::vector<unsigned char>> blackeningData;

	std::vector<std::vector<unsigned char>> backupData;





	int channels = 0;                         // Store the number of channels

	bool to_be_culled = false;

	float health = 1000.0;

	float birth_time = 0;
	// A negative death time means that the bullet is immortal 
	// (it is culled only when colliding with the ally/enemy or goes off screen)
	// A mortal bullet dies after a certain amount of time
	float death_time = -1;

	float stamp_opacity = 1;

	float force_radius = 0.02f;
	float colour_radius = force_radius;

	float force_randomization = 0;// force_radius / 100.0;
	float colour_randomization = 0;// force_radius / 10.0;
	float path_randomization = 0.0f;// force_radius / 50.0;
	float sinusoidal_frequency = 5.0f;
	float sinusoidal_amplitude = 0.001f;
	bool sinusoidal_shift = false;
	float random_forking = 0.0f;

	vector<vec2> curve_path;

	// StampInfo properties
	float posX = 0, posY = 0;                       // Normalized position (0-1)
	float velX = 0, velY = 0;

	size_t currentVariationIndex = 0;              // Which texture variation to use

	vector<ivec2> blackening_points;

	enum powerup_type powerup;// { SINUSOIDAL_POWERUP, RANDOM_POWERUP, HOMING_POWERUP, X3_POWERUP, X5_POWERUP };
};







std::vector<Stamp> allyShips;
std::vector<Stamp> enemyShips;
std::vector<Stamp> allyBullets;
std::vector<Stamp> enemyBullets;
std::vector<Stamp> allyPowerUps;


std::vector<Stamp> allyTemplates;
std::vector<Stamp> enemyTemplates;
std::vector<Stamp> bulletTemplates;
std::vector<Stamp> powerUpTemplates;


int currentAllyTemplateIndex = 0;    // Index for selecting ally template stamps
int currentEnemyTemplateIndex = 0;   // Index for selecting enemy template stamps
int currentPowerUpTemplateIndex = 0;

enum TemplateType { ALLY, ENEMY, BULLET, POWERUP };

TemplateType currentTemplateType = ALLY;


//
//void drawFullScreenQuad() {
//	static GLuint quadVAO = 0;
//	static GLuint quadVBO;
//
//	if (quadVAO == 0) {
//		float quadVertices[] = {
//			-1.0f, -1.0f, 0.0f,  0.0f, 0.0f,
//			 1.0f, -1.0f, 0.0f,  1.0f, 0.0f,
//			 1.0f,  1.0f, 0.0f,  1.0f, 1.0f,
//			-1.0f,  1.0f, 0.0f,  0.0f, 1.0f
//		};
//
//		glGenVertexArrays(1, &quadVAO);
//		glGenBuffers(1, &quadVBO);
//		glBindVertexArray(quadVAO);
//		glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
//		glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
//
//		glEnableVertexAttribArray(0);
//		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
//		glEnableVertexAttribArray(1);
//		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
//	}
//
//	glBindVertexArray(quadVAO);
//	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
//	glBindVertexArray(0);
//}

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




//
//void createThrusterFire() {
//	// Only create thruster fire if we have ally ships
//	if (allyShips.empty() || bulletTemplates.empty()) {
//		return;
//	}
//
//	// Check if the ally ship is moving
//	if (allyShips[0].velX < 0 && allyShips[0].velY == 0) {
//		return;  // Ship is stationary, don't create thruster fire
//	}
//
//	std::chrono::high_resolution_clock::time_point currentTime = std::chrono::high_resolution_clock::now();
//	static std::chrono::high_resolution_clock::time_point lastThrusterTime = currentTime;
//	std::chrono::duration<float> timeSinceThruster = currentTime - lastThrusterTime;
//
//	// Create thruster fire particles at a certain rate
//	const float THRUSTER_INTERVAL = 0.1f; // Faster than regular bullets
//	if (timeSinceThruster.count() < THRUSTER_INTERVAL)
//		return;
//
//	lastThrusterTime = currentTime;
//	std::chrono::duration<float, std::milli> elapsed = currentTime - app_start_time;
//
//	// Use the first bullet template
//	Stamp thrusterParticle = bulletTemplates[0];
//
//	float aspect = WIDTH / float(HEIGHT);
//
//	// Calculate position behind the ship (opposite direction of movement)
//	float normalizedVelX = 1;// allyShips[0].velX;
//	float normalizedVelY = 0;// allyShips[0].velY;
//
//	// Normalize if needed
//	float velLength = sqrt(normalizedVelX * normalizedVelX + normalizedVelY * normalizedVelY);
//	if (velLength > 0) {
//		normalizedVelX /= velLength;
//		normalizedVelY /= velLength;
//	}
//
//	// Position the thruster particle behind the ship
//	float offsetMultiplier = allyShips[0].width / float(WIDTH) / 2.0;
//	float offsetMultiplierY = allyShips[0].height / float(HEIGHT) / 2.0;
//
//
//	thrusterParticle.posX = allyShips[0].posX;// -normalizedVelX * offsetMultiplier;
//	thrusterParticle.posY = allyShips[0].posY;// normalizedVelY* offsetMultiplierY;
//
//	// Add some randomness to the position
//	//float randOffsetX = (rand() / float(RAND_MAX) - 0.5f) * offsetMultiplier * 0.5f;
//	//float randOffsetY = (rand() / float(RAND_MAX) - 0.5f) * offsetMultiplier * 0.5f;
//	//thrusterParticle.posX += randOffsetX;
//	//thrusterParticle.posY += randOffsetY;
//
//	// Set velocity (opposite to ship movement, plus some randomness)
//	thrusterParticle.velX = -normalizedVelX * 0.005f;
//	thrusterParticle.velY = 0;// -normalizedVelY * 0;
//
//	// Add some randomness to velocity
//	//thrusterParticle.velX += (rand() / float(RAND_MAX) - 0.5f) * 0.002f;
//	//thrusterParticle.velY += (rand() / float(RAND_MAX) - 0.5f) * 0.002f;
//
//	// Make smaller than normal bullets
//	thrusterParticle.colour_radius = 0.01;// offsetMultiplier * 0.3f;
//	thrusterParticle.force_radius = 0.01;// offsetMultiplier * 0.3f;
//
//	// Set short lifetime
//	thrusterParticle.birth_time = elapsed.count() / 1000.0;
//	thrusterParticle.death_time = elapsed.count() / 1000.0 + 10.0;
//	// Add some path randomization for a flame-like effect
//	//thrusterParticle.path_randomization = 0.002f;
//
//	thrusterParticle.sinusoidal_amplitude = 0;
//
//	thrusterParticle.posY = allyShips[0].posY + offsetMultiplierY / 8;// normalizedVelY* offsetMultiplierY;
//
//	thrusterParticle.colour_radius = 0.005;// offsetMultiplier * 0.3f;
//	thrusterParticle.force_radius = 0.005;// offsetMultiplier * 0.3f;
//	thrusterParticle.death_time = elapsed.count() / 1000.0 + 2.0;
//
//	allyBullets.push_back(thrusterParticle);
//
//	thrusterParticle.colour_radius = 0.01;// offsetMultiplier * 0.3f;
//	thrusterParticle.force_radius = 0.01;// offsetMultiplier * 0.3f;
//	thrusterParticle.death_time = elapsed.count() / 1000.0 + 1.0;
//	allyBullets.push_back(thrusterParticle);
//
//	thrusterParticle.colour_radius = 0.1;// offsetMultiplier * 0.3f;
//	thrusterParticle.force_radius = 0.1;// offsetMultiplier * 0.3f;
//	thrusterParticle.death_time = elapsed.count() / 1000.0 + 0.1;
//
//	allyBullets.push_back(thrusterParticle);
//
//}






bool loadStampTextures() {
	// Clear previous template textures
	for (auto& stamp : allyTemplates) {
		for (auto& texID : stamp.textureIDs) {
			if (texID != 0) {
				glDeleteTextures(1, &texID);
			}
		}
	}

	for (auto& stamp : enemyTemplates) {
		for (auto& texID : stamp.textureIDs) {
			if (texID != 0) {
				glDeleteTextures(1, &texID);
			}
		}
	}

	for (auto& stamp : powerUpTemplates) {
		for (auto& texID : stamp.textureIDs) {
			if (texID != 0) {
				glDeleteTextures(1, &texID);
			}
		}
	}

	allyTemplates.clear();
	enemyTemplates.clear();
	powerUpTemplates.clear();

	// Define the file prefixes for each type of game object
	const std::vector<std::string> prefixes = { "obstacle", "enemy", "powerup" };
	const std::vector<std::string> variations = { "_centre", "_up", "_down" };

	bool loadedAny = false;

	// For each prefix, attempt to load all indexed textures
	for (const auto& prefix : prefixes)
	{
		int index = 0;

		while (true)
		{
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
					newStamp.pixelData.push_back((pixelData));
					newStamp.blackeningData.push_back(pixelData);
					newStamp.backupData.push_back((pixelData));

					std::cout << "Loaded stamp texture: " << filename << " (" << width << "x" << height << ")" << std::endl;
					loadedAtLeastOne = true;
				}
				else {
					newStamp.textureIDs.push_back(0);
					newStamp.pixelData.push_back(std::vector<unsigned char>());
					newStamp.blackeningData.push_back(std::vector<unsigned char>());
					newStamp.backupData.push_back(std::vector<unsigned char>());
				}
			}

			if (loadedAtLeastOne) {
				// Sort templates by type
				if (prefix == "obstacle") {
					allyTemplates.push_back(std::move(newStamp));
				}
				else if (prefix == "enemy") {
					enemyTemplates.push_back(std::move(newStamp));
				}
				else if (prefix == "powerup") {
					powerUpTemplates.push_back(std::move(newStamp));
				}
				loadedAny = true;
				index++;
			}
			else {
				break; // No more textures with this prefix
			}
		}
	}

	if (loadedAny) {
		currentAllyTemplateIndex = 0;
		currentEnemyTemplateIndex = 0;
		currentTemplateType = ALLY; // Default to ally template
	}



	return loadedAny;
}






void RandomUnitVector(float& x_out, float& y_out)
{
	const static float pi = 4.0f * atanf(1.0f);

	const float a = (rand() / float(RAND_MAX)) * 2.0f * pi;

	x_out = cos(a);
	y_out = sin(a);
}


bool loadBulletTemplates() {
	// Clear previous bullet templates
	for (auto& stamp : bulletTemplates) {
		for (auto& texID : stamp.textureIDs) {
			if (texID != 0) {
				glDeleteTextures(1, &texID);
			}
		}
	}

	bulletTemplates.clear();

	// Load all bullet textures (bullet0, bullet1, etc.)
	int index = 0;
	bool loadedAny = false;

	while (true) {
		std::string baseFilename = "bullet" + std::to_string(index);
		Stamp newStamp;
		newStamp.baseFilename = baseFilename;
		newStamp.textureNames = { "centre", "up", "down" };
		newStamp.currentVariationIndex = 0; // Default to center

		bool loadedAtLeastOne = false;
		const std::vector<std::string> variations = { "_centre", "_up", "_down" };

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



				newStamp.pixelData.push_back((pixelData));
				newStamp.blackeningData.push_back((pixelData));
				newStamp.backupData.push_back((pixelData));


				std::cout << "Loaded bullet template: " << filename << " (" << width << "x" << height << ")" << std::endl;
				loadedAtLeastOne = true;
			}
			else {
				newStamp.textureIDs.push_back(0);

				newStamp.pixelData.push_back(std::vector<unsigned char>());
				newStamp.blackeningData.push_back(std::vector<unsigned char>());
				newStamp.backupData.push_back(std::vector<unsigned char>());
			}
		}

		if (loadedAtLeastOne) {
			bulletTemplates.push_back(std::move(newStamp));
			loadedAny = true;
			index++;
		}
		else {
			break; // No more textures with this prefix
		}
	}

	return loadedAny;
}



const float MIN_BULLET_INTERVAL = 0.25f;

// Add a variable to track the time of the last fired bullet
std::chrono::high_resolution_clock::time_point lastBulletTime = std::chrono::high_resolution_clock::now();





void fireBullet() {
	// Only fire if we have ally ships and bullet templates
	if (allyShips.empty() || bulletTemplates.empty()) {
		return;
	}

	// No more firing while dying
	if (allyShips[0].to_be_culled)
		return;

	std::chrono::high_resolution_clock::time_point currentTime = std::chrono::high_resolution_clock::now();
	std::chrono::duration<float> timeSinceLastBullet = currentTime - lastBulletTime;

	if (timeSinceLastBullet.count() < MIN_BULLET_INTERVAL)
		return;

	lastBulletTime = currentTime;

	std::chrono::high_resolution_clock::time_point global_time_end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<float, std::milli> elapsed = global_time_end - app_start_time;

	// Use the first bullet template
	Stamp bulletTemplate = bulletTemplates[0];

	float aspect = WIDTH / float(HEIGHT);

	// Get the ally ship position to fire from

	if (ally_fire == RANDOM)
	{
		bulletTemplate.posX = allyShips[0].posX;
		bulletTemplate.posY = allyShips[0].posY;
	}
	else
	{
		bulletTemplate.posX = allyShips[0].posX + allyShips[0].width / float(WIDTH) / 2.0f;
		bulletTemplate.posY = allyShips[0].posY + allyShips[0].height / (float(HEIGHT) * aspect) / 8.0f;
	}

	static const float pi = 4.0f * atanf(1.0f);

	float angle_start = 0;
	float angle_end = 0;
	size_t num_streams = 1;

	if (x3_fire) {
		angle_start = 0.1f;
		angle_end = -0.1f;
		num_streams = 3;
	}

	if (x5_fire) {
		angle_start = 0.2f;
		angle_end = -0.2f;
		num_streams = 5;
	}

	float angle_step = 0;
	if (num_streams > 1) {
		angle_step = (angle_end - angle_start) / (num_streams - 1);
	}

	float angle = angle_start;

	// Fire bullets based on current fire type
	switch (ally_fire) {
	case STRAIGHT:
		for (size_t i = 0; i < num_streams; i++, angle += angle_step) {
			Stamp newBullet = bulletTemplate;
			newBullet.velX = 0.01f * cos(angle);
			newBullet.velY = 0.01f * sin(angle);
			newBullet.sinusoidal_amplitude = 0;
			newBullet.birth_time = elapsed.count() / 1000.0f;
			newBullet.death_time = -1;
			allyBullets.push_back(newBullet);
		}
		break;

	case SINUSOIDAL:
		for (size_t i = 0; i < num_streams; i++, angle += angle_step) {
			Stamp newBullet = bulletTemplate;
			newBullet.velX = 0.01f * cos(angle);
			newBullet.velY = 0.01f * sin(angle);
			newBullet.sinusoidal_shift = false;
			newBullet.sinusoidal_amplitude = 0.005f;
			newBullet.birth_time = elapsed.count() / 1000.0f;
			newBullet.death_time = -1;
			allyBullets.push_back(newBullet);

			newBullet.sinusoidal_shift = true;
			allyBullets.push_back(newBullet);
		}
		break;

	case RANDOM:
	{
		size_t num_streams_local = num_streams;

		Stamp newCentralStamp = bulletTemplate;

		bulletTemplate.posX = allyShips[0].posX;// +allyShips[0].width / float(WIDTH) / 2.0;
		bulletTemplate.posY = allyShips[0].posY;// +allyShips[0].height / (float(HEIGHT) * aspect) / 8.0;

		float x_rad = allyShips[0].width / float(WIDTH) / 2.0f;
		float y_rad = allyShips[0].height / float(HEIGHT) / 2.0f;
		float avg_rad = max(x_rad, y_rad);

		newCentralStamp.colour_radius = avg_rad / 2.0f;
		newCentralStamp.force_radius = avg_rad / 2.0f;

		for (size_t j = 0; j < num_streams_local; j++)
		{
			Stamp newStamp = newCentralStamp;
			newStamp.colour_radius = avg_rad / 4.0f;
			newStamp.force_radius = avg_rad / 4.0f;

			// Make elliptical fire
			RandomUnitVector(newStamp.velX, newStamp.velY);
			newStamp.velX *= WIDTH / float(HEIGHT);
			newStamp.velX *= 2.0f;

			newStamp.velX /= 250.0f / (rand() / float(RAND_MAX));
			newStamp.velY /= 250.0f / (rand() / float(RAND_MAX));
			newStamp.path_randomization = (rand() / float(RAND_MAX)) * 0.01f;
			newStamp.birth_time = elapsed.count() / 1000.0f;
			newStamp.death_time = elapsed.count() / 1000.0f + 1.0f * rand() / float(RAND_MAX);
			newStamp.random_forking = 0.001f;
			allyBullets.push_back(newStamp);
		}

		for (size_t j = 0; j < num_streams_local * 2; j++)
		{
			Stamp newStamp = newCentralStamp;
			newStamp.colour_radius = avg_rad / 8.0f;
			newStamp.force_radius = avg_rad / 8.0f;

			// Make elliptical fire
			RandomUnitVector(newStamp.velX, newStamp.velY);
			newStamp.velX *= WIDTH / float(HEIGHT);
			newStamp.velX *= 2.0f;

			newStamp.velX /= 100.0f / (rand() / float(RAND_MAX));
			newStamp.velY /= 100.0f / (rand() / float(RAND_MAX));
			newStamp.path_randomization = (rand() / float(RAND_MAX)) * 0.01f;
			newStamp.birth_time = elapsed.count() / 1000.0f;
			newStamp.death_time = elapsed.count() / 1000.0f + 3.0f * rand() / float(RAND_MAX);
			newStamp.random_forking = 0.01f;
			allyBullets.push_back(newStamp);
		}
	}
	break;

	case HOMING:
		for (size_t i = 0; i < num_streams; i++, angle += angle_step) {
			Stamp newBullet = bulletTemplate;
			newBullet.velX = 0.01f * cos(angle);
			newBullet.velY = 0.01f * sin(angle);
			newBullet.sinusoidal_amplitude = 0;
			newBullet.birth_time = elapsed.count() / 1000.0f;
			newBullet.death_time = -1;
			allyBullets.push_back(newBullet);
		}
		break;


	}
}



bool spacePressed = false;






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







unsigned char getPixelValueFromStamp(const Stamp& stamp, size_t variationIndex, int x, int y, int channel) {
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
			int texAx = int((x - aMinX) / (aMaxX - aMinX) * a.width);
			int texAy = int((y - aMinY) / (aMaxY - aMinY) * a.height);
			int texBx = int((x - bMinX) / (bMaxX - bMinX) * b.width);
			int texBy = int((y - bMinY) / (bMaxY - bMinY) * b.height);

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
GLuint backgroundTexture2;  // New second background texture
GLuint processingFBO;
GLuint tempTexture1;
GLuint tempTexture2;

GLuint vorticityTexture;
GLuint vorticityForceTexture;




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
GLuint dilationProgram;
GLuint gaussianBlurHorizontalProgram;
GLuint gaussianBlurVerticalProgram;
GLuint blackeningProgram;
GLuint curlProgram;
GLuint vorticityForceProgram;
GLuint applyForceProgram;


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




const char* curlFragmentShader = R"(
#version 330 core
uniform sampler2D velocityTexture;
uniform sampler2D obstacleTexture;
uniform vec2 texelSize;

out float FragColor; // Output vorticity (scalar)

in vec2 TexCoord;

void main() {
    float obstacle = texture(obstacleTexture, TexCoord).r;
    //if (obstacle > 0.0) {
    //    FragColor = 0.0;
    //    return;
    //}

    // Compute velocity components
    vec2 left = texture(velocityTexture, TexCoord - vec2(texelSize.x, 0.0)).xy;
    vec2 right = texture(velocityTexture, TexCoord + vec2(texelSize.x, 0.0)).xy;
    vec2 bottom = texture(velocityTexture, TexCoord - vec2(0.0, texelSize.y)).xy;
    vec2 top = texture(velocityTexture, TexCoord + vec2(0.0, texelSize.y)).xy;

    // Compute curl (vorticity component in 2D)
    float curl = (top.x - bottom.x) - (right.y - left.y);

    FragColor = curl;
}

)";

const char* vorticityForceFragmentShader = R"(

#version 330 core
uniform sampler2D vorticityTexture;
uniform sampler2D velocityTexture;
uniform sampler2D obstacleTexture;
uniform vec2 texelSize;
uniform float epsilon; // Vorticity confinement strength

out vec4 FragColor; // Output force

in vec2 TexCoord;

void main() {
    float obstacle = texture(obstacleTexture, TexCoord).r;
    //if (obstacle > 0.0) {
    //    FragColor = vec4(0.0);
    //    return;
    //}

    // Compute vorticity gradient
    float left = texture(vorticityTexture, TexCoord - vec2(texelSize.x, 0.0)).r;
    float right = texture(vorticityTexture, TexCoord + vec2(texelSize.x, 0.0)).r;
    float bottom = texture(vorticityTexture, TexCoord - vec2(0.0, texelSize.y)).r;
    float top = texture(vorticityTexture, TexCoord + vec2(0.0, texelSize.y)).r;

    vec2 grad = vec2(abs(right) - abs(left), abs(top) - abs(bottom));
    float mag = length(grad) + 1e-5; // Prevent division by zero

    // Normalize vorticity gradient
    vec2 N = grad / mag;

    // Compute vorticity at the current pixel
    float curl = texture(vorticityTexture, TexCoord).r;

    vec2 force = epsilon * vec2(-N.y, N.x) * curl;

    FragColor = vec4(force, 0.0, 1.0);
}


)";

const char* applyForceFragmentShader = R"(

#version 330 core
uniform sampler2D velocityTexture;
uniform sampler2D forceTexture;
uniform sampler2D obstacleTexture;
uniform float dt;
out vec4 FragColor;

in vec2 TexCoord;

void main() {
    float obstacle = texture(obstacleTexture, TexCoord).r;
    //if (obstacle > 0.0) {
    //    FragColor = vec4(0.0);
    //    return;
    //}

    // Get current velocity and vorticity confinement force
    vec2 velocity = texture(velocityTexture, TexCoord).xy;
    vec2 force = texture(forceTexture, TexCoord).xy;

    // Apply force
    velocity += dt * force;

    FragColor = vec4(velocity, 0.0, 1.0);
}

)";






const char* dilationFragmentShader = R"(
#version 330 core
uniform sampler2D inputTexture;
uniform int radius;
uniform vec2 texelSize;

out vec4 FragColor;
in vec2 TexCoord;

void main() {
    vec4 maxValue = vec4(0.0, 0.0, 0.0, 0.0);
    
    // Iterate through neighborhood to find maximum value for each channel
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            vec2 offset = vec2(dx, dy) * texelSize;
            vec4 neighborValue = texture(inputTexture, TexCoord + offset);
            maxValue = max(maxValue, neighborValue);
        }
    }
    
    FragColor = maxValue;
}
)";

// New GLSL shader for Gaussian blur (horizontal pass)
const char* gaussianBlurHorizontalFragmentShader = R"(
#version 330 core
uniform sampler2D inputTexture;
uniform float sigma;
uniform vec2 texelSize;

out vec4 FragColor;
in vec2 TexCoord;

void main() {
    float kernelRadius = ceil(2.0 * sigma);
    vec4 sum = vec4(0.0);
    float weightSum = 0.0;
    
    // Apply the horizontal pass of the separable Gaussian kernel
    for (int i = -int(kernelRadius); i <= int(kernelRadius); i++) {
        float weight = exp(-(i * i) / (2.0 * sigma * sigma));
        vec2 offset = vec2(i, 0) * texelSize;
        vec4 sample = texture(inputTexture, TexCoord + offset);
        
        sum += sample * weight;
        weightSum += weight;
    }
    
    FragColor = sum / weightSum;
}
)";

// New GLSL shader for Gaussian blur (vertical pass)
const char* gaussianBlurVerticalFragmentShader = R"(
#version 330 core
uniform sampler2D inputTexture;
uniform float sigma;
uniform vec2 texelSize;

out vec4 FragColor;
in vec2 TexCoord;

void main() {
    float kernelRadius = ceil(2.0 * sigma);
    vec4 sum = vec4(0.0);
    float weightSum = 0.0;
    
    // Apply the vertical pass of the separable Gaussian kernel
    for (int i = -int(kernelRadius); i <= int(kernelRadius); i++) {
        float weight = exp(-(i * i) / (2.0 * sigma * sigma));
        vec2 offset = vec2(0, i) * texelSize;
        vec4 sample = texture(inputTexture, TexCoord + offset);
        
        sum += sample * weight;
        weightSum += weight;
    }
    
    FragColor = sum / weightSum;
}
)";

// New GLSL shader for blackening effect
const char* blackeningFragmentShader = R"(
#version 330 core
uniform sampler2D originalTexture;
uniform sampler2D maskTexture;

out vec4 FragColor;
in vec2 TexCoord;

void main() {
    vec4 original = texture(originalTexture, TexCoord);
    vec4 mask = texture(maskTexture, TexCoord);
    
    // Since the mask texture is white where blackening should occur,
    // we need to invert it to get the right effect
    float maskIntensity = (mask.r + mask.g + mask.b) / 3.0;
    
    // Apply the blackening effect to the original texture
    FragColor = original * (1.0 - maskIntensity);
    FragColor.a = original.a; // Preserve alpha channel
}
)";






const char* diffuseVelocityFragmentShader = R"(
#version 330 core
uniform sampler2D velocityTexture;
uniform sampler2D obstacleTexture;
uniform vec2 texelSize;
uniform float viscosity;
uniform float dt;
out vec4 FragColor;
const float fake_dispersion = 1.0;//0.99;

in vec2 TexCoord;

void main() {
    // Check if we're in an obstacle
    float obstacle = texture(obstacleTexture, TexCoord).r;
    //if (obstacle > 0.0) {
    //    FragColor = vec4(0.0, 0.0, 0.0, 1.0);
    //    return;
    //}

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
    //if (obstacle > 0.0) {
    //    FragColor = 0.0;
    //    return;
    //}



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
uniform float time;
uniform float eddyIntensity;  // Controls overall intensity of eddies
uniform float eddyDensity;    // Controls how many eddies appear
uniform float fbm_amplitude = 100.0;
uniform float fbm_frequency = 10.0;

float WIDTH = texelSize.x;
float HEIGHT = texelSize.y;
float aspect_ratio = WIDTH / HEIGHT;

out vec4 FragColor;
in vec2 TexCoord;

// Hash function for pseudo-random number generation
float hash(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

// Value noise function
float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    
    // Four corners in 2D of a tile
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    
    // Smooth interpolation
    vec2 u = f * f * (3.0 - 2.0 * f);
    
    // Mix the four corners
    return mix(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}

// Fractal Brownian Motion (fBm) for multi-scale noise
float fbm(vec2 p, int octaves) {
    float value = 0.0;
    float amplitude = fbm_amplitude;//100.0;
    float frequency = fbm_frequency;//10.0;
    
    for (int i = 0; i < octaves; i++) {
        value += amplitude * noise(p * frequency);
        amplitude *= 0.5;
        frequency *= 2.0;
    }
    
    return value;
}

// Create a vector field for eddies
vec2 eddyField(vec2 p, float t) {
    // Multi-scale noise for different sized eddies
    float noise1 = fbm(p * 3.0 + vec2(t * 0.1, t * 0.2), 3);
    float noise2 = fbm(p * 8.0 + vec2(t * 0.2, -t * 0.1), 2);
    float noise3 = fbm(p * 15.0 + vec2(-t * 0.3, t * 0.3), 1);
    
    // Calculate rotational vector field based on noise
    vec2 grad1 = vec2(
        fbm(p * 3.0 + vec2(0.01, 0.0) + vec2(t * 0.1, t * 0.2), 3) - noise1,
        fbm(p * 3.0 + vec2(0.0, 0.01) + vec2(t * 0.1, t * 0.2), 3) - noise1
    ) * 2.0;
    
    vec2 grad2 = vec2(
        fbm(p * 8.0 + vec2(0.01, 0.0) + vec2(t * 0.2, -t * 0.1), 2) - noise2,
        fbm(p * 8.0 + vec2(0.0, 0.01) + vec2(t * 0.2, -t * 0.1), 2) - noise2
    ) * 1.0;
    
    vec2 grad3 = vec2(
        fbm(p * 15.0 + vec2(0.01, 0.0) + vec2(-t * 0.3, t * 0.3), 1) - noise3,
        fbm(p * 15.0 + vec2(0.0, 0.01) + vec2(-t * 0.3, t * 0.3), 1) - noise3
    ) * 0.5;
    
    // Create swirling motion by rotating the gradients
    vec2 swirl1 = vec2(-grad1.y, grad1.x);
    vec2 swirl2 = vec2(-grad2.y, grad2.x);
    vec2 swirl3 = vec2(-grad3.y, grad3.x);
    
    // Combine eddies of different scales
    return (swirl1 + swirl2 + swirl3) * eddyIntensity * eddyDensity;
}

void main() {
    float obstacle = texture(obstacleTexture, TexCoord).r;
    //if (obstacle > 0.0) {
    //    FragColor = vec4(0.0, 0.0, 0.0, 1.0);
    //    return;
    //}

    // Get base velocity
    vec2 vel = texture(velocityTexture, TexCoord).xy;
    
    // Calculate multi-scale eddy perturbation
    vec2 eddyVel = eddyField(TexCoord, time);
    
    // Add eddy perturbation to base velocity
    vel = vel + eddyVel * 0.025;// * dt;
    
    // Calculate backtracing position with perturbed velocity
    vec2 pos = TexCoord - dt * vec2(vel.x * aspect_ratio, vel.y) * texelSize;

    // Sample from the back-traced position
    vec4 result = texture(sourceTexture, pos);

    // Prevent sampling from obstacles
    float obstacleSample = texture(obstacleTexture, pos).r;
    if (obstacleSample > 0.0) {
        // If we sampled from an obstacle, reflect the velocity
       //result = vec4(-vel, 0.0, 1.0);


        // If we sampled from an obstacle, kill the velocity
		// We do this to avoid generating the opposite colour as a bug
		//result = vec4(0.0, 0.0, 0.0, 1.0);

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
    //if (obstacle > 0.0) {
    //    FragColor = 0.0;
    //    return;
    //}

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
    //if (obstacle > 0.0) {
    //    FragColor = 0.0;
    //    return;
    //}

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
    //if (obstacle > 0.0) {
    //    FragColor = vec4(0.0, 0.0, 0.0, 1.0);
    //    return;
    //}

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




// Add to the start of the file where other shaders are defined
const char* addColorFragmentShader = R"(
#version 330 core
uniform sampler2D colorTexture;
uniform sampler2D obstacleTexture;
uniform vec2 point;
uniform float radius;

out float FragColor;

in vec2 TexCoord;

void main() 
{
    // Check if we're in an obstacle
    float obstacle = texture(obstacleTexture, TexCoord).r;
    //if (obstacle > 0.0) {
    //    FragColor = 0.0;
    //    return;
    //}

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

const char* addForceFragmentShader = R"(
#version 330 core
uniform sampler2D velocityTexture;
uniform sampler2D obstacleTexture;
uniform vec2 point;
uniform vec2 direction;
uniform float radius;
uniform float strength;
uniform float WIDTH;
uniform float HEIGHT;

out vec4 FragColor;

in vec2 TexCoord;

void main()
{
	float aspect_ratio = WIDTH/HEIGHT;

    vec2 adjustedCoord = TexCoord;
    
    // For non-square textures, adjust sampling to prevent stretching
    if (aspect_ratio > 1.0) {
        adjustedCoord.x = (adjustedCoord.x - 0.5) / aspect_ratio + 0.5;
    } else if (aspect_ratio < 1.0) {
        adjustedCoord.y = (adjustedCoord.y - 0.5) * aspect_ratio + 0.5;
    }



    // Check if we're in an obstacle
    float obstacle = texture(obstacleTexture, adjustedCoord).r;
    //if (obstacle > 0.0) {
    //    FragColor = vec4(0.0, 0.0, 0.0, 1.0);
    //    return;
    //}

    // Get current velocity
    vec2 velocity = texture(velocityTexture, adjustedCoord).xy;
    
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
uniform sampler2D backgroundTexture2;  // New second background texture

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
    
    vec2 adjustedCoord2 = TexCoord;
    
    // For non-square textures, adjust sampling to prevent stretching
    if (1.0/aspect_ratio > 1.0) {
        adjustedCoord2.x = (adjustedCoord2.x - 0.5) * aspect_ratio + 0.5;
    } else if (1.0/aspect_ratio < 1.0) {
        adjustedCoord2.y = (adjustedCoord2.y - 0.5) / aspect_ratio + 0.5;
    }

    // Create scrolled coordinates for each background
    vec2 scrolledCoord = adjustedCoord2;
    scrolledCoord.x += time * 0.01;

    vec2 scrolledCoord2 = adjustedCoord2;
    scrolledCoord2.x += time * 0.02;  // Scroll twice as fast

    // Check for collision at obstacle boundaries
    vec4 collision = texture(collisionTexture, adjustedCoord);
    
    // Check for obstacle
    float obstacle = texture(obstacleTexture, adjustedCoord).r;

    if (obstacle > 0.0) {
        // Render obstacles as background colored
        FragColor = texture(backgroundTexture, adjustedCoord2);
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
    
    // Sample both background textures
    vec4 bgColor1 = texture(backgroundTexture, scrolledCoord);
    vec4 bgColor2 = texture(backgroundTexture2, scrolledCoord2);
    
    vec4 blendedBackground = vec4(0.0, 0.0, 0.0, 0.0);
	blendedBackground.rgb = mix(bgColor1.rgb, bgColor2.rgb, bgColor2.a);
	blendedBackground.a = 1.0;

    vec4 color1 = blendedBackground;
    vec4 color2 = vec4(0.0, 0.125, 0.25, 1.0);
    vec4 color3 = combinedColor;
    vec4 color4 = vec4(0.0, 0.0, 0.0, 1.0);

    if(length(redFluidColor.r) > 0.5)
        color4 = vec4(0.0, 0.0, 0.0, 0.0);
    else
        color4 = vec4(1.0, 1.0, 1.0, 0.0);

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






//
//
//
//void applyGaussianBlurRGBA(const std::vector<unsigned char>& input,
//	std::vector<unsigned char>& output,
//	int width, int height, double sigma) {
//	int kernelRadius = static_cast<int>(std::ceil(2.0 * sigma));
//	int kernelSize = 2 * kernelRadius + 1;
//	std::vector<double> kernel(kernelSize);
//
//	// Generate the Gaussian kernel
//	double sum = 0.0;
//	for (int i = -kernelRadius; i <= kernelRadius; ++i) {
//		kernel[i + kernelRadius] = std::exp(-(i * i) / (2 * sigma * sigma));
//		sum += kernel[i + kernelRadius];
//	}
//	for (auto& value : kernel) {
//		value /= sum;
//	}
//
//	// Apply the Gaussian blur (separable kernel)
//	std::vector<unsigned char> temp(width * height * 4);
//
//	// Horizontal pass
//	for (int y = 0; y < height; ++y) {
//		for (int x = 0; x < width; ++x) {
//			for (int c = 0; c < 4; ++c) { // RGBA channels
//				double value = 0.0;
//				for (int i = -kernelRadius; i <= kernelRadius; ++i) {
//					int px = std::clamp(x + i, 0, width - 1);
//					value += input[(y * width + px) * 4 + c] * kernel[i + kernelRadius];
//				}
//				temp[(y * width + x) * 4 + c] = static_cast<unsigned char>(value);
//			}
//		}
//	}
//
//	// Vertical pass
//	for (int y = 0; y < height; ++y) {
//		for (int x = 0; x < width; ++x) {
//			for (int c = 0; c < 4; ++c) { // RGBA channels
//				double value = 0.0;
//				for (int i = -kernelRadius; i <= kernelRadius; ++i) {
//					int py = std::clamp(y + i, 0, height - 1);
//					value += temp[(py * width + x) * 4 + c] * kernel[i + kernelRadius];
//				}
//				output[(y * width + x) * 4 + c] = static_cast<unsigned char>(value);
//			}
//		}
//	}
//}
//
//
//
//
//void dilateImageRGBA(const std::vector<unsigned char>& input,
//	std::vector<unsigned char>& output,
//	int width, int height, int radius) {
//	int channels = 4; // RGBA channels
//
//	// Initialize output with input image (optional, but clarifies code)
//	output = input;
//
//	// Create a temporary buffer to store the intermediate dilation results
//	std::vector<unsigned char> temp = input;
//
//	// Dilation logic
//	for (int y = 0; y < height; ++y) {
//		for (int x = 0; x < width; ++x) {
//			for (int c = 0; c < channels; ++c) {
//				unsigned char maxValue = 0; // Initialize max value for the channel
//
//				// Check neighboring pixels within the dilation radius
//				for (int dy = -radius; dy <= radius; ++dy) {
//					for (int dx = -radius; dx <= radius; ++dx) {
//						int nx = std::clamp(x + dx, 0, width - 1);
//						int ny = std::clamp(y + dy, 0, height - 1);
//						maxValue = std::max(maxValue, temp[(ny * width + nx) * channels + c]);
//					}
//				}
//
//				// Set the output pixel to the maximum value in the neighborhood
//				output[(y * width + x) * channels + c] = maxValue;
//			}
//		}
//	}
//}
//
//








//
//void applyVorticityConfinement() {
//	glBindFramebuffer(GL_FRAMEBUFFER, processingFBO);
//
//	// Step 1: Compute vorticity (curl)
//	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, vorticityTexture, 0);
//	glUseProgram(curlProgram);
//	glUniform1i(glGetUniformLocation(curlProgram, "velocityTexture"), 0);
//	glUniform1i(glGetUniformLocation(curlProgram, "obstacleTexture"), 1);
//	glUniform2f(glGetUniformLocation(curlProgram, "texelSize"), 1.0f / WIDTH, 1.0f / HEIGHT);
//
//	glActiveTexture(GL_TEXTURE0);
//	glBindTexture(GL_TEXTURE_2D, velocityTexture[velocityIndex]);
//	glActiveTexture(GL_TEXTURE1);
//	glBindTexture(GL_TEXTURE_2D, obstacleTexture);
//
//	drawFullScreenQuad();
//
//	// Step 2: Compute vorticity confinement force
//	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, vorticityForceTexture, 0);
//	glUseProgram(vorticityForceProgram);
//	glUniform1i(glGetUniformLocation(vorticityForceProgram, "vorticityTexture"), 0);
//	glUniform1i(glGetUniformLocation(vorticityForceProgram, "velocityTexture"), 1);
//	glUniform1i(glGetUniformLocation(vorticityForceProgram, "obstacleTexture"), 2);
//	glUniform1f(glGetUniformLocation(vorticityForceProgram, "epsilon"), 1.0f); // Adjust intensity
//	glUniform2f(glGetUniformLocation(vorticityForceProgram, "texelSize"), 1.0f / WIDTH, 1.0f / HEIGHT);
//
//	glActiveTexture(GL_TEXTURE0);
//	glBindTexture(GL_TEXTURE_2D, vorticityTexture);
//	glActiveTexture(GL_TEXTURE1);
//	glBindTexture(GL_TEXTURE_2D, velocityTexture[velocityIndex]);
//	glActiveTexture(GL_TEXTURE2);
//	glBindTexture(GL_TEXTURE_2D, obstacleTexture);
//
//	drawFullScreenQuad();
//
//	// Step 3: Apply vorticity force to velocity field
//	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, velocityTexture[1 - velocityIndex], 0);
//	glUseProgram(applyForceProgram);
//	glUniform1i(glGetUniformLocation(applyForceProgram, "velocityTexture"), 0);
//	glUniform1i(glGetUniformLocation(applyForceProgram, "forceTexture"), 1);
//	glUniform1i(glGetUniformLocation(applyForceProgram, "obstacleTexture"), 2);
//	glUniform1f(glGetUniformLocation(applyForceProgram, "dt"), DT);
//
//	glActiveTexture(GL_TEXTURE0);
//	glBindTexture(GL_TEXTURE_2D, velocityTexture[velocityIndex]);
//	glActiveTexture(GL_TEXTURE1);
//	glBindTexture(GL_TEXTURE_2D, vorticityForceTexture);
//	glActiveTexture(GL_TEXTURE2);
//	glBindTexture(GL_TEXTURE_2D, obstacleTexture);
//
//	drawFullScreenQuad();
//
//	// Swap velocity textures
//	velocityIndex = 1 - velocityIndex;
//
//	glBindFramebuffer(GL_FRAMEBUFFER, 0);
//}



//
//void applyVorticityConfinementColor() {
//	glBindFramebuffer(GL_FRAMEBUFFER, processingFBO);
//
//	// Step 1: Compute vorticity (curl)
//	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, vorticityTexture, 0);
//	glUseProgram(curlProgram);
//	glUniform1i(glGetUniformLocation(curlProgram, "velocityTexture"), 0);
//	glUniform1i(glGetUniformLocation(curlProgram, "obstacleTexture"), 1);
//	glUniform2f(glGetUniformLocation(curlProgram, "texelSize"), 1.0f / WIDTH, 1.0f / HEIGHT);
//
//	glActiveTexture(GL_TEXTURE0);
//	glBindTexture(GL_TEXTURE_2D, colorTexture[colorIndex]);
//	glActiveTexture(GL_TEXTURE1);
//	glBindTexture(GL_TEXTURE_2D, obstacleTexture);
//
//	drawFullScreenQuad();
//
//	// Step 2: Compute vorticity confinement force
//	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, vorticityForceTexture, 0);
//	glUseProgram(vorticityForceProgram);
//	glUniform1i(glGetUniformLocation(vorticityForceProgram, "vorticityTexture"), 0);
//	glUniform1i(glGetUniformLocation(vorticityForceProgram, "velocityTexture"), 1);
//	glUniform1i(glGetUniformLocation(vorticityForceProgram, "obstacleTexture"), 2);
//	glUniform1f(glGetUniformLocation(vorticityForceProgram, "epsilon"), 1.0f); // Adjust intensity
//	glUniform2f(glGetUniformLocation(vorticityForceProgram, "texelSize"), 1.0f / WIDTH, 1.0f / HEIGHT);
//
//	glActiveTexture(GL_TEXTURE0);
//	glBindTexture(GL_TEXTURE_2D, vorticityTexture);
//	glActiveTexture(GL_TEXTURE1);
//	glBindTexture(GL_TEXTURE_2D, colorTexture[colorIndex]);
//	glActiveTexture(GL_TEXTURE2);
//	glBindTexture(GL_TEXTURE_2D, obstacleTexture);
//
//	drawFullScreenQuad();
//
//	// Step 3: Apply vorticity force to velocity field
//	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture[1 - colorIndex], 0);
//	glUseProgram(applyForceProgram);
//	glUniform1i(glGetUniformLocation(applyForceProgram, "velocityTexture"), 0);
//	glUniform1i(glGetUniformLocation(applyForceProgram, "forceTexture"), 1);
//	glUniform1i(glGetUniformLocation(applyForceProgram, "obstacleTexture"), 2);
//	glUniform1f(glGetUniformLocation(applyForceProgram, "dt"), DT);
//
//	glActiveTexture(GL_TEXTURE0);
//	glBindTexture(GL_TEXTURE_2D, colorTexture[colorIndex]);
//	glActiveTexture(GL_TEXTURE1);
//	glBindTexture(GL_TEXTURE_2D, vorticityForceTexture);
//	glActiveTexture(GL_TEXTURE2);
//	glBindTexture(GL_TEXTURE_2D, obstacleTexture);
//
//	drawFullScreenQuad();
//
//	colorIndex = 1 - colorIndex;
//
//	glBindFramebuffer(GL_FRAMEBUFFER, 0);
//}
//
//
//void applyVorticityConfinementFriendlyColor() {
//	glBindFramebuffer(GL_FRAMEBUFFER, processingFBO);
//
//	// Step 1: Compute vorticity (curl)
//	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, vorticityTexture, 0);
//	glUseProgram(curlProgram);
//	glUniform1i(glGetUniformLocation(curlProgram, "velocityTexture"), 0);
//	glUniform1i(glGetUniformLocation(curlProgram, "obstacleTexture"), 1);
//	glUniform2f(glGetUniformLocation(curlProgram, "texelSize"), 1.0f / WIDTH, 1.0f / HEIGHT);
//
//	glActiveTexture(GL_TEXTURE0);
//	glBindTexture(GL_TEXTURE_2D, friendlyColorTexture[friendlyColorIndex]);
//	glActiveTexture(GL_TEXTURE1);
//	glBindTexture(GL_TEXTURE_2D, obstacleTexture);
//
//	drawFullScreenQuad();
//
//	// Step 2: Compute vorticity confinement force
//	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, vorticityForceTexture, 0);
//	glUseProgram(vorticityForceProgram);
//	glUniform1i(glGetUniformLocation(vorticityForceProgram, "vorticityTexture"), 0);
//	glUniform1i(glGetUniformLocation(vorticityForceProgram, "velocityTexture"), 1);
//	glUniform1i(glGetUniformLocation(vorticityForceProgram, "obstacleTexture"), 2);
//	glUniform1f(glGetUniformLocation(vorticityForceProgram, "epsilon"), 1.0f); // Adjust intensity
//	glUniform2f(glGetUniformLocation(vorticityForceProgram, "texelSize"), 1.0f / WIDTH, 1.0f / HEIGHT);
//
//	glActiveTexture(GL_TEXTURE0);
//	glBindTexture(GL_TEXTURE_2D, vorticityTexture);
//	glActiveTexture(GL_TEXTURE1);
//	glBindTexture(GL_TEXTURE_2D, friendlyColorTexture[friendlyColorIndex]);
//	glActiveTexture(GL_TEXTURE2);
//	glBindTexture(GL_TEXTURE_2D, obstacleTexture);
//
//	drawFullScreenQuad();
//
//	// Step 3: Apply vorticity force to velocity field
//	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, friendlyColorTexture[1 - friendlyColorIndex], 0);
//	glUseProgram(applyForceProgram);
//	glUniform1i(glGetUniformLocation(applyForceProgram, "velocityTexture"), 0);
//	glUniform1i(glGetUniformLocation(applyForceProgram, "forceTexture"), 1);
//	glUniform1i(glGetUniformLocation(applyForceProgram, "obstacleTexture"), 2);
//	glUniform1f(glGetUniformLocation(applyForceProgram, "dt"), DT);
//
//	glActiveTexture(GL_TEXTURE0);
//	glBindTexture(GL_TEXTURE_2D, friendlyColorTexture[friendlyColorIndex]);
//	glActiveTexture(GL_TEXTURE1);
//	glBindTexture(GL_TEXTURE_2D, vorticityForceTexture);
//	glActiveTexture(GL_TEXTURE2);
//	glBindTexture(GL_TEXTURE_2D, obstacleTexture);
//
//	drawFullScreenQuad();
//
//	friendlyColorIndex = 1 - friendlyColorIndex;
//
//	glBindFramebuffer(GL_FRAMEBUFFER, 0);
//}



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

			size_t variationIndex = stamp.currentVariationIndex;
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
			glUniform2f(glGetUniformLocation(stampObstacleProgram, "stampSize"), (float)stamp.width, (float)stamp.height);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, obstacleTexture);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, stamp.textureIDs[variationIndex]);

			glBindVertexArray(vao);
			glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
		}
	};

	if (allyShips.empty() && enemyShips.empty() && allyBullets.empty() && enemyBullets.empty() && allyPowerUps.empty()) return;

	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, obstacleTexture, 0);

	glUseProgram(stampObstacleProgram);

	glUniform1i(glGetUniformLocation(stampObstacleProgram, "obstacleTexture"), 0);
	glUniform1i(glGetUniformLocation(stampObstacleProgram, "stampTexture"), 1);
	glUniform1f(glGetUniformLocation(stampObstacleProgram, "threshold"), 0.5f);
	glUniform2f(glGetUniformLocation(stampObstacleProgram, "screenSize"), (float)WIDTH, (float)HEIGHT);

	processStamps(allyShips);
	processStamps(enemyShips);
	processStamps(allyPowerUps);  // Add this line to process power-ups

	// Don't treat bullets as obstacles
	//processStamps(allyBullets);
	//processStamps(enemyBullets);
}










bool isCollisionInStamp(const CollisionPoint& point, const Stamp& stamp, const size_t stamp_index, const string& stamp_type, vector<ivec2>& collision_pixel_locations)
{
	//if (!stamp.active) return false;

	//// Validate variation index
	size_t variationIndex = stamp.currentVariationIndex;
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


	bool is_opaque_enough = opacity > COLOR_DETECTION_THRESHOLD;

	if (is_opaque_enough)
	{
		ivec2 iv;
		iv.x = pixelX;
		iv.y = pixelY;
		iv.stamp_index = stamp_index;
		iv.stamp_type = stamp_type;
		collision_pixel_locations.push_back(iv);
	}

	// Check if the pixel is opaque enough for a collision
	return is_opaque_enough;
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

			vector<ivec2> collision_pixel_locations;

			// Test each collision point against this stamp
			for (const auto& point : collisionPoints)
			{
				float normX = point.x / float(WIDTH);
				float normY = point.y / float(HEIGHT);

				// Perform the actual collision check
				bool collides = isCollisionInStamp(point, stamps[i], i, type, collision_pixel_locations);

				if (collides)
				{
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
				{
					damage = blue_count;

					if (blueStampCollisions > 0)
					{
						for (size_t j = 0; j < collision_pixel_locations.size(); j++)
							allyShips[i].blackening_points.push_back(collision_pixel_locations[j]);
					}
				}
				else
				{
					damage = red_count;

					if (redStampCollisions > 0)
					{
						for (size_t j = 0; j < collision_pixel_locations.size(); j++)
							enemyShips[i].blackening_points.push_back(collision_pixel_locations[j]);

					}


				}

				static std::chrono::high_resolution_clock::time_point last_did_damage_at = std::chrono::high_resolution_clock::now();

				std::chrono::high_resolution_clock::time_point global_time_end = std::chrono::high_resolution_clock::now();
				std::chrono::duration<float, std::milli> elapsed;
				elapsed = global_time_end - last_did_damage_at;

				stamps[i].health -= damage * (elapsed.count() / 1000.0f);// *fps_coeff;
				cout << stamps[i].health << endl;

				last_did_damage_at = global_time_end;

			}
		}
	};

	generateFluidCollisionsForStamps(allyShips, "Ally Ship");
	generateFluidCollisionsForStamps(enemyShips, "Enemy Ship");
}








void applyBitmapObstacle() {
	if (!rightMouseDown || (allyTemplates.empty() && enemyTemplates.empty() && bulletTemplates.empty())) return;

	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, obstacleTexture, 0);

	glUseProgram(stampObstacleProgram);

	float mousePosX = mouseX / (float)WIDTH;
	float mousePosY = 1.0f - (mouseY / (float)HEIGHT);

	lastRightMouseDown = rightMouseDown;

	// Preview the current template stamp at mouse position based on type
	const Stamp* currentStamp = nullptr;

	switch (currentTemplateType) {
	case ALLY:
		if (allyTemplates.empty()) return;
		currentStamp = &allyTemplates[currentAllyTemplateIndex];
		break;
	case ENEMY:
		if (enemyTemplates.empty()) return;
		currentStamp = &enemyTemplates[currentEnemyTemplateIndex];
		break;
	case BULLET:
		if (bulletTemplates.empty()) return;
		currentStamp = &bulletTemplates[0]; // Always use the first bullet template
		break;
	}

	size_t currentVariation = 0;

	if (upKeyPressed) {
		currentVariation = 1;
	}
	else if (downKeyPressed) {
		currentVariation = 2;
	}
	if (currentVariation >= currentStamp->textureIDs.size() ||
		currentStamp->textureIDs[currentVariation] == 0) {
		for (size_t i = 0; i < currentStamp->textureIDs.size(); i++) {
			if (currentStamp->textureIDs[i] != 0) {
				currentVariation = i;
				break;
			}
		}
	}

	glUniform1i(glGetUniformLocation(stampObstacleProgram, "obstacleTexture"), 0);
	glUniform1i(glGetUniformLocation(stampObstacleProgram, "stampTexture"), 1);
	glUniform2f(glGetUniformLocation(stampObstacleProgram, "position"), mousePosX, mousePosY);
	glUniform2f(glGetUniformLocation(stampObstacleProgram, "stampSize"),
		(float)currentStamp->width, (float)currentStamp->height);
	glUniform1f(glGetUniformLocation(stampObstacleProgram, "threshold"), 0.5f);
	glUniform2f(glGetUniformLocation(stampObstacleProgram, "screenSize"), (float)WIDTH, (float)HEIGHT);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, obstacleTexture);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, currentStamp->textureIDs[currentVariation]);

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



void initGPUImageProcessing() {
	// Create shader programs
	dilationProgram = createShaderProgram(vertexShaderSource, dilationFragmentShader);
	gaussianBlurHorizontalProgram = createShaderProgram(vertexShaderSource, gaussianBlurHorizontalFragmentShader);
	gaussianBlurVerticalProgram = createShaderProgram(vertexShaderSource, gaussianBlurVerticalFragmentShader);
	blackeningProgram = createShaderProgram(vertexShaderSource, blackeningFragmentShader);

	// Create framebuffer for processing
	glGenFramebuffers(1, &processingFBO);

	// Create temporary textures for processing
	glGenTextures(1, &tempTexture1);
	glGenTextures(1, &tempTexture2);
}



void setupProcessingTexture(GLuint textureID, int width, int height) {
	glBindTexture(GL_TEXTURE_2D, textureID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
}


void applyDilationGPU(GLuint inputTexture, GLuint outputTexture, int width, int height, int radius) {
	// Bind framebuffer and set output texture
	glBindFramebuffer(GL_FRAMEBUFFER, processingFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, outputTexture, 0);

	// Viewport should match texture dimensions
	glViewport(0, 0, width, height);

	// Use dilation shader program
	glUseProgram(dilationProgram);

	// Set uniforms
	glUniform1i(glGetUniformLocation(dilationProgram, "inputTexture"), 0);
	glUniform1i(glGetUniformLocation(dilationProgram, "radius"), radius);
	glUniform2f(glGetUniformLocation(dilationProgram, "texelSize"), 1.0f / width, 1.0f / height);

	// Bind input texture
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, inputTexture);

	// Render full-screen quad
	glBindVertexArray(vao);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	glViewport(0, 0, WIDTH, HEIGHT);
}

// Function to apply Gaussian blur on the GPU
void applyGaussianBlurGPU(GLuint inputTexture, GLuint outputTexture, int width, int height, double sigma) {
	// First pass: horizontal blur (input -> tempTexture1)
	glBindFramebuffer(GL_FRAMEBUFFER, processingFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tempTexture1, 0);
	glViewport(0, 0, width, height);

	glUseProgram(gaussianBlurHorizontalProgram);
	glUniform1i(glGetUniformLocation(gaussianBlurHorizontalProgram, "inputTexture"), 0);
	glUniform1f(glGetUniformLocation(gaussianBlurHorizontalProgram, "sigma"), (float)sigma);
	glUniform2f(glGetUniformLocation(gaussianBlurHorizontalProgram, "texelSize"), 1.0f / width, 1.0f / height);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, inputTexture);

	glBindVertexArray(vao);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	// Second pass: vertical blur (tempTexture1 -> outputTexture)
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, outputTexture, 0);

	glUseProgram(gaussianBlurVerticalProgram);
	glUniform1i(glGetUniformLocation(gaussianBlurVerticalProgram, "inputTexture"), 0);
	glUniform1f(glGetUniformLocation(gaussianBlurVerticalProgram, "sigma"), (float)sigma);
	glUniform2f(glGetUniformLocation(gaussianBlurVerticalProgram, "texelSize"), 1.0f / width, 1.0f / height);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tempTexture1);

	glBindVertexArray(vao);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	glViewport(0, 0, WIDTH, HEIGHT);
}

// Function to apply blackening effect
void applyBlackeningEffectGPU(GLuint originalTexture, GLuint maskTexture, GLuint outputTexture, int width, int height) {
	glBindFramebuffer(GL_FRAMEBUFFER, processingFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, outputTexture, 0);
	glViewport(0, 0, width, height);

	glUseProgram(blackeningProgram);
	glUniform1i(glGetUniformLocation(blackeningProgram, "originalTexture"), 0);
	glUniform1i(glGetUniformLocation(blackeningProgram, "maskTexture"), 1);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, originalTexture);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, maskTexture);

	glBindVertexArray(vao);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	glViewport(0, 0, WIDTH, HEIGHT);
}

GLuint createBlackeningMaskTexture(const Stamp& stamp, size_t variationIndex) {
	// Create a temporary texture to use as a mask
	GLuint maskTexture;
	glGenTextures(1, &maskTexture);
	glBindTexture(GL_TEXTURE_2D, maskTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// Create an empty black texture
	std::vector<unsigned char> emptyData(stamp.width * stamp.height * 4, 0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, stamp.width, stamp.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, emptyData.data());

	// If there are blackening points, update the texture with white points
	if (!stamp.blackening_points.empty()) {
		// Create a temporary data array to upload to the texture
		std::vector<unsigned char> pointData = emptyData;


		for (const auto& point : stamp.blackening_points) {
			size_t index = (point.y * stamp.width + point.x) * 4;

			if (index >= 0 && index < pointData.size() - 3) {
				pointData[index + 0] = 0; // R
				pointData[index + 1] = 0; // G
				pointData[index + 2] = 0; // B
				pointData[index + 3] = 255; // A
			}
		}

		for (const auto& point : stamp.blackening_points) {
			size_t index = (point.y * stamp.width + point.x) * 4;
			if (index >= 0 && index < pointData.size() - 3) {

				if (pointData[index + 0] < 256 - 1)
				{
					pointData[index + 0] += 1; // R
					pointData[index + 1] += 1; // G
					pointData[index + 2] += 1; // B
				}

				pointData[index + 3] = 255; // A
			}
		}

		// Upload the temporary data to the texture
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, stamp.width, stamp.height, GL_RGBA, GL_UNSIGNED_BYTE, pointData.data());
	}

	return maskTexture;
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
	loadBulletTemplates();





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

	curlProgram = createShaderProgram(vertexShaderSource, curlFragmentShader);
	vorticityForceProgram = createShaderProgram(vertexShaderSource, vorticityForceFragmentShader);
	applyForceProgram = createShaderProgram(vertexShaderSource, applyForceFragmentShader);

	glGenTextures(1, &vorticityTexture);
	glBindTexture(GL_TEXTURE_2D, vorticityTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, WIDTH, HEIGHT, 0, GL_RED, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glGenTextures(1, &vorticityForceTexture);
	glBindTexture(GL_TEXTURE_2D, vorticityForceTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, WIDTH, HEIGHT, 0, GL_RG, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	initGPUImageProcessing();



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
	backgroundTexture2 = loadTexture("grid_wide2.png");



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

	// Eddy parameters - same as in advectVelocity
	glUniform1f(glGetUniformLocation(advectProgram, "eddyIntensity"), eddyIntensity);
	glUniform1f(glGetUniformLocation(advectProgram, "eddyDensity"), eddyDensity);

	std::chrono::high_resolution_clock::time_point global_time_end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<float, std::milli> elapsed;
	elapsed = global_time_end - app_start_time;
	glUniform1f(glGetUniformLocation(advectProgram, "time"), elapsed.count() / 1000.0f);

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

	// Eddy parameters - same as above
	glUniform1f(glGetUniformLocation(advectProgram, "eddyIntensity"), eddyIntensity);
	glUniform1f(glGetUniformLocation(advectProgram, "eddyDensity"), eddyDensity);

	std::chrono::high_resolution_clock::time_point global_time_end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<float, std::milli> elapsed;
	elapsed = global_time_end - app_start_time;
	glUniform1f(glGetUniformLocation(advectProgram, "time"), elapsed.count() / 1000.0f);

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

	// Eddy parameters
	glUniform1f(glGetUniformLocation(advectProgram, "eddyIntensity"), eddyIntensity); // Adjust for desired strength
	glUniform1f(glGetUniformLocation(advectProgram, "eddyDensity"), eddyDensity);   // Adjust for more/fewer eddies

	std::chrono::high_resolution_clock::time_point global_time_end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<float, std::milli> elapsed;
	elapsed = global_time_end - app_start_time;
	glUniform1f(glGetUniformLocation(advectProgram, "time"), elapsed.count() / 1000.0f);

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
void addForce(float posX, float posY, float velX, float velY, float radius, float strength)
{
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, velocityTexture[1 - velocityIndex], 0);

	glUseProgram(addForceProgram);

	//float x_shift = 0.01 * rand() / float(RAND_MAX);
	//float y_shift = 0.01 * rand() / float(RAND_MAX);

	float mousePosX = posX;// +x_shift;
	float mousePosY = posY;// +y_shift;

	float mouseVelX = velX;
	float mouseVelY = velY;

	float mouse_vel_length = sqrt(velX * velX + velY * velY);


	// Set uniforms
	glUniform1i(glGetUniformLocation(addForceProgram, "velocityTexture"), 0);
	glUniform1i(glGetUniformLocation(addForceProgram, "obstacleTexture"), 1);
	glUniform2f(glGetUniformLocation(addForceProgram, "point"), mousePosX, mousePosY);
	glUniform2f(glGetUniformLocation(addForceProgram, "direction"), mouseVelX, mouseVelY);
	glUniform1f(glGetUniformLocation(addForceProgram, "radius"), radius);
	glUniform1f(glGetUniformLocation(addForceProgram, "strength"), mouse_vel_length * strength);
	glUniform1f(glGetUniformLocation(addForceProgram, "WIDTH"), (float)WIDTH);
	glUniform1f(glGetUniformLocation(addForceProgram, "HEIGHT"), (float)HEIGHT);

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


void addMouseForce() {
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
	glUniform1f(glGetUniformLocation(addForceProgram, "strength"), 5000);

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

	float x_shift = 0.01f * rand() / float(RAND_MAX);
	float y_shift = 0.01f * rand() / float(RAND_MAX);

	float mousePosX = posX;// +x_shift;
	float mousePosY = posY;// +y_shift;

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




void updateDynamicTexture(Stamp& stamp) {
	for (size_t i = 0; i < stamp.textureIDs.size(); i++) {
		if (stamp.textureIDs[i] != 0 && i < stamp.pixelData.size() && !stamp.pixelData[i].empty()) {
			if (stamp.blackening_points.size() != 0) {
				// Ensure the temporary textures are ready
				setupProcessingTexture(tempTexture1, stamp.width, stamp.height);
				setupProcessingTexture(tempTexture2, stamp.width, stamp.height);

				// Create a texture from the backup data
				GLuint originalTexture;
				glGenTextures(1, &originalTexture);
				glBindTexture(GL_TEXTURE_2D, originalTexture);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, stamp.width, stamp.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, stamp.backupData[i].data());

				// Create mask texture containing blackening points
				GLuint maskTexture = createBlackeningMaskTexture(stamp, i);

				// Apply dilation to the mask texture
				applyDilationGPU(maskTexture, tempTexture1, stamp.width, stamp.height, 10);

				// Apply Gaussian blur to the dilated mask
				applyGaussianBlurGPU(tempTexture1, tempTexture2, stamp.width, stamp.height, 10.0);

				// Apply the blackening effect to the original texture using the blurred mask
				applyBlackeningEffectGPU(originalTexture, tempTexture2, stamp.textureIDs[i], stamp.width, stamp.height);

				// Clean up temporary textures
				glDeleteTextures(1, &originalTexture);
				glDeleteTextures(1, &maskTexture);

				// Update the pixelData to match what's now in the GPU texture
				// NOTE: This is only needed if the pixelData is used elsewhere in CPU code
				// If possible, avoid this readback for better performance
				glBindTexture(GL_TEXTURE_2D, stamp.textureIDs[i]);
				glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, stamp.pixelData[i].data());
			}
			else {
				// For stamps without blackening points, just ensure the texture is updated
				glBindTexture(GL_TEXTURE_2D, stamp.textureIDs[i]);
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, stamp.width, stamp.height,
					(stamp.channels == 1) ? GL_RED :
					(stamp.channels == 3) ? GL_RGB : GL_RGBA,
					GL_UNSIGNED_BYTE, stamp.pixelData[i].data());
			}
		}
	}
}





void updateObstacle() {
	if (!rightMouseDown || (allyTemplates.empty() && enemyTemplates.empty() && bulletTemplates.empty() && powerUpTemplates.empty())) return;

	if (rightMouseDown && !lastRightMouseDown) {
		float aspect = HEIGHT / float(WIDTH);
		float mousePosX = mouseX / (float)WIDTH;
		float mousePosY = 1.0f - (mouseY / (float)HEIGHT);
		mousePosY = (mousePosY - 0.5f) * aspect + 0.5f;

		// Create new stamp from the current template based on type
		Stamp newStamp;







		switch (currentTemplateType) {
		case ALLY:
			if (allyTemplates.empty()) return;
			newStamp = allyTemplates[currentAllyTemplateIndex];
			break;
		case ENEMY:
			if (enemyTemplates.empty()) return;
			newStamp = enemyTemplates[currentEnemyTemplateIndex];
			break;
		case BULLET:
			if (bulletTemplates.empty()) return;
			newStamp = bulletTemplates[0]; // Always use the first bullet template
			break;
		case POWERUP:
		//	if (powerUpTemplates.empty()) return;
		//	newStamp = powerUpTemplates[currentPowerUpTemplateIndex];
			break;
		}

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

		std::chrono::high_resolution_clock::time_point global_time_end = std::chrono::high_resolution_clock::now();
		std::chrono::duration<float, std::milli> elapsed;
		elapsed = global_time_end - app_start_time;

		// Add the stamp to the appropriate vector based on the template type
		switch (currentTemplateType) {
		case ALLY:
			allyShips.push_back(newStamp);
			std::cout << "Added new ally ship";
			break;

		case ENEMY:
			enemyShips.push_back(newStamp);
			std::cout << "Added new enemy ship";
			break;

		case POWERUP:
			size_t num_powerup_tempates = powerUpTemplates.size();
			size_t index = rand() % num_powerup_tempates;

			newStamp = powerUpTemplates[SINUSOIDAL_POWERUP + index];

			newStamp.powerup = powerup_type(SINUSOIDAL_POWERUP + index);

			std::chrono::high_resolution_clock::time_point global_time_end = std::chrono::high_resolution_clock::now();
			std::chrono::duration<float, std::milli> elapsed;
			elapsed = global_time_end - app_start_time;

			newStamp.posX = 1.0f;
			newStamp.posY = rand()/float(RAND_MAX);

			newStamp.birth_time = elapsed.count() / 1000.0f;
			newStamp.death_time = -1.0f;// elapsed.count() / 1000.0f;
			newStamp.velX = -0.001f;
			newStamp.velY = 0.0f;
			allyPowerUps.push_back(newStamp);

			std::cout << "Added new power up";
			break;
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








void move_and_fork_bullets(void)
{
	auto update_bullets = [&](std::vector<Stamp>& stamps, string type)
	{
		float aspect = HEIGHT / float(WIDTH);

		for (auto& stamp : stamps)
		{
			if (type == "ally" && ally_fire == HOMING)
			{
				long long signed int closest_enemy = -1;
				float closest_distance = FLT_MAX;

				for (size_t i = 0; i < enemyShips.size(); i++)
				{
					if (enemyShips[i].to_be_culled)
						continue;

					float x0 = stamp.posX;
					float y0 = stamp.posY;
					float x1 = enemyShips[i].posX;
					float y1 = enemyShips[i].posY;

					float d = sqrt(pow(x1 - x0, 2.0f) + pow(y1 - y0, 2.0f));

					if (d < closest_distance)
					{
						closest_enemy = i;
						closest_distance = d;
					}
				}

				if (closest_enemy == -1)
				{
					stamp.posX += stamp.velX;
					stamp.posY += stamp.velY;
				}
				else
				{
					float dir_x = enemyShips[closest_enemy].posX - stamp.posX;
					float dir_y = enemyShips[closest_enemy].posY - stamp.posY;

					float len = sqrt(dir_x * dir_x + dir_y * dir_y);


					dir_x /= len;
					dir_y /= len;
					dir_x /= 100;
					dir_y /= 100;


					float rand_x = 0, rand_y = 0;
					RandomUnitVector(rand_x, rand_y);

					stamp.velX = dir_x;
					stamp.velY = dir_y;

					stamp.posX += stamp.velX;
					stamp.posY += stamp.velY;
				}
			}
			else
			{
				std::chrono::high_resolution_clock::time_point global_time_end = std::chrono::high_resolution_clock::now();
				std::chrono::duration<float, std::milli> elapsed;
				elapsed = global_time_end - app_start_time;

				// Store the original direction vector
				float dirX = stamp.velX * aspect;
				float dirY = stamp.velY;

				// Normalize the direction vector
				float dirLength = sqrt(dirX * dirX + dirY * dirY);
				if (dirLength > 0) {
					dirX /= dirLength;
					dirY /= dirLength;
				}

				// Calculate the perpendicular direction vector (rotate 90 degrees)
				float perpX = -dirY;
				float perpY = dirX;

				// Calculate time-based sinusoidal amplitude
				// Use the birth_time to ensure continuous motion
				float timeSinceCreation = elapsed.count() / 1000.0f - stamp.birth_time;
				float frequency = stamp.sinusoidal_frequency; // Controls how many waves appear
				float amplitude = stamp.sinusoidal_amplitude; // Controls wave height


				float sinValue = 0;

				if (stamp.sinusoidal_shift)
					sinValue = -sin(timeSinceCreation * frequency);
				else
					sinValue = sin(timeSinceCreation * frequency);

				// Move forward along original path
				float forwardSpeed = dirLength; // Original velocity magnitude
				stamp.posX += dirX * forwardSpeed;
				stamp.posY += dirY * forwardSpeed;

				// Add sinusoidal motion perpendicular to the path
				stamp.posX += perpX * sinValue * amplitude;
				stamp.posY += perpY * sinValue * amplitude;

				// Add in random walking, like lightning (from original code)
				float rand_x = 0, rand_y = 0;
				RandomUnitVector(rand_x, rand_y);
				stamp.posX += rand_x * stamp.path_randomization;
				stamp.posY += rand_y * stamp.path_randomization;


				float r = rand() / float(RAND_MAX);

				// Split the lightning
				// to do: make the forked lightning smaller
				if (r < stamp.random_forking)
				{
					Stamp newBullet = stamp;

					float rand_x = 0, rand_y = 0;
					RandomUnitVector(rand_x, rand_y);
					newBullet.velX += rand_x * r;
					newBullet.velY += rand_y * r;

					if (type == "ally")
						allyBullets.push_back(newBullet);

					if (type == "enemy")
						enemyBullets.push_back(newBullet);

				}
			}
		}
	};

	update_bullets(allyBullets, "ally");
	update_bullets(enemyBullets, "enemy");
}


void mark_colliding_bullets(void)
{
	std::chrono::high_resolution_clock::time_point global_time_end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<float, std::milli> elapsed;
	elapsed = global_time_end - app_start_time;

	for (size_t i = 0; i < allyBullets.size(); ++i)
		for (size_t j = 0; j < enemyShips.size(); ++j)
			if (isPixelPerfectCollision(allyBullets[i], enemyShips[j]))
				allyBullets[i].death_time = elapsed.count() / 1000.0f;

	for (size_t i = 0; i < enemyBullets.size(); ++i)
		for (size_t j = 0; j < allyShips.size(); ++j)
			if (isPixelPerfectCollision(enemyBullets[i], allyShips[j]))
				enemyBullets[i].death_time = elapsed.count() / 1000.0f;
}

void mark_old_bullets(void)
{
	std::chrono::high_resolution_clock::time_point global_time_end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<float, std::milli> elapsed;
	elapsed = global_time_end - app_start_time;

	for (size_t i = 0; i < allyBullets.size(); ++i)
	{
		if (allyBullets[i].death_time < 0.0f)
			continue;

		if (allyBullets[i].death_time <= elapsed.count() / 1000.0f)
			allyBullets[i].to_be_culled = true;
	}

	for (size_t i = 0; i < enemyBullets.size(); ++i)
	{
		if (enemyBullets[i].death_time < 0.0f)
			continue;

		if (enemyBullets[i].death_time <= elapsed.count() / 1000.0f)
			enemyBullets[i].to_be_culled = true;
	}


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
			if (stamp.posX < -0.5 || stamp.posX > 1.5 ||
				adjustedPosY < -0.5 || adjustedPosY > 1.5)
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
	auto update_ships = [&](std::vector<Stamp>& stamps, bool is_ally)
	{
		for (auto& stamp : stamps)
		{
			const float aspect = WIDTH / float(HEIGHT);

			if (is_ally)
			{
				stamp.posX += stamp.velX / aspect;
				stamp.posY += stamp.velY;

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
			else
			{
				stamp.currentVariationIndex = 0;

				std::chrono::high_resolution_clock::time_point global_time_end = std::chrono::high_resolution_clock::now();
				std::chrono::duration<float, std::milli> elapsed;
				elapsed = global_time_end - app_start_time;

				float t = elapsed.count() / 1000.0f - stamp.birth_time;
				t /= stamp.death_time - stamp.birth_time;

				vec2 vd = get_curve_point(stamp.curve_path, t);
				vec2 vd2 = get_straight_point(stamp.curve_path, t);

				vd.x = lerp(vd.x, vd2.x, 0.15f);
				vd.y = lerp(vd.y, vd2.y, 0.15f);





				const float prevPosY = stamp.posY;

				stamp.posX = vd.x;
				stamp.posY = vd.y;

				const float vel_y = stamp.posY - prevPosY;

				if (vel_y > 0.001)
					stamp.currentVariationIndex = 1;
				else if (vel_y < -0.001)
					stamp.currentVariationIndex = 2;
				else
					stamp.currentVariationIndex = 0;




			}
		}
	};

	update_ships(allyShips, true);
	update_ships(enemyShips, false);
}



void make_dying_bullets(const Stamp& stamp, const bool enemy)
{
	if (stamp.to_be_culled)
		return;

	std::chrono::high_resolution_clock::time_point global_time_end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<float, std::milli> elapsed;
	elapsed = global_time_end - app_start_time;

	Stamp newCentralStamp = bulletTemplates[0];

	float x_rad = stamp.width / float(WIDTH) / 2.0f;
	float y_rad = stamp.height / float(HEIGHT) / 2.0f;

	float avg_rad = max(x_rad, y_rad);// 0.5 * (x_rad + y_rad);

	newCentralStamp.colour_radius = avg_rad / 2.0f;
	newCentralStamp.force_radius = avg_rad / 2.0f;

	newCentralStamp.posX = stamp.posX;
	newCentralStamp.posY = stamp.posY;

	newCentralStamp.birth_time = elapsed.count() / 1000.0f;
	newCentralStamp.death_time = elapsed.count() / 1000.0f + 0.1f;

	if (enemy)
		enemyBullets.push_back(newCentralStamp);
	else
		allyBullets.push_back(newCentralStamp);

	for (size_t j = 0; j < 3; j++)
	{
		Stamp newStamp = newCentralStamp;

		newStamp.colour_radius = avg_rad / 4;
		newStamp.force_radius = avg_rad / 4;

		RandomUnitVector(newStamp.velX, newStamp.velY);

		newStamp.velX /= 250.0f / (rand() / float(RAND_MAX));
		newStamp.velY /= 250.0f / (rand() / float(RAND_MAX));
		newStamp.path_randomization = (rand() / float(RAND_MAX)) * 0.01f;
		newStamp.birth_time = elapsed.count() / 1000.0f;
		newStamp.death_time = elapsed.count() / 1000.0f + 1.0f * rand() / float(RAND_MAX);

		if (enemy)
			enemyBullets.push_back(newStamp);
		else
			allyBullets.push_back(newStamp);
	}

	for (size_t j = 0; j < 5; j++)
	{
		Stamp newStamp = newCentralStamp;

		newStamp.colour_radius = avg_rad / 8;
		newStamp.force_radius = avg_rad / 8;

		RandomUnitVector(newStamp.velX, newStamp.velY);

		newStamp.velX /= 100.0f / (rand() / float(RAND_MAX));
		newStamp.velY /= 100.0f / (rand() / float(RAND_MAX));
		newStamp.path_randomization = (rand() / float(RAND_MAX)) * 0.01f;
		newStamp.birth_time = elapsed.count() / 1000.0f;
		newStamp.death_time = elapsed.count() / 1000.0f + 3.0f * rand() / float(RAND_MAX);

		if (enemy)
			enemyBullets.push_back(newStamp);
		else
			allyBullets.push_back(newStamp);
	}

}




void mark_dying_ships(void)
{
	for (size_t i = 0; i < allyShips.size(); ++i)
	{
		if (allyShips[i].health <= 0)
		{
			make_dying_bullets(allyShips[i], false);
			allyShips[i].to_be_culled = true;
		}
	}

	for (size_t i = 0; i < enemyShips.size(); ++i)
	{
		if (enemyShips[i].health <= 0)
		{
			make_dying_bullets(enemyShips[i], true);
			enemyShips[i].to_be_culled = true;
		}
	}
}

void mark_colliding_ships(void)
{
	for (size_t i = 0; i < allyShips.size(); ++i)
	{
		for (size_t j = 0; j < enemyShips.size(); ++j)
		{
			if (isPixelPerfectCollision(allyShips[i], enemyShips[j]))
			{
				make_dying_bullets(allyShips[i], false);
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
			if (stamp.posX < -0.5 || stamp.posX > 1.5 ||
				adjustedPosY < -0.5 || adjustedPosY > 1.5)
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











void move_powerups(void)
{
	auto update_powerups = [&](std::vector<Stamp>& stamps)
	{
		//for (auto& stamp : stamps)
		//{
		//	const float aspect = WIDTH / float(HEIGHT);

		//	stamp.posX += stamp.velX / aspect;
		//	stamp.posY += stamp.velY;
		//}

		for (auto& stamp : stamps)
		{
			const float aspect = WIDTH / float(HEIGHT);
			std::chrono::high_resolution_clock::time_point global_time_end = std::chrono::high_resolution_clock::now();
			std::chrono::duration<float, std::milli> elapsed;
			elapsed = global_time_end - app_start_time;

			// Store the original direction vector
			float dirX = stamp.velX * aspect;
			float dirY = stamp.velY;

			// Normalize the direction vector
			float dirLength = sqrt(dirX * dirX + dirY * dirY);
			if (dirLength > 0) {
				dirX /= dirLength;
				dirY /= dirLength;
			}

			// Calculate the perpendicular direction vector (rotate 90 degrees)
			float perpX = -dirY;
			float perpY = dirX;

			// Calculate time-based sinusoidal amplitude
			// Use the birth_time to ensure continuous motion
			float timeSinceCreation = elapsed.count() / 1000.0f - stamp.birth_time;
			float frequency = stamp.sinusoidal_frequency; // Controls how many waves appear
			float amplitude = stamp.sinusoidal_amplitude; // Controls wave height

			float sinValue = 0;

			if (stamp.sinusoidal_shift)
				sinValue = -sin(timeSinceCreation * frequency);
			else
				sinValue = sin(timeSinceCreation * frequency);

			// Move forward along original path
			float forwardSpeed = dirLength; // Original velocity magnitude
			stamp.posX += stamp.velX * aspect;
			stamp.posY += stamp.velY;

			// Add sinusoidal motion perpendicular to the path
			stamp.posX += perpX * sinValue * amplitude;
			stamp.posY += perpY * sinValue * amplitude;
		}
	};

	update_powerups(allyPowerUps);
}




void mark_colliding_powerups(void)
{
	for (size_t i = 0; i < allyShips.size(); ++i)
	{
		for (size_t j = 0; j < allyPowerUps.size(); ++j)
		{
			if (isPixelPerfectCollision(allyShips[i], allyPowerUps[j]))
			{
				allyPowerUps[i].to_be_culled = true;

				if (allyPowerUps[j].powerup == SINUSOIDAL_POWERUP)
				{
					has_sinusoidal_fire = true;
					ally_fire = SINUSOIDAL;
				}
				else if (allyPowerUps[j].powerup == RANDOM_POWERUP)
				{
					has_random_fire = true;
					ally_fire = RANDOM;
				}
				else if (allyPowerUps[j].powerup == HOMING_POWERUP)
				{
					has_homing_fire = true;
					ally_fire = HOMING;
				}
				else if (allyPowerUps[j].powerup == X3_POWERUP)
				{
					x3_fire = true;
				}
				else if (allyPowerUps[j].powerup == X5_POWERUP)
				{
					x5_fire = true;
				}
			}
		}
	}
}

void mark_offscreen_powerups(void)
{
	auto update_powerups = [&](std::vector<Stamp>& stamps)
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

	update_powerups(allyPowerUps);
}



void cull_marked_powerups(void)
{
	auto update_powerups = [&](std::vector<Stamp>& stamps)
	{
		for (size_t i = 0; i < stamps.size(); i++)
		{
			if (stamps[i].to_be_culled)
			{
				cout << "culling marked powerup" << endl;
				stamps.erase(stamps.begin() + i);
				i = 0;
			}
		}
	};

	update_powerups(allyPowerUps);
}



void simulationStep()
{
	auto updateDynamicTextures = [&](std::vector<Stamp>& stamps)
	{
		for (auto& stamp : stamps)
			updateDynamicTexture(stamp);
	};

	updateDynamicTextures(allyShips);
	updateDynamicTextures(enemyShips);
	updateDynamicTextures(allyBullets);
	updateDynamicTextures(enemyBullets);


	move_and_fork_bullets();
	mark_colliding_bullets();
	mark_old_bullets();
	mark_offscreen_bullets();
	cull_marked_bullets();


	move_powerups();
	mark_colliding_powerups();
	mark_offscreen_powerups();
	cull_marked_powerups();


	move_ships();
	mark_colliding_ships();
	mark_offscreen_ships();
	mark_dying_ships();
	proceed_stamp_opacity();
	cull_marked_ships();



	bool old_red_mode = red_mode;

	red_mode = true;

	for (size_t i = 0; i < allyBullets.size(); i++)
	{
		addForce(allyBullets[i].posX, allyBullets[i].posY, allyBullets[i].velX, allyBullets[i].velY, allyBullets[i].force_radius, 1);
		addColor(allyBullets[i].posX, allyBullets[i].posY, allyBullets[i].velX, allyBullets[i].velY, allyBullets[i].colour_radius);
	}

	red_mode = false;

	for (size_t i = 0; i < enemyBullets.size(); i++)
	{
		addForce(enemyBullets[i].posX, enemyBullets[i].posY, enemyBullets[i].velX, enemyBullets[i].velY, enemyBullets[i].force_radius, 1);
		addColor(enemyBullets[i].posX, enemyBullets[i].posY, enemyBullets[i].velX, enemyBullets[i].velY, enemyBullets[i].colour_radius);
	}

	red_mode = old_red_mode;


	addMouseForce();
	addMouseColor();



	clearObstacleTexture();
	reapplyAllStamps();

	updateObstacle();



	advectVelocity();
	//applyVorticityConfinement();
	diffuseVelocity();

	advectColor();
	//applyVorticityConfinementColor();
	diffuseColor();

	advectFriendlyColor();
	//applyVorticityConfinementFriendlyColor();
	diffuseFriendlyColor();

	//computeDivergence();
	//solvePressure(20);
	//subtractPressureGradient();

	if (1)//frameCount % 30 == 0)
	{
		detectCollisions();
		generateFluidStampCollisionsDamage();
	}
}







void renderToScreen()
{
	// Bind default framebuffer (the screen)
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Clear the screen
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	// Use a render shader program
	glUseProgram(renderProgram);

	std::chrono::high_resolution_clock::time_point global_time_end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<float, std::milli> elapsed;
	elapsed = global_time_end - app_start_time;

	// Set uniforms
	glUniform1i(glGetUniformLocation(renderProgram, "obstacleTexture"), 1);
	glUniform1i(glGetUniformLocation(renderProgram, "collisionTexture"), 2);
	glUniform1i(glGetUniformLocation(renderProgram, "colorTexture"), 3);
	glUniform1i(glGetUniformLocation(renderProgram, "friendlyColorTexture"), 4);
	glUniform1i(glGetUniformLocation(renderProgram, "backgroundTexture"), 5);
	glUniform1i(glGetUniformLocation(renderProgram, "backgroundTexture2"), 6);  // Add the new texture
	glUniform2f(glGetUniformLocation(renderProgram, "texelSize"), 1.0f / WIDTH, 1.0f / HEIGHT);
	glUniform1f(glGetUniformLocation(renderProgram, "time"), elapsed.count() / 1000.0f);

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
	glActiveTexture(GL_TEXTURE6);  // Add binding for the second texture
	glBindTexture(GL_TEXTURE_2D, backgroundTexture2);

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
			size_t variationIndex = stamp.currentVariationIndex;
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
			glUniform2f(glGetUniformLocation(stampTextureProgram, "stampSize"), (float)stamp.width, (float)stamp.height);
			glUniform1f(glGetUniformLocation(stampTextureProgram, "threshold"), 0.1f);
			glUniform2f(glGetUniformLocation(stampTextureProgram, "screenSize"), (float)WIDTH, (float)HEIGHT);

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
	//renderStamps(allyBullets);
	//renderStamps(enemyBullets);
	renderStamps(allyPowerUps);

	glDisable(GL_BLEND);
}








// GLUT display callback
void display() {
	// Increment frame counter
	frameCount++;

	// Check if it's time to report collisions
	//if (frameCount % FLUID_STAMP_COLLISION_REPORT_INTERVAL == 0)
	//	reportCollisions = true;

	// Render to screen
	renderToScreen();

	// Swap buffers
	glutSwapBuffers();
}






// GLUT idle callback
void idle()
{
	//global_time += DT;
	simulationStep();

	if (spacePressed)
		fireBullet();

	//createThrusterFire();

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



void keyboardup(unsigned char key, int x, int y) {
	switch (key) {
	case ' ': // Space bar

		spacePressed = false;




		break;
	}
}


// GLUT keyboard callback
void keyboard(unsigned char key, int x, int y) {
	switch (key)
	{
	case '0':
	{

		Stamp newStamp = enemyTemplates[currentEnemyTemplateIndex];

		vec2 start;
		start.x = 1.05f;
		start.y = rand() / float(RAND_MAX);

		newStamp.curve_path.push_back(start);

		vec2 middle;

		middle.x = 0.75f;

		if (rand() % 2 == 0)
			middle.y = 0.75f + 0.1f * (rand() / float(RAND_MAX));
		else
			middle.y = 0.75f - 0.1f * (rand() / float(RAND_MAX));

		newStamp.curve_path.push_back(middle);

		middle.x = 0.5f;

		if (rand() % 2 == 0)
			middle.y = 0.5f + 0.1f * (rand() / float(RAND_MAX));
		else
			middle.y = 0.5f - 0.1f * (rand() / float(RAND_MAX));

		newStamp.curve_path.push_back(middle);

		middle.x = 0.25f;

		if (rand() % 2 == 0)
			middle.y = 0.25f + 0.1f * (rand() / float(RAND_MAX));
		else
			middle.y = 0.25f - 0.1f * (rand() / float(RAND_MAX));

		newStamp.curve_path.push_back(middle);


		vec2 end;
		end.x = -0.05f;
		end.y = rand() / float(RAND_MAX);
		newStamp.curve_path.push_back(end);

		newStamp.posX = start.x;
		newStamp.posY = start.y;

		std::chrono::high_resolution_clock::time_point global_time_end = std::chrono::high_resolution_clock::now();
		std::chrono::duration<float, std::milli> elapsed = global_time_end - app_start_time;

		newStamp.birth_time = elapsed.count() / 1000.0f;
		newStamp.death_time = elapsed.count() / 1000.0f + 5.0f;

		enemyShips.push_back(newStamp);
		break;
	}

	case ' ': // Space bar
		spacePressed = true;
		break;

	case 'q':

		ally_fire = STRAIGHT;
		break;

	case 'w':

		if (has_sinusoidal_fire)
			ally_fire = SINUSOIDAL;

		break;
	case 'e':

		if (has_random_fire)
			ally_fire = RANDOM;

		break;
	case 'r':

		if (has_homing_fire)
			ally_fire = HOMING;

		break;


	case 'b':
	case 'B':
		red_mode = !red_mode;
		std::cout << "Switched to " << (red_mode ? "RED" : "BLUE") << " color mode" << std::endl;
		break;

	case 'c':  // Report collisions immediately
	case 'C':
		reportCollisions = true;
		std::cout << "Generating collision report on next frame..." << std::endl;
		break;

	case 't':  // Cycle to the next template type
	case 'T':
		// Cycle through template types
		if (currentTemplateType == ALLY) {
			currentTemplateType = ENEMY;
			std::cout << "Switched to Enemy Ship templates" << std::endl;
		}
		else if (currentTemplateType == ENEMY) {
			currentTemplateType = BULLET;
			std::cout << "Switched to Bullet templates" << std::endl;
		}
		else if (currentTemplateType == BULLET) {
			currentTemplateType = POWERUP;
			std::cout << "Switched to PowerUp templates" << std::endl;
		}
		else {
			currentTemplateType = ALLY;
			std::cout << "Switched to Ally Ship templates" << std::endl;
		}
		break;

	case 'n':  // Cycle through templates of the current type
	case 'N':
		if (currentTemplateType == ALLY && !allyTemplates.empty()) {
			currentAllyTemplateIndex = (currentAllyTemplateIndex + 1) % allyTemplates.size();
			std::cout << "Switched to ally template: "
				<< allyTemplates[currentAllyTemplateIndex].baseFilename
				<< " (" << (currentAllyTemplateIndex + 1) << " of "
				<< allyTemplates.size() << ")" << std::endl;
		}
		else if (currentTemplateType == ENEMY && !enemyTemplates.empty()) {
			currentEnemyTemplateIndex = (currentEnemyTemplateIndex + 1) % enemyTemplates.size();
			std::cout << "Switched to enemy template: "
				<< enemyTemplates[currentEnemyTemplateIndex].baseFilename
				<< " (" << (currentEnemyTemplateIndex + 1) << " of "
				<< enemyTemplates.size() << ")" << std::endl;
		}
		else if (currentTemplateType == BULLET && !bulletTemplates.empty()) {
			std::cout << "Using bullet template: "
				<< bulletTemplates[0].baseFilename << std::endl;
		}
		else if (currentTemplateType == POWERUP && !powerUpTemplates.empty()) {
			currentPowerUpTemplateIndex = (currentPowerUpTemplateIndex + 1) % powerUpTemplates.size();
			std::cout << "Switched to powerup template: "
				<< powerUpTemplates[currentPowerUpTemplateIndex].baseFilename
				<< " (" << (currentPowerUpTemplateIndex + 1) << " of "
				<< powerUpTemplates.size() << ")" << std::endl;
		}
		else {
			std::cout << "No templates of the selected type loaded. Press 'L' to load textures." << std::endl;
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

			allyShips[0].velX *= 0.01f;
			allyShips[0].velY *= 0.01f * (WIDTH / HEIGHT);
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

			allyShips[0].velX *= 0.01f;
			allyShips[0].velY *= 0.01f * (WIDTH / HEIGHT);
		}
	}
}



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
	glDeleteProgram(dilationProgram);
	glDeleteProgram(gaussianBlurHorizontalProgram);
	glDeleteProgram(gaussianBlurVerticalProgram);
	glDeleteProgram(blackeningProgram);
	glDeleteProgram(curlProgram);
	glDeleteProgram(vorticityForceProgram);
	glDeleteProgram(applyForceProgram);


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
	glDeleteTextures(1, &backgroundTexture2);
	glDeleteFramebuffers(1, &processingFBO);
	glDeleteTextures(1, &tempTexture1);
	glDeleteTextures(1, &tempTexture2);
	glDeleteTextures(1, &vorticityTexture);
	glDeleteTextures(1, &vorticityForceTexture);


	for (auto& stamp : allyTemplates) {
		for (auto& textureID : stamp.textureIDs) {
			if (textureID != 0) {
				glDeleteTextures(1, &textureID);
			}
		}
	}

	for (auto& stamp : enemyTemplates) {
		for (auto& textureID : stamp.textureIDs) {
			if (textureID != 0) {
				glDeleteTextures(1, &textureID);
			}
		}
	}

	for (auto& stamp : bulletTemplates) {
		for (auto& textureID : stamp.textureIDs) {
			if (textureID != 0) {
				glDeleteTextures(1, &textureID);
			}
		}
	}

	for (auto& stamp : powerUpTemplates) {
		for (auto& textureID : stamp.textureIDs) {
			if (textureID != 0) {
				glDeleteTextures(1, &textureID);
			}
		}
	}

	allyTemplates.clear();
	enemyTemplates.clear();
	bulletTemplates.clear();
	powerUpTemplates.clear();

	// Delete stamp textures from active stamps
	for (auto& stamp : allyShips) {
		for (auto& textureID : stamp.textureIDs) {
			if (textureID != 0) {
				glDeleteTextures(1, &textureID);
			}
		}
	}

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

	for (auto& stamp : allyPowerUps) {
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
	glutKeyboardUpFunc(keyboardup);

	glutReshapeFunc(reshape);

	glutSpecialFunc(specialKeyboard);
	glutSpecialUpFunc(specialKeyboardUp);

	// Print instructions
	printInstructions();

	// Main loop
	glutMainLoop();

	return 0;
}

