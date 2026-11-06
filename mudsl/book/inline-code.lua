-- Keep long API names and template signatures inside narrow table columns.
function Code(element)
  local escaped = {['\\']='\\textbackslash{}', ['{']='\\{', ['}']='\\}',
    ['_']='\\_', ['#']='\\#', ['$']='\\$', ['%']='\\%', ['&']='\\&',
    ['^']='\\textasciicircum{}', ['~']='\\textasciitilde{}',
    ['<']='\\textless{}', ['>']='\\textgreater{}'}
  local out, run = {}, 0
  for _, point in utf8.codes(element.text) do
    local c = utf8.char(point)
    out[#out+1] = escaped[c] or c
    run = run + 1
    if c:match('[_:/,().<>]') or run >= 16 then
      out[#out+1] = '\\allowbreak{}'
      run = 0
    end
  end
  return pandoc.RawInline('latex', '\\texttt{' .. table.concat(out) .. '}')
end
