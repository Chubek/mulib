#include <on1x/module_algebra.hpp>
#include <stdexcept>
#include <random>
using namespace on1x;
using namespace on1x::oml;
static void check(bool result) { if (!result) throw std::logic_error("environment law failed"); }
template<class F> static void rejects(F f) {
    try { f(); } catch (const std::invalid_argument&) { return; }
    throw std::logic_error("ill-formed environment operation accepted");
}
static void laws(const ModuleProvenance& owner) {
    std::mt19937 rng(314159);
    for (unsigned trial = 0; trial < 64; ++trial) {
        Environment a,b,c;
        std::set<Name> x,y,intersection;
        for (int i=0; i<6; ++i) {
            Name key{Namespace::Value,std::to_string(i)};
            Binding binding{"declaration:"+key.spelling,"int","seal",owner,{}};
            if (rng()%2) a.emplace(key,binding);
            if (rng()%2) b.emplace(key,binding);
            if (rng()%2) c.emplace(key,binding);
            if (rng()%2) x.insert(key);
            if (rng()%2) y.insert(key);
            if (x.contains(key) && y.contains(key)) intersection.insert(key);
        }
        auto union1 = merge(merge(a,b),c), union2 = merge(a,merge(b,c));
        auto pa = project(project(a,x),y), pb = project(a,intersection);
        auto id = merge(a,{}), idem = merge(a,a);
        for(int i=0; i<6; ++i) {
            Name key{Namespace::Value,std::to_string(i)};
            for (const auto* u : {&union1,&union2}) {
                check(u->contains(key) == (a.contains(key)||b.contains(key)||c.contains(key)));
                if (u->contains(key)) check(u->at(key).declaration == "declaration:" + key.spelling);
            }
            for (const auto* p : {&pa,&pb}) check(p->contains(key) == (a.contains(key)&&x.contains(key)&&y.contains(key)));
            check(id.contains(key) == a.contains(key) && idem.contains(key) == a.contains(key));
        }
    }
    Name k{Namespace::Value,"x"}, alias{Namespace::Value,"y"};
    Binding d{"decl-id","int","secret",owner,{DeclarationVisibility::Private}};
    Environment e{{k,d}};
    auto r = rename(e,{{k,alias}});
    check(r.at(alias).seal == d.seal && r.at(alias).declaration == d.declaration);
    check(project(r,{alias},true).empty());
    check(select(r,{alias},owner,false).size()==1);
    rejects([&]{ select(r,{alias},ModuleProvenance{},true); });
    rejects([&]{ select(r,{alias,alias},owner,true); });
    rejects([&]{ select(r,{k},owner,true); });
    rejects([&]{ rename(merge(e,r),{{k,alias}}); });
    d.declaration = "impostor";
    rejects([&]{ merge(e,{{k,d}}); });
    check(e.at(k).declaration == "decl-id");
}
int main() {
    const ModuleProvenance owner{"/p/a", "/p", {"A", "B"}};
    laws(owner);
    for (auto kind : {DeclarationVisibility::Public, DeclarationVisibility::Unspecified,
                      DeclarationVisibility::Shared, DeclarationVisibility::Private})
        for (size_t elevation = 0; elevation <= 3; ++elevation)
            for (auto file : {"/p/a", "/p/b", ""}) for (auto package : {"/p", "/q", ""})
                for (auto path : {std::vector<std::string>{}, {"A"}, {"A","B","C"}, {"B"}, {"."}})
                    for (bool admitted : {false,true}) {
                        ModuleProvenance requester{file, package, path};
                        VisibilityRule rule{kind,elevation};
                        if (oml::access(rule,owner,requester,admitted) != module_visibility_access(rule,owner,requester,admitted))
                            throw std::runtime_error("OML/legacy access mismatch");
                    }
    for(auto a : {ModuleAccess::Allowed,ModuleAccess::Private})
        for(auto b : {ModuleAccess::Allowed,ModuleAccess::SharedScope})
            if(oml::reexport(a,b) != (a == ModuleAccess::Allowed && b == ModuleAccess::Allowed))
                throw std::runtime_error("reexport widened access");
}
