// Claude AI:
// C++ Navier stokes solver using OpenGL. accept mouse input using GLUT
// solve for obstacles too


#include <GL/glew.h>
#include <GL/glut.h>

#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
using namespace std;

#pragma comment(lib, "freeglut")
#pragma comment(lib, "glew32")



// Simulation parameters
const int N = 64;                // Grid size
const float dt = 0.1f;            // Time step
const float diff = 0.0f;          // Diffusion rate
const float visc = 0.0f;          // Viscosity
const float force = 5.0f;         // Force multiplier
const float source = 100.0f;      // Density source
const int iterations = 100;        // Gauss-Seidel iterations
const int FLUID = 0;              // Cell type for fluid
const int OBSTACLE = 1;           // Cell type for obstacle

// Display parameters
const int windowSize = 512;
float cellSize;

// Arrays for velocity, density, and sources
std::vector<float> u, v, u_prev, v_prev;
std::vector<float> dens, dens_prev;
std::vector<int> obstacles;       // Grid to track obstacle cells
int mouseX = 0, mouseY = 0;
bool mouseDown[3] = { false, false, false };
bool addingObstacle = false;      // Flag for obstacle creation mode

// Utility function to convert 2D indices to 1D
int IX(int i, int j) {
	return std::max(0, std::min(N - 1, i)) + std::max(0, std::min(N - 1, j)) * N;
}

// Boundary condition handling with obstacles
void setBoundary(int b, std::vector<float>& x) {
	// Set boundary conditions for domain edges
	//for (int i = 1; i < N - 1; i++) {
	//	x[IX(i, 0)] = b == 2 ? -x[IX(i, 1)] : x[IX(i, 1)];
	//	x[IX(i, N - 1)] = b == 2 ? -x[IX(i, N - 2)] : x[IX(i, N - 2)];
	//}
	//for (int j = 1; j < N - 1; j++) {
	//	x[IX(0, j)] = b == 1 ? -x[IX(1, j)] : x[IX(1, j)];
	//	x[IX(N - 1, j)] = b == 1 ? -x[IX(N - 2, j)] : x[IX(N - 2, j)];
	//}

	//x[IX(0, 0)] = 0.5f * (x[IX(1, 0)] + x[IX(0, 1)]);
	//x[IX(0, N - 1)] = 0.5f * (x[IX(1, N - 1)] + x[IX(0, N - 2)]);
	//x[IX(N - 1, 0)] = 0.5f * (x[IX(N - 2, 0)] + x[IX(N - 1, 1)]);
	//x[IX(N - 1, N - 1)] = 0.5f * (x[IX(N - 2, N - 1)] + x[IX(N - 1, N - 2)]);

	// Set boundary conditions for internal obstacles
	for (int j = 1; j < N - 1; j++) {
		for (int i = 1; i < N - 1; i++) {
			if (obstacles[IX(i, j)] == OBSTACLE) {
				// For each obstacle cell, apply boundary conditions
				// based on neighboring fluid cells

				// For velocities: no-slip condition (zero velocity at boundaries)
				if (b == 1) { // x-velocity component
					float sum = 0.0f;
					int count = 0;

					// Average from fluid neighbors
					if (i > 1 && obstacles[IX(i - 1, j)] != OBSTACLE) {
						sum += x[IX(i - 1, j)];
						count++;
					}
					if (i < N - 2 && obstacles[IX(i + 1, j)] != OBSTACLE) {
						sum += x[IX(i + 1, j)];
						count++;
					}

					x[IX(i, j)] = count > 0 ? -sum / count : 0;
				}
				else if (b == 2) { // y-velocity component
					float sum = 0.0f;
					int count = 0;

					if (j > 1 && obstacles[IX(i, j - 1)] != OBSTACLE) {
						sum += x[IX(i, j - 1)];
						count++;
					}
					if (j < N - 2 && obstacles[IX(i, j + 1)] != OBSTACLE) {
						sum += x[IX(i, j + 1)];
						count++;
					}

					x[IX(i, j)] = count > 0 ? -sum / count : 0;
				}
				else { // For scalar quantities like density or pressure
					float sum = 0.0f;
					int count = 0;

					// Average from fluid neighbors
					if (i > 1 && obstacles[IX(i - 1, j)] != OBSTACLE) {
						sum += x[IX(i - 1, j)];
						count++;
					}
					if (i < N - 2 && obstacles[IX(i + 1, j)] != OBSTACLE) {
						sum += x[IX(i + 1, j)];
						count++;
					}
					if (j > 1 && obstacles[IX(i, j - 1)] != OBSTACLE) {
						sum += x[IX(i, j - 1)];
						count++;
					}
					if (j < N - 2 && obstacles[IX(i, j + 1)] != OBSTACLE) {
						sum += x[IX(i, j + 1)];
						count++;
					}

					x[IX(i, j)] = count > 0 ? sum / count : 0;
				}
			}
		}
	}
}

