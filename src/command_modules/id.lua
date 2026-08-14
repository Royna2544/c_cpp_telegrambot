command = {
    name        = "id",
    description = "Get a Telegram user's numeric ID"
}

function run()
    -- Reply to a message to get that user's ID; otherwise get your own.
    local target_id = message.reply_has_sender and message.reply_user_id or message.user_id
    if target_id == nil then
        reply("This message has no Telegram sender ID.")
        return
    end
    reply(string.format("%d", target_id))
end
