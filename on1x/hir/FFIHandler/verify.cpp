#include <on1x/ffi_hir.hpp>
#include <on1x/kernel.hpp>

namespace on1x::ffi {
namespace {
[[noreturn]] void invalid(const Location& at,const char* message){
 throw Error(at,std::string("ON1X-FFI-VERIFY: ")+message,"ON1X-FFI-VERIFY");
}
}

std::string foreign_hir_name(const HirForeign& reference){
 if(!reference)return {};
 const char* prefix="";
 switch(reference.mode()){
 case ForeignMode::Unsafe:prefix="@ffi_unsafe_";break;
 case ForeignMode::LinkOnly:prefix="@ffi_link_";break;
 case ForeignMode::UnsafeLinkOnly:prefix="@ffi_unsafe_link_";break;
 }
 return prefix+std::string(reference.descriptor()->id.sha256);
}

Type surface_type(SurfaceKind kind){
 switch(kind){
 case SurfaceKind::Void:return Type::Void;
 case SurfaceKind::I64:return Type::I64;
 case SurfaceKind::U8:return Type::U8;
 case SurfaceKind::U16:return Type::U16;
 case SurfaceKind::U32:return Type::U32;
 case SurfaceKind::U64:return Type::U64;
 case SurfaceKind::F32:return Type::F32;
 case SurfaceKind::F64:return Type::F64;
 case SurfaceKind::Bool:return Type::Bool;
 }
 return Type::Invalid;
}

const Descriptor* verify_foreign_declaration(const Function& function,std::string_view expected_target){
 if(!function.foreign){
  if(is_foreign_hir_name(function.name))invalid(function.where,"missing compiler-owned foreign identity");
  return nullptr;
 }
 const auto* descriptor=function.foreign.descriptor();
 if(find_descriptor(descriptor->id)!=descriptor||
    find_descriptor(descriptor->module,descriptor->member)!=descriptor)
  invalid(function.where,"stale or noncanonical foreign descriptor identity");
 if(descriptor->schema_version!=2)invalid(function.where,"unsupported foreign descriptor schema");
 if(!expected_target.empty()&&descriptor->target.triple!=expected_target)
  invalid(function.where,"foreign descriptor is incompatible with the selected native target");
 if(function.name!=foreign_hir_name(function.foreign)||!function.external||
    function.prototype||function.extension_generated||!function.body.empty()||
    !function.attributes.empty()||!function.type_parameters.empty()||!function.constraints.empty()||
    !function.kernel_symbol.empty()||function.kernel_link_only)
  invalid(function.where,"invalid compiler-owned foreign declaration");
 const auto result=surface_type(descriptor->surface_result);
 if(result==Type::Invalid||function.result!=result||function.params.size()!=descriptor->parameters.size())
  invalid(function.where,"foreign declaration does not match the descriptor signature");
 for(std::size_t i=0;i<function.params.size();++i){
  const auto& parameter=function.params[i];
  const auto type=surface_type(descriptor->parameters[i].surface);
  if(type==Type::Invalid||type==Type::Void||parameter.type!=type||
     parameter.borrow!=BorrowMode::Value||parameter.pattern||parameter.context)
   invalid(parameter.where,"foreign parameter does not match the descriptor signature");
 }
 // For the current scalar plan the only user-declared operations are ordered
 // comparisons on existing integral arguments. No initialization, loan or
 // ownership claim is inferred from them; rich operations need their own
 // verifier and lowering before they can enter the executable registry.
 if(descriptor->effect!=Effect::Unknown)invalid(function.where,"unsupported foreign effect");
 for(const auto& guard:descriptor->plan.guards){
  if(guard.lhs>=function.params.size()||guard.rhs>=function.params.size())
   invalid(function.where,"out-of-bounds foreign guard operand");
  const auto& left=function.params[guard.lhs].type;
  if((left!=Type::I64&&!left.is_unsigned())||left!=function.params[guard.rhs].type)
   invalid(function.where,"incompatible foreign guard operands");
 }
 return descriptor;
}

void verify_foreign_reference(const Expr& callee,const Function& declaration){
 if(callee.foreign!=declaration.foreign||
    (declaration.foreign&&
     (callee.kind!=Expr::Kind::Variable||callee.text!=declaration.name||callee.prototype)))
  invalid(callee.where,"foreign call reference does not match its declaration");
}

ArgumentPolicy argument_policy(const Function& declaration,std::size_t index){
 if(index>=declaration.params.size()||declaration.kernel_symbol.empty())return {};
 if(index==0&&declaration.kernel_symbol==ipc_kernel_roundtrip)
  return {ArgumentPolicy::Mode::ConsumeOwner};
 if(index==0&&ipc_kernel_session_loan(declaration.kernel_symbol))
  return {ArgumentPolicy::Mode::ExclusiveOwner};
 if(kernel_span_argument(declaration.kernel_symbol,index))
  return {ArgumentPolicy::Mode::BoundedSlice,256};
 return {};
}

bool argument_type(ArgumentPolicy policy,const Type& expected,const Type& actual){
 if(policy.mode==ArgumentPolicy::Mode::BoundedSlice)
  return actual.kind()==Type::Slice&&expected.kind()==Type::Slice&&
         actual.elements()==expected.elements()&&actual.extent()<=policy.max_extent;
 if(policy.mode==ArgumentPolicy::Mode::ConsumeOwner||policy.mode==ArgumentPolicy::Mode::ExclusiveOwner)
  return expected.is_own()&&actual==expected;
 return actual==expected;
}

bool requires_call_loan(const Function& declaration){
 for(std::size_t i=0;i<declaration.params.size();++i)
  if(argument_policy(declaration,i).mode==ArgumentPolicy::Mode::ExclusiveOwner)return true;
 return false;
}
}
