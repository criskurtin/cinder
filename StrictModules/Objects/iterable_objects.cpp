#include "StrictModules/Objects/iterable_objects.h"
#include "StrictModules/Objects/callable_wrapper.h"
#include "StrictModules/Objects/object_interface.h"
#include "StrictModules/Objects/objects.h"

#include "StrictModules/caller_context.h"
#include "StrictModules/caller_context_impl.h"

#include <fmt/format.h>
namespace strictmod::objects {

// -------------------------Iterable-------------------------

// wrapped methods
static inline bool strictIterableContainsHelper(
    const std::vector<std::shared_ptr<BaseStrictObject>>& data,
    const CallerContext& caller,
    std::shared_ptr<BaseStrictObject> item) {
  for (auto& elem : data) {
    if (iStrictObjectEq(item, elem, caller)) {
      return true;
    }
  }
  return false;
}

// -------------------------Sequence (random access)-------------------------
StrictSequence::StrictSequence(
    std::shared_ptr<StrictType> type,
    std::weak_ptr<StrictModuleObject> creator,
    std::vector<std::shared_ptr<BaseStrictObject>> data)
    : StrictIterable(std::move(type), std::move(creator)),
      data_(std::move(data)) {}

StrictSequence::StrictSequence(
    std::shared_ptr<StrictType> type,
    std::shared_ptr<StrictModuleObject> creator,
    std::vector<std::shared_ptr<BaseStrictObject>> data)
    : StrictIterable(std::move(type), std::move(creator)),
      data_(std::move(data)) {}
// wrapped methods
std::shared_ptr<BaseStrictObject> StrictSequence::sequence__contains__(
    std::shared_ptr<StrictSequence> self,
    const CallerContext& caller,
    std::shared_ptr<BaseStrictObject> item) {
  return strictIterableContainsHelper(self->data_, caller, std::move(item))
      ? StrictTrue()
      : StrictFalse();
}

std::shared_ptr<BaseStrictObject> StrictSequence::sequence__len__(
    std::shared_ptr<StrictSequence> self,
    const CallerContext& caller) {
  return caller.makeInt(self->data_.size());
}

std::shared_ptr<BaseStrictObject> StrictSequence::sequence__eq__(
    std::shared_ptr<StrictSequence> self,
    const CallerContext& caller,
    std::shared_ptr<BaseStrictObject> rhs) {
  // Type are in practice singletons, so we just compare the address
  if (self->getType() != rhs->getType()) {
    return StrictFalse();
  }
  std::shared_ptr<StrictSequence> rhsSeq =
      std::dynamic_pointer_cast<StrictSequence>(rhs);
  if (rhsSeq == nullptr) {
    return StrictFalse();
  }
  if (self->data_.size() != rhsSeq->data_.size()) {
    return StrictFalse();
  }
  for (size_t i = 0; i < self->data_.size(); ++i) {
    if (!iStrictObjectEq(self->data_[i], rhsSeq->data_[i], caller)) {
      return StrictFalse();
    }
  }
  return StrictTrue();
}

std::shared_ptr<BaseStrictObject> StrictSequence::sequence__add__(
    std::shared_ptr<StrictSequence> self,
    const CallerContext& caller,
    std::shared_ptr<BaseStrictObject> rhs) {
  if (self->getType() != rhs->getType()) {
    return NotImplemented();
  }
  std::shared_ptr<StrictSequence> rhsSeq =
      std::dynamic_pointer_cast<StrictSequence>(rhs);
  if (rhsSeq == nullptr) {
    return NotImplemented();
  }
  std::vector<std::shared_ptr<BaseStrictObject>> newData(self->data_);
  newData.insert(newData.end(), rhsSeq->data_.begin(), rhsSeq->data_.end());
  return self->makeSequence(self->getType(), caller.caller, std::move(newData));
}

static std::shared_ptr<BaseStrictObject> sequenceMulHelper(
    std::shared_ptr<StrictSequence> self,
    const CallerContext& caller,
    std::shared_ptr<BaseStrictObject> other) {
  std::shared_ptr<StrictInt> multFactor =
      std::dynamic_pointer_cast<StrictInt>(other);
  if (multFactor == nullptr) {
    return NotImplemented();
  }
  std::vector<std::shared_ptr<BaseStrictObject>> result;
  auto& data = self->getData();
  long repeat = multFactor->getValue();
  result.reserve(data.size() * repeat);
  for (int i = 0; i < repeat; ++i) {
    result.insert(result.end(), data.begin(), data.end());
  }
  return self->makeSequence(self->getType(), caller.caller, std::move(result));
}

std::shared_ptr<BaseStrictObject> StrictSequence::sequence__mul__(
    std::shared_ptr<StrictSequence> self,
    const CallerContext& caller,
    std::shared_ptr<BaseStrictObject> rhs) {
  return sequenceMulHelper(std::move(self), caller, std::move(rhs));
}

std::shared_ptr<BaseStrictObject> StrictSequence::sequence__rmul__(
    std::shared_ptr<StrictSequence> self,
    const CallerContext& caller,
    std::shared_ptr<BaseStrictObject> lhs) {
  return sequenceMulHelper(std::move(self), caller, std::move(lhs));
}

// TODO: __iter__, __reversed__

static inline int normalizeIndex(int index, int size) {
  return index < 0 ? index + size : index;
}

// TODO also provide __getitem__, and call it here
std::shared_ptr<BaseStrictObject> StrictSequenceType::getElement(
    std::shared_ptr<BaseStrictObject> obj,
    std::shared_ptr<BaseStrictObject> index,
    const CallerContext& caller) {
  auto seq = assertStaticCast<StrictSequence>(obj);
  auto& data = seq->getData();
  // TODO handle slice
  std::shared_ptr<StrictInt> intIndex =
      std::dynamic_pointer_cast<StrictInt>(index);
  if (intIndex != nullptr) {
    int idx = normalizeIndex(intIndex->getValue(), data.size());
    if (idx >= 0 && (size_t)idx < data.size()) {
      return data[idx];
    } else {
      caller.raiseTypeError(
          "{} index out of range: {}", seq->getTypeRef().getName(), idx);
    }
  }
  caller.raiseTypeError(
      "{} indices must be integers or slices, not {}",
      seq->getTypeRef().getName(),
      index->getTypeRef().getName());
}

void StrictSequenceType::addMethods() {
  StrictIterableType::addMethods();
  addMethod(kDunderContains, StrictSequence::sequence__contains__);
  addMethod(kDunderLen, StrictSequence::sequence__len__);
  addMethod("__eq__", StrictSequence::sequence__eq__);
  addMethod("__add__", StrictSequence::sequence__add__);
  addMethod("__mul__", StrictSequence::sequence__mul__);
  addMethod("__rmul__", StrictSequence::sequence__rmul__);
}

// -------------------------List-------------------------
std::shared_ptr<StrictSequence> StrictList::makeSequence(
    std::shared_ptr<StrictType> type,
    std::weak_ptr<StrictModuleObject> creator,
    std::vector<std::shared_ptr<BaseStrictObject>> data) {
  return std::make_shared<StrictList>(
      std::move(type), std::move(creator), std::move(data));
}

std::string StrictList::getDisplayName() const {
  return fmt::format("[{}]", fmt::join(data_, ","));
}

PyObject* StrictList::getPyObject() const {
  PyObject* pyObj = PyList_New(data_.size());
  if (pyObj == nullptr) {
    // allocation failed
    return nullptr;
  }
  for (size_t i = 0; i < data_.size(); ++i) {
    PyObject* elem = data_[i]->getPyObject();
    if (elem == nullptr) {
      Py_DECREF(pyObj);
      return nullptr;
    }
    // elem reference is stolen into the list
    PyList_SET_ITEM(pyObj, i, elem);
  }
  return pyObj;
}

// wrapped methods
std::shared_ptr<BaseStrictObject> StrictList::listAppend(
    std::shared_ptr<StrictList> self,
    const CallerContext& caller,
    std::shared_ptr<BaseStrictObject> elem) {
  checkExternalModification(self, caller);
  self->data_.push_back(std::move(elem));
  return NoneObject();
}

std::shared_ptr<BaseStrictObject> StrictList::listCopy(
    std::shared_ptr<StrictList> self,
    const CallerContext& caller) {
  return std::make_shared<StrictList>(ListType(), caller.caller, self->data_);
}
// TODO: __init__, extend

void StrictListType::setElement(
    std::shared_ptr<BaseStrictObject> obj,
    std::shared_ptr<BaseStrictObject> index,
    std::shared_ptr<BaseStrictObject> value,
    const CallerContext& caller) {
  checkExternalModification(obj, caller);
  auto list = assertStaticCast<StrictList>(obj);
  // TODO handle slice
  std::shared_ptr<StrictInt> intIndex =
      std::dynamic_pointer_cast<StrictInt>(index);
  if (intIndex != nullptr) {
    auto& data = list->getData();
    int idx = normalizeIndex(intIndex->getValue(), data.size());
    if (idx >= 0 && (size_t)idx < data.size()) {
      list->setData(idx, std::move(value));
    } else {
      caller.raiseTypeError("list assignment index out of range: {}", idx);
    }
  }
  caller.raiseTypeError(
      "list indices must be integers or slices, not {}",
      index->getTypeRef().getName());
}

std::unique_ptr<BaseStrictObject> StrictListType::constructInstance(
    std::shared_ptr<StrictModuleObject> caller) {
  return std::make_unique<StrictList>(ListType(), caller, kEmptyArgs);
}

PyObject* StrictListType::getPyObject() const {
  Py_INCREF(&PyList_Type);
  return reinterpret_cast<PyObject*>(&PyList_Type);
}

void StrictListType::addMethods() {
  StrictSequenceType::addMethods();
  addMethod("append", StrictList::listAppend);
  addMethod("copy", StrictList::listCopy);
}

// -------------------------Tuple-------------------------
StrictTuple::StrictTuple(
    std::shared_ptr<StrictType> type,
    std::weak_ptr<StrictModuleObject> creator,
    std::vector<std::shared_ptr<BaseStrictObject>> data)
    : StrictSequence(std::move(type), std::move(creator), std::move(data)),
      pyObj_(nullptr),
      displayName_() {}

StrictTuple::StrictTuple(
    std::shared_ptr<StrictType> type,
    std::shared_ptr<StrictModuleObject> creator,
    std::vector<std::shared_ptr<BaseStrictObject>> data)
    : StrictSequence(std::move(type), std::move(creator), std::move(data)),
      pyObj_(nullptr),
      displayName_() {}

StrictTuple::~StrictTuple() {
  Py_XDECREF(pyObj_);
}

std::shared_ptr<StrictSequence> StrictTuple::makeSequence(
    std::shared_ptr<StrictType> type,
    std::weak_ptr<StrictModuleObject> creator,
    std::vector<std::shared_ptr<BaseStrictObject>> data) {
  return std::make_shared<StrictTuple>(
      std::move(type), std::move(creator), std::move(data));
}

bool StrictTuple::isHashable() const {
  for (auto& e : data_) {
    if (!e->isHashable()) {
      return false;
    }
  }
  return true;
}

size_t StrictTuple::hash() const {
  size_t h = data_.size();
  // taken from boost.hash_combine
  for (auto& e : data_) {
    h ^= e->hash() + 0x9e3779b9 + (h << 6) + (h >> 2);
  }
  return h;
}

bool StrictTuple::eq(const BaseStrictObject& other) const {
  if (&other.getTypeRef() != type_.get()) {
    return false;
  }
  const StrictTuple& otherTuple = static_cast<const StrictTuple&>(other);
  if (data_.size() != otherTuple.data_.size()) {
    return false;
  }
  for (size_t i = 0; i < data_.size(); ++i) {
    const auto& dataI = data_[i];
    const auto& otherI = otherTuple.data_[i];
    if (!dataI->eq(*otherI) && !otherI->eq(*dataI)) {
      return false;
    }
  }
  return true;
}

std::string StrictTuple::getDisplayName() const {
  if (displayName_.empty()) {
    displayName_ = fmt::format("({})", fmt::join(data_, ","));
  }
  return displayName_;
}

PyObject* StrictTuple::getPyObject() const {
  // We can cache the PyObject since tuple is immutable
  if (pyObj_ == nullptr) {
    pyObj_ = PyTuple_New(data_.size());
    if (pyObj_ == nullptr) {
      // allocation failed
      return nullptr;
    }
    for (size_t i = 0; i < data_.size(); ++i) {
      PyObject* elem = data_[i]->getPyObject();
      if (elem == nullptr) {
        Py_DECREF(pyObj_);
        return nullptr;
      }
      // elem reference is stolen into the tuple
      PyTuple_SET_ITEM(pyObj_, i, elem);
    }
  }
  Py_INCREF(pyObj_);
  return pyObj_;
}

// wrapped methods
std::shared_ptr<BaseStrictObject> StrictTuple::tupleIndex(
    std::shared_ptr<StrictTuple> self,
    const CallerContext& caller,
    std::shared_ptr<BaseStrictObject> item) {
  const auto& data = self->data_;
  for (size_t i = 0; i < data.size(); ++i) {
    if (iStrictObjectEq(item, data[i], caller)) {
      return caller.makeInt(i);
    }
  }
  caller.raiseExceptionStr(ValueErrorType(), "tuple.index(x): x not in tuple");
}
// TODO: tuple.__new__

std::unique_ptr<BaseStrictObject> StrictTupleType::constructInstance(
    std::shared_ptr<StrictModuleObject> caller) {
  return std::make_unique<StrictTuple>(
      TupleType(), std::move(caller), kEmptyArgs);
}

PyObject* StrictTupleType::getPyObject() const {
  Py_INCREF(&PyTuple_Type);
  return reinterpret_cast<PyObject*>(&PyTuple_Type);
}

void StrictTupleType::addMethods() {
  StrictSequenceType::addMethods();
  addMethod("index", StrictTuple::tupleIndex);
}

// -------------------------Set Like-------------------------
StrictSetLike::StrictSetLike(
    std::shared_ptr<StrictType> type,
    std::weak_ptr<StrictModuleObject> creator,
    SetDataT data)
    : StrictIterable(std::move(type), std::move(creator)),
      data_(std::move(data)) {}

StrictSetLike::StrictSetLike(
    std::shared_ptr<StrictType> type,
    std::shared_ptr<StrictModuleObject> creator,
    SetDataT data)
    : StrictIterable(std::move(type), std::move(creator)),
      data_(std::move(data)) {}

void StrictSetLike::addElement(
    const CallerContext&,
    std::shared_ptr<BaseStrictObject> element) {
  data_.insert(element);
}

static bool strictSetLikeContainsHelper(
    const SetDataT& data,
    const CallerContext&,
    std::shared_ptr<BaseStrictObject> obj) {
  auto got = data.find(std::move(obj));
  return got != data.end();
}

// wrapped methods
std::shared_ptr<BaseStrictObject> StrictSetLike::set__contains__(
    std::shared_ptr<StrictSetLike> self,
    const CallerContext& caller,
    std::shared_ptr<BaseStrictObject> element) {
  if (strictSetLikeContainsHelper(
          self->getData(), caller, std::move(element))) {
    return StrictTrue();
  }
  return StrictFalse();
}

std::shared_ptr<BaseStrictObject> StrictSetLike::set__len__(
    std::shared_ptr<StrictSetLike> self,
    const CallerContext& caller) {
  return caller.makeInt(self->getData().size());
}

std::shared_ptr<BaseStrictObject> StrictSetLike::set__and__(
    std::shared_ptr<StrictSetLike> self,
    const CallerContext& caller,
    std::shared_ptr<BaseStrictObject> rhs) {
  auto rhsSetLike = std::dynamic_pointer_cast<StrictSetLike>(rhs);
  if (rhsSetLike != nullptr) {
    auto& rhsData = rhsSetLike->getData();
    SetDataT newData;
    for (auto& elem : rhsData) {
      if (strictSetLikeContainsHelper(self->data_, caller, elem)) {
        newData.insert(elem);
      }
    }
    return self->makeSetLike(self->type_, caller.caller, std::move(newData));
  }
  return NotImplemented();
}

std::shared_ptr<BaseStrictObject> StrictSetLike::set__or__(
    std::shared_ptr<StrictSetLike> self,
    const CallerContext& caller,
    std::shared_ptr<BaseStrictObject> rhs) {
  auto rhsSetLike = std::dynamic_pointer_cast<StrictSetLike>(rhs);
  if (rhsSetLike != nullptr) {
    auto& rhsData = rhsSetLike->getData();
    SetDataT newData(self->data_);
    for (auto& elem : rhsData) {
      if (!strictSetLikeContainsHelper(newData, caller, elem)) {
        newData.insert(elem);
      }
    }
    return self->makeSetLike(self->type_, caller.caller, std::move(newData));
  }
  return NotImplemented();
}

std::shared_ptr<BaseStrictObject> StrictSetLike::set__xor__(
    std::shared_ptr<StrictSetLike> self,
    const CallerContext& caller,
    std::shared_ptr<BaseStrictObject> rhs) {
  auto rhsSetLike = std::dynamic_pointer_cast<StrictSetLike>(rhs);
  if (rhsSetLike != nullptr) {
    auto& rhsData = rhsSetLike->getData();
    SetDataT newData;
    for (auto& elem : rhsData) {
      if (!strictSetLikeContainsHelper(self->data_, caller, elem)) {
        newData.insert(elem);
      }
    }
    for (auto& elem : self->data_) {
      if (!strictSetLikeContainsHelper(rhsData, caller, elem)) {
        newData.insert(elem);
      }
    }
    return self->makeSetLike(self->type_, caller.caller, std::move(newData));
  }
  return NotImplemented();
}
// TODO __iter__, issubset, issuperset, __le__, __lt__,
// __ge__, __gt__

void StrictSetLikeType::addMethods() {
  addMethod("__contains__", StrictSetLike::set__contains__);
  addMethod("__len__", StrictSetLike::set__len__);
  addMethod("__and__", StrictSetLike::set__and__);
  addMethod("__or__", StrictSetLike::set__or__);
  addMethod("__xor__", StrictSetLike::set__xor__);
}

// -------------------------Set-------------------------
std::string StrictSet::getDisplayName() const {
  if (data_.empty()) {
    return "set()";
  }
  return fmt::format("{{{}}}", fmt::join(data_, ","));
}

PyObject* StrictSet::getPyObject() const {
  // this give empty set
  PyObject* pyObj = PySet_New(nullptr);
  if (pyObj == nullptr) {
    // allocation failed
    return nullptr;
  }
  for (auto& v : data_) {
    PyObject* elem = v->getPyObject();
    if (elem == nullptr) {
      Py_DECREF(pyObj);
      return nullptr;
    }
    // set keeps its own reference to elem
    if (PySet_Add(pyObj, elem) < 0) {
      PyErr_Clear();
      Py_DECREF(elem);
      Py_DECREF(pyObj);
      return nullptr;
    }
    Py_DECREF(elem);
  }
  return pyObj;
}

std::shared_ptr<StrictSetLike> StrictSet::makeSetLike(
    std::shared_ptr<StrictType> type,
    std::weak_ptr<StrictModuleObject> creator,
    SetDataT data) {
  return std::make_shared<StrictSet>(
      std::move(type), std::move(creator), std::move(data));
}

// wrapped methods
std::shared_ptr<BaseStrictObject> StrictSet::setAdd(
    std::shared_ptr<StrictSet> self,
    const CallerContext& caller,
    std::shared_ptr<BaseStrictObject> item) {
  checkExternalModification(self, caller);
  if (!strictSetLikeContainsHelper(self->data_, caller, item)) {
    self->data_.insert(std::move(item));
  }
  return NoneObject();
}
// TODO  __init__, update,

std::unique_ptr<BaseStrictObject> StrictSetType::constructInstance(
    std::shared_ptr<StrictModuleObject> caller) {
  return std::make_unique<StrictSet>(SetType(), caller);
}

void StrictSetType::addMethods() {
  StrictSetLikeType::addMethods();
  addMethod("add", StrictSet::setAdd);
}

// -------------------------FrozenSet-------------------------
StrictFrozenSet::StrictFrozenSet(
    std::shared_ptr<StrictType> type,
    std::weak_ptr<StrictModuleObject> creator,
    SetDataT data)
    : StrictSetLike(std::move(type), std::move(creator), std::move(data)),
      pyObj_(nullptr),
      displayName_() {}

StrictFrozenSet::StrictFrozenSet(
    std::shared_ptr<StrictType> type,
    std::shared_ptr<StrictModuleObject> creator,
    SetDataT data)
    : StrictSetLike(std::move(type), std::move(creator), std::move(data)),
      pyObj_(nullptr),
      displayName_() {}

StrictFrozenSet::~StrictFrozenSet() {
  Py_XDECREF(pyObj_);
}

std::string StrictFrozenSet::getDisplayName() const {
  if (displayName_.empty()) {
    if (data_.empty()) {
      displayName_ = "frozenset()";
    }
    displayName_ = fmt::format("frozenset({{{}}})", fmt::join(data_, ","));
  }
  return displayName_;
}

PyObject* StrictFrozenSet::getPyObject() const {
  if (pyObj_ == nullptr) {
    pyObj_ = PyFrozenSet_New(nullptr);
    if (pyObj_ == nullptr) {
      return nullptr;
    }
    for (auto& v : data_) {
      PyObject* elem = v->getPyObject();
      if (elem == nullptr) {
        Py_DECREF(pyObj_);
        return nullptr;
      }
      // set keeps its own reference to elem
      if (PySet_Add(pyObj_, elem) < 0) {
        PyErr_Clear();
        Py_DECREF(elem);
        Py_DECREF(pyObj_);
        return nullptr;
      }
      Py_DECREF(elem);
    }
  }
  Py_INCREF(pyObj_);
  return pyObj_;
}

std::shared_ptr<StrictSetLike> StrictFrozenSet::makeSetLike(
    std::shared_ptr<StrictType> type,
    std::weak_ptr<StrictModuleObject> creator,
    SetDataT data) {
  return std::make_shared<StrictFrozenSet>(
      std::move(type), std::move(creator), std::move(data));
}

// wrapped methods
// TODO __new__,

std::unique_ptr<BaseStrictObject> StrictFrozenSetType::constructInstance(
    std::shared_ptr<StrictModuleObject> caller) {
  return std::make_unique<StrictFrozenSet>(FrozensetType(), caller);
}

void StrictFrozenSetType::addMethods() {
  StrictSetLikeType::addMethods();
}

} // namespace strictmod::objects