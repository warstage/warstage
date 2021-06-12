#include "./lobby-supervisor.h"
#include "geometry/geometry.h"
#include "runtime/runtime.h"
#include "runtime/session.h"
#include "utilities/logging.h"
#include <glm/gtc/constants.hpp>
#include <glm/gtc/random.hpp>
#include <random>


LobbySupervisor::LobbySupervisor(
        Runtime& runtime,
        const char* federateName,
        std::shared_ptr<Strand_base> strand,
        std::string moduleUrl) :
    federate_{std::make_shared<Federate>(runtime, federateName, strand)},
    strand_{std::move(strand)},
    moduleUrl_{std::move(moduleUrl)} {
}


LobbySupervisor::~LobbySupervisor() {
    if (housekeepingInterval_) {
      clearInterval(*housekeepingInterval_);
    }
}


void LobbySupervisor::Startup(ObjectId federationId) {
    assert(!weak_from_this().expired());

    auto weak_ = weak_from_this();

  federate_->setOwnershipCallback([weak_](const ObjectRef& object, Property& property, OwnershipNotification notification) {
    if (auto this_ = weak_.lock()) {
      this_->OwnershipCallback(object, property, notification);
    }
  });

  federate_->getObjectClass("Module").publish({
      "~",
      "moduleUrl",
      "online"
  });

  federate_->getObjectClass("Session").publish({
      "~",
      "connected",
      "match",
      "ready",
      "playerId",
      "playerName",
      "playerIcon"
  });

  federate_->getObjectClass("Match").publish({
      "~",
      "online",
      "teams",
      "hostingPlayerId",
      "title",
      "started",
      "ended",
      "time",
      "teamsMin",
      "teamsMax",
      "options",
      "settings",
      "map"
  });

  federate_->getObjectClass("Team").publish({
      "~",
      "slots"
  });

  federate_->getObjectClass("Slot").publish({
      "~",
      "playerId"
  });

  federate_->setObjectCallback([federate = federate_](const auto& object) {
    LOG_X("%s %s %s", str(federate->getRuntime().getProcessType()),
        object.getObjectClass().c_str(),
        object.justDiscovered() ? "discovered" : object.justDestroyed() ? "destroyed" : "hasChanged");
    if (object.justDiscovered()) {
      LOG_W("LobbySupervisor, justDiscovered: %s", object.getObjectClass().c_str());
    }
  });

  federate_->getObjectClass("Module").observe([weak_](const ObjectRef& object) {
    if (auto this_ = weak_.lock()) {
      this_->OnModuleChanged(object);
    }
  });

  federate_->getObjectClass("Session").observe([weak_](ObjectRef object) {
    if (auto this_ = weak_.lock()) {
      this_->OnSessionChanged(object);
    }
  });

  federate_->getObjectClass("Match").observe([weak_](ObjectRef object) {
    if (auto this_ = weak_.lock()) {
      this_->OnMatchChanged(object);
    }
  });

  federate_->getObjectClass("Team").observe([weak_](ObjectRef object) {
    if (auto this_ = weak_.lock()) {
      this_->OnTeamChanged(object);
    }
  });

  federate_->getObjectClass("Slot").observe([weak_](ObjectRef object) {
    if (auto this_ = weak_.lock()) {
      this_->OnSlotChanged(object);
    }
  });

    if (!moduleUrl_.empty()) {
        std::mt19937 rng{std::random_device{}()};
      strand_->setTimeout([weak_]() {
        if (auto this_ = weak_.lock()) {
          if (!this_->module_) {
            this_->CreateModule();
          }
        }
      }, std::uniform_int_distribution<std::mt19937::result_type>{1, 100}(rng));
    }

  federate_->startup(federationId);

    if (federate_->getRuntime().getProcessType() == ProcessType::Player) {
        strand_->setImmediate([weak_]() {
            if (auto this_ = weak_.lock()) {
                this_->RegisterPlayerSession(this_->federate_->getRuntime().getProcessId());
            }
        });
    }

    auto processFederations = federate_->getRuntime().addRuntimeObserver_safe(*this);
    for (auto& i : processFederations) {
        if (i.federationId == federationId) {
            PromiseUtils::Strand()->setImmediate([weak_, processId = i.processId, processType = i.processType, federationId = i.federationId]() {
                if (auto this_ = weak_.lock()) {
                  this_->onProcessAdded_main(federationId, processId, processType);
                }
            });
        }
    }

    StartHousekeepingInterval();
}


void LobbySupervisor::StartupForTest(ObjectId federationId) {
    Startup(federationId);
    if (!moduleUrl_.empty()) {
        strand_->setImmediate([this_shared = shared_from_this()]() {
            if (!this_shared->module_) {
                this_shared->CreateModule();
            }
        });
    }
}


Promise<void> LobbySupervisor::shutdown_() {
    if (!federate_->shutdownStarted()) {
      federate_->getRuntime().removeRuntimeObserver_safe(*this);
    }
    co_await federate_->shutdown();
}


/***/


void LobbySupervisor::onProcessAdded_main(ObjectId federationId, ObjectId processId, ProcessType processType) {
    switch (processType) {
        case ProcessType::Player: {
            if (federationId == federate_->getFederationId()) {
                LOG_X("%s:OnProcessAddedFederation/Player %s:%s - %s",
                        str(federate_->getRuntime().getProcessType()),
                        str(processType),
                        processId.debug_str().c_str(),
                        federationId.debug_str().c_str());
                RegisterPlayerSession(processId);
            }
            break;
        }
        case ProcessType::Daemon: {
            if (federationId == federate_->getFederationId()) {
                LOG_X("%s:OnProcessAddedFederation/Master %s:%s - %s",
                        str(federate_->getRuntime().getProcessType()),
                        str(processType),
                        processId.debug_str().c_str(),
                        federationId.debug_str().c_str());
                UpdateModuleServerCountAndOnline(1);
                TryAcquireOrReleaseModuleOwnership();
            } else if (auto match = federate_->getObject(federationId)) {
                if (match.getObjectClass() == "Match") {
                    LOG_X("%s:OnProcessAddedFederation/Match %s:%s - %s",
                            str(federate_->getRuntime().getProcessType()),
                            str(processType),
                            processId.debug_str().c_str(),
                            federationId.debug_str().c_str());
                    UpdateMatchServerCountAndOnline(match, 1);
                }
            }
            break;
        }
        default:
            break;
    }
}


void LobbySupervisor::onProcessRemoved_main(ObjectId federationId, ObjectId processId) {
    LOG_X("OnProcessLeaveFederation %s - %s",
            processId.debug_str().c_str(),
            federationId.debug_str().c_str());

    if (federationId == federate_->getFederationId()) {
        UnregisterPlayerSession(processId);
    }
}


