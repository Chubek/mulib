#pragma once

#include <on1x/ast.hpp>

#include <array>
#include <optional>
#include <string_view>
#include <utility>

namespace on1x {
enum class SharedKind { Rc, Arc, RefCell, Ref, RefMut };
enum class SharedOp { New, Clone, Read, ParallelCount, TryBorrow, TryBorrowMut, Replace };

inline constexpr char shared_borrow_result[]="@shared_v1_RefCellBorrow";
inline constexpr std::array shared_modules{
 std::pair{SharedKind::Rc,"Rc.on1x"},
 std::pair{SharedKind::Arc,"Arc.on1x"},
 std::pair{SharedKind::RefCell,"RefCell.on1x"}
};

struct SharedIntrinsic {
 SharedKind kind;
 SharedOp op;
 std::string_view spelling, identity;
 std::size_t arity;
 BorrowMode loan() const {
  return op==SharedOp::New?BorrowMode::Value:
         op==SharedOp::Replace?BorrowMode::Exclusive:BorrowMode::Shared;
 }
};

inline constexpr std::array shared_intrinsics{
 SharedIntrinsic{SharedKind::Rc,SharedOp::New,"basis_rc_new","@shared_v1_rc_new",1},
 SharedIntrinsic{SharedKind::Rc,SharedOp::Clone,"basis_rc_clone","@shared_v1_rc_clone",1},
 SharedIntrinsic{SharedKind::Rc,SharedOp::Read,"basis_rc_get","@shared_v1_rc_get",1},
 SharedIntrinsic{SharedKind::Arc,SharedOp::New,"basis_arc_new","@shared_v1_arc_new",1},
 SharedIntrinsic{SharedKind::Arc,SharedOp::Clone,"basis_arc_clone","@shared_v1_arc_clone",1},
 SharedIntrinsic{SharedKind::Arc,SharedOp::Read,"basis_arc_get","@shared_v1_arc_get",1},
 SharedIntrinsic{SharedKind::Arc,SharedOp::ParallelCount,"basis_arc_parallel_count","@shared_v1_arc_parallel_count",3},
 SharedIntrinsic{SharedKind::RefCell,SharedOp::New,"basis_refcell_new","@shared_v1_refcell_new",1},
 SharedIntrinsic{SharedKind::RefCell,SharedOp::Clone,"basis_refcell_clone","@shared_v1_refcell_clone",1},
 SharedIntrinsic{SharedKind::RefCell,SharedOp::TryBorrow,"basis_refcell_try_borrow","@shared_v1_refcell_try_borrow",1},
 SharedIntrinsic{SharedKind::RefCell,SharedOp::TryBorrowMut,"basis_refcell_try_borrow_mut","@shared_v1_refcell_try_borrow_mut",1},
 SharedIntrinsic{SharedKind::Ref,SharedOp::Read,"basis_refcell_read","@shared_v1_refcell_read",1},
 SharedIntrinsic{SharedKind::RefMut,SharedOp::Read,"basis_refcell_read_mut","@shared_v1_refcell_read_mut",1},
 SharedIntrinsic{SharedKind::RefMut,SharedOp::Replace,"basis_refcell_replace","@shared_v1_refcell_replace",2}
};

inline const SharedIntrinsic* shared_intrinsic(std::string_view name){
 for(const auto& op:shared_intrinsics)if(name==op.identity)return &op;
 return nullptr;
}
inline constexpr std::string_view shared_type_spelling(SharedKind kind){
 switch(kind){
 case SharedKind::Rc:return "Rc";
 case SharedKind::Arc:return "Arc";
 case SharedKind::RefCell:return "RefCell";
 case SharedKind::Ref:return "Ref";
 case SharedKind::RefMut:return "RefMut";
 }
 return {};
}
inline constexpr const char* shared_nominal(SharedKind kind){
 switch(kind){
 case SharedKind::Rc:return "@shared_v1_Rc";
 case SharedKind::Arc:return "@shared_v1_Arc";
 case SharedKind::RefCell:return "@shared_v1_RefCell";
 case SharedKind::Ref:return "@shared_v1_Ref";
 case SharedKind::RefMut:return "@shared_v1_RefMut";
 }
 return "";
}
inline SharedKind shared_module(SharedKind kind){
 return kind==SharedKind::Ref||kind==SharedKind::RefMut?SharedKind::RefCell:kind;
}
inline std::optional<SharedKind> shared_opaque_kind(std::string_view name){
 for(auto kind:{SharedKind::Rc,SharedKind::Arc,SharedKind::RefCell,SharedKind::Ref,SharedKind::RefMut})
  if(name==shared_nominal(kind))return kind;
 return {};
}
inline std::string_view shared_export_nominal(SharedKind module,std::string_view source){
 if(source==shared_type_spelling(module))return shared_nominal(module);
 if(module==SharedKind::RefCell){
  if(source==shared_type_spelling(SharedKind::Ref))return shared_nominal(SharedKind::Ref);
  if(source==shared_type_spelling(SharedKind::RefMut))return shared_nominal(SharedKind::RefMut);
  if(source=="RefCellBorrow")return shared_borrow_result;
 }
 return {};
}
inline bool shared_opaque_nominal(std::string_view name){
 return shared_opaque_kind(name).has_value();
}
inline bool shared_opaque_owner(const Type& type){
 return type.is_own()&&shared_opaque_nominal(type.owned().nominal());
}
[[noreturn]] inline void shared_error(const Location& at,const std::string& text){
 throw Error(at,text,"ON1X-SHARED");
}
[[noreturn]] inline void shared_opaque_error(const Location& at){
 shared_error(at,"opaque shared owner cannot be boxed, unpacked or borrowed directly");
}

inline bool shared_payload(const Type& type,SharedKind kind,std::size_t& work,
                           std::size_t depth=0){
 if(depth>32||!work)return false;
 --work;
 if(type.is_numeric()||type==Type::Bool||type==Type::Char||
    type.kind()==Type::Generic||type.kind()==Type::Bound)return true;
 if(type==Type::SExpr)return kind!=SharedKind::Arc;
 if(type.is_own())return kind==SharedKind::Rc;
 if(!type.is_tuple()&&!type.is_data())return false;
 // Arc carries only immutable, pointer-free C11 values. In particular it
 // cannot run the single-threaded On1x data/owner runtime from a native worker.
 for(const auto& field:type.elements())
  if(!shared_payload(field,kind,work,depth+1))return false;
 return true;
}
inline void require_shared_payload(const Type& type,SharedKind kind,const Location& at){
 std::size_t work=4096;
 if(!shared_payload(type,kind,work))
  shared_error(at,kind==SharedKind::Arc?
    "Arc payload must be bounded, immutable pointer-free value data":
    kind==SharedKind::Rc?"unsupported Rc payload; expected bounded value data, sexpr, or finite owner":
    "unsupported RefCell payload; expected bounded copyable value data or sexpr");
}
inline Type shared_owner(SharedKind kind,const Type& payload){
 return Type::own(Type::named(shared_nominal(kind),{payload}));
}
inline SharedKind shared_guard(SharedOp op){
 return op==SharedOp::TryBorrow?SharedKind::Ref:SharedKind::RefMut;
}
inline Type shared_borrow_type(SharedOp op,const Type& payload){
 return Type::data(Type::Sum,{Type::data(Type::Record,{}),shared_owner(shared_guard(op),payload)},
                   {"Busy","Granted"}).with_nominal(shared_borrow_result,{shared_owner(shared_guard(op),payload)});
}
inline Type shared_signature(const SharedIntrinsic& operation,const Expr& call){
 if(call.children.size()!=operation.arity)
  shared_error(call.where,"wrong arity for "+std::string(operation.spelling));
 for(const auto& child:call.children)
  if(!child)shared_error(call.where,"missing operand for "+std::string(operation.spelling));
 const auto& operand=*call.children[0];
 const auto& input=operand.type;
 if(operation.op==SharedOp::New){
  require_shared_payload(input,operation.kind,operand.where);
  if(input.is_move_only()&&operand.kind==Expr::Kind::Variable)
   shared_error(operand.where,"sharing a move-only payload requires move(local)");
  return shared_owner(operation.kind,input);
 }
 if(operand.kind!=Expr::Kind::Variable||operand.reference!=Expr::Reference::Local||
    operand.move_out)
  shared_error(operand.where,"shared operation requires a named live owner");
 if(!input.is_own()||input.owned().nominal()!=shared_nominal(operation.kind)||
    input.owned().arguments().size()!=1)
  shared_error(operand.where,"shared operation requires the matching Rc or Arc owner");
 const auto& payload=input.owned().arguments()[0];
 require_shared_payload(payload,operation.kind,operand.where);
 switch(operation.op){
 case SharedOp::New:break;
 case SharedOp::Clone:return input;
 case SharedOp::Read:
  if(payload.is_move_only())shared_error(operand.where,"shared payload cannot be copied");
  return payload;
 case SharedOp::TryBorrow:case SharedOp::TryBorrowMut:
  return shared_borrow_type(operation.op,payload);
 case SharedOp::Replace:
  if(call.children[1]->type!=payload)
   shared_error(call.where,"RefCell replacement must match the cell's payload type");
  return payload;
 case SharedOp::ParallelCount:
  if(payload.kind()!=Type::Array||payload.elements()!=std::vector<Type>{Type::U8}||
     payload.extent()<1||payload.extent()>256)
   shared_error(operand.where,"parallel count requires Arc<array<u8,N>> with 1 <= N <= 256");
  if(call.children[1]->type!=Type::U8||call.children[2]->type!=Type::I64)
   shared_error(call.where,"parallel count expects a u8 byte and i64 worker count");
  return Type::I64;
 }
 shared_error(call.where,"invalid shared operation");
}
} // namespace on1x
