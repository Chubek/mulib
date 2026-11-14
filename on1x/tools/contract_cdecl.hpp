#pragma once
#include "contract_admission.hpp"
namespace on1x::contracts {
// This SDK revision's public CDecl JSON reader only reconstructs declarations.
// Restore every v2 wire field into the PUBLIC CDecl structs, then require a
// lossless round trip through its canonical serializer. No private SDK headers,
// alternate type IDs, inferred layouts, or changes to LibStone are involved.
inline bool boolean(const json::Value& n){require(n.kind=='b',"expected CDecl boolean");return n.text=="true";}
inline std::vector<std::string> strings(const json::Value& n){array(n,131072);std::vector<std::string> out;for(const auto& s:n.array)out.push_back(text(s));return out;}
inline moonstone::SourceSpan source_span(const json::Value& n){
 object(n,{"file","line","column","end_line","end_column"});moonstone::SourceSpan out;out.file=text(n.at("file"));
 out.line=static_cast<std::uint32_t>(json::natural(n.at("line")));out.column=static_cast<std::uint32_t>(json::natural(n.at("column")));out.end_line=static_cast<std::uint32_t>(json::natural(n.at("end_line")));out.end_column=static_cast<std::uint32_t>(json::natural(n.at("end_column")));return out;
}
inline std::optional<std::uint64_t> optional_number(const json::Value& n){return n.kind=='n'?std::nullopt:std::optional<std::uint64_t>{json::natural(n,UINT64_MAX)};}
inline cdecl::TranslationUnit parse_cdecl(const std::string& wire){
 auto root=json::parse(wire);
 object(root,{"schema_version","extractor_version","state","safe_export","unknown_effects","complete","header","revision","fingerprint","target","compilation","preprocessing","arguments","inputs","types","declarations","diagnostics"});
 require(json::natural(root.at("schema_version"))==cdecl::schema_version&&text(root.at("extractor_version"))==cdecl::extractor_version,"unsupported CDecl/extractor version");
 require(text(root.at("state"))=="discovered"&&!boolean(root.at("safe_export"))&&boolean(root.at("unknown_effects")),"CDecl discovery must not claim reviewed/safe semantics");
 auto unit=cdecl::parse_translation_unit_json(wire);
 const auto& target=root.at("target");object(target,{"compiler","triple","pointer_bits"});
 unit.target.compiler=text(target.at("compiler"));unit.target.triple=text(target.at("triple"));unit.target.pointer_bits=static_cast<unsigned>(json::natural(target.at("pointer_bits")));
 unit.arguments=strings(root.at("arguments"));array(root.at("inputs"),131072);
 for(const auto& n:root.at("inputs").array){object(n,{"path","sha256"});unit.inputs.push_back({text(n.at("path")),text(n.at("sha256"))});}
 const auto& compilation=root.at("compilation");
 if(compilation.kind!='n'){
  object(compilation,{"database","directory","file","driver","original_arguments","ignored_arguments"});cdecl::CompilationContext c;
  c.database=text(compilation.at("database"));c.directory=text(compilation.at("directory"));c.file=text(compilation.at("file"));c.driver=text(compilation.at("driver"));c.original_arguments=strings(compilation.at("original_arguments"));c.ignored_arguments=strings(compilation.at("ignored_arguments"));unit.compilation=std::move(c);
 }
 const std::vector<std::string> kinds={"unknown","void","integer","floating","pointer","array","function","typedef","record","enum","elaborated"};
 array(root.at("types"),16384);
 for(const auto& n:root.at("types").array){
  object(n,{"id","canonical_id","spelling","canonical_spelling","declaration_id","kind","const","volatile","restrict","size","alignment","array_count","signed","element","result","arguments","variadic","prototype","union","calling_convention","attributes","unsupported_reason","fields","enumerators"});
  cdecl::Type t;t.id=text(n.at("id"));t.canonical_id=text(n.at("canonical_id"));t.spelling=text(n.at("spelling"));t.canonical_spelling=text(n.at("canonical_spelling"));t.declaration_id=text(n.at("declaration_id"));
  auto kind=text(n.at("kind"));auto found=std::find(kinds.begin(),kinds.end(),kind);require(found!=kinds.end(),"unknown CDecl type kind");t.kind=static_cast<cdecl::TypeKind>(found-kinds.begin());
  t.qualifiers={boolean(n.at("const")),boolean(n.at("volatile")),boolean(n.at("restrict"))};t.size=optional_number(n.at("size"));t.alignment=optional_number(n.at("alignment"));t.array_count=optional_number(n.at("array_count"));
  if(n.at("signed").kind!='n')t.is_signed=boolean(n.at("signed"));
  t.element=text(n.at("element"));t.result=text(n.at("result"));t.arguments=strings(n.at("arguments"));t.variadic=boolean(n.at("variadic"));t.prototype=boolean(n.at("prototype"));t.is_union=boolean(n.at("union"));t.calling_convention=static_cast<unsigned>(json::natural(n.at("calling_convention")));t.attributes=strings(n.at("attributes"));t.unsupported_reason=text(n.at("unsupported_reason"));
  array(n.at("fields"),16384);for(const auto& f:n.at("fields").array){object(f,{"name","type","source","offset_bits","bit_width"});cdecl::Field field;field.name=text(f.at("name"));field.type=text(f.at("type"));field.source=source_span(f.at("source"));field.offset_bits=optional_number(f.at("offset_bits"));if(f.at("bit_width").kind!='n')field.bit_width=static_cast<unsigned>(json::natural(f.at("bit_width")));t.fields.push_back(std::move(field));}
  array(n.at("enumerators"),16384);for(const auto& e:n.at("enumerators").array){object(e,{"name","value"});t.enumerators.push_back({text(e.at("name")),text(e.at("value"))});}
  require(!t.id.empty()&&unit.types.emplace(t.id,t).second,"duplicate/empty CDecl type ID");
 }
 array(root.at("declarations"),16384);require(root.at("declarations").array.size()==unit.declarations.size(),"CDecl declaration transport mismatch");
 for(std::size_t i=0;i<unit.declarations.size();++i){
  const auto& n=root.at("declarations").array[i];object(n,{"id","name","kind","type","source","definition","inline","function_like_macro","linkage","tokens","unsupported_reason","parameters"});auto& d=unit.declarations[i];
  d.source=source_span(n.at("source"));d.tokens=text(n.at("tokens"));d.unsupported_reason=text(n.at("unsupported_reason"));
  array(n.at("parameters"),16384);require(d.parameters.size()==n.at("parameters").array.size(),"CDecl parameter transport mismatch");
  for(std::size_t p=0;p<d.parameters.size();++p){const auto& nparam=n.at("parameters").array[p];object(nparam,{"name","type","source","tokens"});d.parameters[p].source=source_span(nparam.at("source"));d.parameters[p].tokens=text(nparam.at("tokens"));}
 }
 const auto& pp=root.at("preprocessing");object(pp,{"coverage","final_snapshot_complete","definitions","expansions","final_macros","includes","skipped_ranges"});unit.preprocessing.coverage=text(pp.at("coverage"));unit.preprocessing.final_snapshot_complete=boolean(pp.at("final_snapshot_complete"));
 array(pp.at("definitions"),131072);for(const auto& n:pp.at("definitions").array){object(n,{"id","name","tokens","source","function_like","builtin"});cdecl::MacroDefinition d;d.id=text(n.at("id"));d.name=text(n.at("name"));d.tokens=strings(n.at("tokens"));d.source=source_span(n.at("source"));d.function_like=boolean(n.at("function_like"));d.builtin=boolean(n.at("builtin"));unit.preprocessing.definitions.push_back(std::move(d));}
 array(pp.at("expansions"),131072);for(const auto& n:pp.at("expansions").array){object(n,{"name","definition_id","unknown_reason","source"});unit.preprocessing.expansions.push_back({text(n.at("name")),text(n.at("definition_id")),text(n.at("unknown_reason")),source_span(n.at("source"))});}
 array(pp.at("final_macros"),131072);for(const auto& n:pp.at("final_macros").array){object(n,{"name","definition_id","unknown_reason","defined"});cdecl::MacroState s;s.name=text(n.at("name"));s.definition_id=text(n.at("definition_id"));s.unknown_reason=text(n.at("unknown_reason"));if(n.at("defined").kind!='n')s.defined=boolean(n.at("defined"));unit.preprocessing.final_macros.push_back(std::move(s));}
 array(pp.at("includes"),131072);for(const auto& n:pp.at("includes").array){object(n,{"included","source"});unit.preprocessing.includes.push_back({text(n.at("included")),source_span(n.at("source"))});}
 array(pp.at("skipped_ranges"),131072);for(const auto& n:pp.at("skipped_ranges").array)unit.preprocessing.skipped_ranges.push_back(source_span(n));
 array(root.at("diagnostics"),16384);for(const auto& n:root.at("diagnostics").array){object(n,{"id","severity","message","source","related"});moonstone::Diagnostic d;d.id=text(n.at("id"));d.message=text(n.at("message"));d.primary=source_span(n.at("source"));auto s=text(n.at("severity"));require(s=="note"||s=="warning"||s=="error"||s=="fatal","invalid diagnostic severity");d.severity=s=="note"?moonstone::Severity::Note:s=="warning"?moonstone::Severity::Warning:s=="error"?moonstone::Severity::Error:moonstone::Severity::Fatal;array(n.at("related"),16384);for(const auto& related:n.at("related").array)d.related.push_back(source_span(related));unit.diagnostics.push_back(std::move(d));}
 auto reference=[&](const std::string& id){require(!id.empty()&&unit.types.contains(id),"dangling CDecl type reference: "+id);};
 for(const auto& [id,t]:unit.types){
  (void)id;if(!t.canonical_id.empty())reference(t.canonical_id);if(!t.element.empty())reference(t.element);if(!t.result.empty())reference(t.result);
  for(const auto& a:t.arguments)reference(a);
  for(const auto& f:t.fields)reference(f.type);
  if(t.kind==cdecl::TypeKind::Function)reference(t.result);
 }
 for(const auto& d:unit.declarations){
  if(!d.type.empty())reference(d.type);
  for(const auto& p:d.parameters)reference(p.type);
  if(d.kind==cdecl::DeclKind::Function){
   require(!d.type.empty()&&unit.types.at(d.type).kind==cdecl::TypeKind::Function,"function declaration lacks a function type");
   const auto& t=unit.types.at(d.type);require(t.arguments.size()==d.parameters.size(),"CDecl function parameter count mismatch");
   for(std::size_t i=0;i<t.arguments.size();++i)require(t.arguments[i]==d.parameters[i].type,"CDecl function parameter type mismatch");
  }
 }
 require(json::parse(cdecl::describe_translation_unit(unit))==root,"CDecl evidence cannot round-trip losslessly through this SDK model");
 return unit;
}
}
