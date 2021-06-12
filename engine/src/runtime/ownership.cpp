// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "federate.h"
#include "federation.h"
#include "ownership.h"
#include "object.h"
#include "object-class.h"
#include "utilities/logging.h"
#include <unordered_set>


namespace {
  bool tryRegisterOwnershipReport(const OwnershipMap& ownership) {
    static std::unordered_set<const void*> properties_{};
    static std::mutex mutex_{};
    std::lock_guard lock{mutex_};
    if (properties_.find(&ownership) != properties_.end()) {
      return false;
    }
    properties_.insert(&ownership);
    return true;
  }

  bool logInvalidState(OwnershipState state, OwnershipNotification notification, const char* file, int line) {
    auto error = makeString("modifyOwnershipState: invalid state %s for notification %s", state.str().c_str(), str(notification));
    log_error("OWNERSHIP", error.c_str(), makeStack(file, line).c_str(), LogLevel::Error);
    return false;
  }

  bool logInvalidState(OwnershipState state, OwnershipOperation operation, const char* file, int line) {
    auto error = makeString("modifyOwnershipState: invalid state %s for operation %s", state.str().c_str(), str(operation));
    log_error("OWNERSHIP", error.c_str(), makeStack(file, line).c_str(), LogLevel::Error);
    return false;
  }
}


bool isValidStateBeforeOperation(OwnershipState state, OwnershipOperation operation) {
  switch (operation) {
    case OwnershipOperation::None:
      return true;
    case OwnershipOperation::ForcedOwnershipAcquisition:
      return state & OwnershipStateFlag::Unowned;
    case OwnershipOperation::ForcedOwnershipDivestiture:
      return state & OwnershipStateFlag::Owned;
    case OwnershipOperation::CancelNegotiatedOwnershipDivestiture:
      return state & OwnershipStateFlag::Divesting;
    case OwnershipOperation::CancelOwnershipAcquisition:
      return state & OwnershipStateFlag::Acquiring;
    case OwnershipOperation::NegotiatedOwnershipDivestiture:
      return state & OwnershipStateFlag::NotDivesting;
    case OwnershipOperation::OwnershipAcquisition:
      return state & OwnershipStateFlag::NotAcquiring;
    case OwnershipOperation::OwnershipAcquisitionIfAvailable:
      return state & OwnershipStateFlag::NotTryingToAcquire;
    case OwnershipOperation::OwnershipReleaseFailure:
    case OwnershipOperation::OwnershipReleaseSuccess:
      return state & OwnershipStateFlag::AskedToRelease;
    case OwnershipOperation::Publish:
      return state & OwnershipStateFlag::NotAbleToAcquire;
    case OwnershipOperation::UnconditionalOwnershipDivestiture:
      return state & OwnershipStateFlag::Owned;
    case OwnershipOperation::Unpublish:
      return state & OwnershipStateFlag::Owned
          || state & OwnershipStateFlag::AbleToAcquire;
  }
}


#if ENABLE_OWNERSHIP_VALIDATION
bool validateStateBeforeOperation(OwnershipState state, OwnershipOperation operation) {
  if (isValidStateBeforeOperation(state, operation))
    return true;
  auto error = makeString("validateStateBeforeOperation failed: %s --- %s", state.str().c_str(), str(operation));
  log_error("OWNERSHIP", error.c_str(), makeStack(__FILE__, __LINE__).c_str(), LogLevel::Error);
  return false;
}
#endif


bool isValidStateAfterOperation(OwnershipState state, OwnershipOperation operation) {
  switch (operation) {
    case OwnershipOperation::None:
      return true;
    case OwnershipOperation::ForcedOwnershipAcquisition:
      return state & OwnershipStateFlag::Owned;
    case OwnershipOperation::ForcedOwnershipDivestiture:
      return state & OwnershipStateFlag::Unowned;
    case OwnershipOperation::CancelNegotiatedOwnershipDivestiture:
      return state & OwnershipStateFlag::NotDivesting;
    case OwnershipOperation::CancelOwnershipAcquisition:
      return state & OwnershipStateFlag::TryingToCancelAcquisition;
    case OwnershipOperation::NegotiatedOwnershipDivestiture:
      return state & OwnershipStateFlag::Divesting;
    case OwnershipOperation::OwnershipAcquisition:
      return state & OwnershipStateFlag::Acquiring;
    case OwnershipOperation::OwnershipAcquisitionIfAvailable:
      return state & OwnershipStateFlag::WillingToAcquire;
    case OwnershipOperation::OwnershipReleaseFailure:
      return state & OwnershipStateFlag::NotAskedToRelease;
    case OwnershipOperation::OwnershipReleaseSuccess:
      return state & OwnershipStateFlag::AbleToAcquire;
    case OwnershipOperation::Publish:
      return state & OwnershipStateFlag::Owned
          || state & OwnershipStateFlag::AbleToAcquire;
    case OwnershipOperation::UnconditionalOwnershipDivestiture:
      return state & OwnershipStateFlag::AbleToAcquire;
    case OwnershipOperation::Unpublish:
      return state & OwnershipStateFlag::NotAbleToAcquire;
  }
}


