#pragma once
#include <on1x/ast.hpp>
#include <string_view>

namespace on1x::opaque {
inline std::string_view function_owner(const Unit& unit,const std::string& function){
 auto owner=unit.opaque_function_owners.find(function);
 return owner==unit.opaque_function_owners.end()?std::string_view{}:owner->second;
}
inline bool hidden(const Unit& unit,const std::string& nominal,std::string_view scope){
 if(nominal.empty())return false;
 auto seal=unit.opaque_nominal_owners.find(nominal);
 return seal!=unit.opaque_nominal_owners.end()&&(scope.empty()||seal->second!=scope);
}
[[noreturn]] inline void inaccessible(const Location& at){
 throw Error(at,"opaque module type representation is not accessible here",
             "ON1X-INTERFACE-OPAQUE");
}
inline void require_access(const Unit& unit,const std::string& nominal,
                           std::string_view scope,const Location& at){
 if(hidden(unit,nominal,scope))inaccessible(at);
}
}
