#pragma once

#include "helpers.h"

struct Body {
    int index;
    Vec2 position;
    Vec2 velocity;
    Vec2 force;
    double mass;

    Body() : index(0), position(0, 0), velocity(0, 0), force(0, 0), mass(1.0) {}

    Body(int idx, double x, double y, double vx, double vy, double m)
        : index(idx), position(x, y), velocity(0, 0), force(0, 0), mass(m) {}

    void reset_force() {
        force.x = 0;
        force.y = 0;
    }

    void update_position_velocity(double dt) {
        Vec2 acceleration = force * (1.0 / mass);
        position += velocity * dt + acceleration * (0.5 * dt * dt);
        velocity += acceleration * dt;
        // std::cout << "updating velocity.. for index = " << index << " with force = " << force.x << ", " << force.y << std::endl;
    }
};

struct QuadTreeNode {
    Body *body = nullptr;
    Vec2 center_of_mass = Vec2(0,0); 
    double total_mass = 0;
    Vec2 region;
    double side_length;
    bool is_internal = false;
    int n_bodies = 0; // Number of particles in this quadrant

    QuadTreeNode *nw = nullptr;
    QuadTreeNode *ne = nullptr;
    QuadTreeNode *sw = nullptr;
    QuadTreeNode *se = nullptr;

    QuadTreeNode(Vec2 region, double side_length): 
     region(region), 
     side_length(side_length){}

    ~QuadTreeNode() {
        delete nw;
        delete ne;
        delete sw;
        delete se;
    }
    
    void insert(Body* p);
    void insert_to_child(Body *p);
    void subdivide();
};

QuadTreeNode build_tree(int n_bodies, Body* bodies);

void print_body(Body body);
void print_quadtree(QuadTreeNode* node, int depth = 0);