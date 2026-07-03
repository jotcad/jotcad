#ifndef ELEMENT_H
#define ELEMENT_H

#include "grid.h"

// Abstract Element Base Class
class Element {
public:
    virtual ~Element() = default;
    virtual void step(Grid& g, float dt, int step, int total_steps) = 0;
};

#endif // ELEMENT_H
