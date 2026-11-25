-- Public property cards; exercised against a synthetic, static HTML fixture.
local origin = "https://www.agoda.com"
local absolute_prefix, absolute_limit = origin .. "/", origin .. "0"

local function first_text(node, selector)
    local matches = xpath(node, selector)
    if matches[1] then return text(matches[1]) end
end

local function property_url(href)
    if not href then return end
    -- Bare Mune has no string library. A lexicographic interval ending
    -- immediately after a prefix tests for that prefix without slicing.
    if href > absolute_prefix and href < absolute_limit then return href end
    if href > "/" and href < "0" and not (href >= "//" and href < "/0")
       and not (href >= "/\\" and href < "/]") then
        return origin .. href
    end
end

local function property_link(card, name)
    local links = xpath(card, ".//a[@href]")
    for i = 1, #links do
        local link = links[i]
        if attr(link, "data-selenium") == "hotel-name"
           or first_text(link, ".//*[@data-selenium='hotel-name']") == name then
            local url = property_url(attr(link, "href"))
            if url then return url end
        end
    end
end

function on_response(document)
    local cards = xpath(document, "//*[@data-selenium='hotel-item']")
    local seen, count = {}, 0
    for i = 1, #cards do
        local card = cards[i]
        local name = first_text(card, ".//*[@data-selenium='hotel-name']")
        if name and name ~= "" then
            local url = property_link(card, name)
            if url and not seen[url] then
                emit(name, url, first_text(card, ".//*[@data-selenium='display-price']"),
                     first_text(card, ".//*[@data-selenium='area-city']"))
                seen[url] = true
                count = count + 1
                if count == 20 then break end
            end
        end
    end
end
