#include <GL/glew.h>
#include <GL/glut.h>

#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
using namespace std;

#pragma comment(lib, "freeglut")
#pragma comment(lib, "glew32")

#include <GL/glut.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>

// Simulation parameters
const int N = 128;               // Grid size
const int SCALE = 4;             // Display scale
const float dt = 0.1f;           // Time step
const float diff = 0.0f;         // Diffusion rate
const float visc = 0.0f;         // Viscosity
const int iterSolve = 4;         // Linear solver iterations
const float force = 5.0f;        // Force applied with mouse

// Mouse state
int mouseX = 0, mouseY = 0;
bool mouseDown = false;
int oldMouseX = 0, oldMouseY = 0;

// Fluid state
float u[N + 2][N + 2], v[N + 2][N + 2];          // Velocity
float u_prev[N + 2][N + 2], v_prev[N + 2][N + 2]; // Previous velocity
float dens[N + 2][N + 2], dens_prev[N + 2][N + 2]; // Density

// Forward declarations
void init();
void displayFunc();
void idleFunc();
void mouseFunc(int button, int state, int x, int y);
void motionFunc(int x, int y);
void reshapeFunc(int width, int height);
void keyboardFunc(unsigned char key, int x, int y);

// Fluid simulation functions
void add_source(float x[N + 2][N + 2], float s[N + 2][N + 2], float dt);
void set_bnd(int b, float x[N + 2][N + 2]);
void lin_solve(int b, float x[N + 2][N + 2], float x0[N + 2][N + 2], float a, float c);
void diffuse(int b, float x[N + 2][N + 2], float x0[N + 2][N + 2], float diff, float dt);
void advect(int b, float d[N + 2][N + 2], float d0[N + 2][N + 2], float u[N + 2][N + 2], float v[N + 2][N + 2], float dt);
void project(float u[N + 2][N + 2], float v[N + 2][N + 2], float p[N + 2][N + 2], float div[N + 2][N + 2]);
void dens_step(float x[N + 2][N + 2], float x0[N + 2][N + 2], float u[N + 2][N + 2], float v[N + 2][N + 2], float diff, float dt);
void vel_step(float u[N + 2][N + 2], float v[N + 2][N + 2], float u0[N + 2][N + 2], float v0[N + 2][N + 2], float visc, float dt);

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE);
    glutInitWindowSize(N * SCALE, N * SCALE);
    glutCreateWindow("Navier-Stokes Fluid Simulation");

    glutDisplayFunc(displayFunc);
    glutIdleFunc(idleFunc);
    glutMouseFunc(mouseFunc);
    glutMotionFunc(motionFunc);
    glutReshapeFunc(reshapeFunc);
    glutKeyboardFunc(keyboardFunc);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glOrtho(0, N, 0, N, -1, 1);

    init();
    glutMainLoop();

    return 0;
}

void init() {
    // Initialize all arrays to zero
    for (int i = 0; i <= N + 1; i++) {
        for (int j = 0; j <= N + 1; j++) {
            u[i][j] = v[i][j] = u_prev[i][j] = v_prev[i][j] = 0.0f;
            dens[i][j] = dens_prev[i][j] = 0.0f;
        }
    }
}

void displayFunc() {
    glClear(GL_COLOR_BUFFER_BIT);

    // Draw density field
    glBegin(GL_QUADS);
    for (int i = 1; i <= N; i++) {
        for (int j = 1; j <= N; j++) {
            float d = dens[i][j];
            // Visualize the fluid with a color based on density and velocity
            float r = d;
            float g = d * 0.5f + std::sqrt(u[i][j] * u[i][j] + v[i][j] * v[i][j]) * 0.2f;
            float b = d * 0.25f + std::sqrt(u[i][j] * u[i][j] + v[i][j] * v[i][j]) * 0.5f;

            glColor3f(r, g, b);
            glVertex2f(i - 0.5f, j - 0.5f);
            glVertex2f(i + 0.5f, j - 0.5f);
            glVertex2f(i + 0.5f, j + 0.5f);
            glVertex2f(i - 0.5f, j + 0.5f);
        }
    }
    glEnd();

    // Optionally draw velocity field as arrows
    if (false) { // Set to true if you want to see velocity vectors
        glBegin(GL_LINES);
        glColor3f(1.0f, 1.0f, 1.0f);
        for (int i = 1; i <= N; i += 4) {
            for (int j = 1; j <= N; j += 4) {
                float x = i;
                float y = j;
                float dx = u[i][j] * 5.0f;
                float dy = v[i][j] * 5.0f;

                glVertex2f(x, y);
                glVertex2f(x + dx, y + dy);
            }
        }
        glEnd();
    }

    glutSwapBuffers();
}

