#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <cstdio>
#include <unistd.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <GL/glu.h>
#include <GL/glut.h>
#include <GL/freeglut.h>

#include "visualization.h"

#define WIN_X(x) (2*(x)/DOMAIN_MAX_X - 1)
#define WIN_Y(y) (2*(y)/DOMAIN_MAX_Y - 1)
#define DEBUG_TEST 0
#define DEBUG_OUT(x) do { if (DEBUG_TEST) { std::cerr << "DEBUG: " << (x) << std::endl;} } while (0)

const static int WIN_WIDTH=1000, WIN_HEIGHT=1000;

static bool quit = false;
static bool running = false;
static bool next_frame = false;

static GLFWwindow* window;

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
  if (key == GLFW_KEY_Q && action == GLFW_PRESS) {
    DEBUG_OUT("GLFW_KEY_Q -> quit");

    quit = true;
  } else if (key == GLFW_KEY_J && action == GLFW_PRESS) {
    DEBUG_OUT("GLFW_KEY_J -> next_frame");

    next_frame = true;
  } else if (key == GLFW_KEY_L && action == GLFW_PRESS) {
    DEBUG_OUT("GLFW_KEY_L -> running");

    running = !running;
  }
}

int init_visualization(int *argc, char **argv) {
  if (!glfwInit()) {
    fprintf(stderr, "Failed to initialize GLFW\n" );
    return -1;
  }
  // Open a window and create its OpenGL context
  window = glfwCreateWindow(WIN_WIDTH, WIN_HEIGHT, "Simulation", NULL, NULL);
  if (window == NULL) {
    fprintf(stderr, "Failed to open GLFW window.\n" );
    glfwTerminate();
    return -1;
  }
  glfwMakeContextCurrent(window);
  // // Initialize GLEW
  // if (glewInit() != GLEW_OK) {
  //     fprintf(stderr, "Failed to initialize GLEW\n");
  //     return -1;
  // }
  // Ensure we can capture the escape key being pressed below
  glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);

  glfwSetKeyCallback(window, key_callback);

  glutInit(argc, argv);

  return 0;
}

void terminate_visualization() {
  glfwTerminate();
}

void drawOctreeBounds2D(const QuadTreeNode *node) {
  int i;

  if(node == NULL || node->body != NULL) {
    return;
  }

  glBegin(GL_LINES);
  // set the color of lines to be white
  glColor4f(0.8f, 0.8f, 0.8f, 0.7f);

  if(DEBUG_TEST){
    // Vertical line to divide left and right quadrants
    glVertex2f(WIN_X(node->region.x), WIN_Y(node->region.y + node->side_length / 2));
    glVertex2f(WIN_X(node->region.x + node->side_length), WIN_Y(node->region.y + node->side_length/2));

    // Horizontal line to divide top and bottom quadrants
    glVertex2f(WIN_X(node->region.x + node->side_length / 2), WIN_Y(node->region.y));
    glVertex2f(WIN_X(node->region.x + node->side_length / 2), WIN_Y(node->region.y + node->side_length));
  }

  glEnd();

  if(DEBUG_TEST) {
    // node text info
    glColor4f(0.8f, 0.8f, 0.2f, 1.0f);
    glRasterPos2f(WIN_X(node->region.x), WIN_Y(node->region.y) + 0.01f);
    auto n_particles_s = std::to_string(node->n_bodies);
    auto mass_s = std::to_string(node->total_mass/node->n_bodies);
    auto com_x_s = std::to_string(node->center_of_mass.x);
    auto com_y_s = std::to_string(node->center_of_mass.y);
    // auto text_s = n_particles_s + "|" + mass_s + "|" + com_x_s + "|" + com_y_s;

    auto text_s = std::to_string(node->n_bodies);


    auto text_c = reinterpret_cast<const unsigned char *>(text_s.c_str());
    glutBitmapString(GLUT_BITMAP_HELVETICA_12, text_c);
  }


  drawOctreeBounds2D(node->nw);
  drawOctreeBounds2D(node->ne);
  drawOctreeBounds2D(node->sw);
  drawOctreeBounds2D(node->se);
}

void drawParticle2D(const struct Body *particle) {
  float angle = 0.0f;
  float radius = 0.005f * particle->mass;

  //draw particle
  glBegin(GL_TRIANGLE_FAN);

  glColor3f(0.1f, 0.3f, 0.6f);

  auto win_x = WIN_X(particle->position.x);
  auto win_y = WIN_Y(particle->position.y);

  glVertex2f(win_x, win_y);

  for(int i = 0; i < 20; i++) {
    angle=(float) (i)/19*2*3.141592;

    glVertex2f(
        win_x+radius*cos(angle),
        win_y+radius*sin(angle));
  }

  glEnd();

  if(DEBUG_TEST){
    //draw velocity
    glBegin(GL_LINES);

    // set the color of lines to be red
    glColor3f(1.0f, 0.1f, 0.1f);

    glVertex2f(WIN_X(particle->position.x), WIN_Y(particle->position.y));
    glVertex2f(WIN_X(particle->position.x + particle->velocity.x), WIN_Y(particle->position.y + particle->velocity.y));

    glEnd();
  }

  if(DEBUG_TEST) {
    // particle text info
    glColor4f(0.8f, 0.8f, 0.2f, 1.0f);
    glRasterPos2f(WIN_X(particle->position.x), WIN_Y(particle->position.y) + 0.01f);
    auto mass_s = std::to_string(particle->mass);
    auto x_s = std::to_string(particle->position.x);
    auto y_s = std::to_string(particle->position.y);
    // auto text_s = mass_s + "|" + x_s + "|" + y_s;
    auto text_s = std::to_string(particle->index);

    auto text_c = reinterpret_cast<const unsigned char *>(text_s.c_str());
    glutBitmapString(GLUT_BITMAP_HELVETICA_12, text_c);
  }

}

bool render_visualization(int n_particles,
    struct Body *particles,
    struct QuadTreeNode *root) {

  glClear(GL_COLOR_BUFFER_BIT);

  drawOctreeBounds2D(root);

  for(int i = 0; i < n_particles; i++) {
    drawParticle2D(&particles[i]);
  }

  // Swap buffers
  glfwSwapBuffers(window);
  glfwPollEvents();

  while(!next_frame && !running && !quit && !glfwWindowShouldClose(window)) {
    usleep(10);
    glfwPollEvents();
  }
  next_frame = false;

  return quit;
}
