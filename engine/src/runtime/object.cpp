// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./federate.h"
#include "./object.h"
#include "./object-class.h"
#include "./ownership.h"
#include "./runtime.h"


const char* Property::Destructor_cstr = "~";
const std::string Property::Destructor_str{"~"};


CookieBase::~CookieBase() {
}


MasterProperty& MasterInstance::getProperty(const Property& property) {
  auto& p = properties_[property.propertyName_];
  if (!p) {
    p = std::make_unique<MasterProperty>(property.propertyName_);
    p->buffer_ = property.buffer_;
  }
  return *p;
}


ObjectInstance::ObjectInstance(ObjectClass* objectClass) :
    processId_{objectClass->federate_->getRuntime().getProcessId()},
    objectClass_{objectClass},
    properties_{objectClass->propertySymbols_}
{
}


Property& ObjectInstance::getProperty(const char* propertyName) {
  LOG_ASSERT(objectClass_->federate_->isFederateStrandCurrent());

  auto& p = properties_[propertyName];
  if (!p) {
    p = std::make_unique<Property>(this, propertyName);
    synchronize_ = true;
  }
  return *p;
}


Property& ObjectInstance::getProperty(const std::string& propertyName) {
  LOG_ASSERT(objectClass_->federate_->isFederateStrandCurrent());

  auto& p = properties_[propertyName];
  if (!p) {
    p = std::make_unique<Property>(this, propertyName);
    synchronize_ = true;

  }
  return *p;
}


ObjectId ObjectRef::getObjectId() const {
  LOG_ASSERT(!instance_ || instance_->objectClass_->federate_->isFederateStrandCurrent());
  return instance_ ? instance_->objectId_ : ObjectId{};
}


const std::string& ObjectRef::getObjectClass() const {
  LOG_ASSERT(!instance_ || instance_->objectClass_->federate_->isFederateStrandCurrent());
  static std::string _empty;
  return instance_ ? instance_->objectClass_->className_ : _empty;
}


/*bool ObjectRef::HasProperty(const char* propertyName) const {
    LOG_ASSERT(!_instance || _instance->_objectClass->_federate->AssertFederateStrand());
    
    if (_instance) {
        if (auto p = _instance->_properties.FindValue(propertyName)) {
            return (*p)->_value3.is_defined();
        }
    }
    return false;
}*/


void ObjectRef::Delete() {
  if (instance_) {
    LOG_ASSERT(instance_->objectClass_->federate_->isFederateStrandCurrent());
    instance_->getProperty(Property::Destructor_cstr).assertCanSetValue();
    if (!instance_->deletedByObject_ && !instance_->deletedByMaster_) {
      std::lock_guard federate_lock{instance_->objectClass_->federate_->mutex_};
      instance_->deletedByObject_ = true;
      instance_->objectClass_->federate_->tryScheduleImmediateSynchronize_unsafe();
    }
  }
}


/***/


Property::Property(ObjectInstance* objectInstance, std::string propertyName) :
    objectInstance_{objectInstance},
    propertyName_{std::move(propertyName)} {
}


void Property::setValue(const Value& value, double delay, Session* session, ObjectId processId) {
  assertCanSetValue();
  double time = objectInstance_->objectClass_->federate_->currentTime_ + delay;
  if (session) {
    time = std::max(time, time2_);
    processId_ = processId;
    session_ = session;
  } else if (time >= time3_) {
    processId_ = objectInstance_->processId_;
    session_ =  nullptr;
  } else {
    return;
  }

  prepareBuffer();
  buffer_->add_byte(static_cast<unsigned char>(value.type()));
  buffer_->add_byte(0);
  buffer_->add_binary(value.data(), value.size());
  prepareBufferDone(time, true);
}


const char* Property::getObjectClass() const {
  assert(objectInstance_);
  return objectInstance_->objectClass_->className_.c_str();
}


const Value& Property::getValue() const {
  double time = objectInstance_->objectClass_->federate_->currentTime_;
  return time < time2_ ? value1_ : time < time3_ ? value2_ : value3_;
}


double Property::getTime() const {
  double time = objectInstance_->objectClass_->federate_->currentTime_;
  return (time < time2_ ? time1_ : time < time3_ ? time2_ : time3_) - time;
}


int Property::getVersion() const {
  double time = objectInstance_->objectClass_->federate_->currentTime_;
  return time < time2_ ? version1_ : time < time3_ ? version2_ : version3_;
}


TimedValue Property::getTimedValue() const {
  double time = objectInstance_->objectClass_->federate_->currentTime_;
  return time < time3_
      ? TimedValue{time1_ - time, time2_ - time, value1_, value2_}
      : TimedValue{time2_ - time, time3_ - time, value2_, value3_};
}


bool Property::hasDelayedChange() const {
  return time3_ >= objectInstance_->objectClass_->federate_->currentTime_;
}