void LobbySupervisor::onProcessAuthenticated_main(ObjectId processId, const ProcessAuth& processAuth) {
    if (processId == federate_->getRuntime().getProcessId()
            && federate_->getRuntime().getProcessType() == ProcessType::Player
            && module_) {
        LOG_ASSERT(!processAuth.subjectId.empty());
        LOG_ASSERT(module_["ownerId"].canSetValue());
        module_["ownerId"] = processAuth.subjectId;
    }
    if (auto session = FindPlayerSessionWithProcessId(processId)) {
        LOG_X("ProcessAuthenticate: pid=%s sub=%s obj=%s '%s'",
                processId.debug_str().c_str(),
                processAuth.subjectId.c_str(),
            session.getObjectId().str().c_str(),
                processAuth.nickname.c_str());

        session["playerId"] = processAuth.subjectId;
        session["playerName"] = processAuth.nickname;
        session["playerIcon"] = processAuth.imageUrl;

        if (!processAuth.subjectId.empty()) {
            if (auto existing = FindPlayerSessionWithSubjectId(processAuth.subjectId, session.getObjectId())) {
                session["match"] = existing["match"_ObjectId];
                session["ready"] = existing["ready"_bool];
            }
        }
    }
}


/***/


void LobbySupervisor::OwnershipCallback(const ObjectRef& object, Property& property, OwnershipNotification notification) {
    LOG_X("%s %s %s.%s",
            str(federate_->getRuntime().getProcessType()),
            str(notification),
        property.getObjectClass(),
        property.getName().c_str());

    if (federate_->getRuntime().getProcessType() == ProcessType::Daemon) {
      LOG_ASSERT_FORMAT(notification != OwnershipNotification::ForcedOwnershipDivestitureNotification, "%s %s",
          property.getObjectClass(),
          property.getName().c_str());
      LOG_ASSERT_FORMAT(notification != OwnershipNotification::OwnershipDivestitureNotification, "%s %s",
          property.getObjectClass(),
          property.getName().c_str());
    }

    if (object.getObjectId() == module_.getObjectId() && property.getName() == Property::Destructor_str) {
        switch (notification) {
            case OwnershipNotification::RequestOwnershipAssumption:
                if (property.getOwnershipState() & OwnershipStateFlag::NotTryingToAcquire && ShouldHaveModuleOwnership()) {
                  property.modifyOwnershipState(OwnershipOperation::OwnershipAcquisitionIfAvailable);
                }
                break;
            case OwnershipNotification::RequestOwnershipRelease:
                if (property.getOwnershipState() & OwnershipStateFlag::AskedToRelease) {
                    auto op = ShouldHaveModuleOwnership()
                        ? OwnershipOperation::OwnershipReleaseFailure
                        : OwnershipOperation::OwnershipReleaseSuccess;
                  property.modifyOwnershipState(op);
                }
                break;
            default:
                break;
        }
        strand_->setImmediate([this_shared = shared_from_this()]() {
            this_shared->TryAcquireOrReleaseModuleOwnership();
            this_shared->TryDefineOrUndefineLobbyServices();
            this_shared->TryAcquireOwnership();
        });
    } else {
        switch (notification) {
            case OwnershipNotification::RequestOwnershipAssumption:
                if (property.getOwnershipState() & OwnershipStateFlag::NotTryingToAcquire && HasModuleOwnership()) {
                  property.modifyOwnershipState(OwnershipOperation::OwnershipAcquisitionIfAvailable);
                }
                break;
            case OwnershipNotification::RequestOwnershipRelease:
                if (property.getOwnershipState() & OwnershipStateFlag::AskedToRelease) {
                    auto op = HasModuleOwnership()
                            ? OwnershipOperation::OwnershipReleaseFailure
                            : OwnershipOperation::OwnershipReleaseSuccess;
                  property.modifyOwnershipState(op);
                }
                break;
            default:
                break;
        }
    }
}


void LobbySupervisor::CreateModule() {
    assert(!module_);
    assert(!moduleUrl_.empty());
    module_ = federate_->getObjectClass("Module").create(federate_->getFederationId());
    module_["moduleUrl"] = moduleUrl_;
    if (federate_->getRuntime().getProcessType() == ProcessType::Player) {
        const auto subjectId = federate_->getRuntime().getSubjectId_safe();
        if (!subjectId.empty()) {
            module_["ownerId"] = subjectId;
        }
    }
    UpdateModuleServerCountAndOnline(0);
    TryDefineOrUndefineLobbyServices();
}


void LobbySupervisor::OnModuleChanged(const ObjectRef& module) {
    if (module.justDiscovered()) {
        module_ = module;
    } else if (module.justDestroyed() && module == module_) {
        module_ = ObjectRef{};
    }
    strand_->setImmediate([this_shared = shared_from_this()]() {
        this_shared->TryAcquireOrReleaseModuleOwnership();
        this_shared->TryDefineOrUndefineLobbyServices();
    });
}


void LobbySupervisor::TryAcquireOrReleaseModuleOwnership() {
    bool shouldHaveOwnership = ShouldHaveModuleOwnership();
    /*if (shouldHaveOwnership && _module.GetOwnershipState() & OwnershipStateFlag::NotAcquiring) {
        _module.ModifyOwnershipState(OwnershipOperation::OwnershipAcquisition);
    } else */
    if (!shouldHaveOwnership && module_.getOwnershipState() & OwnershipStateFlag::NotDivesting) {
      strand_->setTimeout([module = module_, federate = federate_]() mutable {
        if (module.getOwnershipState() & OwnershipStateFlag::NotDivesting) {
          LOG_X("%s ReleaseModuleOwnership", str(federate->getRuntime().getProcessType()));
          module.modifyOwnershipState(OwnershipOperation::NegotiatedOwnershipDivestiture);
        }
      }, 1000);
    }
}


bool LobbySupervisor::HasModuleOwnership() const {
    return module_.canDelete();
}


bool LobbySupervisor::ShouldHaveModuleOwnership() const {
    return moduleServerCount_ == 0 || federate_->getRuntime().getProcessType() == ProcessType::Daemon;
}


void LobbySupervisor::TryDefineOrUndefineLobbyServices() {
    if (HasModuleOwnership() && !hasDefinedLobbyServices_) {
        LOG_X("DefineLobbyServices %s", str(federate_->getRuntime().getProcessType()));
        DefineLobbyServices();
        hasDefinedLobbyServices_ = true;
    } else if (!HasModuleOwnership() && hasDefinedLobbyServices_) {
        LOG_X("UndefineLobbyServices %s", str(federate_->getRuntime().getProcessType()));
        UndefineLobbyServices();
        hasDefinedLobbyServices_ = false;
    }
}


