module mull.stdlib.Mulib.Native;
import MuDSL;
import mull.stdlib.Mulib.Bridge;
import core.stdc.string : memcpy;
import std.string : fromStringz, toStringz;
import std.math : floor;
import std.algorithm : canFind, max;

private alias V = MuLLValue;
private string text(const(char)* p) { return p ? p.fromStringz.idup : ""; }
private size_t nativeCount(V v, size_t maximum = 64 * 1024 * 1024)
{
    auto n = v.number();
    if (n < 0 || n != floor(n) || n > cast(double) maximum) throw new MuLLError("invalid native size or index");
    return cast(size_t) n;
}
private bool compatible(size_t a, size_t b)
{
    if (a == b) return true;
    auto ak = mull_mb_type_kind(a), bk = mull_mb_type_kind(b);
    if (ak == MULL_MB_VOID || bk == MULL_MB_VOID) return true;
    if (ak == MULL_MB_POINTER && bk == MULL_MB_POINTER)
        return compatible(mull_mb_type_element(a), mull_mb_type_element(b));
    return mull_mb_type_size(a) == 1 && mull_mb_type_size(b) == 1 &&
        (ak == MULL_MB_SIGNED || ak == MULL_MB_UNSIGNED) && (bk == MULL_MB_SIGNED || bk == MULL_MB_UNSIGNED);
}
private class Box
{
    State owner;
    size_t type, count = 1;
    void* data;
    bool readonly, sequence;
    this(State owner, size_t type, void* data, size_t count = 1, bool readonly = false, bool sequence = false)
    { this.owner=owner; this.type=type; this.data=data; this.count=count; this.readonly=readonly; this.sequence=sequence; }
    Box copy() { return new Box(owner,type,data,count,readonly,sequence); }
}
private class Callback
{
    State state; V functionValue;
    this(State s, V f) { state=s; functionValue=f; }
}
private class State
{
    mull_mb_arena* arena;
    Callback[] callbacks;
    V[size_t] modules;
    string pending;
    size_t byteType;
    this(size_t limit)
    {
        arena=mull_mb_arena_create(limit);
        byteType=mull_mb_type_find(MULL_MB_NONE,"uint8_t".ptr);
        if (!arena) throw new MuLLError("MuLIB arena allocation failed");
    }
    ~this() { mull_mb_arena_destroy(arena); }
    void* allocate(size_t type, size_t n = 1)
    {
        if (!mull_mb_type_size(type)) throw new MuLLError("cannot allocate incomplete or void native type");
        auto p=mull_mb_arena_allocate(arena,type,max(n,size_t(1)));
        if (!p) throw new MuLLError("MuLIB native storage limit exceeded");
        return p;
    }
    Box box(V v)
    {
        auto b=cast(Box) v.mulib().payload;
        if (b is null) throw new MuLLError("expected native MuLIB storage");
        if (b.owner !is this) throw new MuLLError("native value belongs to another MuLIB runtime");
        return b;
    }
    size_t type(size_t library, string name)
    {
        if (name.canFind('\0')) throw new MuLLError("invalid native type name");
        auto t=mull_mb_type_find(library,name.toStringz);
        if (t == MULL_MB_NONE) throw new MuLLError("unknown native type: " ~ name);
        return t;
    }
    size_t field(size_t type, string name)
    {
        foreach (i; 0 .. mull_mb_field_count(type)) if (text(mull_mb_field_name(type,i)) == name) return i;
        throw new MuLLError("unknown native field: " ~ name);
    }
    V read(size_t type, void* data, bool readonly = false)
    {
        auto kind=mull_mb_type_kind(type);
        if (kind == MULL_MB_VOID) return V.init;
        if (kind == MULL_MB_SIGNED || kind == MULL_MB_UNSIGNED || kind == MULL_MB_FLOAT)
        {
            double n;
            if (mull_mb_number_read(type,data,&n)) return V(n);
            char[64] buffer;
            if (mull_mb_integer_read(type,data,buffer.ptr,buffer.length)) return V(text(buffer.ptr));
            throw new MuLLError("non-finite native numeric result");
        }
        if (kind == MULL_MB_POINTER && !mull_mb_pointer_read(data)) return V.init;
        return wrap(new Box(this,type,data,1,readonly));
    }
    void write(size_t type, void* out_, V v)
    {
        auto kind=mull_mb_type_kind(type);
        if (kind == MULL_MB_POINTER)
        {
            auto element=mull_mb_type_element(type); auto ek=mull_mb_type_kind(element);
            if (v.type() == "nil") { mull_mb_pointer_write(out_,null); return; }
            if (v.type() == "function" && ek == MULL_MB_FUNCTION)
            {
                auto context=new Callback(this,v);
                if (!mull_mb_arena_callback(arena,element,&dispatch,cast(void*)context,out_))
                    throw new MuLLError("native callback slots or storage exhausted");
                callbacks ~= context; return;
            }
            if (v.type() == "mulib")
            {
                auto b=box(v); auto bt=b.type; auto p=b.data;
                bool readonly=b.readonly;
                if (!b.sequence && mull_mb_type_kind(bt) == MULL_MB_POINTER &&
                    !(ek != MULL_MB_VOID && compatible(element,bt)))
                {
                    readonly=readonly || mull_mb_type_pointee_const(bt) != 0;
                    p=mull_mb_pointer_read(p); bt=mull_mb_type_element(bt);
                }
                else if (!b.sequence && mull_mb_type_kind(bt) == MULL_MB_ARRAY) bt=mull_mb_type_element(bt);
                if (!compatible(element,bt)) throw new MuLLError("incompatible native pointer type");
                if (readonly && !mull_mb_type_pointee_const(type)) throw new MuLLError("native value is read-only");
                mull_mb_pointer_write(out_,p); return;
            }
            if (v.type() == "string" && (ek == MULL_MB_VOID || mull_mb_type_size(element) == 1))
            {
                auto s=v.stringValue(); auto p=allocate(byteType,s.length+1);
                memcpy(p,s.ptr,s.length); mull_mb_pointer_write(out_,p); return;
            }
            if (v.type() == "list" && ek != MULL_MB_VOID && ek != MULL_MB_FUNCTION)
            {
                auto values=v.list();
                if (!values.length) { mull_mb_pointer_write(out_,null); return; }
                auto p=cast(ubyte*)allocate(element,values.length); auto size=mull_mb_type_size(element);
                foreach (i,value; values) write(element,p+i*size,value);
                mull_mb_pointer_write(out_,p); return;
            }
            if (v.type() == "record" && ek == MULL_MB_RECORD)
            { auto p=allocate(element); write(element,p,v); mull_mb_pointer_write(out_,p); return; }
            throw new MuLLError("expected nil, native storage, string, list, record, or typed callback for pointer");
        }
        if (v.type() == "mulib")
        {
            auto b=box(v);
            if (b.type != type || b.count != 1) throw new MuLLError("incompatible native value type");
            memcpy(out_,b.data,mull_mb_type_size(type)); return;
        }
        if (kind == MULL_MB_SIGNED || kind == MULL_MB_UNSIGNED || kind == MULL_MB_FLOAT)
        {
            bool ok;
            if (v.type() == "number") ok=mull_mb_number_write(type,out_,v.number()) != 0;
            else if (v.type() == "bool") ok=mull_mb_number_write(type,out_,v.truth()?1:0) != 0;
            else if (v.type() == "string" && !v.stringValue().canFind('\0'))
                ok=mull_mb_integer_write(type,out_,v.stringValue().toStringz) != 0;
            if (!ok) throw new MuLLError("native numeric value is out of range or has the wrong type");
            return;
        }
        if (kind == MULL_MB_RECORD && v.type() == "record")
        {
            import std.algorithm : sort;
            auto values=v.object();
            foreach (name; values.keys.sort)
            {
                auto f=field(type,name), ft=mull_mb_field_type(type,f); auto storage=allocate(ft);
                write(ft,storage,values[name]);
                if (!mull_mb_field_set(type,f,out_,storage)) throw new MuLLError("native field is read-only");
            }
            return;
        }
        if (kind == MULL_MB_ARRAY)
        {
            auto element=mull_mb_type_element(type); auto n=mull_mb_type_count(type), size=mull_mb_type_size(element);
            if (v.type() == "string" && size == 1 && v.stringValue().length <= n)
            { auto s=v.stringValue(); memcpy(out_,s.ptr,s.length); return; }
            if (v.type() == "list")
            {
                auto values=v.list();
                if (values.length <= n)
                { foreach (i,value; values) write(element,(cast(ubyte*)out_)+i*size,value); return; }
            }
        }
        throw new MuLLError("cannot convert value to native type: " ~ text(mull_mb_type_name(type)));
    }
    extern(C) static void dispatch(void* user, size_t signature, void* out_, void** args) nothrow
    {
        auto context=cast(Callback)user; auto state=context.state;
        if (state.pending.length) return;
        try
        {
            V[] values;
            foreach (i; 0 .. mull_mb_argument_count(signature))
            {
                auto type=mull_mb_argument_type(signature,i); auto copy=state.allocate(type);
                memcpy(copy,args[i],mull_mb_type_size(type)); values ~= state.read(type,copy);
            }
            auto result=context.functionValue.call(values); auto rt=mull_mb_result_type(signature);
            if (mull_mb_type_kind(rt) != MULL_MB_VOID) state.write(rt,out_,result);
        }
        catch (Throwable e) { state.pending=e.msg; }
    }
    V invoke(size_t signature, V[] args, size_t functionId = MULL_MB_NONE, const(void)* pointer = null)
    {
        mullArity(args,mull_mb_argument_count(signature));
        void*[] values;
        foreach (i,arg; args)
        {
            auto t=mull_mb_argument_type(signature,i); auto p=allocate(t); write(t,p,arg); values ~= p;
        }
        auto rt=mull_mb_result_type(signature);
        auto out_=mull_mb_type_kind(rt) == MULL_MB_VOID ? null : allocate(rt);
        if (pointer) mull_mb_invoke_pointer(signature,pointer,out_,values.ptr);
        else mull_mb_invoke(functionId,out_,values.ptr);
        if (pending.length) { auto message=pending; pending=""; throw new MuLLError("MuLIB callback: " ~ message); }
        return read(rt,out_);
    }
    Box view(Box source)
    {
        auto b=source.copy(); auto k=mull_mb_type_kind(b.type);
        if (b.sequence) return b;
        if (k == MULL_MB_POINTER)
        {
            b.readonly=b.readonly || mull_mb_type_pointee_const(b.type) != 0;
            b.data=mull_mb_pointer_read(b.data); b.type=mull_mb_type_element(b.type); b.count=1;
            if (!b.data) throw new MuLLError("null native pointer");
        }
        else if (k == MULL_MB_ARRAY) { b.count=mull_mb_type_count(b.type); b.type=mull_mb_type_element(b.type); }
        b.sequence=true; return b;
    }
    V at(Box source, V index)
    {
        auto b=view(source); auto i=nativeCount(index);
        if (i >= b.count) throw new MuLLError("native index out of bounds");
        return read(b.type,(cast(ubyte*)b.data)+i*mull_mb_type_size(b.type),b.readonly);
    }
    V member(Box b, string name)
    {
        if (name == "Type") return V(text(mull_mb_type_name(b.type)));
        if (name == "Count") return V(cast(double)view(b).count);
        if (name == "Get") return V((V[] a) { mullArity(a,0); return read(b.type,b.data,b.readonly); });
        if (name == "At") return V((V[] a) { mullArity(a,1); return at(b,a[0]); });
        if (name == "As") return V((V[] a) {
            if (!a.length || a.length > 2) throw new MuLLError("As expects a type and optional count");
            auto t=type(MULL_MB_NONE,a[0].stringValue()); auto size=mull_mb_type_size(t);
            auto n=a.length == 2 ? nativeCount(a[1]) : 1; auto v=view(b);
            if (!size || n > size_t.max/size || (cast(size_t)v.data)%mull_mb_type_align(t))
                throw new MuLLError("invalid native view type or size");
            if ((b.sequence || mull_mb_type_kind(b.type) != MULL_MB_POINTER) && n*size > v.count*mull_mb_type_size(v.type))
                throw new MuLLError("native view exceeds storage");
            v.type=t; v.count=n; return wrap(v);
        });
        if (name == "Set") return V((V[] a) {
            if (b.readonly) throw new MuLLError("native value is read-only");
            if (a.length == 1) { write(b.type,b.data,a[0]); return V.init; }
            mullArity(a,2); auto v=view(b);
            if (v.readonly) throw new MuLLError("native value is read-only");
            if (a[0].type() == "string")
            {
                auto f=field(v.type,a[0].stringValue()), ft=mull_mb_field_type(v.type,f); auto p=allocate(ft); write(ft,p,a[1]);
                if (!mull_mb_field_set(v.type,f,v.data,p)) throw new MuLLError("native field is read-only");
            }
            else
            {
                auto i=nativeCount(a[0]); if (i >= v.count) throw new MuLLError("native index out of bounds");
                write(v.type,(cast(ubyte*)v.data)+i*mull_mb_type_size(v.type),a[1]);
            }
            return V.init;
        });
        if (name == "View") return V((V[] a) {
            mullArity(a,1); auto n=nativeCount(a[0]); auto v=view(b);
            if ((b.sequence || mull_mb_type_kind(b.type) != MULL_MB_POINTER) && n > v.count) throw new MuLLError("native view exceeds storage");
            v.count=n; return wrap(v);
        });
        if (name == "String") return V((V[] a) {
            if (a.length > 1) throw new MuLLError("String expects zero or one argument");
            auto v=view(b); auto available=v.count*mull_mb_type_size(v.type);
            bool pointer=!b.sequence && mull_mb_type_kind(b.type) == MULL_MB_POINTER;
            auto bytes=cast(const(char)*)v.data;
            if (a.length)
            {
                auto n=nativeCount(a[0]); if (!pointer && n > available) throw new MuLLError("native string exceeds storage");
                return V(bytes[0 .. n].idup);
            }
            auto limit=pointer ? size_t(1024*1024) : available;
            size_t n; while (n < limit && bytes[n]) ++n;
            if (pointer && n == limit) throw new MuLLError("native C string limit exceeded");
            return V(bytes[0 .. n].idup);
        });
        auto v=view(b); auto f=field(v.type,name), ft=mull_mb_field_type(v.type,f);
        auto p=mull_mb_field_address(v.type,f,v.data);
        if (!p) { p=allocate(ft); mull_mb_field_get(v.type,f,v.data,p); }
        return read(ft,p,v.readonly || mull_mb_field_readonly(v.type,f) != 0);
    }
    V wrap(Box b)
    {
        auto native=new MuLLMulib; native.name=text(mull_mb_type_name(b.type)); native.payload=b;
        native.member=(string name) { return member(b,name); };
        native.index=(V i) { return at(b,i); };
        if (mull_mb_type_kind(b.type) == MULL_MB_POINTER && mull_mb_type_kind(mull_mb_type_element(b.type)) == MULL_MB_FUNCTION)
            native.call=(V[] a) {
                if (!mull_mb_pointer_read(b.data)) throw new MuLLError("null native function");
                return invoke(mull_mb_type_element(b.type),a,MULL_MB_NONE,b.data);
            };
        return V(native);
    }
    V helper(size_t library, string name)
    {
        if (name == "New") return V((V[] a) {
            if (!a.length || a.length > 2) throw new MuLLError("New expects a type and optional initializer");
            auto t=type(library,a[0].stringValue()); auto p=allocate(t);
            if (a.length == 2) write(t,p,a[1]); return wrap(new Box(this,t,p));
        });
        if (name == "Array") return V((V[] a) {
            mullArity(a,2); auto t=type(library,a[0].stringValue());
            auto n=a[1].type() == "list" ? a[1].list().length : nativeCount(a[1]);
            auto p=cast(ubyte*)allocate(t,n);
            if (a[1].type() == "list") foreach (i,value; a[1].list()) write(t,p+i*mull_mb_type_size(t),value);
            return wrap(new Box(this,t,p,n,false,true));
        });
        if (name == "Buffer") return V((V[] a) {
            mullArity(a,1); auto n=a[0].type() == "string" ? a[0].stringValue().length+1 : nativeCount(a[0]); auto p=allocate(byteType,n);
            if (a[0].type() == "string") { auto s=a[0].stringValue(); memcpy(p,s.ptr,s.length); }
            return wrap(new Box(this,byteType,p,n,false,true));
        });
        if (name == "Callback") return V((V[] a) {
            mullArity(a,2); auto t=type(library,a[0].stringValue());
            if (mull_mb_type_kind(t) != MULL_MB_POINTER || mull_mb_type_kind(mull_mb_type_element(t)) != MULL_MB_FUNCTION)
                throw new MuLLError("Callback requires a native function-pointer typedef");
            auto p=allocate(t); write(t,p,a[1]); return wrap(new Box(this,t,p));
        });
        if (name == "SizeOf") return V((V[] a) { mullArity(a,1); return V(cast(double)mull_mb_type_size(type(library,a[0].stringValue()))); });
        if (name == "Libraries") return V((V[] a) {
            mullArity(a,0); V[] names;
            foreach (i; 0 .. mull_mb_library_count()) names ~= V(text(mull_mb_library_name(i)));
            return V(names);
        });
        return V.init;
    }
    V library(size_t id)
    {
        if (auto p=id in modules) return *p;
        auto foreign=new MuLLMulib; foreign.name=text(mull_mb_library_name(id));
        foreign.member=(string name) {
            if (name == "LibraryName") return V(text(mull_mb_library_name(id)));
            if (name == "Header") return V(text(mull_mb_library_header(id)));
            if (name == "Symbols")
            {
                V[] names; foreach (i; 0 .. mull_mb_symbol_count(id)) names ~= V(text(mull_mb_symbol_name(id,i))); return V(names);
            }
            auto h=helper(id,name); if (h.truth()) return h;
            size_t symbol; auto kind=name.canFind('\0') ? -1 : mull_mb_symbol_find(id,name.toStringz,&symbol);
            if (kind == 0) return V((V[] a) { return invoke(mull_mb_function_type(symbol),a,symbol); });
            if (kind == 1) return V((V[] a) {
                if (a.length > 1) throw new MuLLError("native constructor expects zero or one initializer");
                auto p=allocate(symbol); if (a.length) write(symbol,p,a[0]); return wrap(new Box(this,symbol,p));
            });
            if (kind == 2) return read(mull_mb_constant_type(symbol),cast(void*)mull_mb_constant_data(symbol),true);
            throw new MuLLError("unknown MuLIB member: " ~ name);
        };
        auto value=V(foreign); modules[id]=value; return value;
    }
    V load(string name)
    {
        auto id=name.canFind('\0') ? MULL_MB_NONE : mull_mb_library_find(name.toStringz);
        if (id == MULL_MB_NONE) throw new MuLLError("unknown MuLIB library: " ~ name);
        return library(id);
    }
}

/// Install the generated MuLIB C bridge. Link the mull-mulib-bridge library.
void installMulib(MuLLRuntime runtime, size_t nativeBytes = 64*1024*1024)
{
    auto state=new State(nativeBytes);
    runtime.mulibWrapper=(string name) { return state.load(name); };
    V[string] api;
    foreach (name; ["New","Array","Buffer","Callback","SizeOf","Libraries"]) api[name]=state.helper(MULL_MB_NONE,name);
    api["Wrap"]=V((V[] a) { mullArity(a,1); return state.load(a[0].stringValue()); });
    runtime.registerLibrary("Mulib",V(api));
}