// Linear solver using Gauss-Seidel relaxation
void linearSolve(int b, std::vector<float>& x, std::vector<float>& x0, float a, float c) {
	float cRecip = 1.0f / c;
	for (int k = 0; k < iterations; k++) {
		for (int j = 1; j < N - 1; j++) {
			for (int i = 1; i < N - 1; i++) {
				// Skip obstacle cells in the solver
				if (obstacles[IX(i, j)] == OBSTACLE) {
					continue;
				}

				x[IX(i, j)] = (x0[IX(i, j)] + a * (
					x[IX(i + 1, j)] + x[IX(i - 1, j)] +
					x[IX(i, j + 1)] + x[IX(i, j - 1)]
					)) * cRecip;
			}
		}
		setBoundary(b, x);
	}
}

// Diffusion step
void diffuse(int b, std::vector<float>& x, std::vector<float>& x0, float diff) {
	float a = dt * diff * (N - 2) * (N - 2);
	linearSolve(b, x, x0, a, 1 + 4 * a);
}

// Advection step with obstacles
void advect(int b, std::vector<float>& d, std::vector<float>& d0, std::vector<float>& u, std::vector<float>& v) {
	float dt0 = dt * (N - 2);
	for (int j = 1; j < N - 1; j++) {
		for (int i = 1; i < N - 1; i++) {
			// Skip obstacle cells
			if (obstacles[IX(i, j)] == OBSTACLE) {
				continue;
			}

			float x = i - dt0 * u[IX(i, j)];
			float y = j - dt0 * v[IX(i, j)];

			if (x < 0.5f) x = 0.5f;
			if (x > N - 1.5f) x = N - 1.5f;
			int i0 = static_cast<int>(x);
			int i1 = i0 + 1;

			if (y < 0.5f) y = 0.5f;
			if (y > N - 1.5f) y = N - 1.5f;
			int j0 = static_cast<int>(y);
			int j1 = j0 + 1;

			float s1 = x - i0;
			float s0 = 1 - s1;
			float t1 = y - j0;
			float t0 = 1 - t1;

			d[IX(i, j)] = s0 * (t0 * d0[IX(i0, j0)] + t1 * d0[IX(i0, j1)]) +
				s1 * (t0 * d0[IX(i1, j0)] + t1 * d0[IX(i1, j1)]);
		}
	}
	setBoundary(b, d);
}

// Project velocities to conserve mass with obstacles
void project(std::vector<float>& u, std::vector<float>& v, std::vector<float>& p, std::vector<float>& div) {
	for (int j = 1; j < N - 1; j++) {
		for (int i = 1; i < N - 1; i++) {
			// Skip obstacle cells
			if (obstacles[IX(i, j)] == OBSTACLE) {
				div[IX(i, j)] = 0;
				p[IX(i, j)] = 0;
				continue;
			}

			div[IX(i, j)] = -0.5f * (
				u[IX(i + 1, j)] - u[IX(i - 1, j)] +
				v[IX(i, j + 1)] - v[IX(i, j - 1)]
				) / N;
			p[IX(i, j)] = 0;
		}
	}
	setBoundary(0, div);
	setBoundary(0, p);

	linearSolve(0, p, div, 1, 4);

	for (int j = 1; j < N - 1; j++) {
		for (int i = 1; i < N - 1; i++) {
			// Skip obstacle cells
			if (obstacles[IX(i, j)] == OBSTACLE) {
				continue;
			}

			u[IX(i, j)] -= 0.5f * (p[IX(i + 1, j)] - p[IX(i - 1, j)]) * N;
			v[IX(i, j)] -= 0.5f * (p[IX(i, j + 1)] - p[IX(i, j - 1)]) * N;
		}
	}
	setBoundary(1, u);
	setBoundary(2, v);
}

// Main velocity step
void velocityStep() {
	// Add forces
	for (int i = 0; i < u.size(); i++) {
		u_prev[i] = u[i];
		v_prev[i] = v[i];
	}

	diffuse(1, u, u_prev, visc);
	diffuse(2, v, v_prev, visc);

	project(u, v, u_prev, v_prev);

	for (int i = 0; i < u.size(); i++) {
		u_prev[i] = u[i];
		v_prev[i] = v[i];
	}

	advect(1, u, u_prev, u_prev, v_prev);
	advect(2, v, v_prev, u_prev, v_prev);

	project(u, v, u_prev, v_prev);
}

