#include <on1x/analysis.hpp>

#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <tuple>
#include <utility>

namespace on1x::mir::analysis {
namespace {

struct Stop {
 Status status;
 Location where;
 std::string diagnostic;
};

class Budget {
 std::size_t remaining_;
 std::size_t records_;
 std::size_t used_=0;
 const std::function<Control(std::size_t)>& control_;
public:
 Budget(const Options& options)
  :remaining_(options.work),records_(options.records),control_(options.control){}

 void tick(const Location& where,std::size_t amount=1){
  if(control_){
   Control decision=Control::Failure;
   try{decision=control_(used_);}catch(...){decision=Control::Failure;}
   if(decision==Control::Cancel)throw Stop{Status::Cancelled,where,"analysis cancelled"};
   if(decision==Control::Deadline)throw Stop{Status::DeadlineExceeded,where,"analysis deadline exceeded"};
   if(decision==Control::Failure)throw Stop{Status::CallbackFailed,where,"analysis control callback failed"};
  }
  if(amount>remaining_)throw Stop{Status::BudgetExhausted,where,"analysis work budget exhausted"};
  remaining_-=amount;used_+=amount;
 }
 void record(const Location& where){
  tick(where);
  if(!records_)throw Stop{Status::BudgetExhausted,where,"analysis result budget exhausted"};
  --records_;
 }
 void require_records(const Location& where,std::size_t amount,
                      std::size_t implicit=0)const{
  if(implicit>std::numeric_limits<std::size_t>::max()-records_||
     amount>records_+implicit)
   throw Stop{Status::BudgetExhausted,where,"analysis result budget exhausted"};
 }
 std::size_t used()const noexcept{return used_;}
};

template<class T>
void append(std::vector<T>& output,T value,Budget& budget,const Location& where){
 budget.record(where);output.push_back(std::move(value));
}

void charge_sort(std::size_t size,Budget& budget,const Location& where){
 if(size<2)return;
 std::size_t levels=0;
 for(std::size_t value=size-1;value;value>>=1)++levels;
 if(size>std::numeric_limits<std::size_t>::max()/levels)
  throw Stop{Status::BudgetExhausted,where,"analysis sort budget exhausted"};
 budget.tick(where,size*levels);
}

void charge_sum(std::size_t left,std::size_t right,Budget& budget,
                const Location& where){
 if(left>std::numeric_limits<std::size_t>::max()-right)
  throw Stop{Status::BudgetExhausted,where,"analysis work budget exhausted"};
 budget.tick(where,left+right);
}

struct Graph {
 const Function* function=nullptr;
 std::vector<const Block*> blocks;
 std::map<Id,const Block*> by_id;
 std::map<Id,std::vector<Id>> predecessors;
 std::map<Id,std::vector<Id>> successors;
};

Graph graph(const Function& function,Budget& budget){
 Graph result;result.function=&function;
 for(const auto& block:function.blocks){
  budget.tick(block.where);
  result.blocks.push_back(&block);result.by_id.emplace(block.id,&block);
  result.predecessors.try_emplace(block.id);
  result.successors.try_emplace(block.id);
 }
 charge_sort(result.blocks.size(),budget,function.where);
 std::sort(result.blocks.begin(),result.blocks.end(),[](auto a,auto b){return a->id<b->id;});
 for(const auto* block:result.blocks){
  for(const auto& edge:block->exit.edges){
   budget.tick(block->exit.where);
   result.successors[block->id].push_back(edge.block);
   result.predecessors[edge.block].push_back(block->id);
  }
 }
 for(auto& [id,values]:result.successors){
  charge_sort(values.size(),budget,result.by_id.at(id)->where);
  std::sort(values.begin(),values.end());
 }
 for(auto& [id,values]:result.predecessors){
  charge_sort(values.size(),budget,result.by_id.at(id)->where);
  std::sort(values.begin(),values.end());
 }
 return result;
}

struct DomInfo {
 std::map<Id,Id> immediate;
 std::map<Id,std::size_t> depth;
 std::map<Id,std::size_t> begin,end;
 bool dominates(Id dominator,Id block)const{
  return begin.at(dominator)<=begin.at(block)&&end.at(block)<=end.at(dominator);
 }
};

DomInfo dominators(const Graph& graph,Budget& budget){
 const Id entry=graph.function->blocks.front().id;
 std::set<Id> seen{entry};
 std::vector<Id> postorder;
 std::vector<std::pair<Id,std::size_t>> stack{{entry,0}};
 while(!stack.empty()){
  auto& [node,index]=stack.back();
  const auto& next=graph.successors.at(node);
  if(index<next.size()){
   Id child=next[index++];budget.tick(graph.by_id.at(node)->where);
   if(seen.insert(child).second)stack.emplace_back(child,0);
  }else{postorder.push_back(node);stack.pop_back();}
 }
 if(postorder.size()!=graph.blocks.size())
  throw Error(graph.function->where,"ON1X-MIR-ANALYSIS: unreachable block");
 std::vector<Id> order(postorder.rbegin(),postorder.rend());
 std::map<Id,std::size_t> position;
 for(std::size_t index=0;index<order.size();++index){
  budget.tick(graph.by_id.at(order[index])->where);position.emplace(order[index],index);
 }
 DomInfo result;
 result.immediate.emplace(entry,entry);
 auto intersect=[&](Id left,Id right){
  while(left!=right){
   budget.tick(graph.function->where);
   while(position.at(left)>position.at(right)){
    budget.tick(graph.by_id.at(left)->where);left=result.immediate.at(left);
   }
   while(position.at(right)>position.at(left)){
    budget.tick(graph.by_id.at(right)->where);right=result.immediate.at(right);
   }
  }
  return left;
 };
 bool changed=true;
 while(changed){
  changed=false;
  for(std::size_t index=1;index<order.size();++index){
   const Id block=order[index];Id next=0;
   for(Id predecessor:graph.predecessors.at(block)){
    budget.tick(graph.by_id.at(block)->where);
    if(!result.immediate.contains(predecessor))continue;
    next=next?intersect(next,predecessor):predecessor;
   }
   if(!next)throw Error(graph.by_id.at(block)->where,
                        "ON1X-MIR-ANALYSIS: missing processed predecessor");
   auto found=result.immediate.find(block);
   if(found==result.immediate.end()||found->second!=next){
    result.immediate[block]=next;changed=true;
   }
  }
 }
 std::map<Id,std::vector<Id>> children;
 for(auto* block:graph.blocks)children.try_emplace(block->id);
 for(const auto& [block,parent]:result.immediate)
  if(block!=entry)children.at(parent).push_back(block);
 for(auto& [block,values]:children){
  charge_sort(values.size(),budget,graph.by_id.at(block)->where);
  std::sort(values.begin(),values.end());
 }
 result.depth[entry]=0;
 std::size_t timestamp=0;
 std::vector<std::pair<Id,std::size_t>> tree{{entry,0}};
 result.begin[entry]=timestamp++;
 while(!tree.empty()){
  auto& [node,index]=tree.back();const auto& next=children.at(node);
  if(index<next.size()){
   Id child=next[index++];budget.tick(graph.by_id.at(child)->where);
   result.depth[child]=result.depth.at(node)+1;
   result.begin[child]=timestamp++;tree.emplace_back(child,0);
  }else{
   result.end[node]=timestamp++;tree.pop_back();
  }
 }
 result.immediate[entry]=0;
 return result;
}

struct LoopData {
 Id function=0,header=0,latch=0;
 bool reducible=true;
 std::vector<Id> members;
 std::size_t parent=SIZE_MAX;
};

std::vector<std::vector<Id>> components(const Graph& graph,Budget& budget){
 std::set<Id> seen;
 std::vector<Id> finish;
 for(auto* root:graph.blocks){
  if(seen.contains(root->id))continue;
  seen.insert(root->id);
  std::vector<std::pair<Id,std::size_t>> stack{{root->id,0}};
  while(!stack.empty()){
   auto& [node,index]=stack.back();
   const auto& next=graph.successors.at(node);
   if(index<next.size()){
    Id child=next[index++];budget.tick(graph.by_id.at(node)->where);
    if(seen.insert(child).second)stack.emplace_back(child,0);
   }else{finish.push_back(node);stack.pop_back();}
  }
 }
 seen.clear();std::vector<std::vector<Id>> output;
 for(auto it=finish.rbegin();it!=finish.rend();++it){
  if(!seen.insert(*it).second)continue;
  std::vector<Id> part,stack{*it};
  while(!stack.empty()){
   Id node=stack.back();stack.pop_back();part.push_back(node);
   for(Id parent:graph.predecessors.at(node)){
    budget.tick(graph.by_id.at(node)->where);
    if(seen.insert(parent).second)stack.push_back(parent);
   }
  }
  charge_sort(part.size(),budget,graph.function->where);
  std::sort(part.begin(),part.end());output.push_back(std::move(part));
 }
 charge_sort(output.size(),budget,graph.function->where);
 std::sort(output.begin(),output.end());
 return output;
}

std::vector<LoopData> loops(const Graph& graph,const DomInfo& dom,Budget& budget){
 std::vector<LoopData> output;std::size_t provisional_records=0;
 auto add=[&](LoopData loop,const Location& where){
  if(loop.members.size()>std::numeric_limits<std::size_t>::max()-1-
                         provisional_records)
   throw Stop{Status::BudgetExhausted,where,"analysis result budget exhausted"};
  provisional_records+=1+loop.members.size();
  budget.require_records(where,provisional_records);
  output.push_back(std::move(loop));
 };
 std::set<std::pair<Id,Id>> natural;
 for(auto* source:graph.blocks){
  for(const auto& edge:source->exit.edges){
   budget.tick(source->exit.where);
   if(!dom.dominates(edge.block,source->id)||
      !natural.emplace(edge.block,source->id).second)continue;
   std::set<Id> members{edge.block,source->id};
   std::vector<Id> pending{source->id};
   while(!pending.empty()){
    Id node=pending.back();pending.pop_back();
    for(Id predecessor:graph.predecessors.at(node)){
     budget.tick(source->where);
     if(!dom.dominates(edge.block,predecessor))continue;
     if(members.insert(predecessor).second&&predecessor!=edge.block)pending.push_back(predecessor);
    }
   }
   add({graph.function->id,edge.block,source->id,true,
        std::vector<Id>(members.begin(),members.end()),SIZE_MAX},source->where);
  }
 }
 for(const auto& part:components(graph,budget)){
  bool cyclic=part.size()>1;
  if(!cyclic){
   const auto& successors=graph.successors.at(part.front());
   cyclic=std::find(successors.begin(),successors.end(),part.front())!=successors.end();
  }
  if(!cyclic)continue;
  std::set<Id> members(part.begin(),part.end()),entries;
  if(members.contains(graph.function->blocks.front().id))
   entries.insert(graph.function->blocks.front().id);
  for(Id block:part)for(Id predecessor:graph.predecessors.at(block)){
   budget.tick(graph.by_id.at(block)->where);
   if(!members.contains(predecessor))entries.insert(block);
  }
  const bool reducible=entries.size()==1&&std::all_of(part.begin(),part.end(),[&](Id block){
   budget.tick(graph.by_id.at(block)->where);return dom.dominates(*entries.begin(),block);
  });
  if(reducible)continue;
  add({graph.function->id,0,0,false,part,SIZE_MAX},graph.function->where);
 }
 charge_sort(output.size(),budget,graph.function->where);
 std::sort(output.begin(),output.end(),[](const auto& a,const auto& b){
  return std::tie(a.function,a.reducible,a.header,a.latch,a.members)<
         std::tie(b.function,b.reducible,b.header,b.latch,b.members);
 });
 output.erase(std::unique(output.begin(),output.end(),[](const auto& a,const auto& b){
  return a.function==b.function&&a.header==b.header&&a.latch==b.latch&&
         a.reducible==b.reducible&&a.members==b.members;
 }),output.end());
 for(std::size_t child=0;child<output.size();++child){
   std::size_t best=SIZE_MAX,best_size=SIZE_MAX;
   for(std::size_t parent=0;parent<output.size();++parent){
    budget.tick(graph.function->where);
    if(parent==child||output[parent].function!=output[child].function||
       output[parent].members.size()<=output[child].members.size())continue;
    charge_sum(output[parent].members.size(),output[child].members.size(),
               budget,graph.function->where);
    if(std::includes(output[parent].members.begin(),output[parent].members.end(),
                     output[child].members.begin(),output[child].members.end())&&
       output[parent].members.size()<best_size){
     best=parent;best_size=output[parent].members.size();
   }
  }
  output[child].parent=best;
 }
 return output;
}

std::set<Id> edge_transfer(const Block& successor,const Edge& edge,
                            const std::set<Id>& needed,Budget& budget,
                            const Location& where){
 budget.tick(where,needed.size());
 std::set<Id> result=needed;
 std::vector<Id> incoming;
 for(std::size_t index=0;index<successor.parameters.size();++index){
  budget.tick(where);
  if(needed.contains(successor.parameters[index].id)){
   result.erase(successor.parameters[index].id);
   incoming.push_back(edge.arguments[index]);
  }
 }
 for(Id value:incoming){budget.tick(where);result.insert(value);}
 return result;
}

void liveness(const Graph& graph,Result& result,Budget& budget){
 std::size_t parameter_count=0;
 for(auto* block:graph.blocks){
  for(const auto& parameter:block->parameters){
   ++parameter_count;
   append(result.definitions,{graph.function->id,block->id,parameter.id,block->id,
                              DefinitionKind::BlockParameter},budget,parameter.where);
  }
  for(const auto& instruction:block->instructions){
   if(instruction.type!=Type::Void)
    append(result.definitions,{graph.function->id,block->id,instruction.id,instruction.id,
                               DefinitionKind::Instruction},budget,instruction.where);
   for(std::size_t index=0;index<instruction.operands.size();++index)
    append(result.uses,{graph.function->id,block->id,instruction.operands[index],instruction.id,0,
                        UseKind::InstructionOperand,SIZE_MAX,index},budget,instruction.where);
  }
  if(block->exit.value)
   append(result.uses,{graph.function->id,block->id,block->exit.value,block->exit.id,0,
                       UseKind::TerminatorValue,SIZE_MAX,0},budget,block->exit.where);
  for(std::size_t edge_index=0;edge_index<block->exit.edges.size();++edge_index){
   const auto& edge=block->exit.edges[edge_index];
   const auto& successor=*graph.by_id.at(edge.block);
   for(std::size_t index=0;index<edge.arguments.size();++index)
    append(result.uses,{graph.function->id,block->id,edge.arguments[index],block->exit.id,
                        successor.parameters[index].id,UseKind::EdgeArgument,edge_index,index},
                       budget,block->exit.where);
  }
 }
 std::map<Id,std::set<Id>> needed;
 for(auto* block:graph.blocks)needed[block->id]={};
 std::size_t aggregate=0;
 bool changed=true;
 while(changed){
  changed=false;
  for(auto it=graph.blocks.rbegin();it!=graph.blocks.rend();++it){
   const Block& block=**it;std::set<Id> current;
    for(const auto& edge:block.exit.edges){
     auto transferred=edge_transfer(*graph.by_id.at(edge.block),edge,needed.at(edge.block),budget,block.exit.where);
     for(Id value:transferred){budget.tick(block.exit.where);current.insert(value);}
    }
    if(block.exit.value){budget.tick(block.exit.where);current.insert(block.exit.value);}
    for(auto instruction=block.instructions.rbegin();instruction!=block.instructions.rend();++instruction){
     budget.tick(instruction->where);
     if(instruction->type!=Type::Void)current.erase(instruction->id);
     for(Id value:instruction->operands){budget.tick(instruction->where);current.insert(value);}
    }
    const auto previous=needed.at(block.id).size();
    charge_sum(previous,current.size(),budget,block.where);
    if(current!=needed.at(block.id)){
     aggregate-=previous;aggregate+=current.size();
     budget.require_records(block.where,aggregate,parameter_count);
     needed[block.id]=std::move(current);changed=true;
    }
   }
  }
  for(auto* block:graph.blocks){
   std::set<Id> parameters;
   for(const auto& parameter:block->parameters){
    budget.tick(parameter.where);parameters.insert(parameter.id);
   }
   std::set<Id> outgoing;
   for(const auto& edge:block->exit.edges){
    const auto& successor=*graph.by_id.at(edge.block);
    auto transferred=edge_transfer(successor,edge,needed.at(successor.id),budget,block->exit.where);
    for(Id value:transferred){budget.tick(block->exit.where);outgoing.insert(value);}
   }
   for(Id value:needed.at(block->id)){
    budget.tick(block->where);
    if(!parameters.contains(value))
     append(result.liveness,{graph.function->id,block->id,value,true},budget,block->where);
   }
  for(Id value:outgoing)
   append(result.liveness,{graph.function->id,block->id,value,false},budget,block->exit.where);
 }
}

std::uint32_t effect_bit(Effect effect){
 switch(effect){
 case Effect::Pure:return 0;
 case Effect::Trap:return 1u<<0;
 case Effect::Memory:return 1u<<1;
 case Effect::Call:return 1u<<2;
 case Effect::IO:return 1u<<3;
 }
 return 0;
}

void effects(const Module& module,Result& result,Budget& budget){
 std::map<Id,std::uint32_t> summaries;
 std::map<Id,std::vector<Id>> calls;
 for(const auto& function:module.functions){
  auto& summary=summaries[function.id];
  for(const auto& block:function.blocks)for(const auto& instruction:block.instructions){
   budget.tick(instruction.where);auto kind=effect(instruction.op);summary|=effect_bit(kind);
   if(kind!=Effect::Pure)
    append(result.effects,{function.id,block.id,instruction.id,kind},budget,instruction.where);
   if(instruction.op==Op::Call)calls[function.id].push_back(instruction.target);
  }
 }
 bool changed=true;
 while(changed){
  changed=false;
  for(const auto& function:module.functions)for(Id callee:calls[function.id]){
   budget.tick(function.where);auto next=summaries[function.id]|summaries.at(callee);
   if(next!=summaries[function.id]){summaries[function.id]=next;changed=true;}
  }
 }
 for(const auto& [function,mask]:summaries)
  append(result.function_effects,{function,mask},budget,{"<mir-analysis>"});
}

void clear_records(Result& result){
 result.blocks.clear();result.edges.clear();result.dominators.clear();
 result.loops.clear();result.loop_members.clear();result.definitions.clear();
 result.uses.clear();result.liveness.clear();result.effects.clear();
 result.function_effects.clear();
}

} // namespace

std::size_t Result::record_count()const noexcept{
 return blocks.size()+edges.size()+dominators.size()+loops.size()+loop_members.size()+
        definitions.size()+uses.size()+liveness.size()+effects.size()+function_effects.size();
}

Result run(const Module& module,Options options){
 verify(module,options.verification);
 Result result;result.kind=options.kind;Budget budget(options);
 try{
  if(options.kind!=Kind::CFG&&options.kind!=Kind::Dominators&&options.kind!=Kind::Loops&&
     options.kind!=Kind::LivenessUseDef&&options.kind!=Kind::Effects)
   throw Error({"<mir-analysis>"},"ON1X-MIR-ANALYSIS: unsupported analysis kind");
  if(options.kind==Kind::Effects)effects(module,result,budget);
  else for(const auto& function:module.functions){
   auto cfg=graph(function,budget);
   if(options.kind==Kind::CFG){
    for(auto* block:cfg.blocks)
     append(result.blocks,{function.id,block->id,cfg.predecessors[block->id].size(),
                           block->exit.edges.size(),block==&function.blocks.front()},budget,block->where);
    for(auto* block:cfg.blocks)for(std::size_t ordinal=0;ordinal<block->exit.edges.size();++ordinal)
     append(result.edges,{function.id,block->id,block->exit.edges[ordinal].block,
                          block->exit.id,ordinal},budget,block->exit.where);
   }else if(options.kind==Kind::LivenessUseDef)liveness(cfg,result,budget);
   else{
    auto dom=dominators(cfg,budget);
    if(options.kind==Kind::Dominators){
     for(auto* block:cfg.blocks)
      append(result.dominators,{function.id,block->id,dom.immediate.at(block->id),
                                dom.depth.at(block->id)},budget,block->where);
    }else{
     auto found=loops(cfg,dom,budget);
     const std::size_t base=result.loops.size();
     for(std::size_t ordinal=0;ordinal<found.size();++ordinal){
      const auto& loop=found[ordinal];
      append(result.loops,{function.id,loop.header,loop.latch,base+ordinal,
                           loop.parent==SIZE_MAX?SIZE_MAX:base+loop.parent,loop.reducible},
             budget,function.where);
      for(Id block:loop.members)
       append(result.loop_members,{function.id,block,base+ordinal},budget,cfg.by_id.at(block)->where);
     }
    }
   }
  }
  auto sort_all=[&]<class T>(std::vector<T>& values){
   charge_sort(values.size(),budget,{"<mir-analysis>"});
   std::sort(values.begin(),values.end());
  };
  sort_all(result.blocks);
  charge_sort(result.edges.size(),budget,{"<mir-analysis>"});
  std::sort(result.edges.begin(),result.edges.end(),[](const auto& a,const auto& b){
   return std::tie(a.function,a.source,a.ordinal,a.target,a.terminator)<
          std::tie(b.function,b.source,b.ordinal,b.target,b.terminator);
  });
  sort_all(result.dominators);
  charge_sort(result.loop_members.size(),budget,{"<mir-analysis>"});
  std::sort(result.loop_members.begin(),result.loop_members.end(),
            [](const auto& a,const auto& b){
   return std::tie(a.function,a.loop,a.block)<std::tie(b.function,b.loop,b.block);
  });
  sort_all(result.definitions);sort_all(result.uses);
  sort_all(result.liveness);sort_all(result.effects);sort_all(result.function_effects);
 }catch(const Stop& stopped){
  result.status=stopped.status;result.where=stopped.where;result.diagnostic=stopped.diagnostic;
  clear_records(result);
 }
 result.work=budget.used();return result;
}

} // namespace on1x::mir::analysis
