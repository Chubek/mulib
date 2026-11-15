#pragma once
#include <on1x/backend/mir.hpp>
#include <on1x/backend/passes.hpp>
#include <novostone/CCDB/CCDB.hpp>

namespace on1x::mir::relational {
inline constexpr unsigned mapping_version = 1;
struct ImportLimits {
 mir::Limits mir;
 std::size_t facts=262144, bytes=8*1024*1024;
};
struct BudgetExceeded : Error { using Error::Error; };
// Authoritative .dl source is parsed by Novostone's Aurocks/ASDL frontend.
const novostone::datalog::Program& program();
novostone::datalog::Module project(const Module& module, ImportLimits limits={});
// Untrusted fact artifacts have no authority without their exact verified MIR
// and the exact schema/rule catalog. Missing/extra/mistyped facts all reject.
void check_projection(const Module& module, const novostone::datalog::Module& facts,
                      ImportLimits limits={});

class Query;
class Store;
class Snapshot {
public:
 std::uint64_t revision() const { return data_.revision(); }
 const Module& module() const { return *module_; }
 const std::string& source() const { return *source_; }
 const novostone::datalog::Facts& facts() const { return data_.facts(); }
 Query query(const std::string& relation, novostone::datalog::Limits limits={}) const;
 // Rejects stale AND foreign snapshots, even when revision numbers are equal.
 void require_current(const Query& query) const;
private:
 Snapshot(novostone::ccdb::Snapshot data, std::shared_ptr<const Module> module,
          std::shared_ptr<const std::string> source)
   : data_(std::move(data)), module_(std::move(module)), source_(std::move(source)) {}
 novostone::ccdb::Snapshot data_;
 std::shared_ptr<const Module> module_;
 std::shared_ptr<const std::string> source_;
 friend class Store;
 friend class Query;
};
class Query {
public:
 const novostone::ccdb::QueryResult& result() const { return result_; }
 const Snapshot& source() const { return source_; }
 const std::string& relation() const { return relation_; }
private:
 Query(Snapshot source, std::string relation, novostone::ccdb::QueryResult result)
   : source_(std::move(source)), relation_(std::move(relation)), result_(std::move(result)) {}
 Snapshot source_;
 std::string relation_;
 novostone::ccdb::QueryResult result_;
 friend class Snapshot;
};
// Store mutation is single-threaded. Immutable snapshots/queries may outlive it.
// Publication stages a verified complete projection and commits only fact deltas.
struct Optimization { Snapshot snapshot; passes::Result result; };
class Store {
public:
 explicit Store(const Module& module, ImportLimits limits={});
 Snapshot snapshot() const { return head_; }
 Snapshot publish(const Snapshot& expected, const Module& module, ImportLimits limits={},
                  std::string reason="on1x.mir-facts/v1 publication");
 Snapshot replace_function(const Snapshot& expected, const Function& function, ImportLimits limits={});
 // Runs built-in verified passes and publishes their completed prefix against
 // this exact snapshot. No-op/exhausted-without-progress keeps the same revision.
 Optimization optimize(const Snapshot& expected, passes::Options options={}, ImportLimits limits={});
 std::vector<novostone::ccdb::Event> events() const { return database_.events(); }
private:
 novostone::ccdb::Database database_;
 Snapshot head_;
};
std::string dump_facts(const Snapshot& snapshot);
// Deterministic S-expression with exact MIR/rule identities and query dependencies.
// Query work is cumulative across the report; exhaustion produces no report.
std::string report(const Snapshot& snapshot, novostone::datalog::Limits limits={});
}
