local qmex = require "qmex"

local function header(buf)
    local n = 1
    local order = {}
    local title = string.match(buf, '[^\n]*')
    for w in string.gmatch(title, '(%w+)%g*') do
        if order[w] == nil then
            order[w] = n
            n = n + 1
        end
    end
    return order
end

local function mergesort(order, ...)
    local map, keys = {}, {}

    for _,t in ipairs({...}) do
        for k,v in pairs(t) do
            if map[k] == nil then
                table.insert(keys, k)
                map[k] = v
            end
        end
    end

    table.sort(keys, function(a, b)
        local ia = order[a] or math.huge
        local ib = order[b] or math.huge
        return (ia == ib) and (a < b) or (ia < ib)
    end)

    return map, keys
end

local function tostring(map, keys)
    local list = {}
    for _,k in ipairs(keys) do
        local v = map[k] or ''
        table.insert(list, string.format('%s:%s', k, v))
    end
    return table.concat(list, ' ')
end

local function jit(name)
    print("JIT", name)
    _G[name] = function() return math.exp(-100/Average) end
end


local buf = io.open("demo.ini", "rb"):read('a')
local t <close> = qmex.Table(4)
t[1] = header(buf)
t[1].row = 0
t[2] = 0   -- num_queries
t[3] = 0   -- first_error_query_id
t[4] = ""  -- first_error_query
t:parse(buf, jit)

local function query(criteria, data)
    local s, r = pcall(function()
        local row = t:query(criteria)
        t:verify(row, data)
        t:retrieve(row, data)
        return tostring(mergesort(t[1], { row = row }, criteria, data))
    end)

    t[2] = t[2] + 1
    print(string.format('[%d]', t[2]), r)

    if not s and t[3] == 0 then
        t[3] = t[2]
        t[4] = tostring(mergesort(t[1], criteria))
    end
end


-- Query [1]
query({ Grade=2; Subject="Math"; Score=80; }, { "Class", "Average", "Weight", })

-- Query [2] (Query [1] With Validation)
query({ Grade=2; Subject="Math"; Score=80; }, { Class="B"; Average=80; Weight=0.894; })

-- Query [3]
query({ Grade=1; Subject="Math"; Score=99; }, { "Weight" })

-- Query [4] (Query [3] With Validation)
query({ Grade=1; Subject="Math"; Score=99; }, { Weight=0.6; })

-- Query [5]
query({ Grade=2; Subject="Art"; Score=100; }, { Class="A"; Average=95; "Weight", })

-- Query [6]
query({ Grade=2; Subject="Art"; Score=50; }, { Class="C"; "Average", "Weight", })




print("t[#]", #t)
for i,v in ipairs(t) do
    print(string.format("t[%d]", i), v)
end

if t[3] ~= 0 then
    io.stderr:write(string.format("Error[%d]: %s", t[3], t[4]))
    os.exit(t[3])
end

