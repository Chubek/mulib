# Distributed task profiles

`cluster.lua` is a trusted Mune profile loaded **before** a task program by
`mulib_distribtask_load_file(config_path, format, task_path, profile_path, error)`.
`format` is `MULIB_DISTRIBTASK_JSON` or `MULIB_DISTRIBTASK_YAML`. Configuration is
mandatory; `cluster.json` and `cluster.yaml` describe the same two-node mesh.
There is no implicit profile/module search path.

`example.distribtask` demonstrates Mune, container, OCI lifecycle and VMM work.
Its node-local resources must already be provisioned and bound through
`mulib-distribcluster.h`; compilation alone never creates isolated containers
or guest machines. The helper's optional last argument names a prerequisite;
call `distrib_depends(id, prerequisite)` for additional dependencies.

All sources are Mune, not C, regardless of filename extension. Profiles can use
ordinary Mune functions/loops and the `distrib_config` table. Scripts have host
capabilities and must be trusted. Credentials are supplied through channel
options, never these files. See `manual/mulib-distribcomp.md` and
`manual/mulib-distribtask.md` for scheduling, limits and delivery guarantees.
