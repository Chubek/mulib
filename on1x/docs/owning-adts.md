# Nominal ADTs with immutable shared owners

Nominal records and enums may contain `sexpr`, an existing immutable,
reference-counted value, directly, in a tuple payload, or through another
nonrecursive nominal type. Parametric declarations can instantiate their
fields and payloads with `sexpr`:

```on1x
record Cell<T> { item:T }
enum Entry { Empty, Item(Cell<sexpr>) }

fn take(e: Entry) -> sexpr {
    return match e with {
        Empty() => sx_nil(),
        Item(record{item:x}) => x
    };
}

fn main() {
    let v = variant<Entry>(Item, record<Cell<sexpr>>{item:sx_string("example")});
    println(sx_text(take(v)));
}
```

The aggregate itself is a private C11 value; copying it retains each managed
field. A sum retains/releases only the active constructor payload. Field and
pattern projections acquire their own reference. Arguments borrow for the
call; results, locals, captures, and evaluated deferred arguments retain
references as in the existing tuple/closure ABI. Whole-value reassignment
retains the new value before releasing the old one. Lexical exits and early
returns release managed fields. Dropping an immutable S-expression chain
continues to use the existing checked, bounded, iterative RC runtime.
Allocation traps remain terminal and non-unwinding.

This shared-owner profile itself does **not** add recursive nominal layouts,
boxes, or a new resource handle ABI. A `sexpr` may itself contain a recursive
list of nodes, but declarations using only this profile remain acyclic. The
separate [finite unique-owner profile](recursive-owning-adts.md) admits
`own<Named>`, explicit transfer, and recursive nominal layouts through direct
checked HIR to C11. It does not change `sexpr` retain/release behavior.

`buffer`, JSON owners/views, closures, and foreign scalar handles with ownership
semantics remain outside this shared profile. Structural
records/sums/arrays/slices keep their documented field policy; no foreign
aggregate ABI is inferred. The scalar MIR and independent SDK adapters admit
neither this aggregate RC lifecycle nor the later unique-owner lifecycle.

Run `python3 on1x/tests/test_owning_adt.py PATH/TO/on1xc on1x /tmp/opencode/owning-adt-tests gcc`
from the MuLIB root. The test exercises source-module imports, managed copies
and lookups, defer/capture/Result and loop exits, 50,000-node iterative
release, injected allocation failure, and closed rejection. A sanitized
generated-C run can use `clang --cflag=-fsanitize=address,undefined`.