// Main density step
void densityStep() {
	// Add sources from mouse interaction
	for (int i = 0; i < dens.size(); i++) {
		dens_prev[i] = dens[i];
	}

	diffuse(0, dens, dens_prev, diff);

	for (int i = 0; i < dens.size(); i++) {
		dens_prev[i] = dens[i];
	}

	advect(0, dens, dens_prev, u, v);
}

// Add density source from mouse
void addDensity(int x, int y, float amount) {
	int i = static_cast<int>(x / cellSize);
	int j = static_cast<int>(y / cellSize);

	// Don't add density to obstacle cells
	if (obstacles[IX(i, j)] != OBSTACLE) {
		dens[IX(i, j)] += amount;
	}
}

// Add velocity force from mouse
void addVelocity(int x, int y, float amountX, float amountY) {
	int i = static_cast<int>(x / cellSize);
	int j = static_cast<int>(y / cellSize);

	// Don't add velocity to obstacle cells
	if (obstacles[IX(i, j)] != OBSTACLE) {
		u[IX(i, j)] += amountX;
		v[IX(i, j)] += amountY;
	}
}

// Add or remove obstacle at mouse position
void toggleObstacle(int x, int y) {
	int i = static_cast<int>(x / cellSize);
	int j = static_cast<int>(y / cellSize);

	// Don't add obstacles at the boundary
	if (i > 0 && i < N - 1 && j > 0 && j < N - 1) {
		// Toggle the obstacle state
		if (obstacles[IX(i, j)] == OBSTACLE) {
			obstacles[IX(i, j)] = FLUID;
		}
		else {
			obstacles[IX(i, j)] = OBSTACLE;
			// Reset any velocity or density in this cell
			u[IX(i, j)] = 0;
			v[IX(i, j)] = 0;
			dens[IX(i, j)] = 0;
		}
	}
}

// GLUT drawing function
void display() {
	glClear(GL_COLOR_BUFFER_BIT);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);

	// Draw density
	for (int j = 0; j < N; j++) {
		for (int i = 0; i < N; i++) {
			float x = i * cellSize;
			float y = j * cellSize;

			if (obstacles[IX(i, j)] == OBSTACLE) {
				// Draw obstacles as solid red blocks
				glColor3f(0.7f, 0.2f, 0.2f);
				glBegin(GL_QUADS);
				glVertex2f(x, y);
				glVertex2f(x + cellSize, y);
				glVertex2f(x + cellSize, y + cellSize);
				glVertex2f(x, y + cellSize);
				glEnd();
			}
			else {
				// Draw fluid density
				float d_ = dens[IX(i, j)];
				float u_ = u[IX(i, j)];
				float v_ = v[IX(i, j)];


				float r = d_;
				float g = d_ * 0.5f + std::sqrt(u_ * u_ + v_ * v_) * 0.2f;
				float b = d_ * 0.25f + std::sqrt(u_ * u_ + v_ * v_) * 0.5f;

				glColor4f(r, g, b, d_);
				glBegin(GL_QUADS);
				glVertex2f(x, y);
				glVertex2f(x + cellSize, y);
				glVertex2f(x + cellSize, y + cellSize);
				glVertex2f(x, y + cellSize);
				glEnd();
			}
		}
	}

	// Draw a small indicator for the current mode (obstacle or fluid)
	glColor3f(addingObstacle ? 1.0f : 0.0f, addingObstacle ? 0.0f : 1.0f, 0.0f);
	glBegin(GL_QUADS);
	glVertex2f(windowSize - 20, windowSize - 20);
	glVertex2f(windowSize - 5, windowSize - 20);
	glVertex2f(windowSize - 5, windowSize - 5);
	glVertex2f(windowSize - 20, windowSize - 5);
	glEnd();

	glutSwapBuffers();
}

// Updates the simulation
void update() {
	static int lastX = 0, lastY = 0;

	// Process mouse input
	if (mouseDown[0]) {
		if (addingObstacle) {
			// Right-click: add/remove obstacles
			toggleObstacle(mouseX, mouseY);
		}
		else {
			// Left-click: add fluid and velocity
			int dx = mouseX - lastX;
			int dy = mouseY - lastY;
			addVelocity(mouseX, mouseY, dx * force, dy * force);
			addDensity(mouseX, mouseY, source);
		}
	}

	lastX = mouseX;
	lastY = mouseY;

	velocityStep();
	densityStep();

	glutPostRedisplay();
}

