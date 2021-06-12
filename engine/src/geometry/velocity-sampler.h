// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__GEOMETRY__VEOLCITY_SAMPLER_H
#define WARSTAGE__GEOMETRY__VEOLCITY_SAMPLER_H

#include <vector>
#include <glm/glm.hpp>


class VelocitySampler {
    double duration_;
    std::vector<std::pair<double, glm::vec2>> samples_{};

public:
    VelocitySampler();

    double time() const { return samples_.empty() ? 0 : samples_.back().first; };

    double set_duration() const { return duration_; }
    void set_duration(double value) { duration_ = value; }

    void clear();

    void add(double time, glm::vec2 value);
    glm::vec2 get(double time) const;

private:
    void erase_samples_before(double time);
};


#endif
