command = {
    name        = "delay_lua",
    description = "Ping the bot and show network delay"
}

function format_date(ts)
    ts = ts or os.time()                 -- fall back to 'now'
    return os.date("%Y/%m/%d %H:%M:%S", ts)
end

function run()
    -- time difference between message send and bot receive
    local diff = now() - message.date

    -- first reply
    local t0   = now_ms()
    local sent = reply(
        string.format("Request at: %s\nReceived at: %s\nDifference: %.3f s",
                      format_date(message.date), format_date(now()), diff)
    )
    -- update the message once we know how long the send itself took
    local took = now_ms() - t0
    edit(sent, string.format(
        "Request at: %s\nReceived at: %s\nDifference: %.3f s\nReply latency: %.3f ms",
        format_date(message.date), format_date(now()), diff, took))
end
