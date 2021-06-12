#ifndef WARSTAGE__MATCHMAKER__MASTER_SUPERVISION_POLICY_H
#define WARSTAGE__MATCHMAKER__MASTER_SUPERVISION_POLICY_H

#include "runtime/runtime.h"
#include "runtime/supervision-policy.h"

class MasterSupervisionPolicy : public SupervisionPolicy {
public:
    MasterSupervisionPolicy();
    std::shared_ptr<Shutdownable> makeSupervisor(Runtime& runtime, FederationType federationType, ObjectId federationId) override;
};


#endif