bool isValidNotification(OwnershipState state, OwnershipNotification notification) {
  switch (notification) {
    case OwnershipNotification::None:
    case OwnershipNotification::ForcedOwnershipAcquisitionNotification:
    case OwnershipNotification::ForcedOwnershipDivestitureNotification:
      return true;
    case OwnershipNotification::ConfirmOwnershipAcquisitionCancellation:
      return state & OwnershipStateFlag::TryingToCancelAcquisition;
    case OwnershipNotification::OwnershipAcquisitionNotification:
      return state & OwnershipStateFlag::AcquisitionPending
          || state & OwnershipStateFlag::WillingToAcquire;
    case OwnershipNotification::OwnershipDivestitureNotification:
      return state & OwnershipStateFlag::Divesting;
    case OwnershipNotification::OwnershipUnavailable:
      return state & OwnershipStateFlag::WillingToAcquire;
    case OwnershipNotification::RequestOwnershipAssumption:
      return state & OwnershipStateFlag::NotAcquiring
          && state & OwnershipStateFlag::NotTryingToAcquire;
    case OwnershipNotification::RequestOwnershipRelease:
      return state & OwnershipStateFlag::NotDivesting
          && state & OwnershipStateFlag::NotAskedToRelease;
  }
}


#if ENABLE_OWNERSHIP_VALIDATION
std::string validateOwnership(const OwnershipMap& ownership) {
  if (ownership.empty()) {
    return {};
  }
  int owners = 0;
  for (auto& i : ownership) {
    if (!i->masterOwnership_.first.validate()) {
      return makeString("invalid state %s", i->masterOwnership_.first.str().c_str());
    }
    if (!isValidNotification(i->masterOwnership_.first, i->masterOwnership_.second)) {
      return makeString("invalid state %s for notification %s", i->masterOwnership_.first.str().c_str(), str(i->masterOwnership_.second));
    }
    if (hasOwnership(i->masterOwnership_)) {
      ++owners;
    }
  }
  if (owners > 1) {
    return makeString("invalid state: %d owners", owners);
  }

  if (ownership.front()->getName() == Property::Destructor_str && !hasPublisher(ownership)) {
    auto objectInstance = ownership.front()->objectInstance_;
    bool deleted = objectInstance->deletedByObject_
        || objectInstance->deletedByMaster_
        || (objectInstance->masterInstance_ && objectInstance->masterInstance_->deleted_);
    if (!deleted) {
      return "invalid state: no publishers";
    }
  }
  return {};
}
#endif


#if ENABLE_OWNERSHIP_VALIDATION
void assertValidateOwnership(const OwnershipMap& ownership, const char* file, int line) {
  auto error = validateOwnership(ownership);
  if (error.empty() || !tryRegisterOwnershipReport(ownership)) {
    return;
  }
  log_error("OWNERSHIP", error.c_str(), makeStack(file, line).c_str(), LogLevel::Error);
}
#endif


