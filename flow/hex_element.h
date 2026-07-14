#ifndef HEX_ELEMENT_H
#define HEX_ELEMENT_H

#include "hex_grid.h"
#include <vector>
#include <typeindex>

// Abstract HexElement Base Class for hexagonal grid physics processes
class HexElement {
public:
    virtual ~HexElement() = default;
    virtual void step(HexGrid& g, float dt, int step, int total_steps) = 0;

    // Returns the sparse/allocated fields required by this element
    virtual std::vector<std::type_index> get_required_fields() const {
        return {};
    }
};

#endif // HEX_ELEMENT_H
