package jit

import (
	"domscript/internal/ir"
)

// ScheduledInstr is an instruction paired with its scheduled cycle.
type ScheduledInstr struct {
	Instr ir.Instr
	Cycle int
}

// DepEdge represents a data dependency between two instructions.
type DepEdge struct {
	From int // producer instruction index
	To   int // consumer instruction index
	Lat  int // latency from producer to consumer
}

// DAG holds the data-dependency graph for a basic block.
type DAG struct {
	Nodes []int        // instruction indices in original order
	Edges []DepEdge    // data dependencies
	Ready []int        // nodes with no pending dependencies
	InDegree []int     // number of unsatisfied incoming edges
}

// Schedule performs instruction scheduling on a sequence of IR instructions.
// It builds a data-dependency DAG and uses a list-scheduling algorithm
// to reorder instructions for improved ILP, respecting dependencies.
func Schedule(code []ir.Instr) []ScheduledInstr {
	if len(code) == 0 {
		return nil
	}

	dag := buildDAG(code)
	return listSchedule(code, dag)
}

// buildDAG constructs a data-dependency graph for the instruction sequence.
// It tracks which instructions produce values consumed by later instructions.
func buildDAG(code []ir.Instr) *DAG {
	dag := &DAG{
		Nodes:    make([]int, len(code)),
		InDegree: make([]int, len(code)),
	}
	for i := range code {
		dag.Nodes[i] = i
	}

	// Track which instruction last defined each virtual register.
	lastDef := make(map[int64]int) // virtual reg -> last def instruction index

	for i, in := range code {
		defs, uses := defUse(in)

		// Add edges from last defs to this instruction's uses.
		for _, u := range uses {
			if defIdx, ok := lastDef[int64(u)]; ok {
				dag.Edges = append(dag.Edges, DepEdge{
					From: defIdx,
					To:   i,
					Lat:  instrLatency(in),
				})
				dag.InDegree[i]++
			}
		}

		// Update last def for each def.
		for _, d := range defs {
			lastDef[int64(d)] = i
		}
	}

	// Compute ready set: nodes with in-degree 0.
	for i := range code {
		if dag.InDegree[i] == 0 {
			dag.Ready = append(dag.Ready, i)
		}
	}

	return dag
}

// listSchedule performs list scheduling with a resource-constrained approach.
func listSchedule(code []ir.Instr, dag *DAG) []ScheduledInstr {
	out := make([]ScheduledInstr, len(code))
	cycle := 0
	scheduled := make([]bool, len(code))
	ready := make([]int, len(dag.Ready))
	copy(ready, dag.Ready)

	// Track when each instruction finishes.
	finish := make([]int, len(code))
	for i := range finish {
		if dag.InDegree[i] == 0 {
			finish[i] = 0 // ready immediately
		} else {
			finish[i] = -1
		}
	}

	issued := 0
	for issued < len(code) {
		// Find an instruction we can issue this cycle.
		// Prefer instructions with the most successors (critical path heuristic).
		best := -1
		bestScore := -1
		for idx, node := range ready {
			if finish[node] >= 0 && finish[node] <= cycle {
				// Already finished, can issue.
				score := successorCount(dag, node)
				if score > bestScore {
					bestScore = score
					best = idx
				}
			}
		}

		if best >= 0 {
			node := ready[best]
			out[node] = ScheduledInstr{Instr: code[node], Cycle: cycle}
			scheduled[node] = true
			issued++

			// Remove from ready, add successors whose dependencies are satisfied.
			ready = append(ready[:best], ready[best+1:]...)

			// Update successors.
			for _, e := range dag.Edges {
				if e.From == node && !scheduled[e.To] {
					dag.InDegree[e.To]--
					if dag.InDegree[e.To] == 0 {
						ready = append(ready, e.To)
					}
					// Set finish time for this successor.
					if finish[e.To] < cycle+e.Lat {
						finish[e.To] = cycle + e.Lat
					}
				}
			}
		} else {
			cycle++ // advance time
		}
	}

	// Fill in any unscheduled instructions (shouldn't happen, but safety).
	for i, in := range code {
		if !scheduled[i] {
			out[i] = ScheduledInstr{Instr: in, Cycle: cycle}
			cycle++
		}
	}

	return out
}

// successorCount returns the number of direct successors of a node in the DAG.
func successorCount(dag *DAG, node int) int {
	count := 0
	for _, e := range dag.Edges {
		if e.From == node {
			count++
		}
	}
	return count
}

// instrLatency returns an estimated latency for an instruction.
// This is a simplified model; a real JIT would use actual CPU latencies.
func instrLatency(in ir.Instr) int {
	switch in.Op {
	case ir.OpArith:
		// Division and modulo are slower.
		if in.A == ir.ArithDiv || in.A == ir.ArithMod {
			return 4
		}
		return 1
	case ir.OpCall:
		return 10 // calls are expensive
	case ir.OpLoad, ir.OpStore:
		return 2 // memory operations
	case ir.OpIndex, ir.OpField, ir.OpTableSet:
		return 3
	default:
		return 1
	}
}
