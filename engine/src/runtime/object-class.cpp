// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./object-class.h"
#include "./federate.h"


ObjectRef ObjectIterator::operator*() const {
  LOG_ASSERT(federate_.isFederateStrandCurrent());
  return index_ != static_cast<std::size_t>(-1) && index_ < federate_.objectInstances_.size()
      ? ObjectRef{federate_.objectInstances_[index_]}
      : ObjectRef{};
}


const ObjectIterator& ObjectIterator::operator++() {
  index_ = objectClass_.next(index_ + 1);
  return *this;
}


/***/


ObjectClass::ObjectClass(Federate& federate, std::string name) :
    federate_{&federate},
    className_{std::move(name)} {
}


std::size_t ObjectClass::next(std::size_t index) const {
  LOG_ASSERT(federate_->isFederateStrandCurrent());
  auto size = federate_->objectInstances_.size();
  while (index < size) {
    auto object = federate_->objectInstances_[index];
    if (object->objectClass_ == this && !object->deletedByObject_ && !object->deletedByMaster_)
      return index;
    ++index;
  }
  return static_cast<std::size_t>(-1);
}


ObjectClass::PropertyInfo& ObjectClass::getPropertyInfo(const std::string& propertyName) {
  auto& propertyInfo = properties_[propertyName];
  if (propertyInfo.name_.empty())
    propertyInfo.name_ = propertyName;
  return propertyInfo;
}


void ObjectClass::publishProperty(const char* propertyName) {
  auto& property = properties_[propertyName];
  property.name_ = propertyName;
  property.published_ = true;

  for (auto& objectInstance : federate_->objectInstances_) {
    auto& objectProperty = objectInstance->getProperty(propertyName);
    if (objectProperty.getOwnershipState() & OwnershipStateFlag::NotAbleToAcquire) {
      objectProperty.modifyOwnershipState(OwnershipOperation::Publish);
    }
  }
}


void ObjectClass::require(std::initializer_list<const char*> propertyNames) {
  for (const char* propertyName : propertyNames) {
    auto& property = properties_[propertyName];
    property.name_ = propertyName;
    property.required_ = true;
  }
}


void ObjectClass::publish(std::initializer_list<const char*> propertyNames) {
  for (const char* propertyName : propertyNames) {
    publishProperty(propertyName);
  }
}


void ObjectClass::observe(std::function<void(ObjectRef)> observer) {
  std::lock_guard federate_lock{federate_->mutex_};
  observers_.push_back(observer);
}


ObjectRef ObjectClass::create(ObjectId objectId) {
  LOG_ASSERT(objectId);
  LOG_ASSERT(federate_->isFederateStrandCurrent());
  assert(!federate_->shutdownStarted());

  auto existingObject = federate_->getObject(objectId);
  if (existingObject) {
    LOG_ASSERT(!existingObject);
    return existingObject;
  }

  auto objectInstance = std::shared_ptr<ObjectInstance>(new ObjectInstance{this});
  objectInstance->objectId_ = objectId;
  objectInstance->objectClass_ = this;
  objectInstance->synchronize_ = true;
  objectInstance->discoveredAndNotified_ = true;
  objectInstance->getProperty(Property::Destructor_cstr).instanceOwnership_.first = OwnershipState{}
      + OwnershipStateFlag::Owned
      + OwnershipStateFlag::NotDivesting
      + OwnershipStateFlag::NotAskedToRelease;

  federate_->objectInstances_.push_back(objectInstance);

  {
    std::lock_guard federate_lock{federate_->mutex_};
    federate_->tryScheduleImmediateSynchronize_unsafe();
  }

  ObjectRef object;
  object.instance_ = objectInstance;
  return object;
}


ObjectRef ObjectClass::find(std::function<bool(ObjectRef)> predicate) {
  LOG_ASSERT(federate_->isFederateStrandCurrent());

  for (auto& instance : federate_->objectInstances_) {
    if (instance->objectClass_ == this) {
      auto object = ObjectRef{instance};
      if (predicate(object))
        return object;
    }
  };
  return ObjectRef();
}


ObjectIterator ObjectClass::begin() {
  LOG_ASSERT(federate_->isFederateStrandCurrent());
  return ObjectIterator{*federate_, *this, next(0)};
}


ObjectIterator ObjectClass::end() {
  LOG_ASSERT(federate_->isFederateStrandCurrent());
  return ObjectIterator{*federate_, *this, static_cast<std::size_t>(-1)};
}
