//===- Type.h - Jia types ---------------------------------------*- C++ -*-===//
//
// Types are immutable and uniqued: a TypeContext owns every Type and hands
// out const Type* borrows. Because each distinct type exists once, two
// types are equal exactly when their pointers are equal.
//
//===----------------------------------------------------------------------===//
#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <tuple>
#include <vector>

namespace jia {

enum class TypeKind : std::uint8_t {
  Void,
  Char,  // 8-bit signed integer
  Int,   // 32-bit signed integer
  Float, // 32-bit IEEE float
  Pointer,
  Array,
  Function,
};

class TypeContext;

class Type {
public:
  /// Passkey: anyone may name a Key, but only TypeContext can make one, so
  /// only TypeContext can build types.
  class Key {
    friend class TypeContext;
    Key() = default;
  };

  Type(Key, TypeKind kind, const Type *inner = nullptr,
       std::uint64_t length = 0, std::vector<const Type *> params = {},
       bool variadic = false)
      : kind_(kind), inner_(inner), length_(length), params_(std::move(params)),
        variadic_(variadic) {}

  [[nodiscard]] TypeKind kind() const { return kind_; }
  [[nodiscard]] bool is(TypeKind k) const { return kind_ == k; }
  [[nodiscard]] bool isVoid() const { return is(TypeKind::Void); }
  [[nodiscard]] bool isInteger() const {
    return is(TypeKind::Char) || is(TypeKind::Int);
  }
  [[nodiscard]] bool isArithmetic() const {
    return isInteger() || is(TypeKind::Float);
  }
  [[nodiscard]] bool isPointer() const { return is(TypeKind::Pointer); }
  [[nodiscard]] bool isScalar() const { return isArithmetic() || isPointer(); }
  [[nodiscard]] bool isArray() const { return is(TypeKind::Array); }
  [[nodiscard]] bool isFunction() const { return is(TypeKind::Function); }

  /// Pointer: the type pointed to.
  [[nodiscard]] const Type *pointee() const { return inner_; }
  /// Array: the element type.
  [[nodiscard]] const Type *element() const { return inner_; }
  /// Array: the number of elements (0 for an incomplete array "T[]").
  [[nodiscard]] std::uint64_t length() const { return length_; }
  /// Function: the return type.
  [[nodiscard]] const Type *returnType() const { return inner_; }
  /// Function: the parameter types.
  [[nodiscard]] std::span<const Type *const> params() const { return params_; }
  /// Function: whether it ends in "...".
  [[nodiscard]] bool isVariadic() const { return variadic_; }

  /// Size in bytes on our LP64 targets; nullopt for void, functions, and
  /// incomplete arrays.
  [[nodiscard]] std::optional<std::uint64_t> size() const;

  /// The type spelled the way C would write it: "int *", "char [4]".
  [[nodiscard]] std::string str() const;

private:
  TypeKind kind_;
  const Type *inner_;
  std::uint64_t length_;
  std::vector<const Type *> params_;
  bool variadic_;
};

/// Owns and uniques every Type of one translation unit. Moving the context
/// keeps every handed-out pointer valid, because types live on the heap.
class TypeContext {
public:
  TypeContext();
  TypeContext(TypeContext &&) = default;
  TypeContext &operator=(TypeContext &&) = default;
  TypeContext(const TypeContext &) = delete;
  TypeContext &operator=(const TypeContext &) = delete;

  [[nodiscard]] const Type *voidType() const { return void_; }
  [[nodiscard]] const Type *charType() const { return char_; }
  [[nodiscard]] const Type *intType() const { return int_; }
  [[nodiscard]] const Type *floatType() const { return float_; }

  [[nodiscard]] const Type *pointerTo(const Type *pointee);
  [[nodiscard]] const Type *arrayOf(const Type *element, std::uint64_t length);
  [[nodiscard]] const Type *
  function(const Type *ret, std::vector<const Type *> params, bool variadic);

private:
  const Type *own(std::unique_ptr<Type> type);

  std::vector<std::unique_ptr<Type>> storage_;
  std::map<const Type *, const Type *> pointers_;
  std::map<std::pair<const Type *, std::uint64_t>, const Type *> arrays_;
  std::map<std::tuple<const Type *, std::vector<const Type *>, bool>,
           const Type *>
      functions_;
  const Type *void_, *char_, *int_, *float_;
};

} // namespace jia
