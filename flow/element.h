#ifndef ELEMENT_H
#define ELEMENT_H

#include "grid.h"
#include <vector>

// Abstract Element Base Class
class Element {
public:
    virtual ~Element() = default;
    virtual void step(Grid& g, float dt, int step, int total_steps) = 0;

    // Returns the sparse/allocated fields required by this element
    virtual std::vector<FieldType> get_required_fields() const {
        return {};
    }
};

#endif // ELEMENT_H
