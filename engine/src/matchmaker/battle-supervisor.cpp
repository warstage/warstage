#include "./battle-supervisor.h"


BattleSupervisor::BattleSupervisor(Runtime& runtime, const char* federateName, std::shared_ptr<Strand> strand) :
    runtime_{&runtime} {
    battleFederate_ = std::make_shared<Federate>(runtime, federateName, strand);
    lobbyFederate_ = std::make_shared<Federate>(runtime, federateName, strand);
}


BattleSupervisor::~BattleSupervisor() {
    if (interval_) {
      clearInterval(*interval_);
    }
}


void BattleSupervisor::Startup(ObjectId lobbyFederationId, ObjectId battleFederationId) {
    matchId_ = battleFederationId;
    
    auto weak_ = weak_from_this();

  battleFederate_->getObjectClass("Alliance").publish({"~", "teamId", "position"});
  battleFederate_->getObjectClass("Commander").publish({"~", "alliance", "playerId"});
  battleFederate_->getObjectClass("Unit").publish(
      {"~", "commander", "alliance", "unitType", "marker", "stats.unitClass", "stats.fighterCount", "stats.placement"});

    battleFederate_->getServiceClass("PingBattleServices").define([weak_](const Value& params, const std::string& subjectId) {
        return weak_.expired() ? Promise<Value>{}.reject<Value>(Value{}) : resolve(Value{});
    });

    battleFederate_->getServiceClass("SetCommanders").define([weak_](const Value& params, const std::string& subjectId) {
        auto this_ = weak_.lock();
        return this_ ? this_->ProcessSetCommanders(params, subjectId) : Promise<Value>{}.reject<Value>(Value{});
    });
    
    battleFederate_->getServiceClass("CreateUnits").define([weak_](const Value& params, const std::string& subjectId) {
        auto this_ = weak_.lock();
        return this_ ? this_->ProcessCreateUnits(params, subjectId) : Promise<Value>{}.reject<Value>(Value{});
    });

    battleFederate_->getServiceClass("UpdateCommand").define([weak_](const Value& params, const std::string& subjectId) {
        auto this_ = weak_.lock();
        return this_ ? this_->ProcessUpdateCommand(params, subjectId) : Promise<Value>{}.reject<Value>(Value{});
    });

    battleFederate_->getServiceClass("UpdateObject").define([weak_](const Value& params, const std::string& subjectId) {
        auto this_ = weak_.lock();
        return this_ ? this_->ProcessUpdateObject(params, subjectId) : Promise<Value>{}.reject<Value>(Value{});
    });
    
    battleFederate_->getServiceClass("UpdateObjects").define([weak_](const Value& params, const std::string& subjectId) {
        auto this_ = weak_.lock();
        return this_ ? this_->ProcessUpdateObjects(params, subjectId) : Promise<Value>{}.reject<Value>(Value{});
    });

  battleFederate_->startup(battleFederationId);
    
    /***/

  lobbyFederate_->startup(lobbyFederationId);
    
    /***/
    
    interval_ = Strand::getMain()->setInterval([weak_]() {
      if (auto this_ = weak_.lock()) {
        this_->DeleteDeadUnits();

        this_->UpdateCommanderAbandoned();
        this_->UpdateUnitDelegated();
      }
    }, 250);
}


Promise<void> BattleSupervisor::shutdown_() {
    co_await battleFederate_->shutdown();
    co_await lobbyFederate_->shutdown();
}


Promise<Value> BattleSupervisor::ProcessSetCommanders(const Value& params, const std::string& subjectId) {
    for (const auto& item : params["alliances"_value]) {
        auto object = battleFederate_->getObjectClass("Alliance").create(item["_id"_ObjectId]);
        object["position"] = item["position"_int];
    }
    
    for (const auto& item : params["commanders"_value]) {
        auto object = battleFederate_->getObjectClass("Commander").create(item["_id"_ObjectId]);
        object["alliance"] = item["alliance"_ObjectId];
        object["playerId"] = item["playerId"_c_str];
    }
    
    return resolve(Value{});
}


