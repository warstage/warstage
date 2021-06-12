// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "session-federate.h"
#include "session.h"


SessionFederate::SessionFederate(Runtime& runtime, const char* federateName, std::shared_ptr<Strand_base> strand, Session& session)
    : Federate{runtime, federateName, std::move(strand)}, session_{&session}
{
  LOG_LIFECYCLE("%p SessionFederate + %s", this, federateName_.c_str());
}


SessionFederate::~SessionFederate() {
  LOG_LIFECYCLE("%p SessionFederate ~ %s", this, federateName_.c_str());
  LOG_ASSERT(shutdownCompleted());
}


Promise<void> SessionFederate::shutdown_() {
  auto federationId = getFederationId();
  co_await Federate::shutdown_();
  session_->removeFederation_safe(federationId, *this);
  session_->runtime_->federationProcessRemoved_safe(federationId, session_->getProcessId());
}


void SessionFederate::setOwnershipDisabled(bool value) {
  ownershipDisabled_ = value;
}


void SessionFederate::enterBlock_strand() {
  ++blocks_;
  Federate::enterBlock_strand();
}


void SessionFederate::leaveBlock_strand() {
  Federate::leaveBlock_strand();
  if (--blocks_ == 0) {
    flushMessages();
  }
}


void SessionFederate::objectCallback(ObjectId federationId, ObjectRef object) {
  const char doNotDistributePrefix = session_->getDoNotDistributePrefix_strand();
  if (object.getObjectClass()[0] == doNotDistributePrefix) {
    return;
  }

  auto change = object.justDestroyed() ? ObjectChange::Delete
      : object.justDiscovered() ? ObjectChange::Discover
          : ObjectChange::Update;

  auto builder = Struct{}
      << "m" << static_cast<std::int32_t>(Session::Message::ObjectChanges)
      << "x" << federationId.str()
      << "i" << object.getObjectId()
      << "c" << object.getObjectClass()
      << "t" << static_cast<std::int32_t>(change);

  bool distribute = change != ObjectChange::Update;

  if (change == ObjectChange::Delete) {
    builder = std::move(builder) << "p" << Struct{} << ValueEnd{};
  } else {
    bool allowOwnership = !ownershipDisabled_ && session_->getProcessType() != ProcessType::None;

    if (allowOwnership && object.getOwnershipState() & OwnershipStateFlag::NotAbleToAcquire) {
      object.modifyOwnershipState(OwnershipOperation::Publish);
    }
    auto properties = std::move(builder) << "p" << Struct{};
    for (auto& property : object.getProperties()) {
      if (property) {
        bool shouldDistribute = property->hasChanged()
            && property->routing_
            && property->getName()[0] != doNotDistributePrefix;
        if (shouldDistribute) {
          if (allowOwnership && property->getOwnershipState() & OwnershipStateFlag::NotAbleToAcquire) {
            property->modifyOwnershipState(OwnershipOperation::Publish);
          }
          properties = std::move(properties) << property->getName().c_str() << Struct{}
              << "v" << property->value3_
              << "t" << property->time3_ - currentTime_
              << "p" << property->processId_
              << ValueEnd{};
          distribute = true;
        }
      }
    }
    builder = std::move(properties) << ValueEnd{};
  }

  if (distribute) {
    auto value = std::move(builder) << ValueEnd{};
    enqueueMessage(value);
  }
}


void SessionFederate::eventCallback(ObjectId federationId, const char* eventName, const Value& value) {
  if (eventName[0] == session_->getDoNotDistributePrefix_strand()) {
    return;
  }

  enqueueMessage(Struct{}
      << "m" << static_cast<std::int32_t>(Session::Message::EventDispatch)
      << "x" << federationId.str()
      << "e" << eventName
      << "v" << value
      << "d" << getEventDelay()
      << "t" << getEventLatency()
      << ValueEnd{});
}


Promise<Value> SessionFederate::serviceCallback(ObjectId federationId, const char* serviceName, const Value& value, const std::string& subjectId) {
  if (serviceName[0] == session_->getDoNotDistributePrefix_strand()) {
    return Promise<Value>{}.reject<Value>(Value{});
  }

  auto serviceRequest = session_->generateServiceRequest_strand();
  enqueueMessage(Struct{}
      << "m" << static_cast<std::int32_t>(Session::Message::ServiceRequest)
      << "x" << federationId.str()
      << "s" << serviceName
      << "r" << serviceRequest.first
      << "v" << value
      << "i" << subjectId
      << ValueEnd{});

  return std::move(serviceRequest.second);
}


void SessionFederate::ownershipCallback(ObjectId federationId, ObjectRef object, const Property& property, OwnershipNotification notification) {
  const char doNotDistributePrefix = session_->getDoNotDistributePrefix_strand();
  if (object.getObjectClass()[0] == doNotDistributePrefix || property.getName()[0] == doNotDistributePrefix) {
    return;
  }

  auto message = Session::Message::None;
  switch (notification) {
    case OwnershipNotification::RequestOwnershipAssumption: {
      message = Session::Message::RoutingRequestDownstream;
      break;
    }
    case OwnershipNotification::OwnershipAcquisitionNotification:
    case OwnershipNotification::ForcedOwnershipAcquisitionNotification: {
      message = Session::Message::RoutingEnableDownstream;
      break;
    }
    case OwnershipNotification::RequestOwnershipRelease: {
      message = Session::Message::RoutingRequestUpstream;
      break;
    }
    case OwnershipNotification::OwnershipDivestitureNotification:
    case OwnershipNotification::ForcedOwnershipDivestitureNotification: {
      message = Session::Message::RoutingEnableUpstream;
      break;
    }
    case OwnershipNotification::OwnershipUnavailable: {
      message = Session::Message::RoutingUpstreamDenied;
      break;
    }
    default: {
      break;
    }
  }

  if (message != Session::Message::None) {
    enqueueMessage(Struct{}
        << "m" << static_cast<std::int32_t>(message)
        << "x" << federationId.str()
        << "i" << object.getObjectId()
        << "p" << property.getName()
        << ValueEnd{});
  }
}


void SessionFederate::enqueueMessage(const Value& message) {
  LOG_ASSERT(isFederateStrandCurrent());

  messages_.push_back(message);
  if (!blocks_) {
    flushMessages();
  }
}


void SessionFederate::flushMessages() {
  LOG_ASSERT(isFederateStrandCurrent());

  if (!messages_.empty()) {
    auto builder = build_array();
    for (const auto& message : messages_) {
      builder = std::move(builder) << message;
    }
    auto array = std::move(builder) << ValueEnd{};

    auto value = Struct{}
        << "m" << static_cast<std::int32_t>(Session::Packet::Messages)
        << "mm" << array
        << ValueEnd{};

    session_->trySendOutgoingPacket_strand(value);

    messages_.clear();
  }
}
