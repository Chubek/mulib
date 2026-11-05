-- Loaded as the trusted profile argument to mulib_distribtask_load[_file].
-- The required JSON/YAML document is available as distrib_config.
function cluster_task(id, node, kind, action, payload, after)
  distrib_task(id, node, kind, action, payload)
  if after then distrib_depends(id, after) end
  return id
end
function container_task(id, node, action, resource, after)
  return cluster_task(id, node, 'container', action, resource, after)
end
function oci_task(id, node, action, resource, after)
  return cluster_task(id, node, 'ocicontainer', action, resource, after)
end
function vmm_task(id, node, action, resource, after)
  return cluster_task(id, node, 'vmm', action, resource, after)
end
function mune_task(id, node, source, after)
  return cluster_task(id, node, 'mune', 'run', source, after)
end
function daemon_task(id, node, action, specification, after)
  return cluster_task(id, node, 'daemon', action, specification, after)
end
function replicate(prefix, kind, action, payload)
  local ids = {}
  for i, node in ipairs(distrib_config.nodes) do
    ids[i] = cluster_task(prefix .. '-' .. node, node, kind, action, payload)
  end
  return ids
end
