// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__RUNTIME__OBJECT_H
#define WARSTAGE__RUNTIME__OBJECT_H

#include "./ownership.h"
#include "value/dictionary.h"
#include "async/promise.h"
#include "value/value.h"
#include "utilities/logging.h"
#include <array>
#include <mutex>
#include "async/promise.h"

class Federate;
class ObjectClass;
class ObjectRef;
struct ObjectInstance;
struct MasterProperty;
class Session;


struct CookieBase {
  virtual ~CookieBase();
};

template <typename T> struct CookieType : public CookieBase {
  T value;
};

struct TimedValue {
  double t1;
  double t2;
  const Value& v1;
  const Value& v2;
};

class Property  {
  friend class Federate;
  friend struct MasterInstance;
  friend class ObjectClass;
  friend struct ObjectInstance;
  friend class ObjectRef;
  friend class Session;
  friend class SessionFederate;
  friend struct MasterProperty;

  friend bool hasPublisher(const OwnershipMap& ownership);
  friend std::string validateOwnership(const OwnershipMap& ownership);
  friend Property* findPotentialOwnerFederate(const OwnershipMap& ownership, OwnershipStateFlag flag);
  friend Property* findOwnerFederate(const OwnershipMap& ownership);
  friend void updateOwnershipNotifications(OwnershipMap& ownership, Property& property, OwnershipOperation operation);
  friend void beforeUpdateOwnership(OwnershipMap& ownership);
  friend void afterUpdateOwnership(OwnershipMap& ownership, Property& property, OwnershipOperation operation, const char* file, int line);
  friend Property* makePropertyMock(OwnershipState state);
  friend OwnershipNotification testOwnershipNotification(const Property*);

  ObjectInstance* objectInstance_{};
  MasterProperty* masterProperty_{};
  std::string propertyName_{};
  std::shared_ptr<ValueBuffer> buffer_{};

  int ownershipVersion_{};
  OwnershipStateOperation instanceOwnership_{};

  OwnershipStateNotification masterOwnership_{};
  OwnershipStateNotification masterOwnershipBefore_{};

  ObjectId processId_{};
  Session* session_{};
  bool routing_{true};

  bool changed_{};
  double time1_{};
  double time2_{};
  double time3_{};
  Value value1_{};
  Value value2_{};
  Value value3_{};
  int version1_{};
  int version2_{};
  int version3_{};

public:
  static const char* Destructor_cstr;
  static const std::string Destructor_str;

  Property(ObjectInstance* objectInstance, std::string propertyName);

public:
  [[nodiscard]] const std::string& getName() const { return propertyName_; }
  [[nodiscard]] const char* getObjectClass() const;

  [[nodiscard]] bool hasChanged() const { return changed_; }
  [[nodiscard]] const Value& getValue() const;
  [[nodiscard]] double getTime() const;
  [[nodiscard]] int getVersion() const;
  [[nodiscard]] TimedValue getTimedValue() const;
  [[nodiscard]] bool hasDelayedChange() const;

  [[nodiscard]] OwnershipState getOwnershipState() const { return instanceOwnership_.first; }
  void modifyOwnershipState(OwnershipOperation operation);

  [[nodiscard]] bool canSetValue() const {
    return !(instanceOwnership_.first & OwnershipStateFlag::Unowned);
  }
  void setValue(const Value& value, double delay = 0.0, Session* session = nullptr, ObjectId processId = ObjectId{});

  Property& operator=(const Value& value) {
    setValue(value);
    return *this;
  }

  Property& operator=(bool value);
  Property& operator=(int value);
  Property& operator=(double value);
  Property& operator=(const char* value);
  Property& operator=(const std::string& value);
  Property& operator=(ObjectId value);
  Property& operator=(Binary value);

  Property& operator=(glm::vec2 value) {
    setValue(*(Struct{} << "" << value << ValueEnd{}).begin());
    return *this;
  }
  Property& operator=(glm::vec3 value) {
    setValue(*(Struct{} << "" << value << ValueEnd{}).begin());
    return *this;
  }
  Property& operator=(const std::array<float, 25>& value) {
    setValue(*(Struct{} << "" << value << ValueEnd{}).begin());
    return *this;
  }
  Property& operator=(const std::vector<glm::vec2>& value) {
    setValue(*(Struct{} << "" << value << ValueEnd{}).begin());
    return *this;
  }

private:
  void assertCanSetValue();
  void assign(const MasterProperty& other);
  void prepareBuffer(); // AssertFederateStrand
  void prepareBufferDone(double time, bool synchronize);
};

struct MasterProperty {
  std::string propertyName_{};
  std::shared_ptr<ValueBuffer> buffer_{};
  bool syncFlag_{};

  OwnershipMap ownershipMap_{};
  int ownershipVersion_{};
  Property* owner_{};
  ObjectId processId_{};
  Session* session_{};

  double time_{};
  Value value_{};
  int version_{};

  explicit MasterProperty(std::string propertyName) : propertyName_{std::move(propertyName)} {}
  void assign(const Property& other);
};

struct MasterInstance {
  int instanceId_{};
  ObjectId objectId_{};
  int refCount_{};
  bool deleted_{};
  std::string objectClassName_{};

  Dictionary<std::unique_ptr<MasterProperty>> properties_{};
  std::unique_ptr<CookieBase> shared_{};
  std::mutex sharedMutex_{};

