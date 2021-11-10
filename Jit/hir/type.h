// Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com)
#pragma once

#include "Python.h"
#include "frameobject.h"

#include "Jit/hir/type_generated.h"
#include "Jit/log.h"
#include "Jit/util.h"

#include <cstddef>
#include <cstdint>
#include <ostream>

// This file defines jit::hir::Type, which represents types of objects in HIR,
// both Python objects and primitive C types (some of which are exposed to
// Python code in Static Python). For a high-level overview, see
// Jit/hir/type.md.

// The HIR_TYPES() macro used repeatedly in this file is generated by
// Tools/scripts/typed/generate_jit_type_h.py and lives in
// Jit/hir/type_generated.h.

namespace jit {
namespace hir {

class Type {
 public:
  using bits_t = uint64_t;

  // Construct a bits_t for all predefined types.
#define BITS(name, bits, ...) static constexpr bits_t k##name = bits;
  HIR_TYPES(BITS)
#undef BITS

  static constexpr bits_t kLifetimeBottom = 0;
  static constexpr bits_t kLifetimeMortal = 1UL << 0;
  static constexpr bits_t kLifetimeImmortal = 1UL << 1;
  static constexpr bits_t kLifetimeTop = kLifetimeMortal | kLifetimeImmortal;

  // Create a Type with the given bits. This isn't intended for general
  // consumption and is only public for the TFoo predefined Types (created near
  // the bottom of this file).
  explicit constexpr Type(bits_t bits, bits_t lifetime)
      : Type{bits, lifetime, kSpecTop, 0} {}

  std::size_t hash() const;
  std::string toString() const;

  // Parse a Type from the given string. Unions and PyObject* specializations
  // are not supported. Returns TBottom on error.
  static Type parse(std::string str);

  // Create a Type from a PyTypeObject, optionally flagged as not allowing
  // subtypes. The resulting Type is not guaranteed to be specialized (for
  // example, fromType(&PyLong_Type) == TLong).
  static Type fromType(PyTypeObject* type);
  static Type fromTypeExact(PyTypeObject* type);

  // Create a Type from a PyObject. The resulting Type is not guaranteed to be
  // specialized (for example, fromObject(Py_None) == TNoneType).
  static Type fromObject(PyObject* obj);

  // Create a Type specialized with a C value.
  static Type fromCBool(bool b);
  static Type fromCDouble(double d);

  static bool CIntFitsType(int64_t i, Type t);
  static Type fromCInt(int64_t i, Type t);
  static bool CUIntFitsType(uint64_t i, Type t);
  static Type fromCUInt(uint64_t i, Type t);

  // Return the PyTypeObject* that uniquely represents this type, or nullptr if
  // there isn't one. The PyTypeObject* may be from a type
  // specialization. "Uniquely" here means that there should be no loss of
  // information in the Type -> PyTypeObject* conversion, other than mortality
  // and exactness.
  //
  // Some examples:
  // TLong.uniquePyType() == &PyLong_Type
  // TLongExact.uniquePyType() == &PyLong_Type
  // TLongUser.uniquePyType() == nullptr
  // TLongExact[123].uniquePyType() == nullptr
  // TBool.uniquePyType() == &PyBool_Type
  // TObject.uniquePyType() == &PyBaseObject_Type
  // (TObject - TLong).uniquePyType() == nullptr
  PyTypeObject* uniquePyType() const;

  // Return the PyObject* that this type represents, or nullptr if it
  // represents more than one object (or a non-object type). This is similar to
  // objectSpec() (but with support for NoneType) and is the inverse of
  // fromObject(): Type::fromObject(obj).asObject() == obj.
  PyObject* asObject() const;

  // Does this Type represent a single value?
  bool isSingleValue() const;

  // Does this Type have a type specialization, including from an object
  // specialization?
  bool hasTypeSpec() const;

  // Does this Type have an exact type specialization, including from an object
  // specialization?
  bool hasTypeExactSpec() const;

  // Does this Type have an object specialization?
  bool hasObjectSpec() const;

  // Does this Type have a primitive specialization?
  bool hasIntSpec() const;
  bool hasDoubleSpec() const;

  // Does this Type have an object or primitive specialization, and is it a
  // subtype of the given Type?
  bool hasValueSpec(Type ty) const;

  // If this Type has a type specialization, return it. If this Type has an
  // object specialization, return its type.
  PyTypeObject* typeSpec() const;

  // Return this Type's object specialization.
  PyObject* objectSpec() const;

  // Return this Type's primitive specialization.
  intptr_t intSpec() const;
  double_t doubleSpec() const;

  // Return a copy of this Type with its specialization removed.
  Type unspecialized() const;

  // Return a copy of this Type with unknown mortality.
  Type dropMortality() const;