bool updateOwnershipState(OwnershipStateOperation& ownership, OwnershipOperation operation) {
  if (ownership.second == OwnershipOperation::ForcedOwnershipAcquisition
      && (operation == OwnershipOperation::NegotiatedOwnershipDivestiture
      || operation == OwnershipOperation::UnconditionalOwnershipDivestiture
      || operation == OwnershipOperation::ForcedOwnershipDivestiture)) {
    ownership.first = OwnershipState{}
        + OwnershipStateFlag::Unowned
        + OwnershipStateFlag::AbleToAcquire
        + OwnershipStateFlag::NotAcquiring
        + OwnershipStateFlag::NotTryingToAcquire;
    ownership.second = OwnershipOperation::None;
    return true;
  }

  if (ownership.second == OwnershipOperation::ForcedOwnershipDivestiture
      && (operation == OwnershipOperation::OwnershipAcquisition
      || operation == OwnershipOperation::OwnershipAcquisitionIfAvailable
      || operation == OwnershipOperation::ForcedOwnershipAcquisition)) {
    ownership.first = OwnershipState{}
        + OwnershipStateFlag::Owned
        + OwnershipStateFlag::NotDivesting
        + OwnershipStateFlag::NotAskedToRelease;
    ownership.second = OwnershipOperation::None;
    return true;
  }

  switch (operation) {
    case OwnershipOperation::Publish:
      if (ownership.first & OwnershipStateFlag::NotAbleToAcquire) {
        ownership.first -= OwnershipStateFlag::NotAbleToAcquire;
      } else {
        return logInvalidState(ownership.first, operation, __FILE__, __LINE__);
      }
      ownership.first += OwnershipStateFlag::AbleToAcquire;
      ownership.first += OwnershipStateFlag::NotAcquiring;
      ownership.first += OwnershipStateFlag::NotTryingToAcquire;
      ownership.second = OwnershipOperation::Publish;
      return true;

    case OwnershipOperation::Unpublish:
      if (ownership.first & OwnershipStateFlag::NotAbleToAcquire) {
        return logInvalidState(ownership.first, operation, __FILE__, __LINE__);
      }
      ownership.first = OwnershipState{} + OwnershipStateFlag::Unowned + OwnershipStateFlag::NotAbleToAcquire;
      ownership.second = OwnershipOperation::Unpublish;
      return true;

    case OwnershipOperation::CancelNegotiatedOwnershipDivestiture:
      if (ownership.first & OwnershipStateFlag::Divesting) {
        ownership.first -= OwnershipStateFlag::Divesting;
      } else {
        return logInvalidState(ownership.first, operation, __FILE__, __LINE__);
      }
      if (ownership.second == OwnershipOperation::NegotiatedOwnershipDivestiture) {
        ownership.second = OwnershipOperation::None;
      } else {
        ownership.second = OwnershipOperation::CancelNegotiatedOwnershipDivestiture;
      }
      ownership.first += OwnershipStateFlag::NotDivesting;
      return true;

    case OwnershipOperation::CancelOwnershipAcquisition:
      if (ownership.first & OwnershipStateFlag::Acquiring) {
        ownership.first -= OwnershipStateFlag::Acquiring;
      } else {
        return logInvalidState(ownership.first, operation, __FILE__, __LINE__);
      }
      ownership.second = OwnershipOperation::CancelOwnershipAcquisition;
      ownership.first += OwnershipStateFlag::TryingToCancelAcquisition;
      return true;

    case OwnershipOperation::NegotiatedOwnershipDivestiture:
      if (ownership.first & OwnershipStateFlag::NotDivesting) {
        ownership.first -= OwnershipStateFlag::NotDivesting;
      } else {
        return logInvalidState(ownership.first, operation, __FILE__, __LINE__);
      }
      if (ownership.second == OwnershipOperation::CancelNegotiatedOwnershipDivestiture) {
        ownership.second = OwnershipOperation::None;
      } else {
        ownership.second = OwnershipOperation::NegotiatedOwnershipDivestiture;
      }
      ownership.first += OwnershipStateFlag::Divesting;
      return true;

    case OwnershipOperation::OwnershipAcquisition:
      if (ownership.first & OwnershipStateFlag::NotAcquiring) {
        ownership.first -= OwnershipStateFlag::NotAcquiring;
        ownership.first -= OwnershipStateFlag::WillingToAcquire;
      } else {
        return logInvalidState(ownership.first, operation, __FILE__, __LINE__);
      }
      ownership.second = OwnershipOperation::OwnershipAcquisition;
      ownership.first += OwnershipStateFlag::Acquiring;
      ownership.first += OwnershipStateFlag::AcquisitionPending;
      ownership.first += OwnershipStateFlag::NotTryingToAcquire;
      return true;

    case OwnershipOperation::OwnershipAcquisitionIfAvailable:
      if ((ownership.first & OwnershipStateFlag::NotTryingToAcquire) && !(ownership.first & OwnershipStateFlag::AcquisitionPending)) {
        ownership.first -= OwnershipStateFlag::NotTryingToAcquire;
      } else {
        return logInvalidState(ownership.first, operation, __FILE__, __LINE__);
      }
      ownership.first += OwnershipStateFlag::WillingToAcquire;
      ownership.second = OwnershipOperation::OwnershipAcquisitionIfAvailable;
      return true;

    case OwnershipOperation::OwnershipReleaseFailure:
      if (ownership.first & OwnershipStateFlag::AskedToRelease) {
        ownership.first -= OwnershipStateFlag::AskedToRelease;
      } else {
        return logInvalidState(ownership.first, operation, __FILE__, __LINE__);
      }
      ownership.first += OwnershipStateFlag::NotAskedToRelease;
      ownership.second = OwnershipOperation::OwnershipReleaseFailure;
      return true;

    case OwnershipOperation::OwnershipReleaseSuccess:
      if (ownership.first & OwnershipStateFlag::AskedToRelease) {
        ownership.first -= OwnershipStateFlag::AskedToRelease;
        ownership.first -= OwnershipStateFlag::Divesting;
        ownership.first -= OwnershipStateFlag::NotDivesting;
        ownership.first -= OwnershipStateFlag::Owned;
      } else {
        return logInvalidState(ownership.first, operation, __FILE__, __LINE__);
      }
      ownership.first += OwnershipStateFlag::AbleToAcquire;
      ownership.first += OwnershipStateFlag::NotAcquiring;
      ownership.first += OwnershipStateFlag::NotTryingToAcquire;
      ownership.first += OwnershipStateFlag::Unowned;
      ownership.second = OwnershipOperation::OwnershipReleaseSuccess;
      return true;

    case OwnershipOperation::UnconditionalOwnershipDivestiture:
      if (ownership.first & OwnershipStateFlag::Owned) {
        ownership.first -= OwnershipStateFlag::AskedToRelease;
        ownership.first -= OwnershipStateFlag::Divesting;
        ownership.first -= OwnershipStateFlag::NotAskedToRelease;
        ownership.first -= OwnershipStateFlag::NotDivesting;
        ownership.first -= OwnershipStateFlag::Owned;
      } else {
        return logInvalidState(ownership.first, operation, __FILE__, __LINE__);
      }
      ownership.first += OwnershipStateFlag::AbleToAcquire;
      ownership.first += OwnershipStateFlag::NotAcquiring;
      ownership.first += OwnershipStateFlag::NotTryingToAcquire;
      ownership.first += OwnershipStateFlag::Unowned;
      ownership.second = OwnershipOperation::UnconditionalOwnershipDivestiture;
      return true;

    case OwnershipOperation::ForcedOwnershipAcquisition: {
      if (!(ownership.first & OwnershipStateFlag::Unowned)) {
        return logInvalidState(ownership.first, operation, __FILE__, __LINE__);
      }
      ownership.first = OwnershipState{} + OwnershipStateFlag::Owned + OwnershipStateFlag::NotDivesting + OwnershipStateFlag::NotAskedToRelease;
      ownership.second = OwnershipOperation::ForcedOwnershipAcquisition;
      return true;
    }
    case OwnershipOperation::ForcedOwnershipDivestiture: {
      if (!(ownership.first & OwnershipStateFlag::Owned)) {
        return logInvalidState(ownership.first, operation, __FILE__, __LINE__);
      }
      ownership.first = OwnershipState{} + OwnershipStateFlag::Unowned + OwnershipStateFlag::AbleToAcquire + OwnershipStateFlag::NotAcquiring + OwnershipStateFlag::NotTryingToAcquire;
      ownership.second = OwnershipOperation::ForcedOwnershipDivestiture;
      return true;
    }

    default:
      return logInvalidState(ownership.first, operation, __FILE__, __LINE__);
  }
}


