// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__RUNTIME__OWNERSHIP_H
#define WARSTAGE__RUNTIME__OWNERSHIP_H

#include <unordered_map>
#include <sstream>
#include <string>
#include <vector>

class Federate;
class Property;

#define ENABLE_OWNERSHIP_VALIDATION 1


enum class OwnershipNotification {
  None,
  ConfirmOwnershipAcquisitionCancellation,
  ForcedOwnershipAcquisitionNotification,
  ForcedOwnershipDivestitureNotification,
  OwnershipAcquisitionNotification,
  OwnershipDivestitureNotification,
  OwnershipUnavailable,
  RequestOwnershipAssumption,
  RequestOwnershipRelease
};


enum class OwnershipOperation {
  None,
  CancelNegotiatedOwnershipDivestiture,
  CancelOwnershipAcquisition,
  ForcedOwnershipAcquisition,
  ForcedOwnershipDivestiture,
  NegotiatedOwnershipDivestiture,
  OwnershipAcquisition,
  OwnershipAcquisitionIfAvailable,
  OwnershipReleaseFailure,
  OwnershipReleaseSuccess,
  Publish,
  UnconditionalOwnershipDivestiture,
  Unpublish
};


enum class OwnershipStateFlag : unsigned int {
  AbleToAcquire               = 0x0001u,
  Acquiring                   = 0x0002u,
  AcquisitionPending          = 0x0004u,
  AskedToRelease              = 0x0008u,
  Divesting                   = 0x0010u,
  NotAbleToAcquire            = 0x0020u,
  NotAcquiring                = 0x0040u,
  NotAskedToRelease           = 0x0080u,
  NotDivesting                = 0x0100u,
  NotTryingToAcquire          = 0x0200u,
  Owned                       = 0x0400u,
  TryingToCancelAcquisition   = 0x0800u,
  Unowned                     = 0x1000u,
  WillingToAcquire            = 0x2000u
};

/*constexpr unsigned int operator|(unsigned int s, OwnershipStateFlag f) {
    return s | static_cast<unsigned int>(f);
}*/


class OwnershipState {
  unsigned int value_;

public:
  constexpr explicit OwnershipState(unsigned int state = 0u) : value_{state} {}
  constexpr explicit operator unsigned int() const { return static_cast<unsigned int>(value_); }

  [[nodiscard]] std::string str() const;
#if ENABLE_OWNERSHIP_VALIDATION
  [[nodiscard]] bool validate() const;
#else
  [[nodiscard]] bool validate() const { return true; }
#endif

  [[nodiscard]] constexpr bool operator==(OwnershipState other) const {
    return value_ == other.value_;
  }

  [[nodiscard]] constexpr bool operator!=(OwnershipState other) const {
    return value_ != other.value_;
  }

  [[nodiscard]] constexpr bool operator&(OwnershipStateFlag flag) const {
    return (value_ & static_cast<unsigned int>(flag)) != 0;
  }

  [[nodiscard]] constexpr OwnershipState operator+(OwnershipStateFlag flag) const {
    return OwnershipState{value_ | static_cast<unsigned int>(flag)};
  }

  [[nodiscard]] constexpr OwnershipState operator-(OwnershipStateFlag flag) const {
    return OwnershipState{value_ & ~static_cast<unsigned int>(flag)};
  }

  OwnershipState& operator+=(OwnershipStateFlag flag) {
    value_ |= static_cast<unsigned int>(flag);
    return *this;
  }

  OwnershipState& operator-=(OwnershipStateFlag flag) {
    value_ &= ~static_cast<unsigned int>(flag);
    return *this;
  }
};


using OwnershipStateNotification = std::pair<OwnershipState, OwnershipNotification>;
using OwnershipStateOperation = std::pair<OwnershipState, OwnershipOperation>;
using OwnershipMap = std::vector<Property*>;

bool isValidStateBeforeOperation(OwnershipState state, OwnershipOperation operation);
bool isValidStateAfterOperation(OwnershipState state, OwnershipOperation operation);
bool isValidNotification(OwnershipState state, OwnershipNotification notification);

#if ENABLE_OWNERSHIP_VALIDATION
bool validateStateBeforeOperation(OwnershipState state, OwnershipOperation operation);
std::string validateOwnership(const OwnershipMap& ownership);
void assertValidateOwnership(const OwnershipMap& ownership, const char* file, int line);
#else
inline bool validateStateBeforeOperation(OwnershipState, OwnershipOperation) { return true; }
inline bool validateStateAfterOperation(OwnershipState, OwnershipOperation) { return true; }
inline bool validateNotification(OwnershipState, OwnershipNotification) { return true; }
inline std::string validateOwnership(const OwnershipMap&) { return {}; }
inline void assertValidateOwnership(const OwnershipMap&, const char*, int) {}
#endif

bool updateOwnershipState(OwnershipStateOperation& ownership, OwnershipOperation operation);
bool updateOwnershipState(OwnershipState& state, OwnershipNotification notification);

Property* findPotentialOwnerFederate(const OwnershipMap& ownership, OwnershipStateFlag flag);
Property* findOwnerFederate(const OwnershipMap& ownership);
bool hasOwnership(OwnershipStateNotification value);
bool hasPublisher(const OwnershipMap& ownership);

void updateOwnershipNotifications(OwnershipMap& ownership, Property& property, OwnershipOperation operation);
void beforeUpdateOwnership(OwnershipMap& ownership);
void afterUpdateOwnership(OwnershipMap& ownership, Property& property, OwnershipOperation operation, const char* file, int line);

const char* str(OwnershipNotification value);
const char* str(OwnershipOperation value);


inline std::ostream& operator<<(std::ostream& os, OwnershipNotification value) {
  return os << str(value);
}

inline std::ostream& operator<<(std::ostream& os, OwnershipOperation value) {
  return os << str(value);
}
inline std::ostream& operator<<(std::ostream& os, OwnershipState value) {
  return os << value.str();
}


#endif