void LobbySupervisor::DefineLobbyServices() {
    auto weak_ = weak_from_this();
    
    federate_->getServiceClass("PlayerReady").define([weak_](const Value& params, const std::string& subjectId) {
        auto this_ = weak_.lock();
        return this_ ? this_->ProcessPlayerReady(params, subjectId) : Promise<Value>{}.reject<Value>(Value{});
    });
    
    federate_->getServiceClass("PlayerUnready").define([weak_](const Value& params, const std::string& subjectId) {
        auto this_ = weak_.lock();
        return this_ ? this_->ProcessPlayerUnready(params, subjectId) : Promise<Value>{}.reject<Value>(Value{});
    });

    federate_->getServiceClass("CreateMatch").define([weak_](const Value& params, const std::string& subjectId) {
        auto this_ = weak_.lock();
        return this_ ? this_->ProcessCreateMatch(params, subjectId) : Promise<Value>{}.reject<Value>(Value{});
    });

    federate_->getServiceClass("HostMatch").define([weak_](const Value& params, const std::string& subjectId) {
        auto this_ = weak_.lock();
        return this_ ? this_->ProcessHostMatch(params, subjectId) : Promise<Value>{}.reject<Value>(Value{});
    });
    
    federate_->getServiceClass("UpdateMatch").define([weak_](const Value& params, const std::string& subjectId) {
        auto this_ = weak_.lock();
        return this_ ? this_->ProcessUpdateMatch(params, subjectId) : Promise<Value>{}.reject<Value>(Value{});
    });
    
    federate_->getServiceClass("LeaveMatch").define([weak_](const Value& params, const std::string& subjectId) {
        auto this_ = weak_.lock();
        return this_ ? this_->ProcessLeaveMatch(params, subjectId) : Promise<Value>{}.reject<Value>(Value{});
    });
    
    federate_->getServiceClass("JoinMatchAsParticipant").define([weak_](const Value& params, const std::string& subjectId) {
        auto this_ = weak_.lock();
        return this_ ? this_->ProcessJoinMatchAsParticipant(params, subjectId) : Promise<Value>{}.reject<Value>(Value{});
    });
    
    federate_->getServiceClass("JoinMatchAsSpectator").define([weak_](const Value& params, const std::string& subjectId) {
        auto this_ = weak_.lock();
        return this_ ? this_->ProcessJoinMatchAsSpectator(params, subjectId) : Promise<Value>{}.reject<Value>(Value{});
    });
    
    federate_->getServiceClass("AddTeam").define([weak_](const Value& params, const std::string& subjectId) {
        auto this_ = weak_.lock();
        return this_ ? this_->ProcessAddTeam(params, subjectId) : Promise<Value>{}.reject<Value>(Value{});
    });
    
    federate_->getServiceClass("UpdateTeam").define([weak_](const Value& params, const std::string& subjectId) {
        auto this_ = weak_.lock();
        return this_ ? this_->ProcessUpdateTeam(params, subjectId) : Promise<Value>{}.reject<Value>(Value{});
    });
    
    federate_->getServiceClass("RemoveTeam").define([weak_](const Value& params, const std::string& subjectId) {
        auto this_ = weak_.lock();
        return this_ ? this_->ProcessRemoveTeam(params, subjectId) : Promise<Value>{}.reject<Value>(Value{});
    });
    
    federate_->getServiceClass("AddSlot").define([weak_](const Value& params, const std::string& subjectId) {
        auto this_ = weak_.lock();
        return this_ ? this_->ProcessAddSlot(params, subjectId) : Promise<Value>{}.reject<Value>(Value{});
    });
    
    federate_->getServiceClass("RemoveSlot").define([weak_](const Value& params, const std::string& subjectId) {
        auto this_ = weak_.lock();
        return this_ ? this_->ProcessRemoveSlot(params, subjectId) : Promise<Value>{}.reject<Value>(Value{});
    });
    
    federate_->getServiceClass("InvitePlayer").define([weak_](const Value& params, const std::string& subjectId) {
        auto this_ = weak_.lock();
        return this_ ? this_->ProcessInvitePlayer(params, subjectId) : Promise<Value>{}.reject<Value>(Value{});
    });
    
    federate_->getServiceClass("ChatMessage").define([weak_](const Value& params, const std::string& subjectId) {
        auto this_ = weak_.lock();
        return this_ ? this_->ProcessChatMessage(params, subjectId) : Promise<Value>{}.reject<Value>(Value{});
    });
}


void LobbySupervisor::UndefineLobbyServices() {
  federate_->getServiceClass("PlayerReady").undefine();
  federate_->getServiceClass("PlayerUnready").undefine();
  federate_->getServiceClass("CreateMatch").undefine();
  federate_->getServiceClass("UpdateMatch").undefine();
  federate_->getServiceClass("LeaveMatch").undefine();
  federate_->getServiceClass("JoinMatchAsParticipant").undefine();
  federate_->getServiceClass("JoinMatchAsSpectator").undefine();
  federate_->getServiceClass("AddTeam").undefine();
  federate_->getServiceClass("UpdateTeam").undefine();
  federate_->getServiceClass("RemoveTeam").undefine();
  federate_->getServiceClass("AddSlot").undefine();
  federate_->getServiceClass("RemoveSlot").undefine();
  federate_->getServiceClass("InvitePlayer").undefine();
}


void LobbySupervisor::UpdateModuleServerCountAndOnline(const int delta) {
    moduleServerCount_ += delta;
    if (module_ && HasModuleOwnership()) {
        module_["_serverCount"] = moduleServerCount_;
        module_["online"] = moduleServerCount_ != 0;
    }
}


void LobbySupervisor::UpdateMatchServerCountAndOnline(ObjectRef& match, const int delta) {
    if (match["_serverCount"].canSetValue()) {
        int serverCount = match["_serverCount"_int] + delta;
        match["_serverCount"] = serverCount;
        if (match["online"].canSetValue()) {
            match["online"] = serverCount != 0;
        }
    }
}


void LobbySupervisor::RegisterPlayerSession(ObjectId processId) {
    LOG_ASSERT(processId);

    auto session = FindPlayerSessionWithProcessId(processId);
    const auto processAuth = federate_->getRuntime().getProcessAuth_safe(processId);

    if (!session) {
        session = federate_->getObjectClass("Session").create(processId);
        LOG_ASSERT(!session.isDeletedByObject());
        LOG_ASSERT(!session.isDeletedByMaster());
        LOG_I("RegisterPlayerSession: pid=%s sub=%s obj=%s '%s' create",
            processId.debug_str().c_str(),
            processAuth.subjectId.c_str(),
            session.getObjectId().debug_str().c_str(),
            processAuth.nickname.c_str());
    } else {
        LOG_I("RegisterPlayerSession: pid=%s sub=%s obj=%s '%s' exist",
            processId.debug_str().c_str(),
            processAuth.subjectId.c_str(),
            session.getObjectId().debug_str().c_str(),
            processAuth.nickname.c_str());
    }
    
    session["connected"] = true;
    session["playerId"] = processAuth.subjectId;
    session["playerName"] = processAuth.nickname;
    session["playerIcon"] = processAuth.imageUrl;
    
    if (!processAuth.subjectId.empty()) {
        if (auto existing = FindPlayerSessionWithSubjectId(processAuth.subjectId, session.getObjectId())) {
            session["match"] = existing["match"_ObjectId];
            session["ready"] = existing["ready"_bool];
        }
    }
}


void LobbySupervisor::UnregisterPlayerSession(ObjectId processId) {
    LOG_X("UnregisterPlayerSession: pid=%s", processId.debug_str().c_str());

    auto session = FindPlayerSessionWithProcessId(processId);
    if (!session) {
        return LOG_E("MatchFederationServices::TryUnregisterPlayerSession: no player found (%s)", processId.str().c_str());
    }

    session["connected"] = false; // AssertCanSetValue fail

    bool playerHasJoinedMatch = false;

    if (const char* playerId = session["playerId"_c_str]) {
        for (const auto& match : federate_->getObjectClass("Match")) {
            if (FindMatchSlotWithPlayerId(match, playerId)) {
                playerHasJoinedMatch = true;
                break;
            }
        }
    }

    TryDeleteAbandonedMatches();
    TryDeleteEmptyMatches();

    if (!playerHasJoinedMatch) {
        LOG_X("auto-delete PlayerSession: pid=%s obj=%s", processId.str().c_str(), session.getObjectId().str().c_str());
        session.Delete();
    }
}