bool updateOwnershipState(OwnershipState& state, OwnershipNotification notification) {
  switch (notification) {
    case OwnershipNotification::ConfirmOwnershipAcquisitionCancellation:
      if (state & OwnershipStateFlag::TryingToCancelAcquisition) {
        state -= OwnershipStateFlag::AcquisitionPending;
        state -= OwnershipStateFlag::TryingToCancelAcquisition;
      } else {
        return logInvalidState(state, notification, __FILE__, __LINE__);
      }
      state += OwnershipStateFlag::NotAcquiring;
      return true;

    case OwnershipNotification::OwnershipAcquisitionNotification:
      if ((state & OwnershipStateFlag::AcquisitionPending) || (state & OwnershipStateFlag::WillingToAcquire)) {
        state -= OwnershipStateFlag::AbleToAcquire;
        state -= OwnershipStateFlag::Acquiring;
        state -= OwnershipStateFlag::AcquisitionPending;
        state -= OwnershipStateFlag::NotAcquiring;
        state -= OwnershipStateFlag::NotTryingToAcquire;
        state -= OwnershipStateFlag::TryingToCancelAcquisition;
        state -= OwnershipStateFlag::Unowned;
        state -= OwnershipStateFlag::WillingToAcquire;
      } else {
        return logInvalidState(state, notification, __FILE__, __LINE__);
      }
      state += OwnershipStateFlag::NotAskedToRelease;
      state += OwnershipStateFlag::NotDivesting;
      state += OwnershipStateFlag::Owned;
      return true;

    case OwnershipNotification::OwnershipDivestitureNotification:
      if (state & OwnershipStateFlag::Divesting) {
        state -= OwnershipStateFlag::Divesting;
        state -= OwnershipStateFlag::AskedToRelease;
        state -= OwnershipStateFlag::NotAskedToRelease;
        state -= OwnershipStateFlag::Owned;
      } else {
        return logInvalidState(state, notification, __FILE__, __LINE__);
      }
      state += OwnershipStateFlag::AbleToAcquire;
      state += OwnershipStateFlag::NotAcquiring;
      state += OwnershipStateFlag::NotTryingToAcquire;
      state += OwnershipStateFlag::Unowned;
      return true;

    case OwnershipNotification::OwnershipUnavailable:
      if (state & OwnershipStateFlag::WillingToAcquire) {
        state -= OwnershipStateFlag::WillingToAcquire;
      } else {
        return logInvalidState(state, notification, __FILE__, __LINE__);
      }
      state += OwnershipStateFlag::NotTryingToAcquire;
      return true;

    case OwnershipNotification::RequestOwnershipAssumption:
      // no state change
      return true;

    case OwnershipNotification::RequestOwnershipRelease:
      if (state & OwnershipStateFlag::NotAskedToRelease) {
        state -= OwnershipStateFlag::NotAskedToRelease;
      } else {
        return logInvalidState(state, notification, __FILE__, __LINE__);
      }
      state += OwnershipStateFlag::AskedToRelease;
      return true;

    case OwnershipNotification::ForcedOwnershipAcquisitionNotification:
      state = OwnershipState{} + OwnershipStateFlag::Owned + OwnershipStateFlag::NotDivesting + OwnershipStateFlag::NotAskedToRelease;
      return true;

    case OwnershipNotification::ForcedOwnershipDivestitureNotification:
      state = OwnershipState{} + OwnershipStateFlag::Unowned + OwnershipStateFlag::AbleToAcquire + OwnershipStateFlag::NotAcquiring + OwnershipStateFlag::NotTryingToAcquire;
      return true;

    default:
      return logInvalidState(state, notification, __FILE__, __LINE__);
  }
}


