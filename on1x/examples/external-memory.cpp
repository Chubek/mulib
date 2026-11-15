// Compiler-side resource protocol for an external handle API. These events must
// come from an admitted call descriptor; they do not inspect or certify C code.
#include <on1x/safety.hpp>

int main(){
 namespace c=moonstone::chisel;
 auto event=[](c::Op op,c::Id token=0){
  return c::Instruction{op,1,token,{__FILE__,__LINE__,1,__LINE__,1}};
 };
 // acquire -> shared view -> read -> end view -> mutate -> release
 c::Function client{{{0,{
  event(c::Op::Create),event(c::Op::BorrowShared,10),event(c::Op::Use,10),
  event(c::Op::EndBorrow,10),event(c::Op::Mutate),event(c::Op::Drop)
 },{}}},0};
 on1x::safety::verify(client);
}
