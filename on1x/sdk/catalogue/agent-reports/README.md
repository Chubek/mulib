# Subagent reports

Each SDK 2 implementation worker writes exactly one
`subagent-N.json` report here. Reports use statuses `EXECUTABLE`, `PARTIAL`, and
`UNSUPPORTED`, list changed files and public facilities, record exact test
commands/results, and identify cross-owner API requests. The principal agent
merges these reports into `CAPABILITY_MATRIX.json`.
