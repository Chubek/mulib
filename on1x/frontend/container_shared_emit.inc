// Inside the direct C11 Generator. Both source checking and the independent
// canonical verifier have checked the signatures and revalidated the loan.
std::string emit_shared_call(const SharedIntrinsic& operation,const Expr& call){
 uses_shared=true;
 const auto& operand=*call.children[0];
 const std::string atomic=operation.kind==SharedKind::Arc?"true":"false";
 if(operation.op==SharedOp::New){
  auto source=move_source(operand.type,expr(operand));
  if(uses_group_deadlines)cancellation_edge(call.where);
  auto destroy=needs_drop(operand.type)?cleanup_destructor(operand.type):"NULL";
  const bool cell=operation.kind==SharedKind::RefCell;
  std::string constructor=cell?"on1x_shared_v1_cell_new(":"on1x_shared_v1_new(";
  auto result=temp(call.type,constructor+"&"+source+", sizeof("+source+"), "+destroy+
                   (cell?")":", "+atomic+")"));
  allocation_failure(result,call.where);
  return result;
 }
 const auto owner=ref(operand.text);
 if(operation.op==SharedOp::Clone){
  auto result=temp(call.type,"on1x_shared_v1_clone("+owner+", "+atomic+")");
  allocation_failure(result,call.where);
  return result;
 }
 if(operation.op==SharedOp::Read){
  auto result=temp(call.type,"{0}");
  definitions<<pad()<<"on1x_shared_v1_read("<<owner<<", "<<atomic
             <<", &"<<result<<", sizeof("<<result<<"));\n";
  retain(call.type,result);
  return result;
 }
 if(operation.op==SharedOp::TryBorrow||operation.op==SharedOp::TryBorrowMut){
  const auto result=temp(call.type,"{0}"),guard=unique("guard"),status=unique("loan_status");
  definitions<<pad()<<"on1x_unique *"<<guard<<" = NULL;\n"
             <<pad()<<"int "<<status<<" = on1x_shared_v1_cell_try_borrow("
             <<owner<<", "<<(operation.op==SharedOp::TryBorrow?"false":"true")
             <<", &"<<guard<<");\n";
  allocation_failure("("+status+" >= 0)",call.where);
  definitions<<pad()<<"if ("<<status<<" == 1) {\n"
             <<pad()<<"  "<<result<<".payload.element_1 = "<<guard<<";\n"
             <<pad()<<"  "<<result<<".tag = INT64_C(1);\n"<<pad()<<"}\n";
  return result;
 }
 if(operation.op==SharedOp::Replace){
  const auto& replacement=*call.children[1];
  const auto source=move_source(replacement.type,expr(replacement));
  if(uses_group_deadlines)cancellation_edge(call.where);
  auto result=temp(call.type,"{0}");
  definitions<<pad()<<"on1x_shared_v1_cell_replace("<<owner<<", &"<<source
             <<", &"<<result<<", sizeof("<<result<<"));\n";
  return result;
 }
 const auto byte=temp(Type::U8,expr(*call.children[1]));
 const auto workers=temp(Type::I64,expr(*call.children[2]));
 if(uses_group_deadlines)cancellation_edge(call.where);
 return temp(call.type,"on1x_shared_v1_parallel_count("+owner+", "+
              std::to_string(operand.type.owned().arguments()[0].extent())+
              ", "+byte+", "+workers+")");
}
