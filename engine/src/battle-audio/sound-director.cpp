// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./sound-director.h"
#include "runtime/runtime.h"
#include <cstdlib>


static double Random(double min, double max) {
    return min + (max - min) * ((std::rand() & 0x7fff) / (double)0x7fff);
}


SoundDirector::SoundDirector(std::shared_ptr<Federate> systemFederate) :
    systemFederate_{std::move(systemFederate)}
{
}


void SoundDirector::PlaySound(SoundSampleID sample, bool loop, SoundCookieID cookie) {
  systemFederate_->getServiceClass("PlaySound").request(Struct{}
      << "sample" << static_cast<int>(sample)
      << "loop" << loop
      << "cookie" << static_cast<int>(cookie)
      << ValueEnd{}).then<void>([](const auto&) {}, [](const auto& reason) {
        LOG_REJECTION(reason);
    }).done();
}


void SoundDirector::StopSound(SoundChannelID channel) {
  systemFederate_->getServiceClass("StopSound").request(Struct{}
      << "channel" << static_cast<int>(channel)
      << ValueEnd{}).then<void>([](const auto&) {}, [](const auto& reason) {
        LOG_REJECTION(reason);
    }).done();
}


void SoundDirector::StopSound(SoundChannelID channel, SoundCookieID cookie) {
  systemFederate_->getServiceClass("StopSound").request(Struct{}
      << "channel" << static_cast<int>(channel)
      << "cookie" << static_cast<int>(cookie)
      << ValueEnd{}).then<void>([](const auto&) {}, [](const auto& reason) {
        LOG_REJECTION(reason);
    }).done();
}


void SoundDirector::StopAll() {
    LOG_ASSERT(Strand::getMain()->isCurrent());
    infantryWalking_ = false;
    infantryRunning_ = false;
    cavalryWalking_ = false;
    cavalryRunning_ = false;
    cavalryCount_ = 0;
    meleeInfantry_ = false;
    meleeCavalry_ = false;

    for (int i = 0; i < NUMBER_OF_SOUND_CHANNELS; ++i) {
        StopSound((SoundChannelID)i);
    }
}


void SoundDirector::Tick(double secondsSinceLastTick) {
    LOG_ASSERT(Strand::getMain()->isCurrent());

    TickHorse(secondsSinceLastTick);
    TickSword(secondsSinceLastTick);
}


void SoundDirector::TickHorse(double secondsSinceLastTick) {
    horseTimer_ -= secondsSinceLastTick;
    if (horseTimer_ < 0) {
        if (cavalryCount_ != 0)
            PlaySound(RandomHorseSample(), false);

        horseTimer_ = Random(8.0, 16.0);
    }
}


void SoundDirector::TickSword(double secondsSinceLastTick) {
    swordTimer_ -= secondsSinceLastTick;
    if (swordTimer_ < 0) {
        if (meleeInfantry_ || meleeCavalry_)
            PlaySound(RandomSwordSample(), false);
        swordTimer_ = Random(1.0, 3.0);
    }
}


void SoundDirector::PlayBackground() {
    LOG_ASSERT(Strand::getMain()->isCurrent());
    PlaySound(SoundSampleID::Background, true);
}


void SoundDirector::UpdateInfantryWalking(bool value) {
    LOG_ASSERT(Strand::getMain()->isCurrent());
    if (value && !infantryWalking_)
        PlaySound(SoundSampleID::InfantryWalking, true);
    else if (!value && infantryWalking_)
        StopSound(SoundChannelID::InfantryWalking);
    infantryWalking_ = value;
}


void SoundDirector::UpdateInfantryRunning(bool value) {
    LOG_ASSERT(Strand::getMain()->isCurrent());
    if (value && !infantryRunning_)
        PlaySound(SoundSampleID::InfantryRunning, true);
    else if (!value && infantryRunning_)
        StopSound(SoundChannelID::InfantryRunning);
    infantryRunning_ = value;
}


void SoundDirector::UpdateCavalryWalking(bool value) {
    LOG_ASSERT(Strand::getMain()->isCurrent());
    if (value && !cavalryWalking_)
        PlaySound(SoundSampleID::CavalryWalking, true);
    else if (!value && cavalryWalking_)
        StopSound(SoundChannelID::CavalryWalking);
    cavalryWalking_ = value;
}


void SoundDirector::UpdateCavalryRunning(bool value) {
    LOG_ASSERT(Strand::getMain()->isCurrent());
    if (value && !cavalryRunning_)
        PlaySound(SoundSampleID::CavalryRunning, true);
    else if (!value && cavalryRunning_)
        StopSound(SoundChannelID::CavalryRunning);
    cavalryRunning_ = value;
}


void SoundDirector::UpdateCavalryCount(int value) {
    LOG_ASSERT(Strand::getMain()->isCurrent());
    cavalryCount_ = value;
}