/***/


ObjectRef LobbySupervisor::FindPlayerSessionWithProcessId(ObjectId processId) {
    auto result = federate_->getObject(processId);
    LOG_ASSERT(!result || result.getObjectClass() == "Session");
    return result;
}


ObjectRef LobbySupervisor::FindPlayerSessionWithSubjectId(const std::string& subjectId, ObjectId excludedSessionId) {
    for (const auto& session : federate_->getObjectClass("Session")) {
        if (session.getObjectId() != excludedSessionId) {
            if (const char* playerId = session["playerId"_c_str]) {
                if (subjectId == playerId) {
                    return session;
                }
            }
        }
    }
    return ObjectRef{};
}


ObjectRef LobbySupervisor::FindMatchWithTeam(ObjectId teamId) {
    for (const auto& match : federate_->getObjectClass("Match")) {
        for (auto& i : match["teams"_value]) {
            if (i._ObjectId() == teamId) {
                return match;
            }
        }
    }
    return ObjectRef{};
}


ObjectRef LobbySupervisor::FindTeamWithSlot(ObjectId slotId) {
    for (const auto& team : federate_->getObjectClass("Team")) {
        for (auto& i : team["slots"_value]) {
            if (i._ObjectId() == slotId) {
                return team;
            }
        }
    }
    return ObjectRef{};
}


ObjectRef LobbySupervisor::FindMatchSlotWithPlayerId(const ObjectRef& match, const char* playerId) {
    for (auto& teamId : match["teams"_value]) {
        if (auto team = federate_->getObject(teamId._ObjectId())) {
            if (auto slot = FindTeamSlotWithPlayerId(team, playerId)) {
                return slot;
            }
        }
    }
    return ObjectRef{};
}


ObjectRef LobbySupervisor::FindTeamSlotWithPlayerId(const ObjectRef& team, const char* playerId) {
    for (const auto& slotId : team["slots"_value]) {
        if (auto slot = federate_->getObject(slotId._ObjectId())) {
            if (const char* s = slot["playerId"_c_str]) {
                if (s && std::strcmp(s, playerId) == 0) {
                    return slot;
                }
            }
        }
    }
    return ObjectRef{};
}


ObjectRef LobbySupervisor::FindUnassignedMatchSlot(const ObjectRef& match) {
    for (auto& teamId : match["teams"_value]) {
        if (auto team = federate_->getObject(teamId._ObjectId())) {
            if (auto slot = FindUnassignedTeamSlot(team)) {
                return slot;
            }
        }
    }
    return ObjectRef{};
}


ObjectRef LobbySupervisor::FindUnassignedTeamSlot(const ObjectRef& team) {
    for (auto& slotId : team["slots"_value]) {
        if (auto slot = federate_->getObject(slotId._ObjectId())) {
            if (!slot["playerId"_bool]) { // null or empty string
                return slot;
            }
        }
    }
    return ObjectRef{};
}


bool LobbySupervisor::HasPlayerJoinedMatch(const char* playerId, ObjectId matchId) {
    for (const auto& session : federate_->getObjectClass("Session")) {
        if (const char* s = session["playerId"_c_str]) {
            if (std::strcmp(s, playerId) == 0
                    && session["match"_ObjectId] == matchId
                    && (session["connected"_bool] || session["connected"].getTime() > -5.0)) {
                return true;
            }
        }
    }
    return false;
}


bool LobbySupervisor::IsPlayerReadyForMatch(const char* playerId, ObjectId matchId) {
    for (const auto& session : federate_->getObjectClass("Session")) {
        if (const char* s = session["playerId"_c_str]) {
            if (std::strcmp(s, playerId) == 0) {
                if (session["match"_ObjectId] == matchId && session["ready"_bool]) {
                    return true;
                }
            }
        }
    }
    return false;
}


bool LobbySupervisor::ForAllPlayersInMatch(const ObjectRef& match, std::function<bool(const char* playerId)> predicate) {
    int teams = 0;
    for (auto& teamId : match["teams"_value]) {
        auto team = federate_->getObject(teamId._ObjectId());
        if (!team) {
            return false;
        }
        int slots = 0;
        for (auto& slotId : team["slots"_value]) {
            auto slot = federate_->getObject(slotId._ObjectId());
            if (!slot) {
                return false;
            }
            const char* playerId = slot["playerId"_c_str];
            if (!playerId || !*playerId || !predicate(playerId)) {
                return false;
            }
            ++slots;
        }
        if (slots == 0) {
            return false;
        }
        ++teams;
    }
    return teams != 0;
}


bool LobbySupervisor::ShouldStartMatch(const ObjectRef& match) {
    return ForAllPlayersInMatch(match, [this, matchId = match.getObjectId()](const char* playerId) {
        return IsPlayerReadyForMatch(playerId, matchId);
    });
}


std::vector<std::string> LobbySupervisor::GetMatchPlayerIds(const ObjectRef& match) {
    std::vector<std::string> result{};
    ForAllPlayersInMatch(match, [&result, matchId = match.getObjectId()](const char* playerId) {
        std::string s{playerId};
        if (std::find(result.begin(), result.end(), s) == result.end()) {
            result.push_back(std::move(s));
        }
        return true;
    });
    return result;
}


void LobbySupervisor::UnassignSlotsWithPlayerId(const ObjectRef& match, const std::string& playerId) {
    for (auto& teamId : match["teams"_value]) {
        if (auto team = federate_->getObject(teamId._ObjectId())) {
            for (auto& slotId : team["slots"_value]) {
                if (auto slot = federate_->getObject(slotId._ObjectId())) {
                    if (const char* s = slot["playerId"_c_str]) {
                        if (playerId == s) {
                            slot["playerId"] = nullptr;
                        }
                    }
                }
            }
        }
    }
}


bool LobbySupervisor::TryStartMatch(ObjectRef& match) {
    if (!match["started"_bool] && ShouldStartMatch(match)) {
        match["started"] = true;
        match["time"] = 0.0;
        match["options"] = nullptr;
        return true;
    }
    return false;
}


void LobbySupervisor::TryDeleteAbandonedMatches() {
    for (const auto& match : federate_->getObjectClass("Match")) {
        if (IsMatchAbandoned(match)) {
            DeleteMatch(match);
        }
    }
}


bool LobbySupervisor::IsMatchAbandoned(const ObjectRef& match) {
    if (!match["started"_bool]) {
        return false;
    }
    auto teams = match["teams"_value];
    if (teams.is_undefined()) {
        return false;
    }
    auto hasTeams = false;
    for (auto& teamId : teams) {
        hasTeams = true;
        auto team = federate_->getObject(teamId._ObjectId());
        if (!team) {
            return false;
        }
        auto slots = team["slots"_value];
        if (slots.is_undefined()) {
            return false;
        }
        for (auto& slotId : slots) {
            auto slot = federate_->getObject(slotId._ObjectId());
            if (!slot) {
                return false;
            }
            const char* playerId = slot["playerId"_c_str] ?: "";
            if (playerId[0] && HasPlayerJoinedMatch(playerId, match.getObjectId())) {
                return false;
            }
        }
    }
    return hasTeams;
}


