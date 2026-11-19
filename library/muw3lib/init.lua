-- Top-level module; 10 service APIs, HTTP protocols, and native fetch adapter.
local M = {version='0.2.0'}
M.core = require('muw3lib.core')
M.rest = require('muw3lib.protocols.rest')
M.graphql = require('muw3lib.protocols.graphql')
M.jsonrpc = require('muw3lib.protocols.jsonrpc')
M.soap = require('muw3lib.protocols.soap')
M.sse = require('muw3lib.protocols.sse')
M.unsupported = require('muw3lib.protocols.unsupported')
M.websocket = require('muw3lib.protocols.websocket')
M.grpc = require('muw3lib.protocols.grpc')
M.browser = require('muw3lib.browser')
M.github = require('muw3lib.services.github')
M.openai = require('muw3lib.services.openai')
M.googlemaps = require('muw3lib.services.googlemaps')
M.stripe = require('muw3lib.services.stripe')
M.slack = require('muw3lib.services.slack')
M.twilio = require('muw3lib.services.twilio')
M.gitlab = require('muw3lib.services.gitlab')
M.opencode = require('muw3lib.services.opencode')
M.wikipedia = require('muw3lib.services.wikipedia')
M.openweather = require('muw3lib.services.openweather')
-- Both entry points apply the same synchronous HTTP validation and policy.
M.fetch = M.browser.fetch
return M
