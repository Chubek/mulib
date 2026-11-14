#pragma once
#include <compare>
#include <memory>
#include <string>
#include <vector>

namespace on1x {
struct TypeShape;

// Immutable structural types, quantified prototype variables and closed forall
// annotations. Composite types share owned C++ storage;
// nothing refers to the scanner arena or foreign CDecl type IDs.
class Type {
public:
 enum Kind { Invalid, I64, Bool, String, Void, Json, JsonView, Function, Hole, Tuple, Generic, Bound, Forall, Code, SExpr, Buffer };
 Type(Kind kind = Invalid);
 static Type function(std::vector<Type> parameters, Type result);
 static Type tuple(std::vector<Type> elements);
 static Type generic(std::size_t identity);
 static Type bound(std::size_t index);
 static Type for_all(std::size_t variables, Type body);
 static Type code(Type body);
 bool is_code() const { return kind_==Code; }
 bool is_reference() const { return kind_==SExpr||kind_==Buffer; }
 const Type& code_result() const;
 std::size_t generic_identity() const { return variable_; }
 Kind kind() const { return kind_; }
 bool is_function() const { return kind_ == Function; }
 bool is_tuple() const { return kind_ == Tuple; }
 bool is_forall() const { return kind_ == Forall; }
 const Type& body() const;
 bool has_holes() const;
 bool has_generics() const;
 bool has_foralls() const;
 // Whether copying a value requires retaining runtime reference-counted storage.
 bool has_functions() const;
 const std::vector<Type>& parameters() const;
 const std::vector<Type>& elements() const;
 const Type& result() const;
 std::strong_ordering operator<=>(const Type&) const;
 bool operator==(const Type&) const;
private:
 Type(Kind kind, std::shared_ptr<const TypeShape> shape);
 Kind kind_;
 std::size_t variable_ = 0;
 std::shared_ptr<const TypeShape> shape_;
};

std::string type_name(const Type&);
bool has_type_holes(const Type&);
bool forall_instance(const Type& schema, const Type& instance);
bool forall_proof(const Type& schema, const Type& instance);
}