void LobbySupervisor::TryDeleteEmptyMatches() {
    bool found = false;
    for (const auto& match : federate_->getObjectClass("Match")) {
        if (IsMatchEmpty(match)) {
            if (!found) {
                found = true;
            } else {
                DeleteMatch(match);
            }
        }
    }
}


bool LobbySupervisor::IsMatchEmpty(const ObjectRef& match) {
    for (auto& teamId : match["teams"_value]) {
        auto team = federate_->getObject(teamId._ObjectId());
        if (!team) {
            return false;
        }
        for (auto& slotId : team["slots"_value]) {
            auto slot = federate_->getObject(slotId._ObjectId());
            if (!slot) {
                return false;
            }
            if (slot["playerId"_bool]) { // null or empty string
                return false;
            }
        }
    }
    return true;
}


void LobbySupervisor::DeleteMatch(ObjectRef match) {
    TryDeleteDisconnectedPlayerSessions(match.getObjectId());
    
    for (auto& teamId : match["teams"_value]) {
        if (auto team = federate_->getObject(teamId._ObjectId())) {
            for (auto& slotId : team["slots"_value]) {
                if (auto slot = federate_->getObject(slotId._ObjectId())) {
                    slot.Delete();
                }
            }
            team.Delete();
        }
    }
    match.Delete();
    ResetSessionsWithMatch(match.getObjectId());
    ReleaseBattleFederation(match.getObjectId());
}


void LobbySupervisor::ResetSessionsWithMatch(ObjectId matchId) {
    for (auto session : federate_->getObjectClass("Session")) {
        if (session["match"_ObjectId] == matchId && session["match"].canSetValue()) {
            session["match"] = nullptr;
            if (session["ready"].canSetValue()) {
                session["ready"] = nullptr;
            }
        }
    }
}


void LobbySupervisor::ReleaseBattleFederation(ObjectId matchId) {
    auto i = battleFederations_.find(matchId);
    if (i != battleFederations_.end()) {
      federate_->getRuntime().releaseFederation_safe((*i).second);
        battleFederations_.erase(i);
    }
}


void LobbySupervisor::UpdatePlayerMatch(const std::string& subjectId, ObjectId matchId, bool ready) {
    for (auto session : federate_->getObjectClass("Session")) {
        if (const char* playerId = session["playerId"_c_str]) {
            if (subjectId == playerId) {
                session["match"] = matchId;
                session["ready"] = ready;
            }
        }
    }
}


/***/


void LobbySupervisor::OnSessionChanged(ObjectRef& session) {
    if (!HasModuleOwnership()) {
        return;
    }
    if (session["ready"].hasChanged() || session["match"].hasChanged()) {
        OnSessionChangedReadyOrMatch(session);
    }
    TryDeleteAbandonedMatches();
    TryDeleteEmptyMatches();
}


void LobbySupervisor::OnSessionChangedReadyOrMatch(ObjectRef& session) {
    if (!HasModuleOwnership()) {
        return;
    }
    if (session.justDestroyed()) {
        return;
    }
    if (session["ready"_bool])
        if (auto matchId = session["match"_ObjectId])
            if (auto match = federate_->getObject(matchId))
                TryStartMatch(match);
}


void LobbySupervisor::OnMatchChanged(ObjectRef& match) {
    if (!HasModuleOwnership()) {
        return;
    }
    if (match.justDestroyed()) {
        TryDeleteDisconnectedPlayerSessions(match.getObjectId());
        ResetSessionsWithMatch(match.getObjectId());
        ReleaseBattleFederation(match.getObjectId());
    } else {
        TryStartMatch(match);
    }
}


void LobbySupervisor::OnTeamChanged(ObjectRef& team) {
}


void LobbySupervisor::OnSlotChanged(ObjectRef& slot) {
}


/***/


Promise<Value> LobbySupervisor::ProcessPlayerReady(const Value& params, const std::string& subjectId) {
    if (subjectId.empty()) {
        return Promise<Value>{}.reject<Value>(REASON(401, "PlayerReady: missing subjectId"));
    }
    
    bool found = false;
    for (auto session : federate_->getObjectClass("Session")) {
        const char* playerId = session["playerId"_c_str];
        if (playerId && subjectId == playerId) {
            session["ready"] = true;
            OnSessionChangedReadyOrMatch(session);
            found = true;
        }
    }
    return found ? resolve(Value{}) : Promise<Value>{}.reject<Value>(REASON(404, "PlayerReady: player session '%s' not found", subjectId.c_str()));
}


Promise<Value> LobbySupervisor::ProcessPlayerUnready(const Value& params, const std::string& subjectId) {
    if (subjectId.empty()) {
        return Promise<Value>{}.reject<Value>(REASON(401, "PlayerUnready: missing subjectId"));
    }
    
    bool found = false;
    for (auto session : federate_->getObjectClass("Session")) {
        if (const char* playerId = session["playerId"_c_str]) {
            if (subjectId == playerId) {
                session["ready"] = false;
                OnSessionChangedReadyOrMatch(session);
                found = true;
            }
        }
    }
    return found ? resolve(Value{}) : Promise<Value>{}.reject<Value>(REASON(404, "PlayerUnready: player session '%s' not found", subjectId.c_str()));
}


Promise<Value> LobbySupervisor::ProcessCreateMatch(const Value& params, const std::string& subjectId) {
    if (subjectId.empty()) {
        return Promise<Value>{}.reject<Value>(REASON(401, "CreateMatch: missing subjectId"));
    }

    if (module_) {
        if (const char* ownerId = module_["ownerId"_c_str]) {
            if (subjectId != ownerId) {
                return Promise<Value>{}.reject<Value>(REASON(403, "CreateMatch: subject is not owner"));
            }
        }
    }

    if (!FindPlayerSessionWithSubjectId(subjectId)) {
        return Promise<Value>{}.reject<Value>(REASON(403, "CreateMatch: no player session '%s'", subjectId.c_str()));
    }
    
    int teamsMin = params["teamsMin"].is_int32() ? params["teamsMin"_int] : 2;
    int teamsMax = params["teamsMax"].is_int32() ? params["teamsMax"_int] : teamsMin;
    bool started = params["started"_bool];
    
    auto match = federate_->getObjectClass("Match").create();
    if (moduleServerCount_) {
        match["hostingPlayerId"] = subjectId;
    } else {
        match["hostingPlayerId"] = nullptr;
    }
    match["title"] = params["title"_c_str] ?: "";
    match["started"] = started;
    if (started) {
        match["time"] = 0.0;
    } else {
        match["time"] = nullptr;
    }
    match["teamsMin"] = teamsMin;
    match["teamsMax"] = teamsMax;
    
    auto teams = build_array();
    if (params["teams"].is_array()) {
        for (auto& t : params["teams"_value]) {
            auto slots = build_array();
            for (auto& s : t["slots"_value]) {
                auto slot = federate_->getObjectClass("Slot").create();
                slot["playerId"] = s["playerId"_c_str];
                slots = std::move(slots) << slot.getObjectId();
            }
            auto team = federate_->getObjectClass("Team").create();
            team["slots"] = std::move(slots) << ValueEnd{};
            teams = std::move(teams) << team.getObjectId();
        }
    } else {
        for (int i = 0; i < teamsMin; ++i) {
            auto slot = federate_->getObjectClass("Slot").create();
            slot["playerId"] = i == 0 ? subjectId.c_str() : nullptr;
            
            auto team = federate_->getObjectClass("Team").create();
            team["slots"] = Array{} << slot.getObjectId() << ValueEnd{};
            teams = std::move(teams) << team.getObjectId();
        }
    }
    match["teams"] = std::move(teams) << ValueEnd{};
    
    match["options"] = params["options"].is_defined() ? params["options"_value] : Struct{} << ValueEnd{};
    match["settings"] = params["settings"].is_defined() ? params["settings"_value] : Struct{} << ValueEnd{};
    
    for (auto& i : params) {
        if (match[i.name()].getValue().is_undefined()) {
          match[i.name()].setValue(i);
        }
    }
    
    UpdateTeamPositions(match);
    UpdatePlayerMatch(subjectId, match.getObjectId(), false);
    UpdateMatchServerCountAndOnline(match, 0);

    if (federate_->getRuntime().authorizeCreateBattleFederation_safe(subjectId)) {
        auto federation = federate_->getRuntime().initiateFederation_safe(match.getObjectId(), FederationType::Battle);
        battleFederations_[match.getObjectId()] = federation;

        TryStartMatch(match);
    }
    
    return resolve(Struct{} << "match" << match.getObjectId() << ValueEnd{});
}