Property* findPotentialOwnerFederate(const OwnershipMap& ownership, OwnershipStateFlag flag) {
  for (auto& i : ownership) {
    if (i->masterOwnership_.first & flag) {
      if (i->objectInstance_ && !i->objectInstance_->objectClass_->federate_->ownershipPolicy(i->getName())) {
        continue;
      }
      return i;
    }
  }
  return nullptr;
}


Property* findOwnerFederate(const OwnershipMap& ownership) {
  for (auto& i : ownership) {
    if (hasOwnership(i->masterOwnership_)) {
      return i;
    }
  }
  return nullptr;
}


bool hasOwnership(OwnershipStateNotification value) {
  switch (value.second) {
    case OwnershipNotification::None:
      return value.first & OwnershipStateFlag::Owned;
    case OwnershipNotification::ForcedOwnershipAcquisitionNotification:
    case OwnershipNotification::OwnershipAcquisitionNotification:
    case OwnershipNotification::RequestOwnershipRelease:
      return true;
    case OwnershipNotification::ConfirmOwnershipAcquisitionCancellation:
    case OwnershipNotification::ForcedOwnershipDivestitureNotification:
    case OwnershipNotification::OwnershipDivestitureNotification:
    case OwnershipNotification::OwnershipUnavailable:
    case OwnershipNotification::RequestOwnershipAssumption:
      return false;
  }
}


