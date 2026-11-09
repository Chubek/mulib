# lcharts

Auto-loaded Mune Lua chart library for [`mulib-chart.h`](../../include/mulib-chart.h).
No `require` or external Lua runtime is needed. `require('lcharts')` also returns
the installed module. Constructors return bounded SVG trees, not HTML strings.

Builtins: `pie`, `donut`, `bar`, `line`, `scatter`. New charts are ordinary Lua
functions registered with `lcharts.register(name, fn)`; they have the same data
and rendering powers as builtins. See [the module manual](../../manual/mulib-chart.md).

```lua
lcharts.register('constellation', function(data, options)
  local svg = lcharts.svg(400, 300, 'My custom chart')
  for _, point in ipairs(data) do
    svg:add(lcharts.circle(point.x, point.y, point.radius or 4, {fill='navy'}))
  end
  return svg
end)
```

The source is original MuLIB code. Edit `lcharts.lua`, then run
`python3 tools/embed_notation.py`; applications do not read it at runtime.
