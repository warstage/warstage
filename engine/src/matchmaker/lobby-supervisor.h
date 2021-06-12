#ifndef WARSTAGE__MATCHMAKER__LOBBY_SUPERVISOR_H
#define WARSTAGE__MATCHMAKER__LOBBY_SUPERVISOR_H

#include "runtime/runtime.h"

class Session;


class LobbySupervisor :
        public Shutdownable,
        public RuntimeObserver,
        public std::enable_shared_from_this<LobbySupervisor>
{
protected: // should be private
    const std::shared_ptr<Federate> federate_{};
    const std::shared_ptr<Strand_base> strand_; // Strand::getMain()
    const std::string moduleUrl_;
    std::unordered_map<ObjectId, Federation*> battleFederations_{};
    std::shared_ptr<IntervalObject> housekeepingInterval_{};
    ObjectRef module_{};
    int moduleServerCount_{};
    bool hasDefinedLobbyServices_ = false;

public:
    LobbySupervisor(Runtime& runtime, const char* federateName, std::shared_ptr<Strand_base> strand, std::string moduleUrl);
    ~LobbySupervisor() override;
    
    virtual void Startup(ObjectId federationId);
    void StartupForTest(ObjectId federationId);

    ObjectId getFederationId() const { return federate_->getFederationId(); }

protected: // Shutdownable
    [[nodiscard]] Promise<void> shutdown_() override;

protected: // RuntimeObserver
    void onProcessAdded_main(ObjectId federationId, ObjectId processId, ProcessType processType) override;
    void onProcessRemoved_main(ObjectId federationId, ObjectId processId) override;
    void onProcessAuthenticated_main(ObjectId processId, const ProcessAuth& processAuth) override;

protected: // should be private:
    void OwnershipCallback(const ObjectRef& object, Property& property, OwnershipNotification notification);

    void CreateModule();
    void OnModuleChanged(const ObjectRef& matchmaker);
    void TryAcquireOrReleaseModuleOwnership();
    bool HasModuleOwnership() const;
    bool ShouldHaveModuleOwnership() const;
    void TryDefineOrUndefineLobbyServices();
    void DefineLobbyServices();
    void UndefineLobbyServices();

    void UpdateModuleServerCountAndOnline(int delta);
    static void UpdateMatchServerCountAndOnline(ObjectRef& match, int delta);

    void RegisterPlayerSession(ObjectId processId);
    void UnregisterPlayerSession(ObjectId processId);

protected: // should be private
    ObjectRef FindPlayerSessionWithProcessId(ObjectId processId);
    ObjectRef FindPlayerSessionWithSubjectId(const std::string& subjectId, ObjectId excludedSessionId = ObjectId{});
    ObjectRef FindMatchWithTeam(ObjectId teamId);
    ObjectRef FindTeamWithSlot(ObjectId slotId);
    ObjectRef FindMatchSlotWithPlayerId(const ObjectRef& match, const char* playerId);
    ObjectRef FindTeamSlotWithPlayerId(const ObjectRef& team, const char* playerId);
    ObjectRef FindUnassignedMatchSlot(const ObjectRef& team);
    ObjectRef FindUnassignedTeamSlot(const ObjectRef& team);

    bool HasPlayerJoinedMatch(const char* playerId, ObjectId matchId);
    bool IsPlayerReadyForMatch(const char* playerId, ObjectId matchId);
    bool ForAllPlayersInMatch(const ObjectRef& match, std::function<bool(const char* playerId)> predicate);
    bool ShouldStartMatch(const ObjectRef& match);
    std::vector<std::string> GetMatchPlayerIds(const ObjectRef& match);
    
    void UnassignSlotsWithPlayerId(const ObjectRef& match, const std::string& playerId);
    virtual bool TryStartMatch(ObjectRef& match);
    
    void TryDeleteAbandonedMatches();
    bool IsMatchAbandoned(const ObjectRef& match);
    void TryDeleteEmptyMatches();
    bool IsMatchEmpty(const ObjectRef& match);
    void DeleteMatch(ObjectRef match);
    void ResetSessionsWithMatch(ObjectId matchId);
    void ReleaseBattleFederation(ObjectId matchId);
    void UpdatePlayerMatch(const std::string& subjectId, ObjectId matchId, bool ready);
    
private:
    void OnSessionChanged(ObjectRef& session);
    void OnSessionChangedReadyOrMatch(ObjectRef& session);
    void OnMatchChanged(ObjectRef& match);
    void OnTeamChanged(ObjectRef& team);
    void OnSlotChanged(ObjectRef& slot);

private:
    Promise<Value> ProcessPlayerReady(const Value& params, const std::string& subjectId);
    Promise<Value> ProcessPlayerUnready(const Value& params, const std::string& subjectId);
    
    Promise<Value> ProcessCreateMatch(const Value& params, const std::string& subjectId);
    Promise<Value> ProcessHostMatch(const Value& params, const std::string& subjectId);
    Promise<Value> ProcessUpdateMatch(const Value& params, const std::string& subjectId);
    Promise<Value> ProcessLeaveMatch(const Value& params, const std::string& subjectId);
    Promise<Value> ProcessJoinMatchAsParticipant(const Value& params, const std::string& subjectId);
    Promise<Value> ProcessJoinMatchAsSpectator(const Value& params, const std::string& subjectId);
    
    Promise<Value> ProcessAddTeam(const Value& params, const std::string& subjectId);
    Promise<Value> ProcessUpdateTeam(const Value& params, const std::string& subjectId);
    Promise<Value> ProcessRemoveTeam(const Value& params, const std::string& subjectId);
    
    Promise<Value> ProcessAddSlot(const Value& params, const std::string& subjectId);
    Promise<Value> ProcessRemoveSlot(const Value& params, const std::string& subjectId);
    Promise<Value> ProcessInvitePlayer(const Value& params, const std::string& subjectId);
    
    Promise<Value> ProcessChatMessage(const Value& params, const std::string& subjectId);

protected: // should be private
    void UpdateTeamPositions(const ObjectRef& match);
    
protected: // should be private
    void StartHousekeepingInterval();
    void UpdateMatchTime(double deltaTime);
    void TryAcquireOwnership();
    static void TryAcquireOwnershipForObject(ObjectRef& object);

    void TryDeleteDisconnectedPlayerSessions(ObjectId matchId);
};


#endif
