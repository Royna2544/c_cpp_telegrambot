command = {
    name        = "id",
    description = "Get a Telegram user's numeric ID"
}

function run()
    -- Reply to a message to get that user's ID; otherwise get your own.
    local target_id = message.has_reply and message.reply_user_id or message.user_id
    reply(string.format("%d", target_id))
end
