local d = require('ldocstr')
return d.Doc({
  d.Header(1, 'Lua-authored documentation'),
  d.TOC(),
  d.Para({d.Str('Press '), d.Element('kbd', 'Enter', {inline = 'true'}), d.Str('.')}),
  d.Table({
    d.Row({d.Cell('Feature', {header = 'true'}), d.Cell('Status', {header = 'true'})}),
    d.Row({d.Cell('DocstrIL'), d.Cell('Shared by every backend')})
  })
})
