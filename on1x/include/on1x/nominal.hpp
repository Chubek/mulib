#pragma once
#include <on1x/ast.hpp>
#include <set>

namespace on1x {
// Source declaration catalog, independent of inferred Node identities. Layout
// certificates from a PPX are never accepted. The checker reconstructs layouts.
class NominalCatalog {
 std::map<std::string,const NominalDeclaration*> declarations;
 mutable std::size_t work=262144;
 mutable std::map<Type,Type> expanded;
 mutable std::set<Type> verified;
 mutable std::map<Type,std::pair<std::size_t,std::size_t>> layouts; // width, height
 void tick(const Location&,std::size_t depth) const;
 std::pair<std::size_t,std::size_t> layout(const Type&,const Location&,std::size_t depth=0) const;
 Type expand(const Type&,const std::vector<Type>&,const Location&,std::size_t) const;
public:
 explicit NominalCatalog(const Unit&);
 const NominalDeclaration& find(const std::string&,const Location&) const;
 const NominalDeclaration& result(const std::string&,const Location&) const;
 Type instantiate(const std::string&,const std::vector<Type>&,const Location&) const;
 Type resolve(const Type&,const Location&) const;
 void verify(const Type&,const Location&) const;
};
void verify_nominals(const Unit&);
}
