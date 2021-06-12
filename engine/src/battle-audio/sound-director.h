// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WASTAGE__BATTLE_AUDIO__SOUND_DIRECTOR_H
#define WASTAGE__BATTLE_AUDIO__SOUND_DIRECTOR_H

#include "async/strand.h"
#include <memory>

class Federate;

#define NUMBER_OF_SOUND_CHANNELS (static_cast<int>(SoundChannelID::NumberOfSoundChannels))
#define NUMBER_OF_SOUND_SAMPLES (static_cast<int>(SoundSampleID::NumberOfSoundSamples))

enum class SoundChannelID : int {
  UserInterface,
  Background,
  Casualty,
  CavalryWalking,
  CavalryRunning,
  Horse,
  InfantryWalking,
  InfantryRunning,
  MeleeCavalry,
  MeleeCharging,
  MeleeInfantry,
  MissileArrows,
  MissileArrows2,
  MissileArrows3,
  MissileGuns,
  MissileGuns2,
  MissileGuns3,
  MissileImpact,
  Sword,
  NumberOfSoundChannels
};


enum class SoundSampleID : int {
  Background,
  Casualty1,
  Casualty2,
  Casualty3,
  Casualty4,
  Casualty5,
  Casualty6,
  Casualty7,
  Casualty8,
  CavalryRunning,
  CavalryWalking,
  HorseNeigh1,
  HorseNeigh2,
  HorseNeigh3,
  HorseSnort,
  InfantryRunning,
  InfantryWalking,
  MeleeCavalry,
  MeleeCharging,
  MeleeInfantry,
  MissileArrows,
  MissileCannon1,
  MissileImpact1,
  MissileImpact2,
  MissileImpact3,
  MissileImpact4,
  MissileMatchlock1,
  MissileMatchlock2,
  MissileMatchlock3,
  MissileMatchlock4,
  Sword1,
  Sword2,
  Sword3,
  Sword4,
  TapActivate,
  TapDeactivate,
  TapSelect,
  TapSelectMarker,
  TapMovement,
  TapMovementDone,
  TapCharge,
  TapTarget,
  NumberOfSoundSamples
};

enum class SoundCookieID : int {
  None
};


class SoundDirector : public std::enable_shared_from_this<SoundDirector> {
  std::chrono::system_clock::time_point casualtyTimer_{};

    bool infantryWalking_{};
    bool infantryRunning_{};
    bool cavalryWalking_{};
    bool cavalryRunning_{};

    bool meleeCavalry_{};
    bool meleeInfantry_{};
    int cavalryCount_{};
    double horseTimer_{};
    double swordTimer_{};
    bool meleeCharging_{};
    std::chrono::system_clock::time_point meleeChargeTimer_{};

    std::shared_ptr<Federate> systemFederate_;

public:
    explicit SoundDirector(std::shared_ptr<Federate> systemFederate);

    SoundDirector(const SoundDirector&) = delete;
    SoundDirector& operator=(const SoundDirector&) = delete;

    void PlaySound(SoundSampleID sample, bool loop, SoundCookieID cookie = SoundCookieID::None);
    void StopSound(SoundChannelID channel);
    void StopSound(SoundChannelID channel, SoundCookieID cookie);
    void StopAll();

    void Tick(double secondsSinceLastTick);

    void PlayBackground();

    void UpdateInfantryWalking(bool value);
    void UpdateInfantryRunning(bool value);
    void UpdateCavalryWalking(bool value);
    void UpdateCavalryRunning(bool value);
    void UpdateCavalryCount(int value);

    void UpdateMeleeCavalry(bool value);
    void UpdateMeleeInfantry(bool value);
    void UpdateMeleeCharging();

    void PlayMissileArrows(SoundCookieID cookie);
    void PlayMissileImpact();
    void PlayMissileMatchlock();
    void PlayMissileCannon();
    void PlayCasualty();
    void PlayUserInterfaceSound(SoundSampleID soundSampleID);

private:
    void TickHorse(double secondsSinceLastTick);
    void TickSword(double secondsSinceLastTick);

    SoundSampleID RandomCasualtySample() const;
    SoundSampleID RandomHorseSample() const;
    SoundSampleID RandomMissileImpactSample() const;
    SoundSampleID RandomMatchlockSample() const;
    SoundSampleID RandomSwordSample() const;
};


#endif
