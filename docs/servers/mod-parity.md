# Keep server and clients aligned

The server decides gameplay state. Clients need the assets and supported
presentation paths that let them see and use that state. RuneSchema reports
what it loaded; it is not a PAK download or automatic mod synchronization
service.

## Build a simple mod list

For each gameplay mod, record:

| Check | Server | Each participating client |
| --- | --- | --- |
| Same mod version and cooked PAKs | Required for authoritative content | Required for content the player uses or sees |
| Same enabled RuneSchema folder and dependent add-ons | Usually required | Usually required |
| Registry Bridge, if the mod declares it | Enabled | Enabled where the mod needs it |
| RSNetworking, if the mod declares it | Enabled | Enabled |
| Client-only meshes, icons, fonts, audio, and widgets | Usually not used by a headless server | Required to display or play them |
| Visual `logicmods` ModActor | Not started on a dedicated server | Started where the mod requires client presentation |

Use the mod author's requirements for exceptions. A visual-only mod may be
client-only; an item, recipe, quest, enemy, vendor, or combat mod generally
needs matching gameplay data on the server and clients.

## Do not confuse the four checks

1. **Enabled:** the folder has `1` in `runeschema.txt`.
2. **Mounted:** the PAK package was made available to the game.
3. **Registered:** the relevant live item, recipe, building, attack, spell, or
   other registry accepted the content.
4. **Working:** players can perform the action and all sides display the
   result correctly.

A mismatch warning is a diagnosis, not a transfer operation. If a client is
missing content, install the matching mod on that client; do not expect
RuneSchema to copy it from the server. If a client has extra unrelated mods,
those should not be treated as server-owned gameplay content.

## When updating a live server

1. Tell players which mod versions and bridge settings will change.
2. Stop the server; back up its world saves and config.
3. Update the server and client content as one planned change.
4. Start the server and read the registration summaries.
5. Join with one matching client and test the changed content.
6. Only then reopen the server to the rest of the group.

For cues such as custom sound or animation, the server can send a supported
semantic event, but the client still needs the local asset and code that
plays it. [Presentation transport](../PRESENTATION-TRANSPORT.md) explains
that division.
