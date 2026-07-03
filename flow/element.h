#ifndef ELEMENT_H
#define ELEMENT_H

#include "grid.h"
#include <vector>
#include <typeindex>

// Abstract Element Base Class
class Element {
public:
    virtual ~Element() = default;
    virtual void step(Grid& g, float dt, int step, int total_steps) = 0;

    // Returns the sparse/allocated fields required by this element
    virtual std::vector<std::type_index> get_required_fields() const {
        return {};
    }
};

#endif // ELEMENT_H
