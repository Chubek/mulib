#pragma once
#include "MuDSL.hpp"
#include "Bridge.h"
#include <cstring>

namespace mull::stdlib::mulib_detail {
using Id = std::size_t;
struct State;
struct Box {
  State *owner; Id type; void *data; std::size_t count = 1;
  bool readonly = false, sequence = false;
  inline static const char tag = 0;
};
struct Callback { State *state; Value function; };

inline std::size_t count(const Value &v, std::size_t maximum = 64 * 1024 * 1024) {
  double n = v.number();
  if (n < 0 || n != std::floor(n) || n > double(maximum)) throw Error("invalid native size or index");
  return static_cast<std::size_t>(n);
}
inline bool compatible(Id a, Id b) {
  if (a == b) return true;
  auto ak=mull_mb_type_kind(a), bk=mull_mb_type_kind(b);
  if (ak == MULL_MB_VOID || bk == MULL_MB_VOID) return true;
  if (ak == MULL_MB_POINTER && bk == MULL_MB_POINTER)
    return compatible(mull_mb_type_element(a),mull_mb_type_element(b));
  return mull_mb_type_size(a)==1 && mull_mb_type_size(b)==1 &&
    (ak==MULL_MB_SIGNED || ak==MULL_MB_UNSIGNED) && (bk==MULL_MB_SIGNED || bk==MULL_MB_UNSIGNED);
}

struct State {
  mull_mb_arena *arena;
  std::vector<std::unique_ptr<Callback>> callbacks;
  std::map<Id,Value> modules;
  std::string pending;
  Id byteType;
  explicit State(std::size_t limit) : arena(mull_mb_arena_create(limit)),
      byteType(mull_mb_type_find(MULL_MB_NONE,"uint8_t")) {
    if (!arena) throw Error("MuLIB arena allocation failed");
  }
  ~State() { mull_mb_arena_destroy(arena); }
  State(const State &) = delete;
  State &operator=(const State &) = delete;
  void *allocate(Id type,std::size_t n=1) {
    if (!mull_mb_type_size(type)) throw Error("cannot allocate incomplete or void native type");
    auto p=mull_mb_arena_allocate(arena,type,std::max<std::size_t>(n,1));
    if (!p) throw Error("MuLIB native storage limit exceeded");
    return p;
  }
  std::shared_ptr<Box> box(const Value &v) {
    auto foreign=v.mulib();
    if (!foreign->payload || foreign->payloadType!=&Box::tag) throw Error("expected native MuLIB storage");
    auto b=std::static_pointer_cast<Box>(foreign->payload);
    if (b->owner!=this) throw Error("native value belongs to another MuLIB runtime");
    return b;
  }
  Id type(Id library,const std::string &name) {
    if (name.find('\0')!=std::string::npos) throw Error("invalid native type name");
    auto t=mull_mb_type_find(library,name.c_str());
    if (t==MULL_MB_NONE) throw Error("unknown native type: "+name);
    return t;
  }
  Id field(Id type,const std::string &name) {
    for (Id i=0;i<mull_mb_field_count(type);++i)
      if (name==mull_mb_field_name(type,i)) return i;
    throw Error("unknown native field: "+name);
  }
  Value read(Id type,void *data,bool readonly=false) {
    int kind=mull_mb_type_kind(type);
    if (kind==MULL_MB_VOID) return {};
    if (kind==MULL_MB_SIGNED || kind==MULL_MB_UNSIGNED || kind==MULL_MB_FLOAT) {
      double n;
      if (mull_mb_number_read(type,data,&n)) return n;
      char text[64];
      if (mull_mb_integer_read(type,data,text,sizeof text)) return std::string(text);
      throw Error("non-finite native numeric result");
    }
    if (kind==MULL_MB_POINTER && !mull_mb_pointer_read(data)) return {};
    return wrap({this,type,data,1,readonly});
  }
  void write(Id type,void *out,const Value &v) {
    int kind=mull_mb_type_kind(type);
    if (kind==MULL_MB_POINTER) {
      Id element=mull_mb_type_element(type); int ek=mull_mb_type_kind(element);
      if (v.type()=="nil") { mull_mb_pointer_write(out,nullptr); return; }
      if (v.type()=="function" && ek==MULL_MB_FUNCTION) {
        auto context=std::make_unique<Callback>(Callback{this,v});
        if (!mull_mb_arena_callback(arena,element,&dispatch,context.get(),out))
          throw Error("native callback slots or storage exhausted");
        callbacks.push_back(std::move(context)); return;
      }
      if (v.type()=="mulib") {
        auto b=box(v); auto bt=b->type; void *p=b->data;
        bool readonly=b->readonly;
        if (!b->sequence && mull_mb_type_kind(bt)==MULL_MB_POINTER &&
            !(ek!=MULL_MB_VOID && compatible(element,bt))) {
          readonly=readonly || mull_mb_type_pointee_const(bt);
          p=mull_mb_pointer_read(p); bt=mull_mb_type_element(bt);
        } else if (!b->sequence && mull_mb_type_kind(bt)==MULL_MB_ARRAY) bt=mull_mb_type_element(bt);
        if (!compatible(element,bt)) throw Error("incompatible native pointer type");
        if (readonly && !mull_mb_type_pointee_const(type)) throw Error("native value is read-only");
        mull_mb_pointer_write(out,p); return;
      }
      if (v.type()=="string" && (ek==MULL_MB_VOID || mull_mb_type_size(element)==1)) {
        const auto &s=v.string(); auto p=allocate(byteType,s.size()+1);
        std::memcpy(p,s.data(),s.size()); mull_mb_pointer_write(out,p); return;
      }
      if (v.type()=="list" && ek!=MULL_MB_VOID && ek!=MULL_MB_FUNCTION) {
        const auto &values=v.list();
        if (values.empty()) { mull_mb_pointer_write(out,nullptr); return; }
        auto p=static_cast<unsigned char*>(allocate(element,values.size())); auto size=mull_mb_type_size(element);
        for (std::size_t i=0;i<values.size();++i) write(element,p+i*size,values[i]);
        mull_mb_pointer_write(out,p); return;
      }
      if (v.type()=="record" && ek==MULL_MB_RECORD) {
        auto p=allocate(element); write(element,p,v); mull_mb_pointer_write(out,p); return;
      }
      throw Error("expected nil, native storage, string, list, record, or typed callback for pointer");
    }
    if (v.type()=="mulib") {
      auto b=box(v);
      if (b->type!=type || b->count!=1) throw Error("incompatible native value type");
      std::memcpy(out,b->data,mull_mb_type_size(type)); return;
    }
    if (kind==MULL_MB_SIGNED || kind==MULL_MB_UNSIGNED || kind==MULL_MB_FLOAT) {
      bool ok=false;
      if (v.type()=="number") ok=mull_mb_number_write(type,out,v.number());
      else if (v.type()=="bool") ok=mull_mb_number_write(type,out,v.truth()?1:0);
      else if (v.type()=="string" && v.string().find('\0')==std::string::npos)
        ok=mull_mb_integer_write(type,out,v.string().c_str());
      if (!ok) throw Error("native numeric value is out of range or has the wrong type");
      return;
    }
    if (kind==MULL_MB_RECORD && v.type()=="record") {
      for (const auto &[name,value]:v.object()) {
        auto f=field(type,name), ft=mull_mb_field_type(type,f); auto storage=allocate(ft);
        write(ft,storage,value);
        if (!mull_mb_field_set(type,f,out,storage)) throw Error("native field is read-only");
      }
      return;
    }
    if (kind==MULL_MB_ARRAY) {
      Id element=mull_mb_type_element(type); auto n=mull_mb_type_count(type), size=mull_mb_type_size(element);
      if (v.type()=="string" && size==1 && v.string().size()<=n) {
        std::memcpy(out,v.string().data(),v.string().size()); return;
      }
      if (v.type()=="list" && v.list().size()<=n) {
        for (std::size_t i=0;i<v.list().size();++i) write(element,static_cast<unsigned char*>(out)+i*size,v.list()[i]);
        return;
      }
    }
    throw Error("cannot convert value to native type: "+std::string(mull_mb_type_name(type)));
  }
  static void dispatch(void *user,Id signature,void *out,void **args) noexcept {
    auto &context=*static_cast<Callback*>(user); auto &state=*context.state;
    if (!state.pending.empty()) return;
    try {
      Value::List values;
      for (Id i=0;i<mull_mb_argument_count(signature);++i) {
        auto type=mull_mb_argument_type(signature,i); auto copy=state.allocate(type);
        std::memcpy(copy,args[i],mull_mb_type_size(type)); values.push_back(state.read(type,copy));
      }
      auto result=context.function.call(values); auto rt=mull_mb_result_type(signature);
      if (mull_mb_type_kind(rt)!=MULL_MB_VOID) state.write(rt,out,result);
    } catch (const std::exception &e) { state.pending=e.what(); }
      catch (...) { state.pending="native callback failed"; }
  }
  Value invoke(Id signature,const Value::List &args,Id function=MULL_MB_NONE,const void *pointer=nullptr) {
    arity(args,mull_mb_argument_count(signature));
    std::vector<void*> values;
    for (Id i=0;i<args.size();++i) {
      auto t=mull_mb_argument_type(signature,i); auto p=allocate(t); write(t,p,args[i]); values.push_back(p);
    }
    auto rt=mull_mb_result_type(signature);
    auto out=mull_mb_type_kind(rt)==MULL_MB_VOID?nullptr:allocate(rt);
    if (pointer) mull_mb_invoke_pointer(signature,pointer,out,values.data());
    else mull_mb_invoke(function,out,values.data());
    if (!pending.empty()) { auto message=std::exchange(pending,{}); throw Error("MuLIB callback: "+message); }
    return read(rt,out);
  }
  Box view(Box b) {
    if (b.sequence) return b;
    int k=mull_mb_type_kind(b.type);
    if (k==MULL_MB_POINTER) {
      b.readonly=b.readonly || mull_mb_type_pointee_const(b.type);
      b.data=mull_mb_pointer_read(b.data); b.type=mull_mb_type_element(b.type); b.count=1;
      if (!b.data) throw Error("null native pointer");
    } else if (k==MULL_MB_ARRAY) {
      b.count=mull_mb_type_count(b.type); b.type=mull_mb_type_element(b.type);
    }
    b.sequence=true; return b;
  }
  Value at(Box b,const Value &index) {
    b=view(b); auto i=count(index);
    if (i>=b.count) throw Error("native index out of bounds");
    return read(b.type,static_cast<unsigned char*>(b.data)+i*mull_mb_type_size(b.type),b.readonly);
  }
  Value member(Box b,const std::string &name) {
    if (name=="Type") return std::string(mull_mb_type_name(b.type));
    if (name=="Count") return double(view(b).count);
    if (name=="Get") return Value(Value::Native([this,b](const auto &a) { arity(a,0); return read(b.type,b.data,b.readonly); }));
    if (name=="At") return Value(Value::Native([this,b](const auto &a) { arity(a,1); return at(b,a[0]); }));
    if (name=="As") return Value(Value::Native([this,b](const auto &a) {
      if (a.empty() || a.size()>2) throw Error("As expects a type and optional count");
      auto t=type(MULL_MB_NONE,a[0].string()); auto size=mull_mb_type_size(t);
      auto n=a.size()==2?count(a[1]):1; auto v=view(b);
      if (!size || n>std::numeric_limits<std::size_t>::max()/size ||
          reinterpret_cast<std::uintptr_t>(v.data)%mull_mb_type_align(t)) throw Error("invalid native view type or size");
      if ((b.sequence || mull_mb_type_kind(b.type)!=MULL_MB_POINTER) && n*size>v.count*mull_mb_type_size(v.type))
        throw Error("native view exceeds storage");
      v.type=t; v.count=n; return wrap(v);
    }));
    if (name=="Set") return Value(Value::Native([this,b](const auto &a) {
      if (b.readonly) throw Error("native value is read-only");
      if (a.size()==1) { write(b.type,b.data,a[0]); return Value{}; }
      arity(a,2); auto v=view(b);
      if (v.readonly) throw Error("native value is read-only");
      if (a[0].type()=="string") {
        auto f=field(v.type,a[0].string()), ft=mull_mb_field_type(v.type,f); auto p=allocate(ft); write(ft,p,a[1]);
        if (!mull_mb_field_set(v.type,f,v.data,p)) throw Error("native field is read-only");
      } else {
        auto i=count(a[0]); if (i>=v.count) throw Error("native index out of bounds");
        write(v.type,static_cast<unsigned char*>(v.data)+i*mull_mb_type_size(v.type),a[1]);
      }
      return Value{};
    }));
    if (name=="View") return Value(Value::Native([this,b](const auto &a) {
      arity(a,1); auto n=count(a[0]); auto v=view(b);
      if ((b.sequence || mull_mb_type_kind(b.type)!=MULL_MB_POINTER) && n>v.count) throw Error("native view exceeds storage");
      v.count=n; return wrap(v);
    }));
    if (name=="String") return Value(Value::Native([this,b](const auto &a) {
      if (a.size()>1) throw Error("String expects zero or one argument");
      auto v=view(b); auto available=v.count*mull_mb_type_size(v.type);
      bool pointer=!b.sequence && mull_mb_type_kind(b.type)==MULL_MB_POINTER;
      auto bytes=static_cast<const char*>(v.data);
      if (!a.empty()) {
        auto n=count(a[0]); if (!pointer && n>available) throw Error("native string exceeds storage");
        return Value(std::string(bytes,n));
      }
      auto limit=pointer?std::size_t(1024*1024):available;
      std::size_t n=0; while(n<limit && bytes[n]) ++n;
      if (pointer && n==limit) throw Error("native C string limit exceeded");
      return Value(std::string(bytes,n));
    }));
    auto v=view(b); auto f=field(v.type,name), ft=mull_mb_field_type(v.type,f);
    auto p=mull_mb_field_address(v.type,f,v.data);
    if (!p) { p=allocate(ft); mull_mb_field_get(v.type,f,v.data,p); }
    return read(ft,p,v.readonly || mull_mb_field_readonly(v.type,f));
  }
  Value wrap(Box b) {
    auto native=std::make_shared<MulibValue>(); native->name=mull_mb_type_name(b.type);
    native->payload=std::make_shared<Box>(b);
    native->payloadType=&Box::tag;
    native->member=[this,b](const auto &name) { return member(b,name); };
    native->index=[this,b](const auto &i) { return at(b,i); };
    if (mull_mb_type_kind(b.type)==MULL_MB_POINTER && mull_mb_type_kind(mull_mb_type_element(b.type))==MULL_MB_FUNCTION)
      native->call=[this,b](const auto &a) {
        if (!mull_mb_pointer_read(b.data)) throw Error("null native function");
        return invoke(mull_mb_type_element(b.type),a,MULL_MB_NONE,b.data);
      };
    return Value(native);
  }
  Value helper(Id library,const std::string &name) {
    if (name=="New") return Value(Value::Native([this,library](const auto &a) {
      if (a.empty() || a.size()>2) throw Error("New expects a type and optional initializer");
      auto t=type(library,a[0].string()); auto p=allocate(t);
      if (a.size()==2) write(t,p,a[1]); return wrap({this,t,p});
    }));
    if (name=="Array") return Value(Value::Native([this,library](const auto &a) {
      arity(a,2); auto t=type(library,a[0].string()); auto n=a[1].type()=="list"?a[1].list().size():count(a[1]);
      auto p=static_cast<unsigned char*>(allocate(t,n));
      if (a[1].type()=="list") for(std::size_t i=0;i<n;++i) write(t,p+i*mull_mb_type_size(t),a[1].list()[i]);
      return wrap({this,t,p,n,false,true});
    }));
    if (name=="Buffer") return Value(Value::Native([this](const auto &a) {
      arity(a,1); auto n=a[0].type()=="string"?a[0].string().size()+1:count(a[0]); auto p=allocate(byteType,n);
      if (a[0].type()=="string") std::memcpy(p,a[0].string().data(),a[0].string().size());
      return wrap({this,byteType,p,n,false,true});
    }));
    if (name=="Callback") return Value(Value::Native([this,library](const auto &a) {
      arity(a,2); auto t=type(library,a[0].string());
      if (mull_mb_type_kind(t)!=MULL_MB_POINTER || mull_mb_type_kind(mull_mb_type_element(t))!=MULL_MB_FUNCTION)
        throw Error("Callback requires a native function-pointer typedef");
      auto p=allocate(t); write(t,p,a[1]); return wrap({this,t,p});
    }));
    if (name=="SizeOf") return Value(Value::Native([this,library](const auto &a) { arity(a,1); return Value(double(mull_mb_type_size(type(library,a[0].string())))); }));
    if (name=="Libraries") return Value(Value::Native([](const auto &a) {
      arity(a,0); Value::List names;
      for(Id i=0;i<mull_mb_library_count();++i) names.emplace_back(mull_mb_library_name(i));
      return Value(names);
    }));
    return {};
  }
  Value module(Id library) {
    if (auto it=modules.find(library);it!=modules.end()) return it->second;
    auto foreign=std::make_shared<MulibValue>(); foreign->name=mull_mb_library_name(library);
    foreign->member=[this,library](const std::string &name) -> Value {
      if (name=="LibraryName") return mull_mb_library_name(library);
      if (name=="Header") return mull_mb_library_header(library);
      if (name=="Symbols") {
        Value::List names; for(Id i=0;i<mull_mb_symbol_count(library);++i) names.emplace_back(mull_mb_symbol_name(library,i)); return names;
      }
      auto h=helper(library,name); if(h.truth()) return h;
      Id id; int kind=mull_mb_symbol_find(library,name.c_str(),&id);
      if (name.find('\0')!=std::string::npos) kind=-1;
      if (kind==0) return Value(Value::Native([this,id](const auto &a) { return invoke(mull_mb_function_type(id),a,id); }));
      if (kind==1) return Value(Value::Native([this,id](const auto &a) {
        if(a.size()>1) throw Error("native constructor expects zero or one initializer");
        auto p=allocate(id); if(!a.empty()) write(id,p,a[0]); return wrap({this,id,p});
      }));
      if (kind==2) return read(mull_mb_constant_type(id),const_cast<void*>(mull_mb_constant_data(id)),true);
      throw Error("unknown MuLIB member: "+name);
    };
    auto value=Value(foreign); modules.emplace(library,value); return value;
  }
  Value load(const std::string &name) {
    auto id=name.find('\0')==std::string::npos?mull_mb_library_find(name.c_str()):MULL_MB_NONE;
    if (id==MULL_MB_NONE) throw Error("unknown MuLIB library: "+name);
    return module(id);
  }
};
} // namespace mull::stdlib::mulib_detail

namespace mull::stdlib {
inline void installMulib(Runtime &runtime,std::size_t nativeBytes=64*1024*1024) {
  auto state=runtime.retainState(std::make_shared<mulib_detail::State>(nativeBytes));
  runtime.mulibWrapper=[state](const auto &name) { return state->load(name); };
  Value::Object api;
  for(const char *name:{"New","Array","Buffer","Callback","SizeOf","Libraries"}) api[name]=state->helper(MULL_MB_NONE,name);
  api["Wrap"]=Value(Value::Native([state](const auto &a) { arity(a,1); return state->load(a[0].string()); }));
  runtime.registerLibrary("Mulib",Value(api));
}
}
