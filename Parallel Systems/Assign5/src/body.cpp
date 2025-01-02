#include "body.h"

extern bool debug;

void print_body(Body body) {
    std::cout << "Body Index: " << body.index << " | "
              << "Position: (" << body.position.x << ", " << body.position.y << ") | "
              << "Velocity: (" << body.velocity.x << ", " << body.velocity.y << ") | "
              << "Force: (" << body.force.x << ", " << body.force.y << ") | "
              << "Mass: " << body.mass
              << std::endl;
}

void print_quadtree(QuadTreeNode* node, int depth) {
    if (node == nullptr) return;

    std::string indent(depth * 2, ' ');

    std::cout << indent << "QuadTreeNode at region (" 
              << node->region.x << ", " << node->region.y 
              << "), side_length: " << node->side_length
              << ", total_mass: " << node->total_mass
              << ", center_of_mass: (" << node->center_of_mass.x << ", " << node->center_of_mass.y << ")"
              << ", n_bodies: " << node->n_bodies
              << ", is_internal: " << (node->is_internal ? "true" : "false") << "\n";

    // Recursively print each quadrant
    if (node->is_internal) {
        std::cout << indent << " NW:\n";
        print_quadtree(node->nw, depth + 1);

        std::cout << indent << " NE:\n";
        print_quadtree(node->ne, depth + 1);

        std::cout << indent << " SW:\n";
        print_quadtree(node->sw, depth + 1);

        std::cout << indent << " SE:\n";
        print_quadtree(node->se, depth + 1);
    }
}

bool body_out_of_bound(const Body& p) {
    return (p.position.x < DOMAIN_MIN_X || p.position.x > DOMAIN_MAX_X ||
            p.position.y < DOMAIN_MIN_Y || p.position.y > DOMAIN_MAX_Y);
}

bool body_in_region(const Body& p, const Vec2& region, double side_length) {
    return (p.position.x >= region.x && p.position.x <= region.x + side_length &&
            p.position.y >= region.y && p.position.y <= region.y + side_length);
}

void QuadTreeNode::insert(Body* p) {
    if (body_out_of_bound(*p)) {
        p->mass = OUT_OF_BOUND_MASS;
        if(debug)
            cout << "Body " << p->index << " is lost (out of bounds)." << endl;
        return;
    }

    // If this node is a leaf and doesn't contain a Body yet, store the Body here
    if (!is_internal && !body) {
        body = p;
        total_mass = p->mass;
        center_of_mass = p->position;
        n_bodies += 1;
    } else {
        // If this node already contains a Body, subdivide it
        if (!is_internal) {
            // Create child nodes and insert the existing Body
            subdivide();

            // Re-insert the current Body into one of the children
            if (body) {
                insert_to_child(body);
                body = nullptr;  // This node no longer holds the Body directly
            }
        }

        // Insert the new Body into one of the child nodes
        insert_to_child(p);

        // Update the mass and center of mass for this internal node
        center_of_mass = (center_of_mass * total_mass + p->position * p->mass) / (total_mass + p->mass);
        total_mass += p->mass;
        n_bodies += 1;
    }
}

// Subdivide this quadtree node into four children (NW, NE, SW, SE)
void QuadTreeNode::subdivide() {
    is_internal = true;
    double half_side = side_length / 2.0;
    sw = new QuadTreeNode(Vec2(region.x, region.y), half_side);           
    se = new QuadTreeNode(Vec2(region.x + half_side, region.y), half_side); 
    nw = new QuadTreeNode(Vec2(region.x, region.y + half_side), half_side);
    ne = new QuadTreeNode(Vec2(region.x + half_side, region.y + half_side), half_side);
}

// Insert Body into the appropriate child node
void QuadTreeNode::insert_to_child(Body* p) {
    if (body_in_region(*p, nw->region, nw->side_length)) {
        nw->insert(p);
    } else if (body_in_region(*p, ne->region, ne->side_length)) {
        ne->insert(p);
    } else if (body_in_region(*p, sw->region, sw->side_length)) {
        sw->insert(p);
    } else if (body_in_region(*p, se->region, se->side_length)) {
        se->insert(p);
    }
}

// Build the quadtree for all Bodies
QuadTreeNode build_tree(int n_bodies, Body* bodies) {
    Vec2 root_region(0, 0);
    double root_side_length = 4.0;

    QuadTreeNode root(root_region, root_side_length);

    for (int i = 0; i < n_bodies; ++i) {
        if (bodies[i].mass != OUT_OF_BOUND_MASS) {
            root.insert(&bodies[i]);
        }
    }

    return root;
}