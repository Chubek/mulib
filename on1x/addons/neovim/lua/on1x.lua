local M = {}

-- Found only when this runtime is used from the source checkout, not when
-- the editor addon is copied on its own.
local source = vim.fn.resolve(debug.getinfo(1, "S").source:sub(2))
local sibling_server = vim.fn.fnamemodify(source, ":p:h:h:h") .. "/lsp/on1x-lsp.py"

local function get_command(opts)
  if opts.cmd ~= nil then
    if type(opts.cmd) ~= "table" or not vim.islist(opts.cmd) or #opts.cmd == 0 then
      return nil, "cmd must be a non-empty argv list"
    end
    for _, arg in ipairs(opts.cmd) do
      if type(arg) ~= "string" or arg == "" then
        return nil, "cmd entries must be non-empty strings"
      end
    end
    return opts.cmd
  end
  local server = vim.fn.fnamemodify(opts.server or sibling_server, ":p")
  if vim.fn.filereadable(server) == 0 then
    return nil, "cannot read the language server: " .. server .. " (set server or cmd)"
  end
  return {
    "python3",
    server,
    "--on1xc",
    opts.on1xc or "on1xc",
  }
end

---Explicitly start a client for the current .on1x source buffer.
---Returns the client ID, or nil and an explanatory message.
function M.start(opts)
  if vim.fn.has("nvim-0.10") ~= 1 then
    return nil, "the On1x LSP helper requires Neovim 0.10 or newer"
  end
  opts = opts or {}
  if type(opts) ~= "table" then
    return nil, "options must be a table"
  end
  for _, key in ipairs({ "server", "on1xc", "root_dir" }) do
    local value = opts[key]
    if value ~= nil and (type(value) ~= "string" or value == "") then
      return nil, key .. " must be a non-empty string"
    end
  end
  local bufnr = vim.api.nvim_get_current_buf()
  local file = vim.api.nvim_buf_get_name(bufnr)
  if vim.bo[bufnr].filetype ~= "on1x" or vim.bo[bufnr].buftype ~= "" or not file:match("%.on1x$") then
    return nil, "open an On1x source file (*.on1x) first"
  end
  local cmd, err = get_command(opts)
  if not cmd then
    return nil, err
  end
  if vim.fn.executable(cmd[1]) == 0 then
    return nil, "not executable: " .. cmd[1]
  end
  local root = vim.fs.normalize(opts.root_dir or vim.fs.dirname(file), { expand_env = false })
  local id = vim.lsp.start({
    name = "on1x",
    cmd = cmd,
    root_dir = root,
  }, {
    bufnr = bufnr,
    reuse_client = function(client, config)
      return client.name == config.name
        and client.config.root_dir == config.root_dir
        and vim.deep_equal(client.config.cmd, config.cmd)
    end,
  })
  if not id then
    return nil, "could not start the language server (see :LspLog)"
  end
  return id
end

---Register a manual :On1xLspStart command. Does not launch a process.
function M.setup(opts)
  if vim.fn.has("nvim-0.10") ~= 1 then
    error("the On1x LSP helper requires Neovim 0.10 or newer")
  end
  local settings = vim.deepcopy(opts or {})
  vim.api.nvim_create_user_command("On1xLspStart", function()
    local _, err = M.start(settings)
    if err then vim.notify("On1x: " .. err, vim.log.levels.ERROR) end
  end, { desc = "Start the On1x language server for this buffer", force = true })
end

return M
