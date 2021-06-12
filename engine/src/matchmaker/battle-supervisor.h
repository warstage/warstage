#ifndef WARSTAGE__MATCHMAKER__BATTLE_SUPERVISOR_H
#define WARSTAGE__MATCHMAKER__BATTLE_SUPERVISOR_H

#include "runtime/federate.h"


class BattleSupervisor :
        public Shutdownable,
        public std::enable_shared_from_this<BattleSupervisor>
{
    Runtime* runtime_{};
    ObjectId matchId_{};
    std::shared_ptr<Federate> lobbyFederate_{};
    std::shared_ptr<Federate> battleFederate_{};
    std::shared_ptr<IntervalObject> interval_{};
    
public:
    BattleSupervisor(Runtime& runtime, const char* federateName, std::shared_ptr<Strand> strand);
    ~BattleSupervisor() override;
    
    void Startup(ObjectId lobbyFederationId, ObjectId battleFederationId);

protected: // Shutdownable
    [[nodiscard]] Promise<void> shutdown_() override;

private:
    Promise<Value> ProcessSetCommanders(const Value& params, const std::string& subjectId);
    Promise<Value> ProcessCreateUnits(const Value& params, const std::string& subjectId);
    Promise<Value> ProcessUpdateCommand(const Value& params, const std::string& subjectId);
    Promise<Value> ProcessUpdateObject(const Value& params, const std::string& subjectId);
    Promise<Value> ProcessUpdateObjects(const Value& params, const std::string& subjectId);
    
    void DeleteDeadUnits();
    
    void UpdateCommanderAbandoned();
    void UpdateUnitDelegated();
    bool IsPlayerInMatch(const std::string_view& playerId);
};

#endif