void SoundDirector::UpdateMeleeCavalry(bool value) {
    LOG_ASSERT(Strand::getMain()->isCurrent());
    if (value && !meleeCavalry_)
        PlaySound(SoundSampleID::MeleeCavalry, true);
    else if (!value && meleeCavalry_)
        StopSound(SoundChannelID::MeleeCavalry);
    meleeCavalry_ = value;
}


void SoundDirector::UpdateMeleeInfantry(bool value) {
    LOG_ASSERT(Strand::getMain()->isCurrent());
    if (value && !meleeInfantry_)
        PlaySound(SoundSampleID::MeleeInfantry, true);
    else if (!value && meleeInfantry_)
        StopSound(SoundChannelID::MeleeInfantry);
    meleeInfantry_ = value;
}


void SoundDirector::UpdateMeleeCharging() {
    LOG_ASSERT(Strand::getMain()->isCurrent());
    bool isMelee = meleeCavalry_ || meleeInfantry_;
    if (!meleeCharging_ && isMelee) {
        if (std::chrono::system_clock::now() > meleeChargeTimer_) {
            PlaySound(SoundSampleID::MeleeCharging, false);
            meleeCharging_ = true;
        }
    } else if (meleeCharging_ && !isMelee) {
        meleeCharging_ = false;
        meleeChargeTimer_ = std::chrono::system_clock::now() + std::chrono::seconds(15);
    }
}


void SoundDirector::PlayMissileArrows(SoundCookieID cookie) {
    LOG_ASSERT(Strand::getMain()->isCurrent());
    PlaySound(SoundSampleID::MissileArrows, false, cookie);
}


void SoundDirector::PlayMissileImpact() {
    LOG_ASSERT(Strand::getMain()->isCurrent());
    PlaySound(RandomMissileImpactSample(), false);
}


void SoundDirector::PlayMissileMatchlock() {
    LOG_ASSERT(Strand::getMain()->isCurrent());
    SoundSampleID soundSample = RandomMatchlockSample();
    PlaySound(soundSample, false);
}


void SoundDirector::PlayMissileCannon() {
    LOG_ASSERT(Strand::getMain()->isCurrent());
    PlaySound(SoundSampleID::MissileCannon1, false);
}


void SoundDirector::PlayCasualty() {
    LOG_ASSERT(Strand::getMain()->isCurrent());
    auto now = std::chrono::system_clock::now();
    if (now > casualtyTimer_) {
        SoundSampleID soundSample = RandomCasualtySample();
        PlaySound(soundSample, false);
        casualtyTimer_ = now + std::chrono::seconds(2);
    }
}


void SoundDirector::PlayUserInterfaceSound(SoundSampleID soundSampleID) {
    LOG_ASSERT(Strand::getMain()->isCurrent());
    PlaySound(soundSampleID, false);
}


SoundSampleID SoundDirector::RandomCasualtySample() const {
    switch (std::rand() & 7) {
        case 0:
            return SoundSampleID::Casualty1;
        case 1:
            return SoundSampleID::Casualty2;
        case 2:
            return SoundSampleID::Casualty3;
        case 3:
            return SoundSampleID::Casualty4;
        case 4:
            return SoundSampleID::Casualty5;
        case 5:
            return SoundSampleID::Casualty6;
        case 6:
            return SoundSampleID::Casualty7;
        default:
            return SoundSampleID::Casualty8;
    }
}


SoundSampleID SoundDirector::RandomHorseSample() const {
    switch (std::rand() & 3) {
        case 0:
            return SoundSampleID::HorseNeigh1;
        case 1:
            return SoundSampleID::HorseNeigh2;
        case 2:
            return SoundSampleID::HorseNeigh3;
        default:
            return SoundSampleID::HorseSnort;
    }
}


SoundSampleID SoundDirector::RandomMissileImpactSample() const {
    switch (std::rand() & 3) {
        case 0:
            return SoundSampleID::MissileImpact1;
        case 1:
            return SoundSampleID::MissileImpact2;
        case 2:
            return SoundSampleID::MissileImpact3;
        default:
            return SoundSampleID::MissileImpact4;
    }
}


SoundSampleID SoundDirector::RandomMatchlockSample() const {
    switch (std::rand() & 3) {
        case 0:
            return SoundSampleID::MissileMatchlock1;
        case 1:
            return SoundSampleID::MissileMatchlock2;
        case 2:
            return SoundSampleID::MissileMatchlock3;
        default:
            return SoundSampleID::MissileMatchlock4;
    }
}


SoundSampleID SoundDirector::RandomSwordSample() const {
    switch (std::rand() & 3) {
        case 0:
            return SoundSampleID::Sword1;
        case 1:
            return SoundSampleID::Sword2;
        case 2:
            return SoundSampleID::Sword3;
        default:
            return SoundSampleID::Sword4;
    }
}
