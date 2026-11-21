-- Persistent ISA. Append opcodes; never renumber an existing instruction.
local v = require("vmweave")
local ops = {"Constant","Add","Sub","Mul","Div","Mod","Neg","Not","Eq","Ne",
  "Lt","Le","Gt","Ge","Load","Store","Call","Print","Assert","FloatAdd",
  "FloatSub","FloatMul","FloatDiv","FloatNeg","FloatEq","FloatNe","FloatLt",
  "FloatLe","FloatGt","FloatGe","FloatFromI64","FloatToI64","FloatFromBits",
  "FloatToBits","FloatIsNaN","FloatIsFinite","FloatSignbit","Tuple","Extract",
  "Numeric","Enter","Jump","Branch","Return"}
local instructions = {}
for opcode, name in ipairs(ops) do
  -- Typed operands are register/slot descriptors, never guest pointers.
  -- The decoder assigns a dense descriptor index to every instruction.
  local body = "extern int64_t o1vm_" .. name .. "(void *, int64_t);\n" ..
               "int64_t result = o1vm_" .. name .. "(vm->state, operand0);\n" ..
               "if (result == -2) { vm_fail(-12); return; }\n"
  if name == "Return" then
    body = body .. "if (result == -1) { vm_halt(); return; }\nvm_return();"
  elseif name == "Call" then
    body = body .. "vm_call(result);"
  elseif name == "Branch" or name == "Jump" then
    body = body .. "vm_jump(result);"
  end
  local flow = ({Call="call", Jump="jump", Branch="branch", Return="return"})[name] or "next"
  instructions[#instructions+1] = {name=name, opcode=opcode, flow=flow, operands={"cell"},
    semantics=body, source={file="On1xVM.lua", line=opcode}}
end
return v.vm {name="On1xVM",execution="switch",stack={cell="int64_t",capacity=1},
  source={file="On1xVM.lua",line=1},fields={{name="state",type="ptr"}},
  components={"token","opcode","insncode","dispatch","tape","frame"},frame_capacity=256,
  instructions=instructions}