Promise<Value> LobbySupervisor::ProcessHostMatch(const Value& params, const std::string& subjectId) {
    if (subjectId.empty()) {
        return Promise<Value>{}.reject<Value>(REASON(401, "HostMatch: missing subjectId"));
    }

    const ObjectId matchId = params["match"_ObjectId];
    if (!matchId) {
        return Promise<Value>{}.reject<Value>(REASON(401, "HostMatch: missing match"));
    }

    auto match = federate_->getObject(matchId);
    if (!match) {
        return Promise<Value>{}.reject<Value>(REASON(404, "HostMatch: match '%s' not found", matchId.str().c_str()));
    }

    auto& runtime = federate_->getRuntime();
    auto lobbyId = federate_->getFederationId();
    switch (runtime.getProcessType()) {
        case ProcessType::Player:
            runtime.requestHostMatch_safe(lobbyId, matchId);
            match["hostingPlayerId"] = subjectId;
            break;
        case ProcessType::Daemon:
            runtime.processHostMatch_safe(lobbyId, matchId, subjectId);
            match["hostingPlayerId"] = subjectId;
            break;
        default:
            break;
    }

    return resolve(Value{});
}


Promise<Value> LobbySupervisor::ProcessUpdateMatch(const Value& params, const std::string& subjectId) {
    if (auto match = federate_->getObject(params["match"_ObjectId])) {
        if (params["title"].is_defined()) {
            match["title"] = params["title"_value];
        }
        if (params["map"].is_defined()) {
            match["map"] = params["map"_value];
        }
        if (params["settings"].is_defined()) {
            match["settings"] = params["settings"_value];
        }
        if (params["ended"].is_defined()) {
            match["ended"] = params["ended"_value];
        }
    }
    return resolve(Value{});
}


Promise<Value> LobbySupervisor::ProcessLeaveMatch(const Value& params, const std::string& subjectId) {
    if (subjectId.empty()) {
        return Promise<Value>{}.reject<Value>(REASON(401, "LeaveMatch: missing subjectId"));
    }
    
    bool found = false;
    for (auto session : federate_->getObjectClass("Session")) {
        const char* playerId = session["playerId"_c_str];
        if (playerId && subjectId == playerId) {
            session["ready"] = false;
            if (auto matchId = session["match"_ObjectId]) {
                session["match"] = nullptr;
                if (auto match = federate_->getObject(matchId)) {
                    if (!match["started"_bool]) {
                        const char* hostingPlayerId = match["hostingPlayerId"_c_str] ?: "";
                        if (subjectId == hostingPlayerId) {
                            DeleteMatch(match);
                        } else {
                            UnassignSlotsWithPlayerId(match, subjectId);
                        }
                    }
                }
            } else {
                for (const auto& match : federate_->getObjectClass("Match"))
                    if (!match["started"_bool] && !match["hostingPlayerId"_bool])
                        UnassignSlotsWithPlayerId(match, subjectId);
            }
            found = true;
        }
    }
    TryDeleteAbandonedMatches();
    TryDeleteEmptyMatches();
    return found ? resolve(Value{}) : Promise<Value>{}.reject<Value>(REASON(404, "LeaveMatch: player session '%s' not found", subjectId.c_str()));
}


Promise<Value> LobbySupervisor::ProcessJoinMatchAsParticipant(const Value& params, const std::string& subjectId) {
    if (subjectId.empty()) {
        return Promise<Value>{}.reject<Value>(REASON(401, "JoinMatchAsParticipant: missing subjectId"));
    }
    
    if (!FindPlayerSessionWithSubjectId(subjectId)) {
        return Promise<Value>{}.reject<Value>(REASON(404, "JoinMatchAsParticipant: no player session '%s'", subjectId.c_str()));
    }
    
    auto teamId = params["team"_ObjectId];
    if (!teamId) {
        return Promise<Value>{}.reject<Value>(REASON(400, "JoinMatchAsParticipant: missing 'team' parameter"));
    }
    
    auto team = federate_->getObject(teamId);
    if (!team) {
        return Promise<Value>{}.reject<Value>(REASON(404, "JoinMatchAsParticipant: team '%s' not found", teamId.str().c_str()));
    }
    
    auto match = FindMatchWithTeam(teamId);
    if (!match) {
        return Promise<Value>{}.reject<Value>(REASON(404, "JoinMatchAsParticipant: match for team '%s' not found", teamId.str().c_str()));
    }
        
    auto slot = FindTeamSlotWithPlayerId(team, subjectId.c_str()) ?: FindUnassignedTeamSlot(team);
    if (!slot) {
        return Promise<Value>{}.reject<Value>(REASON(404, "JoinMatchAsParticipant: no available slot found"));
    }

    if (!match["settings"].getValue()["sandbox"_bool]) {
        UnassignSlotsWithPlayerId(match, subjectId);
    }
    slot["playerId"] = subjectId;
    
    UpdatePlayerMatch(subjectId, match.getObjectId(), false);
    
    return resolve(Value{});
}


Promise<Value> LobbySupervisor::ProcessJoinMatchAsSpectator(const Value& params, const std::string& subjectId) {
    if (subjectId.empty()) {
        return Promise<Value>{}.reject<Value>(REASON(401, "JoinMatchAsParticipant: missing subjectId"));
    }
    
    if (!FindPlayerSessionWithSubjectId(subjectId)) {
        return Promise<Value>{}.reject<Value>(REASON(404, "JoinMatchAsSpectator: no player session '%s'", subjectId.c_str()));
    }
    
    auto matchId = params["match"_ObjectId];
    if (!matchId) {
        return Promise<Value>{}.reject<Value>(REASON(400, "JoinMatchAsSpectator: missing 'match' parameter"));
    }

    UpdatePlayerMatch(subjectId, matchId, false);
    
    return resolve(Value{});
}


