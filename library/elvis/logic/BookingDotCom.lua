-- Canonical offline fixture profile; not a live-site compatibility claim.
local function first_text(card, selector)
    local nodes = xpath(card, selector)
    if nodes[1] then return text(nodes[1]) end
end

function on_response(document, effective_url)
    local cards = xpath(document, "//*[@data-testid='property-card']")
    for i = 1, #cards do
        local card = cards[i]
        local name = first_text(card, ".//*[@data-testid='title']")
        local links = xpath(card, ".//a[@data-testid='title-link']")
        local url = links[1] and attr(links[1], "href")
        if name and name ~= "" and url and url ~= "" then
            emit(name, url,
                 first_text(card, ".//*[@data-testid='price-and-discounted-price']"),
                 first_text(card, ".//*[@data-testid='address']"))
        end
    end
    local next_links = xpath(document, "//a[@data-testid='pagination-next']")
    local next_url = next_links[1] and attr(next_links[1], "href")
    if next_url and next_url ~= "" then enqueue(next_url) end
end
