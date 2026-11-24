#include "lower.hpp"

#include <algorithm>
#include <array>
#include <set>
#include <stdexcept>
#include <vector>

namespace on1x::ffi::c11 {
namespace {
[[noreturn]] void invalid(const char* message) {
    throw std::invalid_argument(message);
}

bool identifier(std::string_view s) {
    const auto alpha=[](char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_';};
    if(s.empty()||!alpha(s.front())) return false;
    for(char c:s) if(!alpha(c)&&!(c>='0'&&c<='9')) return false;
    return true;
}

void require_target(const Target& target) {
    if(target.triple!="x86_64-linux-gnu"||target.data_model!="LP64"||
       target.abi!="SysVAMD64"||target.calling_convention!="sysv")
        invalid("FFIHandler C11 lowering requires Linux x86-64 SysV LP64");
}

std::string_view surface_type(SurfaceKind kind) {
    switch(kind){
    case SurfaceKind::Void:return "void";
    case SurfaceKind::I64:return "int64_t";
    case SurfaceKind::U8:return "uint8_t";
    case SurfaceKind::U16:return "uint16_t";
    case SurfaceKind::U32:return "uint32_t";
    case SurfaceKind::U64:return "uint64_t";
    case SurfaceKind::F32:return "float";
    case SurfaceKind::F64:return "double";
    case SurfaceKind::Bool:return "_Bool";
    }
    invalid("FFIHandler C11 lowering: unknown surface type");
}

struct Integer {
    std::string_view identity, spelling, min, max;
    unsigned rank, bits;
    bool is_signed;
};

// Exact Cephyr identity, not a fixed-width integer typedef. In particular
// long and long long are incompatible C types even on LP64.
constexpr std::array integers{
    Integer{"bool","_Bool","","",0,1,false},
    Integer{"char","char","CHAR_MIN","CHAR_MAX",1,8,true},
    Integer{"signed-char","signed char","SCHAR_MIN","SCHAR_MAX",1,8,true},
    Integer{"unsigned-char","unsigned char","","UCHAR_MAX",1,8,false},
    Integer{"short","short","SHRT_MIN","SHRT_MAX",2,16,true},
    Integer{"unsigned-short","unsigned short","","USHRT_MAX",2,16,false},
    Integer{"int","int","INT_MIN","INT_MAX",3,32,true},
    Integer{"unsigned-int","unsigned int","","UINT_MAX",3,32,false},
    Integer{"long","long","LONG_MIN","LONG_MAX",4,64,true},
    Integer{"unsigned-long","unsigned long","","ULONG_MAX",4,64,false},
    Integer{"long-long","long long","LLONG_MIN","LLONG_MAX",5,64,true},
    Integer{"unsigned-long-long","unsigned long long","","ULLONG_MAX",5,64,false}
};

const Integer& native_integer(const NativeType& native) {
    for(const auto& i:integers)
        if(i.identity==native.identity&&i.rank==native.rank&&i.bits==native.bits&&
           (i.identity=="char"||i.is_signed==native.is_signed)&&native.format.empty())
            return i;
    invalid("FFIHandler C11 lowering: unsupported native integer");
}

std::string_view native_type(const NativeType& native) {
    switch(native.kind){
    case NativeKind::Void:
        if(native.identity.empty()||native.identity=="void") return "void";
        break;
    case NativeKind::Integer:return native_integer(native).spelling;
    case NativeKind::Real:
        if(native.identity=="float"&&native.format=="binary32"&&native.bits==32) return "float";
        if(native.identity=="double"&&native.format=="binary64"&&native.bits==64) return "double";
        break;
    }
    invalid("FFIHandler C11 lowering: unsupported native type");
}

unsigned unsigned_width(SurfaceKind kind) {
    switch(kind){
    case SurfaceKind::U8:return 8;
    case SurfaceKind::U16:return 16;
    case SurfaceKind::U32:return 32;
    case SurfaceKind::U64:return 64;
    default:return 0;
    }
}

void require_mapping(SurfaceKind surface,const NativeType& native) {
    switch(surface){
    case SurfaceKind::Void:
        if(native.kind==NativeKind::Void) return;
        break;
    case SurfaceKind::I64:
        if(native.kind==NativeKind::Integer&&native.identity!="bool"&&
           (native.is_signed||native.bits<64)) return;
        break;
    case SurfaceKind::Bool:
        if(native.kind==NativeKind::Integer&&native.identity=="bool") return;
        break;
    case SurfaceKind::F32:case SurfaceKind::F64:
        if(native.kind==NativeKind::Real&&native_type(native)==surface_type(surface)) return;
        break;
    default:
        if(native.kind==NativeKind::Integer&&native.identity!="bool"&&!native.is_signed&&
           unsigned_width(surface)==native.bits) return;
    }
    invalid("FFIHandler C11 lowering: unsupported scalar conversion");
}

void require_descriptor(const Descriptor& desc) {
    require_target(desc.target);
    if(!identifier(desc.symbol)||desc.parameters.size()>16||
       (desc.placement!=Placement::External&&desc.placement!=Placement::StaticInline))
        invalid("FFIHandler C11 lowering: invalid scalar descriptor");
    require_mapping(desc.surface_result,desc.native_result);
    (void)native_type(desc.native_result);
    for(const auto& p:desc.parameters){
        if(p.surface==SurfaceKind::Void)
            invalid("FFIHandler C11 lowering: void parameter");
        require_mapping(p.surface,p.native);
        (void)native_type(p.native);
    }
    if(desc.plan.guards.size()>16) invalid("FFIHandler C11 lowering: too many guards");
    for(const auto& guard:desc.plan.guards){
        if(guard.lhs>=desc.parameters.size()||guard.rhs>=desc.parameters.size())
            invalid("FFIHandler C11 lowering: invalid guard position");
        const auto surface=desc.parameters[guard.lhs].surface;
        if(surface!=desc.parameters[guard.rhs].surface||
           (surface!=SurfaceKind::I64&&!unsigned_width(surface)))
            invalid("FFIHandler C11 lowering: guard operands must have the same integral surface type");
    }
}

std::string layout_assertion(const NativeType& native) {
    if(native.kind==NativeKind::Void) return {};
    const std::string type(native_type(native));
    if(!native.size||!native.alignment)
        invalid("FFIHandler C11 lowering: missing native layout");
    std::string check="sizeof("+type+") == "+std::to_string(native.size)+
        " && _Alignof("+type+") == "+std::to_string(native.alignment);
    if(native.kind==NativeKind::Integer&&native.identity!="bool"){
        const auto& i=native_integer(native);
        const auto bits=std::to_string(native.bits);
        check+=" && sizeof("+type+") * CHAR_BIT == "+bits;
        if(native.is_signed){
            check+=" && "+std::string(i.min)+" == INT"+bits+"_MIN && "+
                std::string(i.max)+" == INT"+bits+"_MAX";
        }else{
            if(!i.min.empty()) check+=" && "+std::string(i.min)+" == 0";
            check+=" && "+std::string(i.max)+" == UINT"+bits+"_MAX";
        }
    }else if(native.kind==NativeKind::Real){
        check+=native.bits==32?
            " && FLT_RADIX == 2 && FLT_MANT_DIG == 24 && FLT_MIN_EXP == -125 && FLT_MAX_EXP == 128 && FLT_EVAL_METHOD == 0":
            " && FLT_RADIX == 2 && DBL_MANT_DIG == 53 && DBL_MIN_EXP == -1021 && DBL_MAX_EXP == 1024 && FLT_EVAL_METHOD == 0";
    }
    return "_Static_assert("+check+", \"FFIHandler primitive layout drift\");\n";
}

std::string params(const Descriptor& desc,bool native,bool named) {
    if(desc.parameters.empty()) return "void";
    std::string out;
    std::size_t index=0;
    for(const auto& p:desc.parameters){
        if(index) out+=", ";
        out+=native?native_type(p.native):surface_type(p.surface);
        if(named) out+=" a"+std::to_string(index);
        ++index;
    }
    return out;
}

std::string range_guard(const Parameter& p,const std::string& arg) {
    if(p.surface!=SurfaceKind::I64||p.native.bits==64) return {};
    const auto& native=native_integer(p.native);
    const auto min=p.native.is_signed?std::string(native.min):"0";
    return "    on1x_assert("+arg+" >= "+min+" && "+arg+" <= "+std::string(native.max)+");\n";
}
}

std::string target_requirements(const Target& target) {
    require_target(target);
    return "#include <float.h>\n#include <limits.h>\n#include <stdint.h>\n"
           "#if !defined(ON1X_KERNEL_ABI_V1) || ON1X_KERNEL_ABI_V1 != 1 || "
           "!defined(__linux__) || !defined(__x86_64__) || defined(__ILP32__)\n"
           "#error \"FFIHandler requires the verified native Linux x86-64 build profile\"\n#endif\n"
           "_Static_assert(CHAR_BIT == 8 && sizeof(int64_t) == 8 && _Alignof(int64_t) == 8 && "
           "sizeof(long) == 8 && sizeof(void *) == 8, \"FFIHandler target ABI drift\");\n";
}

std::string scalar_header(const Descriptor& desc) {
    // Exactly the validated manifest-key header spelling. No arbitrary
    // includes or C injection via descriptor strings.
    if(desc.provider.empty()||desc.header!="mulib-"+std::string(desc.provider)+".h")
        invalid("FFIHandler C11 lowering: invalid canonical header");
    for(char c:desc.provider)
        if(!(c>='a'&&c<='z')&&!(c>='0'&&c<='9')&&c!='-'&&c!='+'&&c!='_')
            invalid("FFIHandler C11 lowering: invalid provider key");
    return "#include <"+std::string(desc.header)+">\n";
}

std::string linked_name(const Descriptor& desc,LinkName kind) {
    if(!identifier(desc.symbol)) invalid("FFIHandler C11 lowering: invalid native identifier");
    if(kind==LinkName::Legacy) return "on1x_kernel_scalar_v1_"+std::string(desc.symbol);
    const auto id=desc.id.sha256;
    if(id.size()!=64||!std::all_of(id.begin(),id.end(),[](char c){
        return (c>='a'&&c<='f')||(c>='0'&&c<='9');
    })) invalid("FFIHandler C11 lowering: invalid descriptor ID");
    return "on1x_ffi_v2_"+std::string(id);
}

std::string scalar_prototype(const Descriptor& desc,std::string_view name) {
    require_descriptor(desc);
    if(!identifier(name)) invalid("FFIHandler C11 lowering: invalid emitted identifier");
    return "extern "+std::string(surface_type(desc.surface_result))+" "+std::string(name)+
           "("+params(desc,false,false)+");\n";
}

std::string scalar_wrapper(const Descriptor& desc,std::string_view name,Linkage linkage) {
    require_descriptor(desc);
    if(!identifier(name)||name==desc.symbol) invalid("FFIHandler C11 lowering: invalid emitted identifier");
    const std::string symbol(desc.symbol);
    std::string checks;
    bool real=false;
    auto layout=[&](const NativeType& native){
        const auto assertion=layout_assertion(native);
        if(!assertion.empty()&&checks.find(assertion)==std::string::npos) checks+=assertion;
        real|=native.kind==NativeKind::Real;
    };
    layout(desc.native_result);
    for(const auto& p:desc.parameters) layout(p.native);
    std::string out=(linkage==Linkage::Local?"/* unsafe_foreign: ":"/* foreign adapter: ")+symbol+" */\n";
    if(real) out+="#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) || "
                  "defined(__NO_SIGNED_ZEROS__) || defined(__ASSOCIATIVE_MATH__) || defined(__RECIPROCAL_MATH__)\n"
                  "#error \"FFIHandler forbids relaxed floating-point modes\"\n#endif\n";
    out+=checks+"_Static_assert(_Generic(&"+symbol+", "+std::string(native_type(desc.native_result))+
         " (*)("+params(desc,true,false)+"): 1, default: 0), \"FFIHandler prototype drift\");\n";
    if(linkage==Linkage::Local) out+="static ";
    out+=std::string(surface_type(desc.surface_result))+" "+std::string(name)+"("+params(desc,false,true)+") {\n";
    std::string args;
    for(std::size_t i=0;i<desc.parameters.size();++i){
        const auto& p=desc.parameters[i];
        const auto index=std::to_string(i), arg="a"+index, slot="native_"+index;
        out+=range_guard(p,arg);
        const std::string type(native_type(p.native));
        out+="    "+type+" "+slot+" = ("+type+")"+arg+";\n";
        if(i) args+=", ";
        args+=slot;
    }
    const auto call="("+symbol+")("+args+")";
    if(desc.surface_result==SurfaceKind::Void) out+="    "+call+";\n";
    else out+="    return ("+std::string(surface_type(desc.surface_result))+")"+call+";\n";
    return out+="}\n";
}

std::string linked_scalar_tu(std::span<const Descriptor* const> descriptors,LinkName names) {
    if(descriptors.empty()||descriptors.size()>256)
        invalid("FFIHandler C11 lowering: invalid adapter partition size");
    std::vector<const Descriptor*> sorted;
    for(const auto* desc:descriptors){
        if(!desc) invalid("FFIHandler C11 lowering: missing descriptor");
        if(names==LinkName::DescriptorId&&desc->schema_version!=2)
            invalid("FFIHandler C11 lowering: v2 adapter needs a projected descriptor");
        require_descriptor(*desc);
        sorted.push_back(desc);
    }
    std::sort(sorted.begin(),sorted.end(),[](const Descriptor* lhs,const Descriptor* rhs){
        return lhs->id.sha256<rhs->id.sha256;
    });
    std::string out="/* Generated by on1xc for checked foreign descriptors. */\n#include <on1x_runtime.h>\n"+
                    target_requirements(sorted.front()->target);
    std::set<std::string_view> headers;
    for(const auto* desc:sorted)
        if(headers.insert(desc->header).second) out+=scalar_header(*desc);
    std::set<std::string> emitted;
    for(const auto* desc:sorted){
        const auto name=linked_name(*desc,names);
        if(!emitted.insert(name).second) invalid("FFIHandler C11 lowering: duplicate adapter name");
        out+=scalar_prototype(*desc,name)+scalar_wrapper(*desc,name,Linkage::External);
    }
    return out;
}

std::string scalar_guards(const Descriptor& desc,std::span<const std::string> values,
                          std::string_view indent) {
    require_descriptor(desc);
    if(values.size()!=desc.parameters.size()) invalid("FFIHandler C11 lowering: guard arity mismatch");
    for(const auto& value:values)
        if(!identifier(value)) invalid("FFIHandler C11 lowering: expected a saved argument temporary");
    std::string out;
    for(const auto& guard:desc.plan.guards)
        out+=std::string(indent)+"on1x_assert("+values[guard.lhs]+" <= "+values[guard.rhs]+");\n";
    return out;
}
}