Promise<Value> BattleSupervisor::ProcessCreateUnits(const Value& params, const std::string& subjectId) {
    for (const auto& item : params["units"_value]) {
        auto object = battleFederate_->getObjectClass("Unit").create(item["_id"_ObjectId]);
        object["commander"] = item["commander"_ObjectId];
        object["alliance"] = item["alliance"_ObjectId];
        object["stats.unitClass"] = item["stats"]["unitClass"_c_str];
        object["stats.fighterCount"] = item["stats"]["fighterCount"_int];
        object["stats.placement"] = item["placement"_vec3];
    }
    
    return resolve(Value{});
}


Promise<Value> BattleSupervisor::ProcessUpdateCommand(const Value& params, const std::string& subjectId) {
  battleFederate_->getEventClass("Command").dispatch(params);
    return resolve(Value{});
}


Promise<Value> BattleSupervisor::ProcessUpdateObject(const Value& params, const std::string& subjectId) {
    ObjectRef object{};
    if (const char* create = params["_create"_c_str]) {
        object = battleFederate_->getObjectClass(create).create(params["_id"_ObjectId] ?: ObjectId::create());
    } else  {
        object = battleFederate_->getObject(params["_id"_ObjectId]);
        if (object && params["_delete"_bool]) {
            object.Delete();
            object = ObjectRef{};
        }
    }
    if (object) {
        for (const auto& p : params) {
            if (p.name()[0] != '_') {
                auto& property = object.getProperty(p.name());
                if (property.canSetValue()) {
                  property.setValue(p);
                } else {
                    // error ???
                }
            }
        }
    }

    return resolve(Struct{} << "_id" << object.getObjectId() << ValueEnd{});
}



Promise<Value> BattleSupervisor::ProcessUpdateObjects(const Value& params, const std::string& subjectId) {
    auto result = Struct{} << "result" << Array{};
    for (const auto& item : params["objects"_value]) {
        ObjectRef object{};
        if (const char* create = item["_create"_c_str]) {
            object = battleFederate_->getObjectClass(create).create(item["_id"_ObjectId] ?: ObjectId::create());
        } else  {
            object = battleFederate_->getObject(item["_id"_ObjectId]);
            if (object && item["_delete"_bool]) {
                object.Delete();
                object = ObjectRef{};
            }
        }
        if (object) {
            for (const auto& p : item) {
                if (p.name()[0] != '_') {
                    auto& property = object.getProperty(p.name());
                    if (property.canSetValue()) {
                      property.setValue(p);
                    } else {
                        // error ???
                    }
                }
            }
        }
        result = std::move(result) << object.getObjectId();
    }
    
    return resolve(std::move(result) << ValueEnd{} << ValueEnd{});
}


void BattleSupervisor::DeleteDeadUnits() {
    for (auto unit : battleFederate_->getObjectClass("Unit")) {
        if (unit.canDelete() && unit["fighters"_value].is_defined() && !unit["fighters"_value].is_array()) {
            unit.Delete();
        }
    }
}


void BattleSupervisor::UpdateCommanderAbandoned() {
    for (auto commander : battleFederate_->getObjectClass("Commander")) {
        if (commander["abandoned"].canSetValue()) {
            const char* playerId = commander["playerId"_c_str];
            auto match = lobbyFederate_->getObject(matchId_);
            commander["abandoned"] = match && match["started"_bool]
                    && (!playerId || !IsPlayerInMatch(playerId));
        }
    }
}


bool BattleSupervisor::IsPlayerInMatch(const std::string_view& playerId) {
    if (playerId == "$")
        return true;
    
    for (const auto& session : lobbyFederate_->getObjectClass("Session")) {
        if (const char* s = session["playerId"_c_str]) {
            if (playerId == s) {
                if (session["connected"_bool] && session["match"_ObjectId] == matchId_)
                    return true;
            }
        }
    }
    
    return false;
}


void BattleSupervisor::UpdateUnitDelegated() {
    for (auto unit : battleFederate_->getObjectClass("Unit")) {
        if (unit["delegated"].canSetValue()) {
            auto commanderId = unit["commander"_ObjectId];
            auto commander = battleFederate_->getObject(commanderId);
            unit["delegated"] = !commander || commander["abandoned"_bool];
        }
    }
}