bool hasPublisher(const OwnershipMap& ownership) {
  for (auto& i : ownership) {
    if (!(i->masterOwnership_.first & OwnershipStateFlag::NotAbleToAcquire)) {
      return true;
    }
  }
  return false;
}


void updateOwnershipNotifications(OwnershipMap& ownership, Property& property, OwnershipOperation operation) {
  LOG_ASSERT(property.masterOwnership_.second == OwnershipNotification::None);
  switch (operation) {
    case OwnershipOperation::OwnershipAcquisition: {
      auto owner = findOwnerFederate(ownership);
      if (!owner) {
        property.masterOwnership_.second = OwnershipNotification::OwnershipAcquisitionNotification;
      } else if (owner->masterOwnership_.first & OwnershipStateFlag::Divesting) {
        property.masterOwnership_.second = OwnershipNotification::OwnershipAcquisitionNotification;
        owner->masterOwnership_.second = OwnershipNotification::OwnershipDivestitureNotification;
      } else if (owner->masterOwnership_.first & OwnershipStateFlag::NotAskedToRelease) {
        owner->masterOwnership_.second = OwnershipNotification::RequestOwnershipRelease;
      }
      break;
    }
    case OwnershipOperation::OwnershipAcquisitionIfAvailable: {
      auto owner = findOwnerFederate(ownership);
      if (!owner) {
        property.masterOwnership_.second = OwnershipNotification::OwnershipAcquisitionNotification;
      } else if (owner->masterOwnership_.first & OwnershipStateFlag::Divesting) {
        property.masterOwnership_.second = OwnershipNotification::OwnershipAcquisitionNotification;
        owner->masterOwnership_.second = OwnershipNotification::OwnershipDivestitureNotification;
      } else {
        property.masterOwnership_.second = OwnershipNotification::OwnershipUnavailable;
      }
      break;
    }
    case OwnershipOperation::NegotiatedOwnershipDivestiture: {
      auto target = findPotentialOwnerFederate(ownership, OwnershipStateFlag::WillingToAcquire);
      if (!target) {
        target = findPotentialOwnerFederate(ownership, OwnershipStateFlag::AcquisitionPending);
      }
      if (target) {
        property.masterOwnership_.second = OwnershipNotification::OwnershipDivestitureNotification;
        target->masterOwnership_.second = OwnershipNotification::OwnershipAcquisitionNotification;
      } else {
        for (auto& i : ownership) {
          if (i != &property && i->masterOwnership_.first & OwnershipStateFlag::AbleToAcquire) {
            i->masterOwnership_.second = OwnershipNotification::RequestOwnershipAssumption;
          }
        }
      }
      break;
    }
    case OwnershipOperation::OwnershipReleaseSuccess: {
      property.masterOwnership_.second = OwnershipNotification::None;
      auto target = findPotentialOwnerFederate(ownership, OwnershipStateFlag::WillingToAcquire);
      if (!target) {
        target = findPotentialOwnerFederate(ownership, OwnershipStateFlag::AcquisitionPending);
      }
      if (target) {
        target->masterOwnership_.second = OwnershipNotification::OwnershipAcquisitionNotification;
      } else {
        for (auto& i : ownership) {
          if (i != &property && i->masterOwnership_.first & OwnershipStateFlag::AbleToAcquire) {
            i->masterOwnership_.second = OwnershipNotification::RequestOwnershipAssumption;
          }
        }
      }
      break;
    }
    case OwnershipOperation::UnconditionalOwnershipDivestiture: {
      auto target = findPotentialOwnerFederate(ownership, OwnershipStateFlag::WillingToAcquire);
      if (!target) {
        target = findPotentialOwnerFederate(ownership, OwnershipStateFlag::AcquisitionPending);
      }
      if (target) {
        target->masterOwnership_.second = OwnershipNotification::OwnershipAcquisitionNotification;
      } else {
        for (auto& i : ownership) {
          if (i != &property && i->masterOwnership_.first & OwnershipStateFlag::AbleToAcquire) {
            i->masterOwnership_.second = OwnershipNotification::RequestOwnershipAssumption;
          }
        }
      }
      break;
    }
    case OwnershipOperation::Unpublish: {
      if (property.masterOwnership_.first & OwnershipStateFlag::Owned) {
        auto target = findPotentialOwnerFederate(ownership, OwnershipStateFlag::WillingToAcquire);
        if (!target) {
          target = findPotentialOwnerFederate(ownership, OwnershipStateFlag::AcquisitionPending);
        }
        if (target) {
          target->masterOwnership_.second = OwnershipNotification::OwnershipAcquisitionNotification;
        } else {
          for (auto& i : ownership) {
            if (i != &property && i->masterOwnership_.first & OwnershipStateFlag::AbleToAcquire) {
              i->masterOwnership_.second = OwnershipNotification::RequestOwnershipAssumption;
            }
          }
        }
      }
      break;
    }
    case OwnershipOperation::ForcedOwnershipAcquisition: {
      property.masterOwnership_.second = OwnershipNotification::ForcedOwnershipAcquisitionNotification;
      for (auto& i : ownership) {
        if (i != &property) {
          if (i->masterOwnership_.first & OwnershipStateFlag::Owned) {
            i->masterOwnership_.second = OwnershipNotification::ForcedOwnershipDivestitureNotification;
          } else if (i->masterOwnership_.second == OwnershipNotification::ForcedOwnershipAcquisitionNotification
              || i->masterOwnership_.second == OwnershipNotification::OwnershipAcquisitionNotification) {
            i->masterOwnership_.second = OwnershipNotification::None;
          }
        }
      }
      break;
    }
    case OwnershipOperation::ForcedOwnershipDivestiture: {
      property.masterOwnership_.second = OwnershipNotification::ForcedOwnershipDivestitureNotification;
      auto target = findPotentialOwnerFederate(ownership, OwnershipStateFlag::WillingToAcquire);
      if (!target) {
        target = findPotentialOwnerFederate(ownership, OwnershipStateFlag::AcquisitionPending);
      }
      if (target) {
        target->masterOwnership_.second = OwnershipNotification::OwnershipAcquisitionNotification;
      } else {
        for (auto& i : ownership) {
          if (i != &property && i->masterOwnership_.first & OwnershipStateFlag::AbleToAcquire) {
            i->masterOwnership_.second = OwnershipNotification::RequestOwnershipAssumption;
          }
        }
      }
      break;
    }
    default:
      break;
  }
}


