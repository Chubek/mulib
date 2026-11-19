-- build/muw3 --weblet library/muw3lib/examples/github.lua
package.path = 'library/?.lua;library/?/init.lua;' .. package.path
local api = require('muw3lib')
local github = api.github.new(os.getenv('GITHUB_TOKEN'))
local result = github.repository('Chubek', 'mulib')
print(result.full_name)
