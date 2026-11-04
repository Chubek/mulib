-- Git execution DSL. Loaded by mulib_git_bind after installing exec.
-- All documentation below is runtime string data, not discarded comments.
local git = {docs = {}, commands = {}, catalog = {}}
git.executable = "git"

git.docs.raw = [[
raw(arguments, options) constructs an inert execution plan for the Git executable.
Arguments is a dense array of literal strings, excluding argv[0]. It may contain
global options, a subcommand, options for that subcommand, revisions, pathspecs,
refspecs, repository locations, or extension-specific arguments. No arguments are
interpreted by MuLIB: spaces, quotes, wildcard characters, dollar signs, semicolons,
newlines and empty strings retain their literal byte values. Embedded NUL is
rejected at execution because the operating system cannot represent it in argv.

Options is a table. executable overrides git.executable (initially "git"). cwd
sets the child directory without changing the parent. global is a string array
inserted before arguments; use it for -C, -c, --git-dir, --work-tree, --bare,
--no-pager, --literal-pathspecs, --no-optional-locks or other Git global options.
env replaces the child environment with an array of NAME=value strings; omission
inherits it. stdin, stdout and stderr redirect to file paths relative to cwd;
append_stdout and append_stderr select append mode. merge_stderr duplicates
stdout after redirection. backend selects auto, fork or spawn. Auto uses fork
when cwd is set, otherwise posix_spawn; explicit spawn with cwd is unsupported.
doc overrides the plan's string docstring, including multiline long-bracket text.

The returned table has argv and doc fields and is compatible with exec.command,
exec.pipeline, exec.run, exec.capture, exec.check and exec.sequence. Construction
does not start Git, inspect a repository, validate Git's version, or open files.
All execution options, such as input, timeout_ms, max_output and pipefail, belong
to exec.run/capture rather than this constructor. Arguments/options are copied
shallowly; nested environment arrays remain shared, so finish edits before run.
Git configuration, hooks, credential helpers, filters, pagers and transports keep
their ordinary Git behavior. Set their configuration/environment explicitly when
deterministic noninteractive operation is required.
]]

git.docs.command = [[
command(name, arguments, options) inserts one subcommand after global options and
before arguments, then delegates to raw. Name must be a nonempty string that does
not start with '-'. The catalogue is descriptive, never an execution allowlist:
new Git commands, user aliases and installed git-* extensions work unchanged.
Arguments defaults to an empty array. Names containing hyphens are supported;
named constructors replace hyphens with underscores (git.cherry_pick), while
git.commands["cherry-pick"] always preserves the exact spelling. git.commands
also provides commands whose names conflict with DSL helpers, such as "help".
Unknown commands have the generic command contract as their docstring. Git,
rather than the DSL, diagnoses unavailable commands or invalid options at run.
]]

git.docs.run = [[
run(name, arguments, command_options, execution_options) constructs one Git plan
and synchronously calls exec.run. The result contains status, code, ok, stdout,
stderr, system_error, failed_stage and per-stage exit/signal information. A
nonzero Git exit is a normal result, not a Lua exception. status reports execution
infrastructure failure independently of Git's exit code. Use exec.check on a
constructed plan when nonzero exit must raise an exception. stdout/stderr remain
inherited unless capture is requested; redirects and captures cannot conflict.
For arbitrary global-only invocations such as --version, use exec.run(git.raw()).
For pipelines, construct stages with git.command and pass them to exec.pipeline.
]]

git.docs.capture = [[
capture(name, arguments, command_options, execution_options) is run with both
stdout and stderr captured. Each stream defaults to a 16 MiB limit, configurable
through max_output. Captured strings preserve every byte, including NUL. Do not
split on newlines when using Git's -z records or binary object/pack/archive data.
Inspect status and code before consuming output: partial output is retained on
capture-limit, timeout and I/O failure. timeout_ms uses a monotonic deadline and
reaps direct children; hooks/helpers may create descendants whose lifecycle is
application-managed. exec.capture copies execution_options before enabling
capture. Large streaming transfers can instead use file redirects or pipelines.
]]

git.docs.help = [[
help(name) returns the complete catalogue docstring without launching a process.
The text describes purpose, invocation, effects, output interpretation and the
shared execution contract. Unknown names receive the generic command contract.
To ask the installed Git version for authoritative option documentation, construct
git.commands.help({name}) or git.command(name,{"-h"}) and execute explicitly.
The catalogue describes command families; installed Git versions and build
options determine availability. git.docs[name] exposes registered strings, and
git.catalog[name] exposes purpose, synopsis, effects and details separately.
]]

local contract = [[

EXECUTION AND ARGUMENT CONTRACT
Call git.command(NAME, arguments, command_options), or the named constructor,
to build a plan. Supply each flag and each value as separate string elements
unless Git's documented syntax requires --option=value. Never add shell quotes
around paths: {"--", "a file.txt"} passes one literal path. The '--' separator is
command-specific; use it where Git documents separation of revisions/options
from pathspecs. Git itself interprets revision expressions, refspecs, pathspec
magic and configured aliases. For literal pathspec behavior use Git's global
--literal-pathspecs option. Unrecognised options are passed through unchanged,
which permits options added in newer Git releases and extension-specific modes.

REPOSITORY, ENVIRONMENT AND SIDE EFFECTS
Repository discovery starts in command_options.cwd, or in the inherited cwd.
Global options (command_options.global) precede the subcommand, so -C, -c,
--git-dir and --work-tree have normal Git meaning. An omitted env inherits the
parent; an explicit env array replaces it entirely, including PATH, HOME, locale,
Git identity and transport settings. The DSL neither invents an author identity
nor overrides repository configuration. Hooks, editors, pagers, credential
helpers, filters, signing programs and transports run according to Git's own
configuration. Network commands use Git's installed transport implementations.
Commands may fail because the repository is absent, locked, conflicted, shallow,
bare, permission-limited, or because a required object/ref/remote is unavailable.
Inspect stderr and the exit status; the DSL does not translate Git diagnostics.

RESULTS, COMPOSITION AND FAILURE
Execute with exec.run(plan, options) or exec.capture(plan, options). Construction
alone has no external side effects. status==0 means the execution machinery
completed, not that Git succeeded. code is Git's exit code, or 128+signal; ok is
true only when status and code are both zero. Some commands intentionally use
nonzero codes for predicates, differences, conflicts or absent matches; consult
the command details before treating every nonzero as exceptional. stdout and
stderr captures are binary strings, separately bounded by max_output. For
machine parsing prefer documented porcelain, --format, --raw or -z modes where
available, rather than localized human-readable summaries. input supplies exact
stdin bytes from a temporary regular file. File redirects and pipeline stages
allow larger data without capture. No tty is allocated. Interactive commands
can inherit the caller's terminal; callers manage terminal/job-control policy.

Pipelines launch all stages before waiting. pipefail=true reports the rightmost
nonzero stage; otherwise the last stage determines code. Use result.stages to
inspect every process. exec.sequence stops on failure unless keep_going is true;
ordinary Lua conditionals and loops provide retry and workflow logic. External
effects are not rolled back on Lua errors, Git failures, capture limits or
timeouts. timeout_ms terminates and reaps direct children, not an entire process
tree. For exhaustive version-specific flags, run git help NAME using the installed
Git manual. The DSL intentionally preserves Git CLI semantics and does not
promise repository transactions across separate commands.
]]

local function copy(t)
  local r = {}
  for k,v in pairs(t or {}) do r[k]=v end
  return r
