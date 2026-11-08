local M = {}

-- Neovim 0.8+: call once from init.lua after installing this runtime package.
function M.setup(options)
  options = options or {}
  local group = vim.api.nvim_create_augroup('mu_lsp', { clear = true })
  local filetypes = options.native_api and { 'mull', 'c', 'cpp', 'd' } or { 'mull' }
  local function attach()
    if not vim.tbl_contains(filetypes, vim.bo.filetype) then return end
    local file = vim.api.nvim_buf_get_name(0)
    local root = options.root
    if not root then
      local marker = vim.fs.find({ 'MuDSL.hpp', '.git' }, { path = vim.fs.dirname(file), upward = true })[1]
      root = marker and vim.fs.dirname(marker) or vim.fn.getcwd()
    end
    vim.lsp.start({
      name = 'mu', cmd = options.cmd or { 'mull-lsp' }, root_dir = root,
      init_options = { repositoryRoot = options.root, packagePaths = options.package_paths or {} },
      capabilities = options.capabilities,
      on_attach = options.on_attach,
    })
  end
  vim.api.nvim_create_autocmd('FileType', { group = group, pattern = filetypes, callback = attach })
  attach()
end

return M
