-- Load all 30 optional widgets. Individual modules can be required instead.
local ui = require('immgtk.core')
local widgets = {
  'panel', 'label', 'button', 'checkbox', 'slider', 'toggle', 'radio',
  'selectable', 'progress', 'vslider', 'scrollbar', 'stepper', 'tab',
  'menu_item', 'link', 'badge', 'separator', 'heading', 'tooltip', 'group_box',
  'collapsing_header', 'tree_node', 'list_box', 'combo_box', 'image',
  'color_swatch', 'sparkline', 'histogram', 'meter', 'spinner'
}
for i = 1, #widgets do require('immgtk.' .. widgets[i]) end
return ui
