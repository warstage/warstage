// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__RUNTIME__SESSION_FEDERATE_H
#define WARSTAGE__RUNTIME__SESSION_FEDERATE_H

#include "./federate.h"
#include "./runtime.h"


class SessionFederate : public Federate {
  friend class Session;

  Session* const session_{};
  int blocks_{};
  std::vector<Value> messages_{};
  bool ownershipDisabled_{};

public:
  SessionFederate(Runtime& runtime, const char* federateName, std::shared_ptr<Strand_base> strand, Session& session);
  ~SessionFederate() override;

protected: // Shutdownable
  [[nodiscard]] Promise<void> shutdown_() override;

public:
  void setOwnershipDisabled(bool value);

  void enterBlock_strand() override; // AssertFederateStrand
  void leaveBlock_strand() override; // AssertFederateStrand

  void objectCallback(ObjectId federationId, ObjectRef object);
  void eventCallback(ObjectId federationId, const char* eventName, const Value& value);
  [[nodiscard]] Promise<Value> serviceCallback(ObjectId federationId, const char* serviceName, const Value& value, const std::string& subjectId);
  void ownershipCallback(ObjectId federationId, ObjectRef object, const Property& property, OwnershipNotification notification);

  void enqueueMessage(const Value& message); // AssertFederateStrand
  void flushMessages(); // AssertFederateStrand
};


#endif
