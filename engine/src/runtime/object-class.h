// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__RUNTIME__OBJECT_CLASS_H
#define WARSTAGE__RUNTIME__OBJECT_CLASS_H

#include "value/dictionary.h"
#include "./object.h"
#include <functional>

class Federate;


class ObjectIterator {
  friend class ObjectClass;
  Federate& federate_;
  ObjectClass& objectClass_;
  std::size_t index_;
  ObjectIterator(Federate& federate, ObjectClass& objectClass, std::size_t index) : federate_{federate}, objectClass_{objectClass}, index_{index} {
  }
public:
  ObjectRef operator*() const; // AssertFederateStrand
  const ObjectIterator& operator++();

  [[nodiscard]] ObjectIterator operator++(int) {
    ObjectIterator i = *this;
    ++(*this);
    return i;
  }

  [[nodiscard]] bool operator==(const ObjectIterator& i) const { return index_ == i.index_; }
  [[nodiscard]] bool operator!=(const ObjectIterator& i) const { return index_ != i.index_; }
};

class ObjectClass {
  friend class Federate;
  friend struct MasterProperty;
  friend class ObjectRef;
  friend struct ObjectInstance;
  friend class ObjectIterator;
  friend class Property;
  friend Property* findPotentialOwnerFederate(const OwnershipMap& ownership, OwnershipStateFlag flag);

  struct PropertyInfo {
    std::string name_{};
    bool required_{};
    bool published_{};
  };

  Federate* federate_;
  std::string className_;
  std::vector<std::function<void(ObjectRef)>> observers_{};
  SymbolTable propertySymbols_{};
  ValueTable<PropertyInfo> properties_{propertySymbols_};

  ObjectClass(Federate& federate, std::string name);
  [[nodiscard]] std::size_t next(std::size_t index) const; // AssertFederateStrand
  [[nodiscard]] PropertyInfo& getPropertyInfo(const std::string& propertyName);
  void publishProperty(const char* propertyName);

public:
  [[nodiscard]] const std::string& getName() const { return className_; }

  void require(std::initializer_list<const char*> propertyNames);
  void publish(std::initializer_list<const char*> propertyNames);
  void observe(std::function<void(ObjectRef)> observer);

  [[nodiscard]] ObjectRef create(ObjectId objectId = ObjectId::create()); // AssertFederateStrand
  [[nodiscard]] ObjectRef find(std::function<bool(ObjectRef)> predicate); // AssertFederateStrand

  [[nodiscard]] ObjectIterator begin(); // AssertFederateStrand
  [[nodiscard]] ObjectIterator end(); // AssertFederateStrand
};


#endif