  // Return true iff this Type is specialized with an exact PyTypeObject* or is
  // a subtype of all builtin exact types.
  bool isExact() const;

  // Equality.
  bool operator==(Type other) const;
  bool operator!=(Type other) const;

  // Strict and non-strict subtype checking.
  bool operator<(Type other) const;
  bool operator<=(Type other) const;

  // Shortcut for (*this & other) != TBottom.
  bool couldBe(Type other) const;

  // Set operations: union, intersection, and subtraction.
  Type operator|(Type other) const;
  Type operator&(Type other) const;
  Type operator-(Type other) const;

  Type& operator|=(Type other);
  Type& operator&=(Type other);
  Type& operator-=(Type other);

 private:
  // Validity and kind of specialization. Note that this is a regular enum
  // rather than a bitset, so the bit values of each kind aren't important.
  enum SpecKind : bits_t {
    // No specialization: the Top type in the specialization lattice, and a
    // supertype of all specializations. See Type::specSubtype() for details on
    // subtype relationships between the other kinds.
    kSpecTop,

    // Type specialization: pytype_ is valid.
    kSpecType,

    // Exact type specialization: pytype_ is valid and its subtypes are
    // excluded.
    kSpecTypeExact,

    // Object specialization: pyobject_ is valid.
    kSpecObject,

    // Integral specialization: int_ is valid
    kSpecInt,

    // Double specialization: double_ is valid
    kSpecDouble,
  };

  // Constructors used to create specialized Types.
  constexpr Type(
      bits_t bits,
      bits_t lifetime,
      PyTypeObject* type_spec,
      bool exact)
      : Type{
            bits,
            lifetime,
            exact ? kSpecTypeExact : kSpecType,
            reinterpret_cast<intptr_t>(type_spec)} {}

  constexpr Type(bits_t bits, bits_t lifetime, PyObject* value_spec)
      : Type{
            bits,
            lifetime,
            kSpecObject,
            reinterpret_cast<intptr_t>(value_spec)} {}

  constexpr Type(bits_t bits, double_t spec)
      : Type{bits, kLifetimeBottom, kSpecDouble, bit_cast<intptr_t>(spec)} {}

  constexpr Type(
      bits_t bits,
      bits_t lifetime,
      SpecKind spec_kind,
      intptr_t spec)
      : bits_{bits},
        lifetime_{lifetime},
        spec_kind_{spec_kind},
        padding_{},
        int_{spec} {
    JIT_DCHECK(
        bits != kBottom || (spec_kind == kSpecTop && spec == 0),
        "Bottom can't be specialized");
    JIT_DCHECK(
        (lifetime == kLifetimeBottom) == ((bits & kObject) == 0),
        "lifetime component should be kLifetimeBottom if and only if no "
        "kObject bits are set");
    JIT_DCHECK(padding_ == 0, "Invalid padding");
  }

  // What is this Type's specialization kind?
  SpecKind specKind() const;

  // Shorthand for specKind() != kSpecTop: does this Type have a non-Top
  // specialization?
  bool hasSpec() const;

  // String representation of this Type's specialization, which must not be
  // kSpecTop.
  std::string specString() const;

  // Is this Type's specialization a subtype of the other Type's
  // specialization?
  bool specSubtype(Type other) const;

  static Type fromTypeImpl(PyTypeObject* type, bool exact);

  // Bit field sizes, computed to fill any padding with zeros to make comparing
  // cheaper.
  static constexpr int kLifetimeBits = 2;
  static constexpr int kSpecBits = 3;
  static constexpr int kPaddingBits =
      int{sizeof(bits_t) * CHAR_BIT} - kNumTypeBits - kLifetimeBits - kSpecBits;
  static_assert(
      kPaddingBits > 0,
      "Too many basic types and/or specialization kinds");

  bits_t bits_ : kNumTypeBits;
  bits_t lifetime_ : kLifetimeBits;
  bits_t spec_kind_ : kSpecBits;
  bits_t padding_ : kPaddingBits;

  // Specialization. Active field determined by spec_kind_.
  union {
    PyTypeObject* pytype_;
    PyObject* pyobject_;
    intptr_t int_;
    double_t double_;
  };
};

inline std::ostream& operator<<(std::ostream& os, const Type& ty) {
  return os << ty.toString();
}

// Define TFoo constants for all predefined types.
#define TY(name, bits, lifetime, ...) \
  constexpr Type T##name{Type::k##name, Type::lifetime};
HIR_TYPES(TY)
#undef TY

} // namespace hir
} // namespace jit

template <>
struct std::hash<jit::hir::Type> {
  std::size_t operator()(const jit::hir::Type& ty) const {
    return ty.hash();
  }
};

#define incl_JIT_HIR_TYPE_INL_H
#include "Jit/hir/type_inl.h"
#undef incl_JIT_HIR_TYPE_INL_H