end
local function append(to, from)
  assert(type(from)=="table", "arguments must be a dense string array")
  local n = #from
  for k,v in pairs(from) do
    assert(type(k)=="number" and k>=1 and k<=n and k%1==0 and type(v)=="string",
           "arguments must be a dense string array")
  end
  for i=1,n do assert(type(from[i])=="string", "missing argument"); to[#to+1]=from[i] end
end
function git.raw(arguments, options)
  local c = copy(options)
  local a = {c.executable or git.executable}
  append(a, c.global or {})
  append(a, arguments or {})
  c.argv=a; c.doc=c.doc or git.docs.raw
  c.executable=nil; c.global=nil
  return exec.command(c)
end
function git.command(name, arguments, options)
  assert(type(name)=="string" and #name>0 and string.sub(name,1,1)~="-", "invalid Git command")
  local a = {name}
  append(a, arguments or {})
  local o=copy(options)
  o.doc=o.doc or git.docs[name] or git.docs.command
  return git.raw(a,o)
end
function git.run(name,a,c,o) return exec.run(git.command(name,a,c),o) end
function git.capture(name,a,c,o) return exec.capture(git.command(name,a,c),o) end
function git.help(name) return git.docs[name] or git.docs.command end

-- Each row is original command-specific documentation: name, purpose, synopsis,
-- effects and interpretation. Shared mechanics above become part of EVERY
-- registered command's full string docstring and every constructed plan.
local entries = {
  {"add", "Stage working-tree content in the index.", "add [options] [--] <pathspec>...",
   "Updates the index, not HEAD. -A includes additions, modifications and removals; -u updates tracked paths only. -p opens interactive hunk selection. Use --intent-to-add for intent entries. Ignored paths need -f; missing paths and index locks can fail. Inspect staged content with diff --cached before committing."},
  {"am", "Apply a mailbox series as commits.", "am [options] [<mbox>...] | --continue | --skip | --abort",
   "Consumes mail from files or stdin and updates index, worktree and branch history. Author metadata comes from patches; committer identity comes from configuration/environment. Conflicts leave resumable state: resolve, stage, then --continue, or --abort. --3way needs base objects. Hooks may run and stop application."},
  {"annotate", "Display line attribution in annotation format.", "annotate [options] [<rev>] [--] <file>",
   "Reads history and prints the last responsible commit/author for lines. This is a blame-format interface; -L limits ranges and revision options affect traversal. It does not modify history. Prefer blame --line-porcelain for structured processing rather than parsing presentation-oriented columns."},
  {"apply", "Apply or inspect a patch without creating commits.", "apply [--check | --stat | --numstat] [--index | --cached] [<patch>...]",
   "By default modifies working files; --cached modifies only the index, and --index requires index/worktree agreement. --check validates without applying. --3way can leave conflicts. Patch bytes may arrive on stdin; binary patches remain binary. Exit failure can indicate context mismatch, path restrictions or whitespace policy."},
  {"archive", "Produce a tree archive.", "archive --format=<format> [--prefix=<prefix>/] [--output=<file>] <tree-ish> [<path>...]",
   "Writes tar/zip or another configured format to stdout unless --output is provided. Output is binary, not a line protocol. Attributes can exclude or substitute content. --remote requests server-side archiving subject to server policy. Redirect to a file or pipeline for archives larger than the capture limit."},
  {"backfill", "Download missing objects for a partial clone.", "backfill [options]",
   "Populates missing promised objects according to installed Git's backfill policy, usually in path-oriented batches. Requires a configured promisor remote and transport access. Modifies local object storage and may transfer substantial data. Availability and batching/filter options depend on Git version; failures do not undo previously fetched objects."},
  {"bisect", "Locate a regression by binary search through commits.", "bisect <start|good|bad|skip|next|reset|run|log|replay|visualize> [arguments]",
   "Maintains bisection state and may check out candidate revisions. Mark known good/bad revisions, test each candidate, then reset to restore the starting checkout. bisect run invokes a test program: exit 0 is good, ordinary nonzero is bad, and 125 skips. Save unrelated work before checkout transitions."},
  {"blame", "Attribute file lines to commits.", "blame [--porcelain | --line-porcelain | --incremental] [-L <range>] [<rev>] [--] <file>",
   "Reads history and reports ownership of each line, optionally following moved/copied lines with -M/-C. Attribution reflects the selected revision and ignore-revision policy, not necessarily original authorship. Porcelain records carry object IDs, line numbers and metadata. Output can be large; range limits or streaming avoid capture exhaustion."},
  {"branch", "List, create, rename or delete branches.", "branch [--list] [<pattern>...] | [-f] <name> [<start>] | (-d|-D) <name>",
   "Updates branch refs/configuration but normally does not switch the worktree. -d checks merge safety; -D forces deletion. -m renames, -c copies, and --set-upstream-to changes tracking. --format supports machine-oriented output. Branches checked out in another worktree impose restrictions; deleting a ref does not immediately remove objects."},
  {"bugreport", "Write a Git diagnostic report template.", "bugreport [--output-directory <dir>] [--suffix <format>]",
   "Collects version/platform/configuration context into a local report file and may invoke an editor. The file is not automatically submitted. Output paths and supported diagnostic additions are version-specific. Run with inherited terminal/editor configuration for interactive editing, or configure an appropriate noninteractive editor."},
  {"bundle", "Transport repository objects and refs in a bundle file.", "bundle <create|verify|list-heads|unbundle> <file> [<revisions>...]",
   "create writes a self-contained or prerequisite-dependent pack bundle; verify checks prerequisites against the repository. list-heads prints advertised refs, and unbundle imports objects and prints refs without necessarily creating local branches. Bundles support offline transfer, but incremental bundles require prerequisite history. Use file paths for binary bundle content."},
  {"cat-file", "Inspect object type, size or content.", "cat-file (-t|-s|-p) <object> | --batch[=<format>] | --batch-check[=<format>]",
   "Reads the object database. -p pretty-prints according to type; blob data may contain arbitrary bytes. Batch protocols accept object queries on stdin and emit headers plus length-framed content; read exact sizes instead of splitting all output into lines. Missing objects may be represented per record in batch mode rather than by a process-wide failure."},
  {"check-attr", "Resolve Git attributes for paths.", "check-attr [-z] [--cached] (--all | <attr>...) [--] <path>...",
   "Reports set, unset, unspecified or string-valued attributes. --stdin reads paths and -z provides NUL framing. --cached reads attributes from the index rather than the worktree. This is a read operation, but results depend on attribute source precedence and the chosen repository; attributes are not equivalent to ignore rules."},
  {"check-ignore", "Explain ignore matches.", "check-ignore [-v] [-z] [--stdin] [--no-index] [<path>...]",
   "Tests paths against exclude rules, with -v reporting the source file, line and pattern. Normally tracked files are not reported; --no-index changes that behavior. Exit 0 means a match, 1 means no match, and fatal errors use another nonzero code. Use -z when filenames may contain whitespace or newlines."},
  {"check-mailmap", "Canonicalize author identities through mailmap.", "check-mailmap [--stdin] [<contact>...]",
   "Reads mailmap rules and emits canonical name/email identities. Each contact is a single argument containing the documented identity syntax, or a line on stdin. It changes neither commits nor configuration. The chosen repository and mailmap configuration determine mapping sources; malformed contact syntax can fail."},
  {"check-ref-format", "Validate reference naming syntax.", "check-ref-format [--normalize] [--allow-onelevel] <refname> | --branch <name>",
   "Returns zero for a valid reference name and nonzero for invalid syntax. --normalize emits a normalized form; --branch applies branch-specific constraints and may expand prior-checkout syntax in a repository. Validation does not establish that a ref exists, is available for creation, or is not locked."},
  {"checkout", "Switch revisions or restore selected paths.", "checkout [options] <branch-or-commit> | checkout [<tree-ish>] -- <pathspec>...",
   "Branch mode changes HEAD, index and worktree; path mode replaces selected paths without moving HEAD. Ambiguous branch/path syntax is resolved by Git, so use -- for path restoration. Detached HEAD results from checking out a commit. Conflict, overwrite and linked-worktree checks may reject switching. switch/restore separate these two roles."},
  {"checkout-index", "Copy index entries into working files.", "checkout-index [-a] [-f] [--prefix=<prefix>] [--stdin] [-z] [--] <file>...",
   "Low-level export of staged entries; it does not move HEAD or create commits. -a selects every indexed path, -f permits overwrites, and --prefix writes beneath another prefix. Stage selection matters for unmerged entries. Use --stdin -z for arbitrary filenames; parent-directory creation and overwrite policy follow Git's plumbing semantics."},
  {"cherry", "Compare patch equivalence between branches.", "cherry [-v] [<upstream> [<head> [<limit>]]]",
   "Prints commits from head marked '-' when an equivalent patch exists upstream and '+' otherwise. Equivalence is patch-based, not object-ID identity, and ignores some representation differences. It reads history only. Do not confuse this report with cherry-pick, which applies commits and can modify branch history."},
  {"cherry-pick", "Apply selected commits onto the current branch.", "cherry-pick [options] <commit>... | --continue | --skip | --abort | --quit",
   "Normally creates new commits while updating index/worktree; --no-commit leaves changes staged. Merge commits require a mainline selection with -m. Conflicts leave sequencer state and unmerged entries for manual resolution. --abort restores the pre-sequence state; --quit forgets sequencing state without undoing all resulting changes."},
  {"clean", "Remove untracked working-tree files.", "clean [-n | -f] [-d] [-x | -X] [--] <pathspec>...",
   "-n previews removals; -f enables removal according to clean.requireForce. -d includes directories, -x includes ignored files, and -X selects only ignored files. Nested repositories have additional force requirements. Removed untracked content is not recoverable from Git history unless separately recorded. Does not remove tracked paths."},
  {"clone", "Create a local repository from another repository.", "clone [--bare | --mirror] [--depth <n>] [--filter=<spec>] <repository> [<directory>]",
   "Creates directories, imports objects/refs, configures origin and normally checks out a worktree. Bare and mirror modes change layout/ref mappings; shallow/partial clone options change available history and objects. URLs are one literal argument. Transport, authentication, submodule recursion and checkout filters use ordinary Git configuration."},
  {"column", "Format stdin as columns.", "column [--mode=<mode>] [--width=<width>] [--indent=<text>]",
   "Presentation helper that reads lines and writes a column layout. It does not modify repository data. Width and mode control layout; terminal/configuration defaults may influence output. Avoid column formatting when downstream tools need unambiguous machine records, especially when values contain control characters or newlines."},
  {"commit", "Record the staged tree as a commit.", "commit [-m <message> | -F <file>] [--amend] [--allow-empty] [options]",
   "Writes tree/commit objects and updates the current branch or detached HEAD. The index is the default source; -a stages tracked modifications/removals first. --amend replaces the branch tip with a new commit identity. Author/committer identity, signing and hooks may be required. Without a message option, an editor can be launched."},
  {"commit-graph", "Write or verify commit-graph acceleration data.", "commit-graph <write|verify> [options]",
   "Updates auxiliary reachability/generation data in the object directory, or checks its consistency. write may consume selected commits/packs via stdin and can maintain split graphs. It does not change commit identities or refs. Repository locking, concurrent maintenance, object availability and version-specific format options affect success."},
  {"commit-tree", "Create a commit object from a tree.", "commit-tree <tree> [-p <parent>]... [-m <message> | -F <file>] [-S[<key>]]",
   "Writes a commit object and prints its object ID; it does not update a branch or HEAD. Repeated -p arguments define parent order, including merge commits. Message can come from stdin. Author/committer environment determines identity and timestamps. Use update-ref separately if a reference should point at the new object."},
  {"config", "Read and modify Git configuration.", "config [<scope-options>] <get|set|unset|list|edit|...> [arguments]",
   "Supports modern subcommands and legacy --get/--add/--unset modes according to installed Git. Scope may be local, worktree, global, system or an explicit file. Reads and predicate-like queries have meaningful nonzero statuses for missing/multiple values. Writes modify selected config files. Use --null/-z and --show-origin/--show-scope where suitable."},
  {"count-objects", "Report object-store size and loose-object counts.", "count-objects [-v] [-H]",
   "Reads object-store metadata and prints counts/sizes. -v adds pack, garbage and alternate information; -H uses human-readable units, which are less suitable for parsing. This reports storage rather than reachable-history size. It does not repack, prune or remove objects, and concurrent maintenance can change observed values."},
  {"credential", "Query or update the credential-helper chain.", "credential <fill|approve|reject>",
   "Reads a blank-line-terminated attribute protocol from stdin. fill consults helpers and may prompt, returning attributes such as username/password; approve stores successful credentials and reject asks helpers to erase them. Protocol extensions vary by Git version. Treat returned bytes as credential data; the DSL neither reformats nor automatically logs them."},
  {"credential-cache", "Temporarily cache credentials in a daemon.", "credential-cache [--timeout=<seconds>] [--socket=<path>] <action>",
   "Helper protocol commands read credential attributes from stdin. Cache entries are held by a local daemon until expiration or shutdown, rather than persisted as ordinary credential-store records. Socket ownership/path and daemon lifecycle matter. The helper is usually invoked by Git's credential subsystem, but literal protocol calls are supported."},
  {"credential-store", "Store credentials in a plaintext credential file.", "credential-store [--file=<path>] <get|store|erase>",
   "Consumes the credential-helper attribute protocol and reads/modifies matching records in a local file. get can emit matching username/password fields; store and erase update persistent records. File selection follows helper defaults unless --file is supplied. This helper does not encrypt the credential file and does not initiate a repository transport itself."},
  {"describe", "Name an object relative to a reachable reference.", "describe [--tags | --all] [--always] [--dirty[=<mark>]] [<commit-ish>...]",
   "Finds a suitable tag/ref and may append distance and abbreviated object ID. Annotated tags are preferred by default; --tags includes lightweight tags. --always allows an object-ID fallback. Output is a descriptive label, not a stable unique identifier under changing refs/configuration. --dirty adds worktree state to the description."},
  {"diagnose", "Collect repository diagnostic material into an archive.", "diagnose [--output-directory <dir>] [--suffix <format>] [--mode=<mode>]",
   "Writes a diagnostic archive containing selected repository/environment information. It is a local collection operation, not automatic issue submission. Modes and collected files depend on Git version. The archive can include repository paths and configuration context; consumers decide what to share. Binary archive output should be handled as a file."},
  {"diff", "Compare worktree, index, commits or arbitrary paths.", "diff [options] [<commit> [<commit>]] [--] <pathspec>... | diff --no-index <path> <path>",
   "Without revisions compares worktree to index; --cached compares index to HEAD or a supplied commit. Two revisions compare trees; three-dot syntax compares from a merge base. --exit-code/-q return 1 when differences exist, which is not an execution error. --no-index also uses diff status semantics. Prefer --raw/-z/--numstat for structured consumers."},
  {"diff-files", "Compare index entries with working files.", "diff-files [options] [--] <path>...",
   "Plumbing diff that reads worktree/index state without comparing two commits. Raw records expose index/object/mode differences; patch options produce textual or binary patches. --exit-code can report differences via exit 1. Unmerged paths and stat refresh behavior follow Git semantics. Does not stage changes or write commits."},
  {"diff-index", "Compare a tree with the index or worktree.", "diff-index [--cached] [options] <tree-ish> [--] <path>...",
   "--cached compares the tree directly to index entries; without it working-tree state participates. Emits raw or patch records depending on options. This is frequently used for dirty-state checks, where nonzero may simply mean differences. Refresh/racy-index considerations can affect results; select options deliberately for scripts."},
  {"diff-tree", "Compare trees represented by commits or tree objects.", "diff-tree [options] <tree-ish> [<tree-ish>] | --stdin",
   "Reads object data and emits raw/patch differences. A single commit normally compares with its parent; --root includes root commits and merge options control parent handling. --stdin permits batches. Recursive traversal requires the appropriate -r behavior. It does not modify index/worktree or write a merge result."},
  {"difftool", "Display differences using an external tool.", "difftool [--tool=<tool>] [--no-prompt] [<diff-options>]",
   "Builds on diff selection and invokes configured external viewers, potentially GUI or interactive programs. Temporary files and tool-specific exit behavior may be involved. --no-prompt suppresses Git's per-file confirmation, not necessarily the viewer's UI. Use ordinary diff for deterministic machine-readable patch streams."},
  {"fast-export", "Export history as a fast-import stream.", "fast-export [options] <revision-range>...",
   "Writes a structured stream containing commits, refs and length-framed file data. It is not safely parsed by splitting every line, since data blocks can contain arbitrary bytes. Marks/import-export options support incremental workflows. Redirect or pipeline large exports. Object IDs/signatures may not survive transformations or import into different object formats unchanged."},
  {"fast-import", "Import a stream of commits, refs and file data.", "fast-import [options] < <stream>",
   "The '<' above denotes stdin, not a literal argument: supply execution_options.input, a stdin path, or a pipeline. Writes objects and updates refs according to the stream, including marks and checkpoints. Protocol commands contain byte-counted payloads. Errors can leave imported objects or earlier ref changes; the whole stream is not an application-level rollback transaction."},
  {"fetch", "Download objects and update remote-tracking refs.", "fetch [options] [<repository> [<refspec>...]]",
   "Uses configured transport/refspecs, writes objects, updates FETCH_HEAD and usually remote-tracking refs. It normally does not merge into the current branch. --prune removes stale destinations according to refspecs, and --tags/--depth/--filter alter selection. Authentication and helpers may prompt. Partial transfer or ref-update errors can leave fetched objects available locally."},
  {"fetch-pack", "Low-level object retrieval from a remote repository.", "fetch-pack [options] <repository> [<refs>...]",
   "Negotiates with upload-pack and retrieves objects, exposing plumbing-oriented results. It is not a substitute for fetch's complete remote-tracking/configuration workflow. Protocol capabilities, shallow state and transport options affect behavior. Use pipes/files for protocol data; avoid interpreting packet or pack bytes as ordinary diagnostic text."},
  {"fmt-merge-msg", "Compose a merge commit message.", "fmt-merge-msg [--log[=<n>]] [-m <message>] [-F <file>]",
   "Reads FETCH_HEAD-style merge input from stdin or a file and writes a suggested message. It may consult branch descriptions and history, but does not perform the merge or create a commit. Preserve message newlines exactly when feeding commit-tree/commit. Configuration controls shortlog and branch-description content."},
  {"for-each-ref", "Enumerate refs with programmable formatting.", "for-each-ref [--format=<format>] [--sort=<key>] [--count=<n>] [<pattern>...]",
   "Reads refs and object metadata. Format atoms expose names, object IDs, upstream state and more; quoting flags format for other languages but do not execute that output. Sort/count/filter options determine order and selection. Choose explicit separators and fields for machine consumers rather than assuming the default display layout is stable."},
  {"for-each-repo", "Run Git in repositories listed by configuration.", "for-each-repo --config=<key> [--] <git-arguments>...",
   "Looks up multivalued repository paths in a configuration key and invokes the requested Git operation for each. Each operation can have independent side effects and failures. This is Git's own multi-repository dispatcher, not exec.sequence. Config scope and command version determine enumeration and stop/continuation behavior."},
  {"format-patch", "Create a mailbox-formatted patch series.", "format-patch [options] <since> | <revision-range>",
   "By default writes numbered patch files; --stdout emits the series to stdout. Subject prefixes, cover letters, threading and reroll metadata shape email review workflows. It reads commits without modifying their IDs. Binary patch support and signatures depend on options. Resulting mail can be consumed by am, but merge commits require special treatment."},
  {"fsck", "Check object connectivity and integrity.", "fsck [--full] [--strict] [--no-reflogs] [<object>...]",
   "Traverses selected object roots and validates stored objects/references. Reports corruption, missing objects, dangling objects and other diagnostics with severity determined by configuration. --lost-found writes selected dangling content to files. A diagnostic about unreachable objects is different from corruption; interpret output and exit status using installed Git's fsck rules."},
  {"gc", "Perform repository housekeeping.", "gc [--auto] [--prune=<date>] [--aggressive]",
   "May repack objects, prune expired unreachable data, expire reflogs and update auxiliary indexes. --auto consults thresholds; --aggressive changes optimization effort. Concurrent writers and pruning windows affect behavior. It changes storage/layout, not reachable commit identities. Reflog expiration and pruning can remove recovery paths for previously unreachable commits."},
  {"get-tar-commit-id", "Extract an embedded commit ID from a Git tar archive.", "get-tar-commit-id < <archive.tar>",
   "Reads tar bytes from stdin and prints the recorded commit ID when an appropriate archive header exists. Supply bytes via input, stdin file redirection or a pipeline; '<' is not an argv token. Archives created from a tree rather than a commit may lack this metadata. Failure to find metadata is reflected in the exit status."},
  {"grep", "Search tracked content or tree objects.", "grep [options] [-e] <pattern> [<tree>...] [--] <pathspec>...",
   "Searches worktree files by default, with options for cached content, untracked files or specific trees. Regex mode and binary handling are Git options, not DSL expansion. Exit 0 means matches, 1 means none, and another nonzero indicates an error. -z and explicit output modes help preserve filenames; -q suppresses normal match output."},
  {"hash-object", "Compute an object identity and optionally store the object.", "hash-object [-t <type>] [-w] [--stdin | --stdin-paths] [--path=<path>] [<file>...]",
   "Without -w reports object IDs without storing objects; -w writes to the selected object database. Filters/attribute context can change blob content when path-based conversion is enabled. --stdin accepts arbitrary bytes. This uses the repository's supported object format, unlike mulib_git_sha1 which is explicitly SHA-1-only."},
  {"help", "Read installed Git documentation.", "help [-a | -g] [--man | --web | --info] [<command-or-guide>]",
   "May launch a pager, browser or info viewer according to configuration. -a lists available commands, including extensions; -g lists guides. For short command usage, pass -h to that subcommand. Access this constructor as git.commands.help because git.help(name) is the in-memory DSL docstring lookup."},
  {"hook", "Run a named repository hook.", "hook run [--ignore-missing] <hook-name> [-- <arguments>...]",
   "Invokes the hook selected by repository hook configuration and forwards its arguments. Hook behavior and side effects belong to that program; stdout/stderr and exit status propagate through Git. --ignore-missing permits absence according to Git's semantics. This does not simulate the surrounding porcelain operation that normally triggers the hook."},
  {"index-pack", "Validate a pack and build its index.", "index-pack [--stdin] [-o <index>] [options] [<pack-file>]",
   "Reads binary pack data, resolves deltas and writes index/pack metadata. --stdin can install received packs into the repository; thin packs require appropriate fixup support and base objects. Validation errors may arise from corrupt/truncated input or missing bases. Use binary stdin/file handling, and plan disk space for large packs."},
  {"init", "Initialize or reinitialize a repository.", "init [--bare] [--initial-branch=<name>] [--object-format=<format>] [<directory>]",
   "Creates administrative directories/configuration and an initial symbolic HEAD; it does not create an initial commit. Reinitializing an existing repository preserves existing objects and refs while applying supported setup behavior. --bare omits a worktree; templates and shared permissions affect layout. Object-format availability depends on the installed Git version."},
  {"interpret-trailers", "Parse or update structured message trailers.", "interpret-trailers [--parse] [--in-place] [--trailer <token=value>] [<file>...]",
   "Reads commit-message-like text, recognizes trailer blocks and emits transformed or parsed text. --in-place updates files; otherwise output goes to stdout. Trailer configuration controls token aliases, placement and duplicate handling. This does not create or modify a commit object unless a later command records the resulting message."},
  {"log", "Traverse and display commit history.", "log [<revision-options>] [<format-options>] [--] <pathspec>...",
   "Reads the selected history range. Parent traversal, simplification, path filtering and ordering options change which commits appear and in what order. --format and -z can produce machine-oriented records; patch/stat modes add potentially binary or large output. Default output may invoke a pager. Use explicit limits for bounded capture."},
  {"ls-files", "Inspect index and working-tree file classifications.", "ls-files [--stage | --unmerged | --others | --deleted | --modified] [-z] [--] <path>...",
   "Reports tracked index entries by default. --stage adds mode, object ID and stage; unmerged paths can have multiple entries. --others and exclusion options select untracked content. -z preserves arbitrary filenames. This is enumeration, not checkout or staging; sparse-index/worktree options affect visibility."},
  {"ls-remote", "List refs advertised by a remote.", "ls-remote [--heads] [--tags] [--symref] [--exit-code] [<repository> [<patterns>...]]",
   "Contacts the remote without fetching all advertised objects into the local store. Output usually pairs object IDs with refnames; --symref adds symbolic information and annotated tags may have peeled records. --exit-code distinguishes no matching refs from successful matches. Transport/authentication errors remain separate failures."},
  {"ls-tree", "List entries in a tree object.", "ls-tree [-r] [-t] [-l] [-z] [--name-only] <tree-ish> [<path>...]",
   "Reads tree metadata: mode, object type, object ID and path. -r descends recursively, -l reports blob size, and -z makes path framing unambiguous. It does not read unstaged working files. Path selection is relative to Git's repository/cwd semantics; use --full-tree/--full-name where appropriate for stable repository-root paths."},
  {"mailinfo", "Extract commit metadata and patch content from email.", "mailinfo [options] <message-file> <patch-file>",
   "Reads mail on stdin, writes decoded message and patch files, and emits extracted author/subject/date information. Encoding and scissors options affect extraction. It does not apply the patch or create commits. MIME/mail parsing can reject malformed input; preserve binary patch payloads and inspect both file outputs before downstream application."},
  {"mailsplit", "Split a mailbox into individual message files.", "mailsplit [-d<digits>] [-o<directory>] [--mboxrd] [<mbox>...]",
   "Consumes mailbox files or stdin and creates numbered output files, reporting the count. This is mail preparation for tools such as am, not repository mutation. Envelope quoting and bare-message options affect parsing. Existing output files, malformed mail and filesystem permissions can prevent splitting; output files are ordinary caller-managed artifacts."},
  {"maintenance", "Run or schedule repository maintenance tasks.", "maintenance <run|start|stop|register|unregister> [options]",
   "run performs selected housekeeping tasks; start/stop can alter background scheduling, and register/unregister changes the configured repository list. Tasks may repack/prune objects or update acceleration indexes. Scheduler support is platform-specific. Configuration and concurrency determine work performed; it is not a no-op merely because branch refs remain unchanged."},
  {"merge", "Integrate histories into the current branch.", "merge [options] <commit>... | --continue | --abort | --quit",
   "May fast-forward, create a merge commit or leave conflicts in index/worktree. --no-commit pauses commit creation where applicable; --ff-only rejects non-fast-forward integration. Strategies and conflict resolution options affect content. Hooks, signing and editors may run. Conflicts are ordinary nonzero Git results and require explicit resolution or abort."},
  {"merge-base", "Find common ancestors and test ancestry.", "merge-base [--all] <commit> <commit> | --is-ancestor <a> <b> | --fork-point <ref> [<commit>]",
   "Reads the commit graph without modifying refs. There can be multiple best common ancestors; --all exposes them. --is-ancestor is a predicate: 0 means true, 1 false, other nonzero error. --fork-point uses reflog information and can fail when relevant entries have expired. Output object IDs use the repository's hash format."},
  {"merge-file", "Perform a three-way merge of ordinary files.", "merge-file [-p] [--ours | --theirs | --union] <current> <base> <other>",
   "Normally replaces current with merged content; -p writes the result to stdout. Conflict markers and labels are configurable. A positive exit can indicate conflict count rather than infrastructure failure. This works on file contents and does not itself stage results or update repository refs; caller handles resulting files and conflict policy."},
  {"merge-index", "Invoke a merge program for unmerged index entries.", "merge-index [-o] [-q] <merge-program> (-a | [--] <file>...)",
   "Reads index stages and calls a low-level program with object IDs/modes and path arguments. The program controls working-file resolution and may update index entries. -a selects all unmerged paths. This is plumbing for merge implementations, not the usual branch merge interface. Program failures propagate according to merge-index's processing options."},
  {"merge-tree", "Compute a tree merge without normal checkout.", "merge-tree [--write-tree] [options] <branch1> <branch2> | <base-tree> <branch1> <branch2>",
   "Modern --write-tree mode computes a merged tree and can write objects while reporting conflicts; it does not update the current branch/index/worktree like merge. Historical three-tree mode has different textual output. Select the installed version's documented mode explicitly. Conflict status is meaningful and emitted records may include multiple conflict paths/messages."},
  {"mktag", "Create a validated tag object from stdin.", "mktag < <tag-object-text>",
   "Reads canonical tag object content, validates its headers/references and writes the object, printing its ID. It does not create refs/tags/name automatically. Provide exact bytes using input or a stdin file; the displayed '<' denotes redirection, not an argument. Use update-ref or tag separately to make the object reachable by a tag ref."},
  {"mktree", "Build a tree object from entry records.", "mktree [-z] [--missing] [--batch]",
   "Reads ls-tree-like records on stdin, writes tree objects and prints their IDs. Modes, types, object IDs and names must satisfy Git's tree-entry rules. -z permits arbitrary path component bytes other than NUL; --batch handles multiple trees. It does not populate the index or worktree and does not create a commit."},
  {"multi-pack-index", "Maintain an index spanning multiple packs.", "multi-pack-index [--object-dir=<dir>] <write|verify|expire|repack> [options]",
   "Writes/verifies auxiliary object-location metadata or consolidates/expires packs under its management. Ref identities remain unchanged, but object storage can be rewritten or removed. Bitmap and incremental-chain options vary by Git version. Concurrent maintenance and alternate object directories require deliberate coordination by the application."},
  {"mv", "Move or rename tracked paths.", "mv [-f] [-k] [-n] <source>... <destination>",
   "Changes working paths and records corresponding index changes. -n previews, -f permits supported destination overwrites, and -k skips errors where documented. Submodule moves may update gitfile/configuration-related paths. Git later detects renames from content; there is no permanent rename object. Commit the staged changes to record them in history."},
  {"name-rev", "Describe revisions using symbolic ref-relative names.", "name-rev [--name-only] [--tags] [--all | --annotate-stdin | <commit-ish>...]",
   "Reads refs/history and finds names such as branch~N for objects. It does not rename objects or refs. --annotate-stdin substitutes/annotates recognized object IDs in input text according to mode. Names depend on current refs and selection rules, so store full object IDs when a durable identity is required."},
  {"notes", "Manage notes attached to objects.", "notes [--ref=<notes-ref>] <list|show|add|append|edit|copy|remove|merge|prune|get-ref> [arguments]",
   "Stores annotations in a separate notes ref rather than rewriting the annotated object. Notes commands can write commits/refs and invoke editors; merge may leave resumable notes-merge state. --ref selects a notes namespace. Notes are not automatically transferred by every fetch/push configuration; configure refspecs when sharing them."},
  {"pack-objects", "Create pack files from selected objects.", "pack-objects [--stdout] [--revs] [options] [<base-name>]",
   "Reads object IDs or revision input from stdin and emits binary pack data or named pack/index files. Delta compression, thin packs, reuse and bitmap options alter storage/transport characteristics. Pack bytes are not text. It does not by itself update branch refs. Large pack generation should use files/pipelines rather than small capture limits."},
  {"pack-redundant", "Identify redundant pack files.", "pack-redundant [--all] [--verbose] [<pack>...]",
   "Reports packs whose objects are covered elsewhere according to the command's selection model. This legacy plumbing is discouraged or disabled in some versions; availability must be checked against installed Git. Reporting does not automatically delete files. Prefer supported repack/multi-pack-index maintenance workflows when managing object storage."},
  {"pack-refs", "Consolidate reference storage.", "pack-refs [--all] [--no-prune] [options]",
   "Packs eligible loose refs or performs the corresponding operation for the configured ref backend. Ref names and targets should remain semantically unchanged. --all broadens selection; --no-prune retains loose forms where supported. Locking/concurrent ref updates and backend/version-specific include/exclude options affect behavior."},
  {"patch-id", "Compute patch-equivalence identifiers.", "patch-id [--stable | --unstable | --verbatim]",
   "Reads diff text on stdin and emits patch IDs, often paired with commit IDs when the input contains them. Stable/verbatim modes change normalization. A patch ID is an equivalence heuristic, not a Git object ID or cryptographic proof of identical histories. No repository refs or files are modified."},
  {"prune", "Remove eligible unreachable loose objects.", "prune [-n] [-v] [--expire=<time>] [<head>...]",
   "Traverses reachability and removes old unreachable loose objects according to roots and expiry policy. -n reports without removal. Additional heads protect their reachable objects. Concurrent object writers and reflog roots matter; pruning is irreversible if no other copy remains. gc is the higher-level maintenance entry point for most workflows."},
  {"prune-packed", "Remove loose copies of packed objects.", "prune-packed [-n] [-q]",
   "Deletes loose object files already represented in packs, reducing duplicate storage while preserving object availability through packs. -n previews paths instead of deleting them; -q reduces progress output. This differs from prune's unreachable-object policy. It does not repack or change branch tips, and filesystem errors may leave some duplicates."},
  {"pull", "Fetch and integrate changes into the current branch.", "pull [options] [<repository> [<refspec>...]]",
   "Combines fetch with merge or rebase according to options/configuration. It can update remote-tracking refs, index/worktree and local history, and can stop with conflicts. --ff-only restricts integration; --rebase rewrites local commits. Failed integration does not undo the preceding successful fetch. Configure reconciliation policy explicitly in automation."},
  {"push", "Update remote refs and transfer required objects.", "push [--dry-run] [--atomic] [--force-with-lease[=<ref>:<expect>]] [<repository> [<refspec>...]]",
   "Uses refspecs to select remote updates, including deletion and forced history changes. --force-with-lease checks expected remote state; --atomic requires server support for all-or-nothing ref updates. Hooks/policy may reject some or all updates. --porcelain gives structured per-ref reporting. Authentication, network failures and server-side helpers keep native Git behavior."},
  {"range-diff", "Compare two versions of a commit series.", "range-diff [options] <range1> <range2> | <base> <rev1> <rev2>",
   "Matches commits by patch similarity and displays differences between corresponding patches, useful after a rebase or reroll. This is a review presentation, not a patch suitable for direct apply. Matching heuristics and creation-factor options affect pairing. It reads history without modifying refs; color/pager configuration can affect output."},
  {"read-tree", "Load tree entries into the index.", "read-tree [--empty] [-m [-u]] [--prefix=<prefix>/] <tree-ish>...",
   "Replaces/merges index contents; -u also updates working files when allowed. Multiple trees enable low-level merge modes and may produce unmerged index stages. --empty clears the index. This does not create commits or move HEAD. GIT_INDEX_FILE can select a temporary index for plumbing workflows without touching the ordinary index."},
  {"rebase", "Replay commits onto another base.", "rebase [--onto <newbase>] [<upstream> [<branch>]] | --continue | --skip | --abort | --quit",
   "Rewrites commit identities and updates the selected branch, index and worktree. Interactive mode invokes an editor and may execute todo commands. Conflicts stop in resumable state. --abort attempts to restore the original branch state; --quit leaves current changes while discarding state. Rewritten published history needs deliberate downstream coordination."},
  {"receive-pack", "Serve the receiving side of a push.", "receive-pack <repository>",
   "Consumes/emits Git transport protocol on standard streams, receives objects and conditionally updates refs under repository policy. Hooks, quarantine storage and capability negotiation are part of server-side behavior. Protocol streams are binary/framed, not human text. Usually started by a transport/server; the DSL can launch it with explicit descriptors and environment."},
  {"reflog", "Inspect or maintain reference update logs.", "reflog [show] [<ref>] | reflog <expire|delete|exists|drop|write|...> [arguments]",
   "show reads historical ref positions; maintenance subcommands can expire or delete recovery records. Reflogs are local metadata rather than shared commit history. Expiration can make objects eligible for later pruning. Subcommand availability varies by Git version. Use rev-parse with reflog syntax to resolve a selected entry to an object ID."},
  {"refs", "Inspect, verify or migrate reference storage.", "refs <migrate|verify|list|...> [options]",
   "Low-level ref database operations depend on the installed Git version and configured backend. Migration can change storage format while preserving logical refs; verification checks backend consistency. This may require exclusive repository access and can affect compatibility with older Git tools. Pass documented options verbatim and inspect status/diagnostics."},
  {"remote", "Manage configured remotes and tracking mappings.", "remote [-v] | remote <add|rename|remove|set-url|get-url|show|prune|update|set-head|set-branches> [arguments]",
   "Most subcommands edit local configuration/remote-tracking refs; show/update/prune can contact the network. A remote name is not necessarily a URL. set-url can manage separate fetch/push URLs, and rename adjusts associated configuration. Removing a remote does not delete the remote repository or automatically remove every object fetched from it."},
  {"remote-ext", "Bridge Git transport to an external command.", "remote-ext <remote-name> <url>",
   "Implements a remote-helper transport whose URL syntax can describe an external command. The DSL passes the URL literally, but Git's remote-ext helper interprets its own command/placeholder language. Protocol permissions and protocol.ext.allow may restrict use. This is normally invoked by Git transport machinery rather than directly by an application."},
  {"remote-fd", "Connect a Git remote helper to existing descriptors.", "remote-fd <remote-name> <url>",
   "Uses helper-specific URL syntax to communicate over inherited file descriptors. Descriptor lifetime, direction and protocol setup belong to the embedding application. Unlike ordinary capture pipes, these descriptors may need to survive exec, so configure inheritance intentionally. No network connection is created merely by supplying a descriptor number."},
  {"repack", "Rewrite objects into pack files.", "repack [-a] [-d] [--cruft] [options]",
   "Consolidates/compresses objects and can remove redundant packs with -d. Cruft packs retain unreachable objects with expiry metadata on supporting versions. Bitmap, geometric and incremental options affect layout/performance. It can use significant memory and disk space. Ref identities remain unchanged, but storage cleanup may interact with pruning and concurrent maintenance."},
  {"replace", "Manage replacement-object references.", "replace [-f] <object> <replacement> | replace <list|delete|...> [arguments]",
   "Creates or removes refs/replace mappings that redirect many Git object reads without rewriting the original object. --graft and edit modes can construct replacements. Replacement interpretation is optional/configurable and not automatically propagated by every transfer. Use --no-replace-objects globally when raw original-object interpretation is required."},
  {"replay", "Replay commits without a normal worktree rebase.", "replay [--onto <newbase>] <revision-range>...",
   "Experimental/version-dependent plumbing for computing replayed commits and proposed ref updates. It can write new objects without directly changing the ordinary index/worktree. Read the installed manual for exact ref-update output and supported merge/conflict behavior. Do not assume its output is a finished branch update; apply reported updates explicitly where required."},
  {"rerere", "Record and reuse conflict resolutions.", "rerere [status | remaining | diff | gc | clear | forget <pathspec>...]",
   "Maintains recorded preimages/resolutions for repeated conflicts. It may update conflicted working files and, with configuration, stage resolved entries. status/diff/remaining inspect state; forget and gc change the resolution cache. Reused resolutions still need semantic review by the workflow; this command does not by itself complete a merge/rebase commit."},
  {"reset", "Reset branch/index/worktree state or unstage paths.", "reset [--soft | --mixed | --hard | --merge | --keep] [<commit>] | reset [<tree-ish>] -- <pathspec>...",
   "Commit mode can move HEAD/current branch and selectively reset index/worktree. --soft preserves index/worktree, --mixed resets index, and --hard also replaces tracked working content. Path mode updates index entries without moving HEAD. Reflogs may retain previous tips, but overwritten uncommitted content is not necessarily recoverable. Choose the intended mode explicitly."},
  {"restore", "Restore paths in worktree and/or index.", "restore [--source=<tree>] [--staged] [--worktree] [--] <pathspec>...",
   "Separates path restoration from branch switching. Default source/target depend on --staged: worktree normally restores from index, while staged restoration defaults to HEAD. --source selects another tree. It can overwrite uncommitted content and resolve selected conflict stages according to options, but does not move the current branch."},
  {"rev-list", "Enumerate objects or commits selected by revision rules.", "rev-list [options] <revisions>... [--] <paths>...",
   "Plumbing traversal supporting ranges, exclusions, counts, parents, object enumeration and filters. --objects includes reachable object/path associations; --count returns counts rather than commit records. Ordering and history simplification are option-dependent. Shallow/partial repositories can limit available history/objects. It is read-only with respect to refs and worktree."},
  {"rev-parse", "Resolve revisions and query repository layout.", "rev-parse [--verify] [--quiet] <revision> | <repository-query-options>",
   "Resolves revision expressions, separates flags/revisions, and reports paths or repository properties such as --show-toplevel and --git-dir. --verify expects a single resolvable object; append ^{commit} when commit type is required. --end-of-options helps separate option-like input where supported. Output depends on requested queries; it is not one universal record format."},
  {"revert", "Record inverse changes as new commits.", "revert [--no-commit] [-m <parent>] <commit>... | --continue | --skip | --abort | --quit",
   "Applies inverses without moving history backwards. Normally creates commits; --no-commit leaves index/worktree changes for later recording. Merge reverts require a mainline parent and affect future merge interpretation. Conflicts leave sequencer state. This differs from reset: original commits remain in history and the reversal itself is recorded."},
  {"rm", "Remove tracked paths from index and optionally worktree.", "rm [-f] [-r] [--cached] [--ignore-unmatch] [--] <pathspec>...",
   "Stages deletions and normally deletes working files. --cached removes only index entries, retaining working content. Safety checks reject some staged/unstaged mismatches unless forced; -r permits recursive removal. --ignore-unmatch changes no-match failure behavior. Commit afterwards to record deletions; it does not purge old file content from existing history."},
  {"send-pack", "Low-level push protocol client.", "send-pack [--dry-run] [--force] [--all] <repository> [<refspec>...]",
   "Communicates with receive-pack, transfers necessary objects and requests ref updates. This plumbing exposes protocol-level controls and differs from push's complete remote/configuration interface. Server hooks and policy may reject updates. Binary packet/pack streams and per-ref status must be interpreted according to the negotiated protocol."},
  {"shortlog", "Summarize commits grouped by identity or trailer.", "shortlog [-s] [-n] [-e] [--group=<group>] [<revision-range>]",
   "Reads selected history or supported log input and prints contributor-oriented summaries. Mailmap, grouping and sorting affect totals and names. -s suppresses subjects, -n sorts by count and -e includes email. This is presentation output rather than a stable identity database; identical people may map differently under repository mailmap configuration."},
  {"show", "Display objects and their associated content.", "show [options] [<object>...] [--] <pathspec>...",
   "Commits display metadata plus diffs, tags show tag data, trees list entries and blobs emit content. A <revision>:<path> expression can select a historical blob. Binary blobs are returned exactly and may include NUL. Default formatting/pagers are human-oriented; select explicit formats or cat-file when precise object framing is required."},
  {"show-branch", "Compare branch histories in a compact display.", "show-branch [options] [<rev>...]",
   "Reads multiple histories and displays branch/commit relationships. Merge-base, independent and reflog modes change the report. The normal matrix is intended for human inspection and may have branch-count/display constraints. It does not switch branches or merge them. Use merge-base/rev-list/for-each-ref for more directly structured automation."},
  {"show-index", "Print entries from a pack index on stdin.", "show-index < <pack.idx>",
   "Consumes binary pack-index bytes and emits offset/object-ID information, with checksum-related fields depending on format. Supply a stdin file or input bytes rather than a literal '<' argument. It is a diagnostic reader and does not repair/rewrite the index. Invalid/truncated index data can produce a nonzero exit."},
  {"show-ref", "List or verify local refs.", "show-ref [--head] [--heads] [--tags] [--dereference] | --verify [--quiet] <ref>...",
   "Prints object-ID/refname records; --dereference includes peeled annotated tags. --verify requires exact ref names. Predicate/existence modes use meaningful nonzero statuses for missing refs, with details varying by subcommand/version. It does not contact remotes or create refs. Prefer full refs/heads/... names to avoid abbreviation ambiguity."},
  {"sparse-checkout", "Manage sparse working-tree population.", "sparse-checkout <init|set|add|list|reapply|disable|check-rules> [arguments]",
   "Writes sparse patterns/configuration and can update working files/index representation. Cone and non-cone modes interpret arguments differently; --stdin supports bulk pattern input. Sparse-index support depends on Git/tool compatibility. Disabling expands the checkout, while list inspects rules. Uncommitted files and conflicts can restrict updates."},
  {"stash", "Temporarily record and restore local changes.", "stash <push|list|show|pop|apply|drop|clear|branch|create|store|export|import> [arguments]",
   "push records index/worktree state and normally cleans selected changes; -u/-a include untracked/ignored content. apply restores without deleting the entry; pop drops only after successful application. Conflicts can leave staged/unmerged state. Stashes are commits referenced by refs/stash/reflogs, not an external backup. Subcommands vary with Git version."},
  {"status", "Report worktree and index state.", "status [--short | --porcelain[=<version>]] [-z] [--branch] [--untracked-files=<mode>]",
   "Reads staged, unstaged, conflicted and untracked classifications; it may refresh index stat metadata under normal Git locking policy. For scripts use a documented porcelain version with -z to preserve arbitrary filenames. A dirty repository normally still exits zero. Branch/upstream fields and ignored/submodule handling require explicit options."},
  {"stripspace", "Normalize whitespace or comment prefixes in text.", "stripspace [--strip-comments | --comment-lines]",
   "Reads stdin and writes normalized text, commonly for commit messages. Removes trailing whitespace and excess blank lines according to mode, or handles configured comment prefixes. It does not modify objects or refs. The result is intentionally transformed text, so it is inappropriate for binary payloads or byte-for-byte file copying."},
  {"submodule", "Manage repositories embedded as submodules.", "submodule <add|status|init|deinit|update|summary|foreach|sync|absorbgitdirs|set-branch|set-url> [arguments]",
   "Can modify .gitmodules/configuration, clone/fetch subrepositories and change their checkouts. update --init initializes missing modules; --recursive descends further. foreach interprets its command using Git's own shell semantics even though the DSL itself does not invoke a shell. Gitlinks record commit IDs, not the entire nested worktree content."},
  {"switch", "Switch branches or create a new branch checkout.", "switch [--create <name> | --force-create <name>] [<branch>] | --detach <commit>",
   "Updates HEAD, index and working files. --create makes a branch at the selected starting point; --detach checks out a commit without a branch. Local-change and linked-worktree restrictions may reject a switch. It does not restore arbitrary paths; use restore for that operation. Remote-branch guessing follows Git configuration/options."},
  {"symbolic-ref", "Read or write a symbolic reference.", "symbolic-ref [-q] <name> | [-m <reason>] <name> <ref> | --delete <name>",
   "Commonly reads/sets HEAD's symbolic target. Reading a detached HEAD returns a meaningful nonzero result rather than an object ID. Writing changes the symbolic reference, not the target commit or worktree checkout. Use full ref names and coordinate index/worktree state separately in low-level repository setup workflows."},
  {"tag", "Create, list, delete or verify tags.", "tag [-a | -s] [-m <message>] <name> [<object>] | --list [<pattern>...] | -d <name> | -v <name>",
   "Lightweight tags are refs; annotated/signed tags also write tag objects with identity/message data. Existing tags require force to replace. Listing can use --format and sorting. Verification invokes configured signing backends and returns signature status. Tags are not necessarily pushed/fetched by every refspec; transfer selection is a separate operation."},
  {"unpack-file", "Materialize a blob into a temporary file.", "unpack-file <blob>",
   "Reads a blob object, creates a temporary file and prints its path. The caller owns cleanup of that filesystem artifact. This does not stage or check out the file at its historical path. Blob bytes may be binary; avoid treating the printed filename as file content or interpolating it through a shell."},
  {"unpack-objects", "Read a pack from stdin into loose objects.", "unpack-objects [-n] [-q] [-r] [--strict]",
   "Consumes binary pack data and writes loose objects into the object database. -n validates/previews without writing, while recovery/strict options change error handling. It does not create branch refs. Truncation, corrupt deltas or missing bases can fail after some objects have been written; no automatic repository-wide rollback is provided."},
  {"update-index", "Modify index entries and index behavior directly.", "update-index [options] [--] <file>...",
   "Supports cacheinfo/index-info plumbing, additions/removals, stat refresh and flags such as skip-worktree or assume-unchanged. --stdin/-z and --index-info accept structured bulk input. It does not create commits or move HEAD. Incorrect low-level entries/flags can make worktree reporting surprising; use a separate GIT_INDEX_FILE for isolated tree-building workflows."},
  {"update-ref", "Update refs with optional expected-old-value checks.", "update-ref [-m <reason>] <ref> <new> [<old>] | --stdin [-z] | -d <ref> [<old>]",
   "Creates/updates/deletes refs, optionally comparing the previous value to detect concurrent changes. --stdin supports batches and transaction commands on supporting versions; -z changes framing. Reflog policy follows options/configuration. This changes refs without checking out trees or running a porcelain commit workflow; keep worktree/index coordination explicit."},
  {"update-server-info", "Regenerate metadata for dumb transports.", "update-server-info [--force]",
   "Writes auxiliary ref/pack advertisement files used by non-smart HTTP/other dumb transports. It does not launch a server, configure authentication or transfer objects. Run after relevant repository changes when serving through those transports. Smart Git servers generally negotiate directly and do not depend on the same static advertisement files."},
  {"upload-archive", "Serve archive requests over Git protocol.", "upload-archive <repository>",
   "Reads a client request and emits framed archive data/errors. Server policy limits which trees/paths may be archived. It is normally launched by a transport for git archive --remote. Standard streams carry protocol bytes rather than ordinary archive-only stdout, so direct consumers must implement the framing contract."},
  {"upload-pack", "Serve fetch negotiation and object transfer.", "upload-pack [--strict] [--timeout=<seconds>] <repository>",
   "Advertises refs/capabilities, negotiates wants/haves and emits pack data according to Git transport protocol. Usually started by SSH or a Git server. Configuration controls reachable-object requests and other policy. Capturing stdout as text does not implement a fetch client; use Git fetch or a protocol-aware peer."},
  {"var", "Print Git logical variables.", "var (-l | <variable>)",
   "Queries values such as author/committer identity, editor or pager selection, depending on Git version. -l lists available values/configuration context. Identity queries can fail when required names/emails are missing. This does not commit changes, but its output reflects environment and configuration that later commands will use."},
  {"verify-commit", "Verify signatures on commit objects.", "verify-commit [--raw] [--verbose] <commit>...",
   "Invokes configured cryptographic signing backends and reports signature verification status. Object existence alone does not imply a valid signature or trusted signer. Raw diagnostics differ by backend; use exit status plus an application-selected trust policy. It does not alter commits, refs or the worktree."},
  {"verify-pack", "Validate pack/index consistency and optionally report objects.", "verify-pack [-v] [-s] <pack-index>...",
   "Checks pack data addressed by index files. -v emits per-object type/size/offset/delta information; -s reports statistics. These reports describe storage representation, not repository reachability or ref validity. Large packs can take substantial I/O. Nonzero status indicates validation/read failure; no repair is performed."},
  {"verify-tag", "Verify signatures on annotated tag objects.", "verify-tag [--raw] [--verbose] <tag>...",
   "Reads tag objects and invokes the configured signing backend. Lightweight tags have no tag-object signature to verify. Successful cryptographic verification and application trust are separate decisions. Output/diagnostics are backend-specific, and verification does not fetch missing keys/objects unless the configured backend itself does so."},
  {"version", "Report the installed Git version and build information.", "version [--build-options]",
   "Runs without requiring a repository. --build-options adds architecture/build-related details where supported. Version text can include vendor suffixes and should not be parsed as a rigid three-integer format. Prefer probing a documented command/option when capability behavior matters. It does not mutate repository state."},
  {"whatchanged", "Display history with raw change information.", "whatchanged [<log-options>] [--] <pathspec>...",
   "Legacy history presentation related to log with raw diff output. Parent/merge filtering and default behavior differ from a plain log invocation. It is read-only and may invoke a pager. New scripts should select explicit log/diff-tree options, but this constructor preserves access for existing workflows and installed versions."},
  {"worktree", "Manage multiple working trees for one repository.", "worktree <add|list|lock|unlock|move|prune|remove|repair> [arguments]",
   "add creates a linked checkout with its own HEAD/index; shared objects and most refs remain in the common repository. list --porcelain supports automation. lock prevents administrative pruning/movement; remove deletes a worktree subject to cleanliness checks. prune removes stale administrative records, not equivalent to ordinary directory removal."},
  {"write-tree", "Write a tree object from the index.", "write-tree [--missing-ok] [--prefix=<prefix>/]",
   "Creates tree objects and prints the root tree ID without making a commit or updating refs. Unmerged index stages normally prevent tree creation. --prefix selects a subtree, and --missing-ok relaxes object-existence checks. Pair with a temporary GIT_INDEX_FILE, update-index/read-tree and commit-tree for low-level construction workflows."},
  {"daemon", "Serve repositories using the native Git protocol.", "daemon [--listen=<address>] [--port=<port>] [--base-path=<path>] [options] [<directory>...]",
   "Starts a long-running server whose export/service permissions follow Git daemon configuration. It may daemonize or remain attached depending on options. Repository paths, logging and connection limits are server settings. A synchronous exec.run waits for termination; use mulib_exec_start and explicit signal/wait ownership for a managed server process."},
  {"send-email", "Send patch messages through mail transport.", "send-email [options] <file-or-directory-or-revision-range>...",
   "Optional Git component that can format/read patches, prompt for recipients and send mail through SMTP or a local sendmail program. It performs external delivery, not merely patch generation. --dry-run inspects the workflow without actual delivery according to Git behavior. Availability depends on installed scripting modules and mail configuration."},
  {"request-pull", "Compose a pull-request summary.", "request-pull [-p] <start> <url> [<end>]",
   "Writes a summary of changes and a repository URL/ref intended for human communication. It can inspect remote refs to check availability. It does not create a hosting-provider pull request or push commits. Ensure the selected commits are published at the advertised URL; -p adds patch content to the report."},
  {"svn", "Interact with a Subversion repository through Git.", "svn <clone|init|fetch|rebase|dcommit|log|...> [arguments]",
   "Optional interoperability component with its own metadata, configuration and revision mapping. fetch/rebase import Subversion changes; dcommit publishes commits to Subversion and can rewrite local history to record mappings. Requires installed SVN bindings and transport/authentication support. Subcommand-specific options pass through without DSL filtering."},
  {"p4", "Import from and submit to Perforce.", "p4 <clone|sync|rebase|submit|branches> [arguments]",
   "Optional interoperability script that uses the Perforce client and mapping configuration. Import writes Git objects/refs; submit creates changes in the external Perforce service and may require interactive editing. Availability depends on installed components and credentials. The execution DSL preserves its native command-line and exit-status behavior."},
  {"gitk", "Open the graphical repository history viewer.", "gitk [<revision-options>] [--] <pathspec>...",
   "gitk is generally a separate executable rather than a git subcommand. Use exec.command{argv={\"gitk\",...}} or git.raw(arguments,{executable=\"gitk\"}) to launch it. A display/toolkit is required. The catalogue entry documents the distinction; generic git.command('gitk') succeeds only if an installed Git extension supplies that command."},
  {"gui", "Open Git's graphical interface.", "gui [<subcommand>] [arguments]",
   "Optional graphical component that can stage, commit, inspect and manage repository operations interactively. Requires an installed GUI/toolkit and display environment. Synchronous execution waits for the GUI process. For unattended workflows use explicit porcelain/plumbing plans instead of relying on GUI dialog behavior."},
  {"filter-branch", "Rewrite history using legacy filters.", "filter-branch [filter-options] [--] <rev-list-options>...",
   "Legacy history rewriting tool whose filter arguments are interpreted as shell programs by Git. The DSL preserves each filter as one argument but does not remove that embedded language. Can rewrite refs, create backup refs and leave temporary files. Complex correctness/performance caveats apply; consult the installed manual and verify rewritten history before publishing."},
  {"mergetool", "Resolve conflicts using an external merge tool.", "mergetool [--tool=<tool>] [--no-prompt] [--] <file>...",
   "Invokes configured external tools for unmerged paths, creates temporary/base/local/remote files and may update the index after resolution. Tool exit codes, backup retention and prompting depend on configuration. It does not necessarily finish the surrounding merge/rebase/cherry-pick. Continue the owning operation after all conflicts are resolved."},
  {"instaweb", "Launch a local web interface for a repository.", "instaweb [--httpd=<server>] [--port=<port>] [--start | --stop | --restart]",
   "Optional helper that configures/starts an HTTP server for gitweb and may launch a browser. Requires external server/web components. Start/stop operations manage a service that may outlive the invoking process. It does not publish through a hosted Git provider; binding/address/access policy follows the chosen server configuration."},
  {"cvsexportcommit", "Export one Git commit into a CVS checkout.", "cvsexportcommit [options] [<parent>] <commit>",
   "Optional interoperability script that applies a selected Git commit to a CVS working tree and may commit it there depending on options. Requires CVS tooling and a configured target checkout. Git and CVS side effects are not one atomic transaction. Inspect mapping, binary-file handling and conflict results using the installed command manual."},
  {"cvsimport", "Import CVS history into Git.", "cvsimport [options] [<CVS-module>]",
   "Optional importer requiring CVS conversion tooling. Creates Git commits/refs from inferred changesets, with author/branch/tag mapping controlled by options. CVS history grouping can be ambiguous and imported identities may depend on conversion heuristics. Repeated imports use importer metadata; this is not a normal fetch from a native Git remote."},
  {"cvsserver", "Expose Git history through a CVS-compatible server.", "cvsserver [options] [<repository>...]",
   "Optional compatibility server implementing selected CVS operations over Git storage. It may run as a protocol service and maintain auxiliary databases. Authentication, exported modules and permitted updates follow server configuration. Protocol stdin/stdout is not ordinary text output for casual parsing; use an appropriate CVS client peer."},
  {"archimport", "Import a GNU Arch repository into Git.", "archimport [options] <archive/branch>...",
   "Legacy optional importer that invokes Arch tooling and maps revisions/branches into Git objects and refs. It requires access to the original archives and mapping metadata. Imported commit identity and topology follow conversion rules rather than a native Git transfer. Availability is installation-dependent; generic command dispatch preserves its full option surface."},
  {"quiltimport", "Apply a quilt patch series as Git commits.", "quiltimport [--dry-run] [--author <identity>] [--patches <dir>] [--series <file>]",
   "Reads series/patch files, derives author/message information and creates commits while updating the worktree/index. --dry-run checks the planned interpretation according to the command's behavior. Missing metadata or patch conflicts can stop the sequence. This is an optional scripted tool, not a replacement for am's mailbox protocol."},
  {"shell", "Provide a restricted login command interpreter for Git services.", "shell -c <command>",
   "Normally configured as a user's login shell for allowed Git service commands. The -c string is interpreted by git-shell's own restricted command language; the DSL passes it literally but does not replace that parser. Interactive custom-command support depends on the user's git-shell-commands directory. This is not a general-purpose POSIX shell."},
  {"http-backend", "Serve smart/dumb Git HTTP through CGI.", "http-backend",
   "Reads CGI environment and request bytes from stdin, writes HTTP response headers/body, and invokes upload/receive services according to repository export policy. Requires server-provided PATH_INFO, REQUEST_METHOD and other variables. It is not an HTTP listener. Use a proper CGI/server integration to frame requests, authenticate users and stream binary responses."},
  {"http-fetch", "Fetch objects over a dumb HTTP transport.", "http-fetch [options] <commit> <url>",
   "Legacy plumbing for downloading reachable objects via static HTTP object storage. It differs from modern smart fetch negotiation and does not automatically reproduce fetch's complete remote-ref workflow. Network/TLS/authentication behavior follows Git's HTTP implementation. Prefer ordinary fetch unless a dumb-protocol integration specifically requires this command."},
  {"http-push", "Push refs over a dumb HTTP/WebDAV transport.", "http-push [options] <url> <refspec>...",
   "Optional transport plumbing requiring suitable server-side WebDAV/locking capabilities. It writes remote objects and refs according to refspecs. Modern hosted repositories generally use smart HTTP through push instead. Build-time HTTP support, authentication, locking and server policy determine availability and success."}
}

-- Internal/compatibility entry points are callable too. Their documentation
-- explicitly identifies the narrower contract rather than pretending they are
-- stable porcelain commands.
local internal = {
  {"checkout--worker", "Parallel-checkout worker", "Consumes the internal checkout worker protocol on stdin and writes worker responses. Started by checkout's parallel engine; protocol details are implementation-specific and can change between versions. It can write working-tree files. Use checkout/switch for normal callers."},
  {"credential-cache--daemon", "Credential-cache service", "Runs the local credential-cache daemon with a socket path and internal options. Holds credentials for helper clients and can outlive a single request. Service/socket ownership and shutdown are caller-managed when launched directly. Prefer credential-cache for the public helper interface."},
  {"fsmonitor--daemon", "Filesystem-monitor service", "Starts/stops/queries Git's built-in filesystem monitor on supported platforms. Maintains IPC and change-tracking state for worktrees. Availability and daemon subcommands are version/platform-specific; it may create a long-lived process. Normal status/index operations consume its service through configuration."},
  {"submodule--helper", "Submodule implementation helper", "Provides internal subcommands used by Git's submodule implementation. It can inspect or mutate submodule metadata and invoke further Git processes. Argument contracts may change without porcelain compatibility guarantees. Prefer git submodule for stable user-facing operations."},
  {"upload-archive--writer", "Archive service writer", "Internal writer used by upload-archive to produce requested archive bytes and diagnostics. Expects implementation-specific request context. Output can contain binary archive data and is not a shell command string. Prefer archive or upload-archive depending on whether local generation or protocol service is needed."},
  {"merge-ours", "Ours merge-strategy helper", "Strategy implementation that resolves the merge to the current tree rather than incorporating other trees. Usually invoked by merge -s ours with internal ancestor/head arguments. This is distinct from the -X ours conflict preference. Use the porcelain merge strategy selector for normal operations."},
  {"merge-recursive", "Recursive merge-strategy helper", "Legacy recursive strategy entry point consuming internal base/head argument conventions. May update index/worktree and report conflicts. Modern Git may route or retire strategy implementations differently. Use merge -s with the installed version's supported strategy names for a stable interface."},
  {"merge-subtree", "Subtree merge-strategy helper", "Strategy entry point that adjusts tree paths while performing a recursive-style merge. It expects strategy-level arguments, not the ordinary merge CLI. Can modify index/worktree and leave conflicts. Prefer merge -s subtree or the documented strategy option for the installed Git version."},
  {"merge-recursive-ours", "Compatibility merge-strategy entry", "Version-specific compatibility entry for recursive merging with an ours preference. Internal argument and availability rules are not stable across Git versions. Prefer merge with a documented -s strategy and -X ours option. A conflict preference is not the same as replacing the entire result with the current tree."},
  {"merge-recursive-theirs", "Compatibility merge-strategy entry", "Version-specific compatibility entry for recursive merging with a theirs preference. Internal strategy arguments differ from porcelain merge options. Prefer a documented merge strategy with -X theirs when that behavior is desired. Cleanly merged content is still incorporated; only conflict resolution preference is affected."},
  {"diff-pairs", "Pairwise diff helper", "Version-specific command for comparing supplied pairs according to the installed Git interface. Treat input/output formats as command-specific rather than assuming ordinary diff-tree framing. It reads selected content and reports differences; query the installed -h/manual for supported pair input and exit semantics."},
  {"format-rev", "Revision formatting helper", "Version-specific revision presentation command. Pass revision and formatting arguments exactly as documented by the installed Git build. Formatting is an output operation, not a ref update. Do not assume that its format vocabulary or output is identical to log --format without checking the local command contract."},
  {"history", "History inspection interface", "Version-specific history command whose supported subcommands/options depend on the installed Git release or distribution. The DSL forwards the full argument vector without translating it to log. Use -h or installed documentation to choose output modes and determine whether a requested operation is read-only."},
  {"last-modified", "Path modification query", "Version-specific command for querying modification history. Selection, traversal and output fields follow the installed implementation. It is not a filesystem mtime query unless explicitly documented as such. Use exact revisions/path separators and consult local help before parsing its output."},
  {"pickaxe", "Content-change search interface", "Version-specific interface for history searches based on content changes. Search expressions belong to Git's parser, not the execution DSL. Do not assume equivalence to every log -S/-G mode; use installed help for matching and output rules. Nonzero exit interpretation is command-specific."},
  {"repo", "Repository management interface", "Version-specific repository command family. Subcommands may inspect or alter repository configuration/storage depending on the installed build. Arguments pass through literally and no unsupported subcommand is emulated. Check local help before choosing a mutating operation or interpreting structured output."},
  {"url-parse", "Git URL parsing helper", "Version-specific helper for inspecting Git transport URL syntax. It does not imply an HTTP request or repository fetch merely by parsing a string. Supported schemes, output fields and invalid-input statuses follow the installed implementation. Treat URL bytes as one argument unless its manual specifies another input protocol."}
}
for _,e in ipairs(internal) do
  entries[#entries+1] = {e[1], e[2]..".", e[1].." [implementation-specific arguments]", e[3]}
end

local function register(e)
  local name = e[1]
  local doc = "git "..name.." — "..e[2].."\n\nSYNOPSIS\n  git "..e[3]..
              "\n\nBEHAVIOR AND INTERPRETATION\n"..e[4]..contract
  git.docs[name]=doc
  git.catalog[name]={purpose=e[2], synopsis=e[3], details=e[4], doc=doc}
  local constructor = function(arguments,options) return git.command(name,arguments,options) end
  git.commands[name]=constructor
  local alias=string.gsub(name,"-","_")
  if git[alias]==nil then git[alias]=constructor end
end
for _,e in ipairs(entries) do register(e) end
for name,target in pairs({stage="add", ["init-db"]="init", ["fsck-objects"]="fsck"}) do
  register({name, "Compatibility alias for git "..target..".", name.." [arguments]",
    "Uses the same operation family as "..target..". "..git.catalog[target].details})
end
-- Keep helper documentation distinct from same-named Git command documentation.
git.docs.dsl_help = [[git.help(name) returns git.docs[name] or the generic command
contract. To execute Git's help command, use git.commands.help(arguments,options).
No process is started by the docstring lookup.]]
_G.git=git
return git
