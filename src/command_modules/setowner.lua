-- Define metadata
command = {
    name = "setowner_lua",
    description = "Sets the bot owner if not already set",
    permissive = true,
    hide_description = true
}

-- Main entry function
function run()
    if db.get("owner_id") == nil then
        db.set("owner_id", user.id)
        reply("You are now the owner.")
    else
        reply("Owner already set.")
    end
end
