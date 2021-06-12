// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./velocity-sampler.h"


VelocitySampler::VelocitySampler() :
    duration_(1) {
}


void VelocitySampler::clear() {
    samples_.clear();
}


void VelocitySampler::add(double time, glm::vec2 value) {
    if (!samples_.empty()) {
        erase_samples_before(samples_.back().first - duration_);
    }

    if (!samples_.empty() && time == samples_.back().first) {
        samples_.back().second = (samples_.back().second + value) / 2.0f;
    } else {
        samples_.emplace_back(time, value);
    }
}


glm::vec2 VelocitySampler::get(double time) const {
    if (samples_.empty())
        return glm::vec2();

    if (time < samples_.front().first)
        return samples_.front().second;

    auto i2 = samples_.begin() + 1;
    while (i2 != samples_.end() && time > i2->first) {
        ++i2;
    }

    if (i2 == samples_.end())
        return samples_.back().second;

    auto i1 = i2 - 1;

    auto i0 = i1;
    if (i0 != samples_.begin()) {
        --i0;
    }

    auto i3 = i2;
    if (i3 != samples_.end() - 1) {
        ++i3;
    }

    double t1 = i1->first;
    double t2 = i2->first;

    auto p0 = i0->second;
    auto p1 = i1->second;
    auto p2 = i2->second;
    auto p3 = i3->second;

    if (t1 == t2)
        return (p1 + p2) / 2.0f;

    float mu = (float)((time - t1) / (t2 - t1));
    float mu2 = mu * mu;
    float mu3 = mu * mu2;

    glm::vec2 a0 = p3 - p2 - p0 + p1;
    glm::vec2 a1 = p0 - p1 - a0;
    glm::vec2 a2 = p2 - p0;
    glm::vec2 a3 = p1;

    return a0 * mu3 + a1 * mu2 + a2 * mu + a3;
}


void VelocitySampler::erase_samples_before(double time) {
    auto i = samples_.begin();
    while (i != samples_.end() && i->first < time) {
        ++i;
    }
    samples_.erase(samples_.begin(), i);
}