Promise<Value> LobbySupervisor::ProcessAddTeam(const Value& params, const std::string& subjectId) {
    if (subjectId.empty()) {
        return Promise<Value>{}.reject<Value>(REASON(401, "AddTeam: missing subjectId"));
    }
    
    auto matchId = params["match"_ObjectId];
    if (!matchId) {
        return Promise<Value>{}.reject<Value>(REASON(400, "AddTeam: missing 'match' parameter"));
    }
    
    auto match = federate_->getObject(matchId);
    if (!match) {
        return Promise<Value>{}.reject<Value>(REASON(404, "AddTeam: match '%s' not found", matchId.str().c_str()));
    }
    
    const char* hostingPlayerId = match["hostingPlayerId"_c_str];
    if (!hostingPlayerId || subjectId != hostingPlayerId) {
        return Promise<Value>{}.reject<Value>(REASON(403, "AddTeam: subject is not hosting player"));
    }
        
    auto team = federate_->getObjectClass("Team").create();
    team["slots"] = build_array() << ValueEnd{};
    
    auto teams = build_array();
    for (auto& i : match["teams"_value]) {
        teams = std::move(teams) << i._ObjectId();
    }
    
    match["teams"] = std::move(teams) << team.getObjectId() << ValueEnd{};
    
    return resolve(Value{});
}


Promise<Value> LobbySupervisor::ProcessUpdateTeam(const Value& params, const std::string& subjectId) {
    if (subjectId.empty()) {
        return Promise<Value>{}.reject<Value>(REASON(401, "UpdateTeam: missing subjectId"));
    }
    
    auto teamId = params["team"_ObjectId];
    if (!teamId) {
        return Promise<Value>{}.reject<Value>(REASON(400, "UpdateTeam: missing 'team' parameter"));
    }
    
    auto team = federate_->getObject(teamId);
    if (!team) {
        return Promise<Value>{}.reject<Value>(REASON(404, "UpdateTeam: team '%s' not found", teamId.str().c_str()));
    }
    
    // auto match = FindMatchWithTeam(teamId);
    // if (!match)
    //     return Promise<Value>{}.reject<Value>(Reason(404, "UpdateTeam: match for team '%s' not found", teamId.str().c_str()));
    
    // const char* hostingPlayerId = match["hostingPlayerId"_c_str];
    // if (!hostingPlayerId || subjectId != hostingPlayerId)
    //     return Promise<Value>{}.reject<Value>(Reason(403, "UpdateTeam: subject is not hosting player"));
    
    if (params["outcome"].is_defined()) {
        if (team["outcome"].canSetValue()) {
            team["outcome"] = params["outcome"_value];
        } else {
            // reject ???
        }
    }
    
    if (params["score"].is_defined()) {
        if (team["score"].canSetValue()) {
            team["score"] = params["score"_value];
        } else {
            // reject ???
        }
    }
    
    return resolve(Value{});
}


Promise<Value> LobbySupervisor::ProcessRemoveTeam(const Value& params, const std::string& subjectId) {
    if (subjectId.empty()) {
        return Promise<Value>{}.reject<Value>(REASON(401, "RemoveTeam: missing subjectId"));
    }
    
    auto teamId = params["team"_ObjectId];
    if (!teamId) {
        return Promise<Value>{}.reject<Value>(REASON(400, "RemoveTeam: missing 'team' parameter"));
    }
    
    auto team = federate_->getObject(teamId);
    if (!team) {
        return Promise<Value>{}.reject<Value>(REASON(404, "RemoveTeam: team '%s' not found", teamId.str().c_str()));
    }
    
    auto match = FindMatchWithTeam(teamId);
    if (!match) {
        return Promise<Value>{}.reject<Value>(REASON(404, "RemoveTeam: match for team '%s' not found", teamId.str().c_str()));
    }
    if (!match["teams"].canSetValue()) {
        return Promise<Value>{}.reject<Value>(REASON(503, "RemoveTeam: can't set match.teams", teamId.str().c_str()));
    }

    const char* hostingPlayerId = match["hostingPlayerId"_c_str];
    if (!hostingPlayerId || subjectId != hostingPlayerId) {
        return Promise<Value>{}.reject<Value>(REASON(403, "RemoveTeam: subject is not hosting player"));
    }

    team.Delete();
    
    auto teams = build_array();
    for (auto& i : match["teams"_value]) {
        if (i._ObjectId() != teamId) {
            teams = std::move(teams) << i._ObjectId();
        }
    }
    
    match["teams"] = std::move(teams) << ValueEnd{};
    
    return resolve(Value{});
}


Promise<Value> LobbySupervisor::ProcessAddSlot(const Value& params, const std::string& subjectId) {
    if (subjectId.empty()) {
        return Promise<Value>{}.reject<Value>(REASON(401, "AddSlot: missing subjectId"));
    }
    
    auto teamId = params["team"_ObjectId];
    if (!teamId) {
        return Promise<Value>{}.reject<Value>(REASON(400, "AddSlot: missing 'team' parameter"));
    }
        
    auto team = federate_->getObject(teamId);
    if (!team) {
        return Promise<Value>{}.reject<Value>(REASON(404, "AddSlot: team '%s' not found", teamId.str().c_str()));
    }
    if (!team["slots"].canSetValue()) {
        return Promise<Value>{}.reject<Value>(REASON(503, "AddSlot: can't set team.slots", teamId.str().c_str()));
    }
        
    auto match = FindMatchWithTeam(teamId);
    if (!match) {
        return Promise<Value>{}.reject<Value>(REASON(404, "AddSlot: match for team '%s' not found", teamId.str().c_str()));
    }
    
    const char* hostingPlayerId = match["hostingPlayerId"_c_str];
    // bool hostless = SamuraiLobbySupervisor::IsHostlessMatch(hostingPlayerId);
    // if (!hostless && subjectId != hostingPlayerId) {
    //     return Promise<Value>{}.reject<Value>(REASON(403, "AddSlot: subject is not hosting player"));
    // }
    
    ObjectRef slot{};
    const char* playerId = params["playerId"_c_str];
    if (playerId && *playerId) {
        slot = FindMatchSlotWithPlayerId(match, playerId);
        if (!slot) {
            slot = FindUnassignedTeamSlot(team);
            if (slot) {
                slot["playerId"] = playerId;
            }
        }
    }
    if (!slot) {
        int count = 0;
        auto slots = build_array();
        for (auto& i : team["slots"_value]) {
            slots = std::move(slots) << i._ObjectId();
            ++count;
        }
        if (/*hostless &&*/ count > 1) {
            return Promise<Value>{}.reject<Value>(REASON(403, "AddSlot: maximum number of slots reached"));
        }
        
        slot = federate_->getObjectClass("Slot").create();
        if (playerId && *playerId) {
            slot["playerId"] = playerId;
        }
        team["slots"] = std::move(slots) << slot.getObjectId() << ValueEnd{};
    }
    
    return resolve(Value{});
}