// Timer callback for animation
void timer(int) {
	update();
	glutTimerFunc(16, timer, 0); // ~60 FPS
}

// Mouse button callback
void mouseFunc(int button, int state, int x, int y) {
	mouseDown[button] = state == GLUT_DOWN;
	mouseX = x;
	mouseY = y;

	// Right mouse button toggles obstacle creation mode
	if (button == 2 && state == GLUT_DOWN) {
		addingObstacle = !addingObstacle;
	}
}

// Mouse motion callback
void motionFunc(int x, int y) {
	mouseX = x;
	mouseY = y;
}

// Keyboard callback
void keyboardFunc(unsigned char key, int x, int y) {
	if (key == 'c' || key == 'C') {
		// Clear the simulation (but keep obstacles)
		std::fill(u.begin(), u.end(), 0.0f);
		std::fill(v.begin(), v.end(), 0.0f);
		std::fill(dens.begin(), dens.end(), 0.0f);
	}
	if (key == 'x' || key == 'X') {
		// Clear everything including obstacles
		std::fill(u.begin(), u.end(), 0.0f);
		std::fill(v.begin(), v.end(), 0.0f);
		std::fill(dens.begin(), dens.end(), 0.0f);
		std::fill(obstacles.begin(), obstacles.end(), FLUID);
	}
	if (key == 'o' || key == 'O') {
		// Toggle obstacle creation mode
		addingObstacle = !addingObstacle;
	}
	if (key == 27) { // ESC key
		exit(0);
	}
}

// Initialization
void init() {
	// Initialize arrays
	int size = N * N;
	u.resize(size, 0.0f);
	v.resize(size, 0.0f);
	u_prev.resize(size, 0.0f);
	v_prev.resize(size, 0.0f);
	dens.resize(size, 0.0f);
	dens_prev.resize(size, 0.0f);
	obstacles.resize(size, FLUID);

	// Set up OpenGL
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glOrtho(0, windowSize, windowSize, 0, -1, 1);

	cellSize = static_cast<float>(windowSize) / N;
}
// Create a preset obstacle pattern
void createPresetObstacles()
{
	// Clear any existing obstacles
	std::fill(obstacles.begin(), obstacles.end(), FLUID);

	// Create a circular obstacle in the center
	int centerX = N / 2;
	int centerY = N / 2;
	int radius = N / 8;

	for (int j = 1; j < N - 1; j++) {
		for (int i = 1; i < N - 1; i++) {
			float dx = i - centerX;
			float dy = j - centerY;
			float distSq = dx * dx + dy * dy;

			if (distSq < radius * radius) {
				obstacles[IX(i, j)] = OBSTACLE;
			}
		}
	}

	// Create some rectangular obstacles
	const int blockSize = N / 16;

	// Left side obstacle
	for (int j = N / 4; j < N / 4 + N / 8; j++) {
		for (int i = N / 8; i < N / 8 + blockSize; i++) {
			obstacles[IX(i, j)] = OBSTACLE;
		}
	}

	// Right side obstacle
	for (int j = N / 2; j < N / 2 + N / 8; j++) {
		for (int i = N - N / 4; i < N - N / 4 + blockSize; i++) {
			obstacles[IX(i, j)] = OBSTACLE;
		}
	}
}

int main(int argc, char** argv) {
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);
	glutInitWindowSize(windowSize, windowSize);
	glutCreateWindow("Navier-Stokes Fluid Simulation with Obstacles");

	glutDisplayFunc(display);
	glutTimerFunc(0, timer, 0);
	glutMouseFunc(mouseFunc);
	glutMotionFunc(motionFunc);
	glutKeyboardFunc(keyboardFunc);

	init();

	// Create some initial obstacles
	createPresetObstacles();

	std::cout << "Navier-Stokes Fluid Simulation with Obstacles" << std::endl;
	std::cout << "----------------------------------------" << std::endl;
	std::cout << "Left click and drag to add fluid and velocity" << std::endl;
	std::cout << "Press 'o' or right-click to toggle obstacle creation mode" << std::endl;
	std::cout << "  - When in obstacle mode (red indicator), click to add/remove obstacles" << std::endl;
	std::cout << "  - When in fluid mode (green indicator), click to add fluid" << std::endl;
	std::cout << "Press 'c' to clear fluid (keeping obstacles)" << std::endl;
	std::cout << "Press 'x' to clear everything including obstacles" << std::endl;
	std::cout << "Press 'ESC' to exit" << std::endl;

	glutMainLoop();
	return 0;
}