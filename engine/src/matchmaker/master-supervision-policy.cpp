#include "master-supervision-policy.h"
#include "lobby-supervisor.h"

MasterSupervisionPolicy::MasterSupervisionPolicy() {
}

std::shared_ptr<Shutdownable> MasterSupervisionPolicy::makeSupervisor(Runtime& runtime, FederationType federationType, ObjectId federationId) {
    if (federationType == FederationType::Lobby) {
          auto result = std::make_shared<LobbySupervisor>(runtime, "Supervisor", Strand::getMain(), "");
          result->Startup(federationId);
          return result;
    }
    return nullptr;
}
