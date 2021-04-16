#ifndef CAPSULE_H
#define CAPSULE_H

#include "Python.h"

namespace jit {

namespace detail {
template <typename T>
void capsuleDestructor(PyObject* capsule) {
  auto ptr = static_cast<T*>(PyCapsule_GetPointer(capsule, nullptr));
  if (ptr == nullptr) {
    JIT_LOG("ERROR: Couldn't retrieve value from capsule %p", capsule);
    return;
  }
  delete ptr;
}
} // namespace detail

// Create a PyCapsule to hold the given C++ object, with a destructor that
// deletes the object.
template <typename T>
PyObject* makeCapsule(T* ptr) {
  return PyCapsule_New(ptr, nullptr, detail::capsuleDestructor<T>);
}

} // namespace jit

#endif