void beforeUpdateOwnership(OwnershipMap& ownership) {
  for (auto& property : ownership) {
    property->masterOwnershipBefore_ = property->masterOwnership_;
  }
}


void afterUpdateOwnership(OwnershipMap& ownership, Property& property, OwnershipOperation operation, const char* file, int line) {
  auto error = validateOwnership(ownership);
  if (error.empty() || !tryRegisterOwnershipReport(ownership)) {
    return;
  }

  std::ostringstream os{};
  os << property.getName() << ":" << str(operation) << ":";

  for (auto p : ownership) {
    os << " " << p->masterOwnershipBefore_.first.str() << ":" << str(p->masterOwnershipBefore_.second)
        << (p == &property ? "*" : ">")
        << p->masterOwnership_.first.str() << ":" << str(p->masterOwnership_.second);
  }

  os << '\n' << error;

  log_error("OWNERSHIP", os.str().c_str(), makeStack(file, line).c_str(), LogLevel::Error);
}


const char* str(OwnershipNotification value) {
  switch (value) {
    case OwnershipNotification::None:
      return "-";
    case OwnershipNotification::ConfirmOwnershipAcquisitionCancellation:
      return "ConfirmOwnershipAcquisitionCancellation";
    case OwnershipNotification::OwnershipAcquisitionNotification:
      return "OwnershipAcquisitionNotification";
    case OwnershipNotification::OwnershipDivestitureNotification:
      return "OwnershipDivestitureNotification";
    case OwnershipNotification::OwnershipUnavailable:
      return "OwnershipUnavailable";
    case OwnershipNotification::RequestOwnershipAssumption:
      return "RequestOwnershipAssumption";
    case OwnershipNotification::RequestOwnershipRelease:
      return "RequestOwnershipRelease";
    case OwnershipNotification::ForcedOwnershipAcquisitionNotification:
      return "ForcedOwnershipAcquisitionNotification";
    case OwnershipNotification::ForcedOwnershipDivestitureNotification:
      return "ForcedOwnershipDivestitureNotification";
    default:
      return "?";
  }
}


