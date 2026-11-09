local style = require('ldstr-style')
return style.filter({
  heading = { color = 'navy', ['font-weight'] = 'bold' },
  kbd = { border = '1px solid gray', padding = '2px' }
})
