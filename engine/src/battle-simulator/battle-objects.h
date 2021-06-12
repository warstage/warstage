// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__BATTLE_SIMULATOR__BATTLE_OBJECTS_H
#define WARSTAGE__BATTLE_SIMULATOR__BATTLE_OBJECTS_H

#include "battle-model/battle-sm.h"


void UpdateUnitOrdersPath(std::vector<glm::vec2>& path, glm::vec2 center, BattleSM::Unit* meleeTarget);
void UpdateMovementPathStart(std::vector<glm::vec2>& path, glm::vec2 startPosition, float spacing);
void UpdateMovementPath(std::vector<glm::vec2>& path, glm::vec2 startPosition, glm::vec2 endPosition, float spacing);
float MovementPathLength(const std::vector<glm::vec2>& path);
bool IsForwardMotion(const std::vector<glm::vec2>& path, glm::vec2 position);


#endif