void idleFunc() {
    // Apply forces from mouse input
    if (mouseDown) {
        int i = std::min(std::max(1, mouseX), N);
        int j = std::min(std::max(1, mouseY), N);

        // Calculate velocity from mouse movement
        float dx = mouseX - oldMouseX;
        float dy = mouseY - oldMouseY;

        u_prev[i][j] = force * dx;
        v_prev[i][j] = force * dy;

        // Add density where the mouse is
        dens_prev[i][j] = 100.0f;

        // Spread the force to neighboring cells
        for (int ni = -2; ni <= 2; ni++) {
            for (int nj = -2; nj <= 2; nj++) {
                int ci = std::min(std::max(1, i + ni), N);
                int cj = std::min(std::max(1, j + nj), N);

                float factor = 0.5f / (1.0f + std::sqrt(ni * ni + nj * nj));
                u_prev[ci][cj] += force * dx * factor;
                v_prev[ci][cj] += force * dy * factor;
                dens_prev[ci][cj] += 50.0f * factor;
            }
        }
    }

    // Save old mouse position
    oldMouseX = mouseX;
    oldMouseY = mouseY;

    // Fluid simulation steps
    vel_step(u, v, u_prev, v_prev, visc, dt);
    dens_step(dens, dens_prev, u, v, diff, dt);

    // Gradually dissipate density and velocity to prevent buildup
    for (int i = 1; i <= N; i++) {
        for (int j = 1; j <= N; j++) {
            dens_prev[i][j] = 0.0f;
            u_prev[i][j] *= 0.99f;
            v_prev[i][j] *= 0.99f;
        }
    }

    glutPostRedisplay();
}

void mouseFunc(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON) {
        if (state == GLUT_DOWN) {
            mouseDown = true;
            mouseX = x / SCALE;
            mouseY = (glutGet(GLUT_WINDOW_HEIGHT) - y) / SCALE; // Convert to simulation coordinates
            oldMouseX = mouseX;
            oldMouseY = mouseY;
        }
        else {
            mouseDown = false;
        }
    }
}

void motionFunc(int x, int y) {
    mouseX = x / SCALE;
    mouseY = (glutGet(GLUT_WINDOW_HEIGHT) - y) / SCALE; // Convert to simulation coordinates
}

void reshapeFunc(int width, int height) {
    glutReshapeWindow(N * SCALE, N * SCALE);
}

void keyboardFunc(unsigned char key, int x, int y) {
    if (key == 'r' || key == 'R') {
        init(); // Reset simulation
    }
    else if (key == 27) { // ESC key
        exit(0);
    }
}

// Fluid simulation implementation
void add_source(float x[N + 2][N + 2], float s[N + 2][N + 2], float dt) {
    for (int i = 0; i <= N + 1; i++) {
        for (int j = 0; j <= N + 1; j++) {
            x[i][j] += dt * s[i][j];
        }
    }
}

void set_bnd(int b, float x[N + 2][N + 2]) {
    // Handle boundary conditions
    for (int i = 1; i <= N; i++) {
        x[0][i] = b == 1 ? -x[1][i] : x[1][i];
        x[N + 1][i] = b == 1 ? -x[N][i] : x[N][i];
        x[i][0] = b == 2 ? -x[i][1] : x[i][1];
        x[i][N + 1] = b == 2 ? -x[i][N] : x[i][N];
    }

    // Handle corners
    x[0][0] = 0.5f * (x[1][0] + x[0][1]);
    x[0][N + 1] = 0.5f * (x[1][N + 1] + x[0][N]);
    x[N + 1][0] = 0.5f * (x[N][0] + x[N + 1][1]);
    x[N + 1][N + 1] = 0.5f * (x[N][N + 1] + x[N + 1][N]);
}