Promise<Value> LobbySupervisor::ProcessRemoveSlot(const Value& params, const std::string& subjectId) {
    if (subjectId.empty()) {
        return Promise<Value>{}.reject<Value>(REASON(401, "RemoveSlot: missing subjectId"));
    }
    
    auto slotId = params["slot"_ObjectId];
    if (!slotId) {
        return Promise<Value>{}.reject<Value>(REASON(400, "RemoveSlot: missing 'slot' parameter"));
    }
    
    auto slot = federate_->getObject(slotId);
    if (!slot) {
        return Promise<Value>{}.reject<Value>(REASON(404, "RemoveSlot: slot '%s' not found", slotId.str().c_str()));
    }
    
    auto team = FindTeamWithSlot(slotId);
    if (!team) {
        return Promise<Value>{}.reject<Value>(REASON(404, "RemoveSlot: team for slot '%s' not found", slotId.str().c_str()));
    }
    
    auto match = FindMatchWithTeam(team.getObjectId());
    if (!match) {
        return Promise<Value>{}.reject<Value>(REASON(404, "RemoveSlot: match for slot '%s' not found", slotId.str().c_str()));
    }
    
    int count = 0;
    auto slots = build_array();
    for (auto& i : team["slots"_value]) {
        if (i._ObjectId() != slotId) {
            slots = std::move(slots) << i._ObjectId();
            ++count;
        }
    }
    
    if (const char* playerId = slot["playerId"_c_str]) {
        if (playerId[0]) {
            for (auto session : federate_->getObjectClass("Session")) {
                if (const char* p = session["playerId"_c_str]) {
                    if (std::strcmp(playerId, p) == 0) {
                        session["ready"] = false;
                    }
                }
            }
        }
    }
    
    if (count != 0 /*|| !SamuraiLobbySupervisor::IsHostlessMatch(match["hostingPlayerId"_c_str])*/ ) {
        team["slots"] = std::move(slots) << ValueEnd{};
        slot.Delete();
    } else {
        slot["playerId"] = nullptr;
    }
    
    return resolve(Value{});
}


Promise<Value> LobbySupervisor::ProcessInvitePlayer(const Value& params, const std::string& subjectId) {
    if (subjectId.empty()) {
        return Promise<Value>{}.reject<Value>(REASON(401, "InvitePlayer: missing subjectId"));
    }
    
    auto slotId = params["slot"_ObjectId];
    if (!slotId) {
        return Promise<Value>{}.reject<Value>(REASON(400, "InvitePlayer: missing 'slot' parameter"));
    }
    
    auto slot = federate_->getObject(slotId);
    if (!slot) {
        return Promise<Value>{}.reject<Value>(REASON(404, "InvitePlayer: slot '%s' not found", slotId.str().c_str()));
    }
    if (!slot["playerId"].canSetValue()) {
        return Promise<Value>{}.reject<Value>(REASON(503, "InvitePlayer: can't set slot.playerId", slotId.str().c_str()));
    }

    const char* playerId = params["playerId"_c_str];
    if (!playerId || !*playerId) {
        return Promise<Value>{}.reject<Value>(REASON(400, "InvitePlayer: missing 'playerId' parameter"));
    }

    auto team = FindTeamWithSlot(slotId);
    if (!team) {
        return Promise<Value>{}.reject<Value>(REASON(404, "InvitePlayer: team for slot '%s' not found", slotId.str().c_str()));
    }

    auto match = FindMatchWithTeam(team.getObjectId());
    if (!match) {
        return Promise<Value>{}.reject<Value>(REASON(404, "InvitePlayer: match for slot '%s' not found", slotId.str().c_str()));
    }

    if (!FindMatchSlotWithPlayerId(match, playerId)) {
        slot["playerId"] = playerId;
    }
    
    return resolve(Value{});
}


Promise<Value> LobbySupervisor::ProcessChatMessage(const Value& params, const std::string& subjectId) {
  const char* message = params["message"_c_str];

#ifdef WARSTAGE_ENABLE_CHATMESSAGE_CRASH
  if (message && std::strcmp(message, "#crash c++") == 0) {
    std::memset(nullptr, 0, 0x10000000);
  }
  if (message && std::strncmp(message, "#throw c++", 10) == 0) {
    throw std::runtime_error(message);
  }
  if (message && std::strncmp(message, "#error c++", 10) == 0) {
    LOG_EXCEPTION(std::runtime_error(message));
    return resolve(Value{});
  }
#endif

  if (subjectId.empty()) {
    return Promise<Value>{}.reject<Value>(REASON(401, "ChatMessage: missing subjectId"));
  }

  if (!FindPlayerSessionWithSubjectId(subjectId)) {
    return Promise<Value>{}.reject<Value>(REASON(403, "ChatMessage: no player session '%s'", subjectId.c_str()));
  }

  federate_->getEventClass("ChatMessage").dispatch(Struct{}
      << "playerId" << subjectId
      << "message" << message
      << "channel" << params["channel"_c_str]
      << "match" << params["match"_ObjectId]
      << "team" << params["team"_ObjectId]
      << ValueEnd{});

  return resolve(Value{});
}


void LobbySupervisor::UpdateTeamPositions(const ObjectRef& match) {
    int position = 1;
    for (auto& i : match["teams"_value]) {
        if (auto t = federate_->getObject(i._ObjectId())) {
            t["position"] = position++;
        }
    }
}


void LobbySupervisor::StartHousekeepingInterval() {
    if (!housekeepingInterval_) {
        housekeepingInterval_ = strand_->setInterval([weak_ = weak_from_this()]() {
          if (auto this_ = weak_.lock()) {
            this_->UpdateMatchTime(5.0);
            this_->TryAcquireOwnership();
            this_->TryDeleteAbandonedMatches();
          }
        }, 5000);
    }
}


void LobbySupervisor::UpdateMatchTime(double deltaTime) {
    for (const auto& match : federate_->getObjectClass("Match")) {
        if (match["started"_bool] && !match["ended"_bool] && match["time"].canSetValue()) {
            ObjectRef{match}["time"] = match["time"_double] + deltaTime;
        }
    }
}


void LobbySupervisor::TryAcquireOwnership() {
    if (!module_.canDelete()) {
        return;
    }

    for (auto object : federate_->getObjectClass("Module")) {
        TryAcquireOwnershipForObject(object);
    }
    for (auto object : federate_->getObjectClass("Match")) {
        TryAcquireOwnershipForObject(object);
    }
    for (auto object : federate_->getObjectClass("Team")) {
        TryAcquireOwnershipForObject(object);
    }
    for (auto object : federate_->getObjectClass("Slot")) {
        TryAcquireOwnershipForObject(object);
    }
    for (auto object : federate_->getObjectClass("Session")) {
        TryAcquireOwnershipForObject(object);
    }
}


void LobbySupervisor::TryAcquireOwnershipForObject(ObjectRef& object) {
    for (auto& property : object.getProperties()) {
        if (property) {
            if (property->getOwnershipState() & OwnershipStateFlag::NotAcquiring) {
                // LOG_W("LobbyFederationServices::UpdateOwnershipForObject: acquire %s", property->Name().c_str());
              property->modifyOwnershipState(OwnershipOperation::OwnershipAcquisition);
            }
        }
    }
}


void LobbySupervisor::TryDeleteDisconnectedPlayerSessions(ObjectId matchId) {
    for (auto session : federate_->getObjectClass("Session")) {
        if (!session["connected"_bool] && session["match"_ObjectId] == matchId && session.canDelete()) {
            LOG_X("delete PlayerSession: obj=%s sub=%s", session.getObjectId().str().c_str(), session["playerId"_c_str] ?: "");
            session.Delete();
        }
    }
}