  MasterProperty& getProperty(const Property& property);
};

struct ObjectInstance {
  const ObjectId processId_;
  ObjectClass* objectClass_;
  ValueTable<std::unique_ptr<Property>> properties_;
  MasterInstance* masterInstance_{};
  ObjectId objectId_{};
  bool spurious_{};

  bool deletedByObject_{};
  bool deletedByMaster_{};
  bool synchronize_{};
  bool notify_{};
  bool discoveredNotNotified_{};
  bool discoveredAndNotified_{};
  std::unique_ptr<CookieBase> cookie_{};
  std::unique_ptr<CookieBase> shared_{};

  explicit ObjectInstance(ObjectClass* objectClass);
  Property& getProperty(const char* propertyName); // AssertFederateStrand
  Property& getProperty(const std::string& propertyName); // AssertFederateStrand
};

class ObjectRef {
  friend class Federate;
  friend class ObjectClass;
  friend class Property;
  friend class SessionFederate;

  std::shared_ptr<ObjectInstance> instance_;

public:
  explicit ObjectRef(std::shared_ptr<ObjectInstance> instance) : instance_{std::move(instance)} { }

public:
  ObjectRef() = default;

  explicit operator bool() const { return static_cast<bool>(instance_); }
  bool operator!() const { return !instance_; }

  bool operator==(const ObjectRef& other) const { return instance_ == other.instance_; }
  bool operator!=(const ObjectRef& other) const { return instance_ != other.instance_; }


  [[nodiscard]] ObjectId getObjectId() const; // AssertFederateStrand
  [[nodiscard]] const std::string& getObjectClass() const; // AssertFederateStrand

  //bool hasProperty(const char* propertyName) const; // AsertAsyncStrand

  Property& operator[](const char* propertyName) {
    return instance_->getProperty(std::string{propertyName});
  }

  const Property& operator[](const char* propertyName) const {
    return instance_->getProperty(std::string{propertyName});
  }

  Property& getProperty(const std::string& propertyName) {
    return instance_->getProperty(propertyName);
  }

  [[nodiscard]] const Property& getProperty(const std::string& propertyName) const {
    return instance_->getProperty(propertyName);
  }

  Property& operator[](const ValueSymbol<ValueProperty>& s) {
    return instance_->getProperty(s.name);
  }

  const Property& operator[](const ValueSymbol<ValueProperty>& s) const {
    return instance_->getProperty(s.name);
  }

  const Value& operator[](const ValueSymbol<Value>& s) const {
    return instance_->getProperty(s.name).getValue();
  }

  template <typename T> T operator[](const ValueSymbol<T>& s) const {
    return instance_->getProperty(s.name).getValue().template cast<T>();
  }

  [[nodiscard]] const std::vector<std::unique_ptr<Property>>& getProperties() const {
    // TODO: find out why values contains nullptr items
    return instance_->properties_.Values();
  }

  // [[nodiscard]] std::vector<std::unique_ptr<Property>>::const_iterator begin() const {
  //     return _instance ? _instance->_properties.Values().begin() : std::vector<std::unique_ptr<Property>>::const_iterator{};
  // }

  // [[nodiscard]] std::vector<std::unique_ptr<Property>>::const_iterator end() const {
  //     return _instance ? _instance->_properties.Values().end() : std::vector<std::unique_ptr<Property>>::const_iterator{};
  // }

  [[nodiscard]] bool canDelete() const { return instance_ ? instance_->getProperty(Property::Destructor_cstr).canSetValue() : false; }
  void Delete(); // AssertFederateStrand

  [[nodiscard]] bool justDiscovered() const {
    return instance_ ? instance_->discoveredNotNotified_ : false;
  }
  [[nodiscard]] bool justDestroyed() const {
    return instance_ ? instance_->deletedByMaster_ : false;
  }
  [[nodiscard]] bool isDeletedByObject() const {
    return instance_ ? instance_->deletedByObject_ : false;
  }
  [[nodiscard]] bool isDeletedByMaster() const {
    return instance_ ? instance_->deletedByMaster_ : false;
  }

  [[nodiscard]] OwnershipState getOwnershipState() const { return instance_ ? instance_->getProperty(Property::Destructor_cstr).getOwnershipState() : OwnershipState{}; }
  void modifyOwnershipState(OwnershipOperation operation);

  template <typename T> T& getCookie() {
    LOG_ASSERT(instance_);
    auto cookie = dynamic_cast<CookieType<T>*>(instance_->cookie_.get());
    if (!cookie) {
      cookie = new CookieType<T>{};
      instance_->cookie_ = std::unique_ptr<CookieBase>{cookie};
    }
    return cookie->value;
  }

  template <typename T> T& acquireShared() {
    LOG_ASSERT(instance_);
    if (instance_->masterInstance_)
      instance_->masterInstance_->sharedMutex_.lock();
    auto& variable = instance_->masterInstance_ ? instance_->masterInstance_->shared_ : instance_->shared_;
    auto cookie = dynamic_cast<CookieType<T>*>(variable.get());
    if (!cookie) {
      cookie = new CookieType<T>{};
      variable = std::unique_ptr<CookieType<T>>{cookie};
    }
    return cookie->value;
  }

  void releaseShared() {
    LOG_ASSERT(instance_);
    if (instance_->masterInstance_)
      instance_->masterInstance_->sharedMutex_.unlock();
  }
};

#endif
