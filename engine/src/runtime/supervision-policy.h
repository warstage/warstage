// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__RUNTIME__SUPERVISION_POLICY_H
#define WARSTAGE__RUNTIME__SUPERVISION_POLICY_H

#include <memory>
#include "federation.h"

class Shutdownable;

class SupervisionPolicy {
public:
  virtual ~SupervisionPolicy() = default;

  virtual std::shared_ptr<Shutdownable> makeSupervisor(Runtime& runtime, FederationType federationType, ObjectId federationId) = 0;
};


#endif
