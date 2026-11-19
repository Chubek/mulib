local c = require('muw3lib.core')
local s = require('muw3lib.services._util')
local M = {}
function M.new(secret_key, options)
  s.token(secret_key, 'Stripe secret key')
  local opt=s.object(options, 'Stripe options', true)
  local headers={}
  if opt.api_version ~= nil then headers['Stripe-Version']=s.token(opt.api_version, 'Stripe API version') end
  local cli=c.new(s.base(opt.base_url, 'https://api.stripe.com'), {bearer=secret_key, headers=headers})
  local api={raw=cli}
  local function send(path, fields, idempotency)
    local h={}
    if idempotency ~= nil then
      h['Idempotency-Key']=s.header(idempotency, 'idempotency key')
      assert(#idempotency <= 255, 'idempotency key exceeds 255 bytes')
    end
    assert(type(fields) == 'table' and fields ~= c.muw3.null, 'Stripe form fields required')
    return cli:json('POST', path, {form=fields, headers=h})
  end
  local function get(path, id, label)
    return cli:get(path..'/'..s.id(id, label, '^[A-Za-z0-9_]+$'))
  end
  function api.customer(id) return get('/v1/customers', id, 'customer ID') end
  function api.customers(query) return cli:get('/v1/customers', query) end
  function api.create_customer(fields, idempotency) return send('/v1/customers', fields, idempotency) end
  function api.payment_intent(id) return get('/v1/payment_intents', id, 'payment intent ID') end
  function api.create_payment_intent(fields, idempotency)
    return send('/v1/payment_intents', fields, idempotency)
  end
  function api.create_checkout_session(fields, idempotency)
    return send('/v1/checkout/sessions', fields, idempotency)
  end
  function api.checkout_session(id) return get('/v1/checkout/sessions', id, 'checkout session ID') end
  function api.subscription(id) return get('/v1/subscriptions', id, 'subscription ID') end
  function api.create_subscription(fields, idempotency)
    return send('/v1/subscriptions', fields, idempotency)
  end
  return api
end
return M
