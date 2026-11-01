//===- Type.cpp - Jia types -----------------------------------------------===//
#include "jia/AST/Type.h"

#include <format>

namespace jia {

std::optional<std::uint64_t> Type::size() const {
  switch (kind_) {
  case TypeKind::Char:
    return 1;
  case TypeKind::Int:
  case TypeKind::Float:
    return 4;
  case TypeKind::Pointer:
    return 8;
  case TypeKind::Array: {
    const auto elementSize = element()->size();
    if (!elementSize || length_ == 0)
      return std::nullopt;
    return *elementSize * length_;
  }
  case TypeKind::Void:
  case TypeKind::Function:
    return std::nullopt;
  }
  return std::nullopt;
}

namespace {

/// Spells \p type around a declarator \p inner, the way C nests them:
/// pointer to array of 3 int is "int (*)[3]".
std::string spell(const Type &type, const std::string &inner) {
  const auto join = [&](std::string_view base) {
    if (inner.empty())
      return std::string(base);
    const bool tight = inner.front() == '[' || inner.front() == '(';
    return std::format("{}{}{}", base, tight ? "" : " ", inner);
  };
  switch (type.kind()) {
  case TypeKind::Void:
    return join("void");
  case TypeKind::Char:
    return join("char");
  case TypeKind::Int:
    return join("int");
  case TypeKind::Float:
    return join("float");
  case TypeKind::Pointer: {
    const Type &pointee = *type.pointee();
    if (pointee.isArray() || pointee.isFunction())
      return spell(pointee, "(*" + inner + ")");
    return spell(pointee, "*" + inner);
  }
  case TypeKind::Array:
    return spell(*type.element(),
                 type.length() == 0 ? inner + "[]"
                                    : std::format("{}[{}]", inner, type.length()));
  case TypeKind::Function: {
    std::string params;
    for (const Type *param : type.params()) {
      if (!params.empty())
        params += ", ";
      params += param->str();
    }
    if (type.isVariadic())
      params += params.empty() ? "..." : ", ...";
    return spell(*type.returnType(), std::format("{}({})", inner, params));
  }
  }
  return "<invalid>";
}

} // namespace

std::string Type::str() const { return spell(*this, ""); }

TypeContext::TypeContext()
    : void_(own(std::make_unique<Type>(Type::Key{}, TypeKind::Void))),
      char_(own(std::make_unique<Type>(Type::Key{}, TypeKind::Char))),
      int_(own(std::make_unique<Type>(Type::Key{}, TypeKind::Int))),
      float_(own(std::make_unique<Type>(Type::Key{}, TypeKind::Float))) {}

const Type *TypeContext::own(std::unique_ptr<Type> type) {
  storage_.push_back(std::move(type));
  return storage_.back().get();
}

const Type *TypeContext::pointerTo(const Type *pointee) {
  auto [it, inserted] = pointers_.try_emplace(pointee, nullptr);
  if (inserted)
    it->second = own(std::make_unique<Type>(Type::Key{}, TypeKind::Pointer, pointee));
  return it->second;
}

const Type *TypeContext::arrayOf(const Type *element, std::uint64_t length) {
  auto [it, inserted] = arrays_.try_emplace({element, length}, nullptr);
  if (inserted)
    it->second = own(
        std::make_unique<Type>(Type::Key{}, TypeKind::Array, element, length));
  return it->second;
}

const Type *TypeContext::function(const Type *ret,
                                  std::vector<const Type *> params,
                                  bool variadic) {
  auto [it, inserted] = functions_.try_emplace({ret, params, variadic}, nullptr);
  if (inserted)
    it->second = own(std::make_unique<Type>(Type::Key{}, TypeKind::Function, ret,
                                            0, std::move(params), variadic));
  return it->second;
}

} // namespace jia