void Property::modifyOwnershipState(OwnershipOperation operation) {
  LOG_X("%s.%s %s %s.%s",
      str(objectInstance_->objectClass_->federate_->getRuntime().getProcessType()),
      objectInstance_->objectClass_->federate_->getFederateName().c_str(),
      str(operation),
      objectInstance_->objectClass_->className_.c_str(),
      propertyName_.c_str());
  /*if (_propertyName == "path") {
      LOG_I("%s ModifyOwnershipState path %p %s %s", _objectInstance->_objectClass->_federate->GetDescription().c_str(), _objectInstance, _instanceOwnership.first.str().c_str(), str(operation));
  }*/

  LOG_ASSERT_FORMAT(validateStateBeforeOperation(instanceOwnership_.first, operation), "%s[%s].%s %s:%s",
      objectInstance_->objectClass_->className_.c_str(),
      objectInstance_->objectId_.str().c_str(),
      propertyName_.c_str(),
      instanceOwnership_.first.str().c_str(),
      str(instanceOwnership_.second));
  updateOwnershipState(instanceOwnership_, operation);
  objectInstance_->synchronize_ = true;
  objectInstance_->objectClass_->federate_->tryScheduleImmediateSynchronize_unsafe();
}


Property& Property::operator=(bool value) {
  assertCanSetValue();
  double time = objectInstance_->objectClass_->federate_->currentTime_;
  if (time < time3_)
    return *this;
  if (value3_.is_boolean() && value3_._bool() == value)
    return *this;
  processId_ = objectInstance_->processId_;
  session_ = nullptr;
  prepareBuffer();
  buffer_->add_byte(static_cast<unsigned char>(ValueType::_boolean));
  buffer_->add_byte(0);
  buffer_->add_byte(static_cast<unsigned char>(value ? 1 : 0));
  prepareBufferDone(time, true);
  return *this;
}


Property& Property::operator=(int value) {
  assertCanSetValue();
  double time = objectInstance_->objectClass_->federate_->currentTime_;
  if (time < time3_)
    return *this;
  if (value3_.is_int32() && value3_._int() == value)
    return *this;
  processId_ = objectInstance_->processId_;
  session_ = nullptr;
  prepareBuffer();
  buffer_->add_byte(static_cast<unsigned char>(ValueType::_int32));
  buffer_->add_byte(0);
  buffer_->add_int32(static_cast<std::int32_t>(value));
  prepareBufferDone(time, true);
  return *this;
}


Property& Property::operator=(double value) {
  assertCanSetValue();
  double time = objectInstance_->objectClass_->federate_->currentTime_;
  if (time < time3_)
    return *this;
  if (value3_.is_double() && value3_._double() == value)
    return *this;
  processId_ = objectInstance_->processId_;
  session_ = nullptr;
  prepareBuffer();
  buffer_->add_byte(static_cast<unsigned char>(ValueType::_double));
  buffer_->add_byte(0);
  buffer_->add_double(value);
  prepareBufferDone(time, true);
  return *this;
}

Property& Property::operator=(const char* value) {
  assertCanSetValue();
  double time = objectInstance_->objectClass_->federate_->currentTime_;
  if (time < time3_)
    return *this;
  if (value ? (value3_.is_string() && std::strcmp(value3_._c_str(), value) == 0) : value3_.is_null())
    return *this;
  processId_ = objectInstance_->processId_;
  session_ = nullptr;
  prepareBuffer();
  if (value) {
    buffer_->add_byte(static_cast<unsigned char>(ValueType::_string));
    buffer_->add_byte(0);
    buffer_->add_int32(static_cast<std::int32_t>(std::strlen(value) + 1));
    buffer_->add_string(value);
  } else {
    buffer_->add_byte(static_cast<unsigned char>(ValueType::_null));
    buffer_->add_byte(0);
  }
  prepareBufferDone(time, true);
  return *this;
}

Property& Property::operator=(const std::string& value) {
  assertCanSetValue();
  double time = objectInstance_->objectClass_->federate_->currentTime_;
  if (time < time3_)
    return *this;
  if (value3_.is_string() && value == value3_._c_str())
    return *this;
  processId_ = objectInstance_->processId_;
  session_ = nullptr;
  prepareBuffer();
  buffer_->add_byte(static_cast<unsigned char>(ValueType::_string));
  buffer_->add_byte(0);
  buffer_->add_int32(static_cast<std::int32_t>(value.size() + 1));
  buffer_->add_binary(value.data(), value.size());
  buffer_->add_byte(0);
  prepareBufferDone(time, true);
  return *this;
}

