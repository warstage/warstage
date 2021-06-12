// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./battle-objects.h"


void UpdateUnitOrdersPath(std::vector<glm::vec2>& path, glm::vec2 center, BattleSM::Unit* meleeTarget) {
    if (meleeTarget) {
        UpdateMovementPath(path, center, meleeTarget->state.formation.center, 10);
    } else if (path.empty()) {
        path.push_back(center);
    } else {
        UpdateMovementPath(path, center, path.back(), 10);
    }
}


void UpdateMovementPathStart(std::vector<glm::vec2>& path, glm::vec2 startPosition, float spacing) {
    while (!path.empty() && glm::distance(path.front(), startPosition) < spacing)
        path.erase(path.begin());

    path.insert(path.begin(), startPosition);
}


void UpdateMovementPath(std::vector<glm::vec2>& path, glm::vec2 startPosition, glm::vec2 endPosition, float spacing) {
    while (!path.empty() && glm::distance(path.front(), startPosition) < spacing)
        path.erase(path.begin());

    while (!path.empty() && glm::distance(path.back(), endPosition) < spacing)
        path.erase(path.end() - 1);

    while (!IsForwardMotion(path, endPosition))
        path.pop_back();

    path.insert(path.begin(), startPosition);

    int n = 20;
    glm::vec2 p = path.back();
    while (n-- != 0 && glm::length(p - endPosition) > 2 * spacing) {
        p += spacing * glm::normalize(endPosition - p);
        path.push_back(p);
    }

    if (glm::length(p - endPosition) > spacing) {
        path.push_back(0.5f * (p + endPosition));
    }

    path.push_back(endPosition);
}


float MovementPathLength(const std::vector<glm::vec2>& path) {
    float result = 0;

    for (auto i = path.begin() + 1; i < path.end(); ++i)
        result += glm::distance(*(i - 1), *i);

    return result;
}


bool IsForwardMotion(const std::vector<glm::vec2>& path, glm::vec2 position) {
    if (path.size() < 2)
        return true;

    glm::vec2 last = *(path.end() - 1);
    glm::vec2 prev = *(path.end() - 2);
    glm::vec2 next = last + (last - prev);

    return glm::length(position - next) < glm::length(position - prev);
}


/***/


void BattleSM::Formation::SetDirection(float direction) {
    _direction = direction;
    float sin = glm::sin(direction);
    float cos = glm::cos(direction);
    towardRight = glm::vec2(sin, -cos) * fileDistance;
    towardBack = glm::vec2(-cos, -sin) * rankDistance;
}
