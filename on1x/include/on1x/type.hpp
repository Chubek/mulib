#pragma once
#include <compare>
#include <memory>
#include <string>
#include <vector>

namespace on1x {
struct FunctionType;

// Immutable structural monotypes. Function types share owned C++ storage;
// nothing refers to the scanner arena or foreign CDecl type IDs.
class Type {
public:
 enum Kind { Invalid, I64, Bool, String, Void, Json, JsonView, Function, Hole };
 Type(Kind kind = Invalid);
 static Type function(std::vector<Type> parameters, Type result);
 Kind kind() const { return kind_; }
 bool is_function() const { return kind_ == Function; }
 bool has_holes() const;
 const std::vector<Type>& parameters() const;
 const Type& result() const;
 std::strong_ordering operator<=>(const Type&) const;
 bool operator==(const Type&) const;
private:
 explicit Type(std::shared_ptr<const FunctionType> signature);
 Kind kind_;
 std::shared_ptr<const FunctionType> signature_;
};

std::string type_name(const Type&);
bool has_type_holes(const Type&);
}
