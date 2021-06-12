// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./convert-value.h"


std::vector<glm::vec2> DecodeArrayVec2(const Value& value) {
    std::vector<glm::vec2> r{};
    for (auto& element : value)
        r.push_back(element._vec2());
    return r;
}


std::array<float, 25> DecodeRangeValues(const Value& value) {
    std::array<float, 25> r{};
    std::size_t i = 0;
    for (auto& element : value) {
        if (i == 25)
            break;
        r[i++] = element._float();
    }
    return r;
}


Value ProjectileToBson(const std::vector<BattleSM::Projectile>& value) {
    auto arr = Struct{} << "" << Array{};
    for (auto& projectile : value) {
        arr = std::move(arr) << Struct{}
            << "position1" << projectile.position1
            << "position2" << projectile.position2
            << "delay" << projectile.delay
            << ValueEnd{};
    }
    return *(std::move(arr) << ValueEnd{} << ValueEnd{}).begin();
}


std::vector<BattleSM::Projectile> ProjectileFromBson(const Value& value) {
    std::vector<BattleSM::Projectile> result{};
    for (auto& projectile : value) {
        result.emplace_back(
            projectile["position1"_vec2],
            projectile["position2"_vec2],
            projectile["delay"_float]
        );
    }
    return result;
}


Value FormationToBson(const BattleSM::Formation& value) {
    return Struct{}
        << "rankDistance" << value.rankDistance
        << "fileDistance" << value.fileDistance
        << "numberOfRanks" << value.numberOfRanks
        << "numberOfFiles" << value.numberOfFiles
        << "direction" << value._direction
        << "towardRight" << value.towardRight
        << "towardBack" << value.towardBack
        << ValueEnd{};
}


BattleSM::Formation FormationFromBson(const Value& value) {
    BattleSM::Formation result;
    result.rankDistance = value["rankDistance"_float];
    result.fileDistance = value["fileDistance"_float];
    result.numberOfRanks = value["numberOfRanks"_int];
    result.numberOfFiles = value["numberOfFiles"_int];
    result._direction = value["direction"_float];
    result.towardRight = value["towardRight"_vec2];
    result.towardBack = value["towardBack"_vec2];
    return result;
}

