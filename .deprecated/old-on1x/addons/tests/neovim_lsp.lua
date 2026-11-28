-- Run with nvim --headless -u NONE -i NONE -l neovim_lsp.lua ON1XC PYTHON.
local addons = vim.fs.dirname(vim.fs.dirname(debug.getinfo(1, 'S').source:sub(2)))
addons = vim.fn.fnamemodify(addons, ':p'):gsub('/$', '')
local compiler = vim.fn.fnamemodify(assert(arg[1], 'missing on1xc'), ':p')
local python = assert(arg[2], 'missing Python interpreter')
local work = vim.fn.tempname()
local source = work .. '/sample with spaces.on1x'

local ok, err = xpcall(function()
  assert(vim.fn.mkdir(work, 'p') == 1)
  assert(vim.fn.writefile({ 'fn main() { let a = ; }' }, source) == 0)
  vim.opt.runtimepath:prepend(addons .. '/neovim')
  vim.cmd('filetype plugin indent on')
  require('on1x').setup({
    cmd = { python, addons .. '/lsp/on1x-lsp.py', '--on1xc', compiler },
  })
  vim.cmd('edit ' .. vim.fn.fnameescape(source))
  local buf = vim.api.nvim_get_current_buf()
  assert(vim.bo[buf].filetype == 'on1x', 'filetype was not detected')
  assert(#vim.lsp.get_clients({ bufnr = buf }) == 0, 'LSP started without opt-in')
  vim.cmd('On1xLspStart')
  assert(vim.wait(8000, function()
    for _, client in ipairs(vim.lsp.get_clients({ bufnr = buf, name = 'on1x' })) do
      if client.initialized then
        return true
      end
    end
    return false
  end), 'On1x LSP initialization timed out')
  vim.cmd('write')
  assert(vim.wait(8000, function()
    return #vim.diagnostic.get(buf) > 0
  end), 'no compiler diagnostic received by Neovim')
  assert(vim.diagnostic.get(buf)[1].lnum == 0, 'wrong diagnostic line')

  vim.api.nvim_buf_set_lines(buf, 0, -1, false, { 'fn main() {', '  let a = ;', '}' })
  vim.cmd('write')
  assert(vim.wait(8000, function()
    local diagnostics = vim.diagnostic.get(buf)
    return #diagnostics > 0 and diagnostics[1].lnum == 1
  end), 'save did not refresh the diagnostic for the edited source')

  vim.api.nvim_buf_set_lines(buf, 0, -1, false, { 'fn main(){println(21);}' })
  vim.cmd('write')
  assert(vim.wait(8000, function()
    return #vim.diagnostic.get(buf) == 0
  end), 'stale diagnostic after save')
  vim.lsp.buf.format({ bufnr = buf, timeout_ms = 8000 })
  local text = table.concat(vim.api.nvim_buf_get_lines(buf, 0, -1, false), '\n')
  assert(text == 'fn main() {\n  println(21);\n}', 'unexpected formatted text: ' .. text)
  assert(vim.fn.readfile(source)[1] == 'fn main(){println(21);}', 'formatter wrote to disk')
end, debug.traceback)

vim.fn.delete(work, 'rf')
if not ok then
  io.stderr:write(err .. '\n')
  vim.cmd('cquit 1')
end