const char* str(OwnershipOperation value) {
  switch (value) {
    case OwnershipOperation::None:
      return "-";
    case OwnershipOperation::CancelNegotiatedOwnershipDivestiture:
      return "CancelNegotiatedOwnershipDivestiture";
    case OwnershipOperation::CancelOwnershipAcquisition:
      return "CancelOwnershipAcquisition";
    case OwnershipOperation::ForcedOwnershipAcquisition:
      return "ForcedOwnershipAcquisition";
    case OwnershipOperation::ForcedOwnershipDivestiture:
      return "ForcedOwnershipDivestiture";
    case OwnershipOperation::NegotiatedOwnershipDivestiture:
      return "NegotiatedOwnershipDivestiture";
    case OwnershipOperation::OwnershipAcquisition:
      return "OwnershipAcquisition";
    case OwnershipOperation::OwnershipAcquisitionIfAvailable:
      return "OwnershipAcquisitionIfAvailable";
    case OwnershipOperation::OwnershipReleaseFailure:
      return "OwnershipReleaseFailure";
    case OwnershipOperation::OwnershipReleaseSuccess:
      return "OwnershipReleaseSuccess";
    case OwnershipOperation::Publish:
      return "publish";
    case OwnershipOperation::UnconditionalOwnershipDivestiture:
      return "UnconditionalOwnershipDivestiture";
    case OwnershipOperation::Unpublish:
      return "Unpublish";
    default:
      return "?";
  }
}


std::string OwnershipState::str() const {
  std::ostringstream s;

  if (*this & OwnershipStateFlag::Owned) s << "|Owned";
  if (*this & OwnershipStateFlag::Unowned) s << "|Unowned";
  if (*this & OwnershipStateFlag::Divesting) s << "|Divesting";
  if (*this & OwnershipStateFlag::NotDivesting) s << "|NotDivesting";
  if (*this & OwnershipStateFlag::AskedToRelease) s << "|AskedToRelease";
  if (*this & OwnershipStateFlag::NotAskedToRelease) s << "|NotAskedToRelease";
  if (*this & OwnershipStateFlag::AbleToAcquire) s << "|AbleToAcquire";
  if (*this & OwnershipStateFlag::NotAbleToAcquire) s << "|NotAbleToAcquire";
  if (*this & OwnershipStateFlag::AcquisitionPending) s << "|AcquisitionPending";
  if (*this & OwnershipStateFlag::NotAcquiring) s << "|NotAcquiring";
  if (*this & OwnershipStateFlag::Acquiring) s << "|Acquiring";
  if (*this & OwnershipStateFlag::TryingToCancelAcquisition) s << "|TryingToCancelAcquisition";
  if (*this & OwnershipStateFlag::WillingToAcquire) s << "|WillingToAcquire";
  if (*this & OwnershipStateFlag::NotTryingToAcquire) s << "|NotTryingToAcquire";

  std::string str = s.str();
  return str.empty() ? str : str.substr(1);
}


#if ENABLE_OWNERSHIP_VALIDATION

static bool validate_xor(OwnershipState state, OwnershipStateFlag condition, OwnershipStateFlag flag1, OwnershipStateFlag flag2) {
  return (state & condition) ? (state & flag1) != (state & flag2) : !(state & flag1) && !(state & flag2);
}

static bool validate_or(OwnershipState state, OwnershipStateFlag condition, OwnershipStateFlag flag1, OwnershipStateFlag flag2) {
  return (state & condition) ? (state & flag1) || (state & flag2) : true;
}


bool OwnershipState::validate() const {
  auto& s = *this;
  return (s & OwnershipStateFlag::Owned) != (s & OwnershipStateFlag::Unowned)
      && validate_xor(s, OwnershipStateFlag::Owned, OwnershipStateFlag::NotDivesting, OwnershipStateFlag::Divesting)
      && validate_xor(s, OwnershipStateFlag::Owned, OwnershipStateFlag::NotAskedToRelease, OwnershipStateFlag::AskedToRelease)
      && validate_xor(s, OwnershipStateFlag::Unowned, OwnershipStateFlag::NotAbleToAcquire, OwnershipStateFlag::AbleToAcquire)
      && validate_xor(s, OwnershipStateFlag::AbleToAcquire, OwnershipStateFlag::NotAcquiring, OwnershipStateFlag::AcquisitionPending)
      && validate_xor(s, OwnershipStateFlag::AbleToAcquire, OwnershipStateFlag::NotTryingToAcquire, OwnershipStateFlag::WillingToAcquire)
      && validate_or(s, OwnershipStateFlag::AbleToAcquire, OwnershipStateFlag::NotAcquiring, OwnershipStateFlag::NotTryingToAcquire);
}

#endif