void lin_solve(int b, float x[N + 2][N + 2], float x0[N + 2][N + 2], float a, float c) {
    // Gauss-Seidel relaxation
    for (int k = 0; k < iterSolve; k++) {
        for (int i = 1; i <= N; i++) {
            for (int j = 1; j <= N; j++) {
                x[i][j] = (x0[i][j] + a * (x[i - 1][j] + x[i + 1][j] + x[i][j - 1] + x[i][j + 1])) / c;
            }
        }
        set_bnd(b, x);
    }
}

void diffuse(int b, float x[N + 2][N + 2], float x0[N + 2][N + 2], float diff, float dt) {
    float a = dt * diff * N * N;
    lin_solve(b, x, x0, a, 1 + 4 * a);
}

void advect(int b, float d[N + 2][N + 2], float d0[N + 2][N + 2], float u[N + 2][N + 2], float v[N + 2][N + 2], float dt) {
    int i0, j0, i1, j1;
    float x, y, s0, t0, s1, t1;

    float dt0 = dt * N;
    for (int i = 1; i <= N; i++) {
        for (int j = 1; j <= N; j++) {
            // Trace particle back in time
            x = i - dt0 * u[i][j];
            y = j - dt0 * v[i][j];

            // Clamp to grid bounds
            x = std::max(0.5f, std::min(N + 0.5f, x));
            y = std::max(0.5f, std::min(N + 0.5f, y));

            // Get grid cell indices
            i0 = (int)x;
            j0 = (int)y;
            i1 = i0 + 1;
            j1 = j0 + 1;

            // Bilinear interpolation weights
            s1 = x - i0;
            s0 = 1 - s1;
            t1 = y - j0;
            t0 = 1 - t1;

            // Apply bilinear interpolation
            d[i][j] = s0 * (t0 * d0[i0][j0] + t1 * d0[i0][j1]) +
                s1 * (t0 * d0[i1][j0] + t1 * d0[i1][j1]);
        }
    }
    set_bnd(b, d);
}

void project(float u[N + 2][N + 2], float v[N + 2][N + 2], float p[N + 2][N + 2], float div[N + 2][N + 2]) {
    // Helmholtz-Hodge Decomposition to make the velocity field mass-conserving
    for (int i = 1; i <= N; i++) {
        for (int j = 1; j <= N; j++) {
            div[i][j] = -0.5f * (u[i + 1][j] - u[i - 1][j] + v[i][j + 1] - v[i][j - 1]) / N;
            p[i][j] = 0;
        }
    }
    set_bnd(0, div);
    set_bnd(0, p);

    lin_solve(0, p, div, 1, 4);

    for (int i = 1; i <= N; i++) {
        for (int j = 1; j <= N; j++) {
            u[i][j] -= 0.5f * N * (p[i + 1][j] - p[i - 1][j]);
            v[i][j] -= 0.5f * N * (p[i][j + 1] - p[i][j - 1]);
        }
    }
    set_bnd(1, u);
    set_bnd(2, v);
}

void dens_step(float x[N + 2][N + 2], float x0[N + 2][N + 2], float u[N + 2][N + 2], float v[N + 2][N + 2], float diff, float dt) {
    add_source(x, x0, dt);
    std::swap(x0, x);
    diffuse(0, x, x0, diff, dt);
    std::swap(x0, x);
    advect(0, x, x0, u, v, dt);
}

void vel_step(float u[N + 2][N + 2], float v[N + 2][N + 2], float u0[N + 2][N + 2], float v0[N + 2][N + 2], float visc, float dt) {
    add_source(u, u0, dt);
    add_source(v, v0, dt);
    std::swap(u0, u);
    diffuse(1, u, u0, visc, dt);
    std::swap(v0, v);
    diffuse(2, v, v0, visc, dt);
    project(u, v, u0, v0);
    std::swap(u0, u);
    std::swap(v0, v);
    advect(1, u, u0, u0, v0, dt);
    advect(2, v, v0, u0, v0, dt);
    project(u, v, u0, v0);
}