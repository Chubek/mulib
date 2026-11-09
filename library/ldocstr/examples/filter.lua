return {
  Str = function(node)
    if node.text == 'Enter' then return ldocstr.Str('Return') end
  end
}