Property& Property::operator=(ObjectId value) {
  assertCanSetValue();
  double time = objectInstance_->objectClass_->federate_->currentTime_;
  if (time < time3_)
    return *this;
  if (value3_.is_ObjectId() && value3_._ObjectId() == value)
    return *this;
  processId_ = objectInstance_->processId_;
  session_ = nullptr;
  prepareBuffer();
  if (value) {
    buffer_->add_byte(static_cast<unsigned char>(ValueType::_ObjectId));
    buffer_->add_byte(0);
    buffer_->add_binary(value.data(), value.size());
  } else {
    buffer_->add_byte(static_cast<unsigned char>(ValueType::_null));
    buffer_->add_byte(0);
  }
  prepareBufferDone(time, true);
  return *this;
}


Property& Property::operator=(Binary value) {
  assertCanSetValue();
  double time = objectInstance_->objectClass_->federate_->currentTime_;
  if (time < time3_)
    return *this;
  processId_ = objectInstance_->processId_;
  session_ = nullptr;
  prepareBuffer();
  buffer_->add_byte(static_cast<unsigned char>(ValueType::_binary));
  buffer_->add_byte(0);
  buffer_->add_int32(static_cast<std::int32_t>(value.size));
  buffer_->add_byte(0);
  buffer_->add_binary(value.data, value.size);
  prepareBufferDone(time, true);
  return *this;
}


void Property::assertCanSetValue() {
  LOG_ASSERT(objectInstance_);
  LOG_ASSERT_FORMAT(canSetValue(), "%s[%s].%s %s:%s",
      objectInstance_->objectClass_->className_.c_str(),
      objectInstance_->objectId_.str().c_str(),
      propertyName_.c_str(),
      instanceOwnership_.first.str().c_str(),
      str(instanceOwnership_.second));
}


void Property::assign(const MasterProperty& other) {
  LOG_ASSERT(other.buffer_);
  processId_ = other.processId_;
  session_ = other.session_;
  prepareBuffer();
  buffer_->value_.append(reinterpret_cast<const char*>(other.buffer_->data()), other.buffer_->size());

  double time = other.time_ + objectInstance_->objectClass_->federate_->currentTime_;

  prepareBufferDone(std::max(time2_, time), false);
  version3_ = other.version_;
}


void Property::prepareBuffer() {
  LOG_ASSERT(objectInstance_->objectClass_->federate_->isFederateStrandCurrent());

  if (buffer_ && buffer_.unique())
    buffer_->value_.resize(0);
  else
    buffer_ = std::make_shared<ValueBuffer>();
}


void Property::prepareBufferDone(double time, bool synchronize) {
  if (objectInstance_->objectClass_->federate_->currentTime_ >= time2_) {
    time1_ = time2_;
    time2_ = time3_;
    std::swap(value1_, value2_);
    std::swap(value2_, value3_);
    version1_ = version2_;
    version2_ = version3_;
  }

  auto ptr = buffer_->value_.data();
  auto end = ptr + buffer_->value_.size();
  value3_ = Value{buffer_, ptr, end};
  time3_ = time;

  if (instanceOwnership_.first == OwnershipState{}) {
    if (synchronize) {
      instanceOwnership_.first = OwnershipState{}
          + OwnershipStateFlag::Owned
          + OwnershipStateFlag::NotDivesting
          + OwnershipStateFlag::NotAskedToRelease;
    } else if (objectInstance_->objectClass_->getPropertyInfo(propertyName_).published_) {
      instanceOwnership_.first = OwnershipState{}
          + OwnershipStateFlag::Unowned
          + OwnershipStateFlag::AbleToAcquire
          + OwnershipStateFlag::NotAcquiring
          + OwnershipStateFlag::NotTryingToAcquire;
    } else {
      instanceOwnership_.first = OwnershipState{}
          + OwnershipStateFlag::Unowned
          + OwnershipStateFlag::NotAbleToAcquire;
    }
  }

  if (synchronize) {
    ++version3_;
    objectInstance_->synchronize_ = true;
    std::lock_guard federate_lock{objectInstance_->objectClass_->federate_->mutex_};
    objectInstance_->objectClass_->federate_->tryScheduleImmediateSynchronize_unsafe();
  }
}


void MasterProperty::assign(const Property& other) {
  processId_ = other.processId_;
  session_ = other.session_;
  if (buffer_ && buffer_.unique())
    buffer_->value_.resize(0);
  else
    buffer_ = std::make_shared<ValueBuffer>();
  buffer_->value_.append(reinterpret_cast<const char*>(other.buffer_->data()), other.buffer_->size());

  auto ptr = buffer_->value_.data();
  auto end = ptr + buffer_->value_.size();
  value_ = Value{buffer_, ptr, end};
  time_ = other.time3_ - other.objectInstance_->objectClass_->federate_->currentTime_;
  version_ = other.version3_;
}


/***/


void ObjectRef::modifyOwnershipState(OwnershipOperation operation) {
  if (!instance_)
    return LOG_W("modifyOwnershipState: no instance");
  instance_->getProperty(Property::Destructor_cstr).modifyOwnershipState(operation);
}
