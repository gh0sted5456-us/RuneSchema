#pragma once
#include <nlohmann/json.hpp>
#include "Generator/LoaderCapabilities.h"
#include "Loader/HumanNpc.h"
namespace PS::JsonSchemaGenerator {
// Structural authoring contracts; reflected asset fields remain open-ended.
inline nlohmann::json LoaderSchemas() {
    using nlohmann::json;
    const json object={{"type","object"}};
    const json text={{"type","string"}};
    const json textList={{"type","array"},{"items",text}};
    const json record={{"type","object"},{"additionalProperties",true}};
    const json patch={{"type","object"},{"required",{"$Patch","$Target"}},
        {"properties",{{"$Patch",text},{"$Target",object}}}};
    const auto map=[&](const json& value) {return json{{"type","object"},
        {"patternProperties",{{"^[^$]",value}}},{"additionalProperties",true}};};
    const auto many=[&](const json& item) {return json{{"anyOf",{item,{{"type","array"},{"items",item}}}}};};
    const json number={{"type","number"}};
    const json placementRotator={{"type","object"},{"additionalProperties",false},{"required",{"Pitch","Yaw","Roll"}},
        {"properties",{{"Pitch",number},{"Yaw",number},{"Roll",number}}},
        {"description","Unreal rotator in degrees. Use the named Pitch, Yaw and Roll fields."}};
    json result=json::object();
    for(const auto* name:{"assets","journal","lore"}) result[name]=map(record);
    const json cookedDeclaration={{"type","object"},{"additionalProperties",false},{"required",{"Kind","Path","PersistenceID"}},
        {"properties",{
            {"Kind",{{"enum",{"Item","Recipe"}},{"description","Persistent native registry kind for /assets."}}},
            {"Path",{{"type","string"},{"pattern","^/[^\\r\\n\\t]+\\.[^\\r\\n\\t]+$"},{"description","Exact cooked object path shipped by the mod PAK."}}},
            {"PersistenceID",{{"type","string"},{"pattern","^[A-Za-z0-9_-]{21}[AQgw]$"},{"description","Canonical PersistenceID baked into the cooked asset. RuneSchema verifies it against the loaded object."}}},
            {"InternalName",{{"type","string"},{"minLength",1},{"maxLength",1024},{"description","Optional cooked InternalName assertion. When omitted, RuneSchema reads it from the asset."}}}
        }}};
    const auto declarationValue=[&](const std::string& kind) {
        auto value=cookedDeclaration;
        value["required"]={"Path","PersistenceID"};
        value["properties"]["Kind"]={{"const",kind},{"description","Optional assertion; the containing loader determines this kind."}};
        return json{{"oneOf",{value,json{{"type","array"},{"minItems",1},{"maxItems",4096},{"items",value}}}},
            {"description","Ownership declaration for PAK-cooked persistent content. RuneSchema verifies the live asset identity before recording it."}};
    };
    const auto declarationDocument=[&](const std::string& kind) {
        return json{{"type","object"},{"required",{"$declaration"}},{"additionalProperties",false},
            {"properties",{{"$declaration",declarationValue(kind)}}}};
    };
    for (const auto* name : {"journal", "lore"}) {
        auto& entry = result[name]["patternProperties"]["^[^$]"];
        auto& fields = entry["properties"];
        fields["Type"]={{"enum",{"Lore","People","Place","Treasure","World","Recipe"}},{"default","Lore"}};
        const json localizedText={{"type",{"string","object"}}};
        fields["DisplayName"]=localizedText;
        fields["Image"]={{"type",{"string","object","null"}},{"description","Cooked image reference using the native Image field. The asset must load and match the live property class."}};
        fields["PageDescriptions"]={{"type","array"},{"items",{{"type","object"},{"properties",{{"Description",localizedText}}},{"additionalProperties",true}}}};
        fields["Unlock"]={{"type","boolean"},{"default",true},{"description","Unlock the journal entry, not its crafting recipe. False does not revoke previous discovery."}};
        fields["RecipeData"]={{"type",{"string","object"}},{"description","Required for Recipe entries; full asset path or unique loaded recipe name."}};
        fields["ItemData"]={{"type",{"string","object"}},{"description","Recipe output item. Inferred only when ItemsCreated has one distinct item."}};
        fields["StationTableRowHandle"]={{"type","object"},{"required",{"DataTable","RowName"}},{"properties",{{"DataTable",text},{"RowName",text}}},{"additionalProperties",false}};
        const json group={{"type","object"},{"required",{"Id"}},{"additionalProperties",false},
            {"properties",{{"Id",text},{"DisplayName",text},{"CreateIfMissing",{{"type","boolean"},{"default",false}}}}}};
        fields["AddTo"]={{"type","object"},{"additionalProperties",false},
            {"oneOf",{json{{"required",{"SubCategory"}},{"not",{{"anyOf",{json{{"required",{"Section"}}},json{{"required",{"Category"}}}}}}}},
                json{{"required",{"Section","Category"}},{"not",{{"required",{"SubCategory"}}}}}}},
            {"properties",{{"SubCategory",{{"type","string"},{"minLength",1},{"description","Full cooked JournalSubCategoryData path or an unambiguous loaded category name."}}},
                {"Section",{{"type","string"},{"minLength",1},{"description","Friendly native section name, used with Category."}}},
                {"Category",{{"type","string"},{"minLength",1},{"description","Friendly native category name, used with Section."}}},
                {"Key",text},{"Group",group}}},
            {"description","Place into any loaded native subcategory. For categories that support groups, Group.CreateIfMissing creates a mod-owned visible category within that native section."}};
    }
    result["lore"]["patternProperties"]["^[^$]"]["properties"]["Type"]={{"const","Lore"},{"default","Lore"}};
    result["journal"]["properties"]["$declaration"]=declarationValue("Journal");
    result["lore"]["properties"]["$declaration"]=declarationValue("Lore");
    json recipe=record;
    const json recipePlacement={{"type","object"},{"additionalProperties",false},{"required",{"Row"}},
        {"allOf",{
            json{{"oneOf",{
                json{{"required",{"Table"}},{"not",{{"required",{"DataTable"}}}}},
                json{{"required",{"DataTable"}},{"not",{{"required",{"Table"}}}}}
            }}},
            json{{"oneOf",{
                json{{"required",{"Array"}},{"not",{{"required",{"Category"}}}}},
                json{{"required",{"Category"}},{"not",{{"required",{"Array"}}}}}
            }}}
        }},
        {"properties",{
            {"Table",{{"type","string"},{"minLength",1},{"description","Legacy DataTable object-name lookup. Retained for vanilla tables and existing definitions; use DataTable for a custom cooked table."}}},
            {"DataTable",{{"type","string"},{"pattern","^/[^\\r\\n\\t]+\\.[^\\r\\n\\t]+$"},{"description","Exact cooked DataTable object path, for example /Game/MyMod/Data/DT_MyStation.DT_MyStation. The asset is loaded on demand and also matched when serialized later."}}},
            {"Row",{{"type","string"},{"minLength",1}}},
            {"Array",{{"type","string"},{"minLength",1},{"description","Recipe UObject array field on the selected row, used by crafting and processing stations."}}},
            {"Category",{{"type","string"},{"minLength",1},{"description","Merchant LabeledRecipes category label."}}},
            {"Replaces",{{"type","string"},{"minLength",1},{"description","Optional existing recipe name to replace in Array instead of appending."}}}
        }}};
    recipe["properties"]["AddTo"]={{"type","array"},{"minItems",1},{"maxItems",128},{"items",recipePlacement},{"description","Place this recipe into station rows or merchant categories. Use exactly one of Table (legacy short name) or DataTable (exact cooked asset path)."}};
    recipe["properties"]["Unlock"]={{"type","boolean"},{"default",false},{"description","Opt into automatic grants. False does not revoke learned recipes; AddTo alone never grants them."}};
    recipe["properties"]["VendorID"]={{"type","string"},{"minLength",1},{"description","Contribute a store offer using a local store Id or Mod:Id. Requires Item, Currency and Price; permits Count and Category. Cannot be combined with ordinary recipe properties or Unlock. Restart required."}};
    recipe["properties"]["Order"]={{"type","integer"},{"minimum",0},{"maximum",1000000},{"description","Optional sort position within the offer Category for RuneSchema and vanilla vendors. Lower numbers appear first; equal/missing positions retain source order."}};
    recipe["properties"]["RuneSchemaVendors"]={{"type","array"},{"maxItems",128},{"items",{{"type","string"},{"minLength",1}}},{"description","Local store IDs or Mod:Id. Duplicate references collapse. Can be combined with VanillaVendors and legacy VendorID. Restart required."}};
    recipe["properties"]["VanillaVendors"]={{"type","array"},{"maxItems",128},{"items",{{"type","object"},{"required",{"Table","Row"}},{"additionalProperties",false},{"properties",{{"Table",{{"type","string"},{"minLength",1}}},{"Row",{{"type","string"},{"minLength",1}}}}}}},{"description","Native merchant Table object name and Row. Appends to the offer Category, never replaces native offers. Duplicate targets collapse."}};
    result["recipes"]=map(recipe);
    result["blueprints"]=many(map(record));
    const json buildingPlacement={{"type","object"},{"additionalProperties",false},{"required",{"Collection"}},
        {"properties",{{"Collection",{{"type","string"},{"minLength",1}}},{"PageIndex",{{"type","integer"},{"minimum",0}}}}}};
    const json buildingRequirement={{"type","object"},{"additionalProperties",false},{"required",{"ItemData","Amount"}},
        {"properties",{{"ItemData",{{"type","string"},{"minLength",1}}},{"Amount",{{"type","integer"},{"minimum",1}}}}}};
    const json buildingDefinition={{"type","object"},{"additionalProperties",false},
        {"oneOf",{json{{"required",{"$Clone"}},{"not",{{"required",{"Asset"}}}}},
                   json{{"required",{"Asset"}},{"not",{{"required",{"$Clone"}}}}}}},
        {"properties",{{"$Clone",{{"type","string"},{"pattern","^/"},{"description","Clone a cooked BuildingPieceData asset. The clone receives a new RuneSchema identity and remains a separate build-menu entry."}}},
            {"Asset",{{"type","string"},{"pattern","^/"},{"description","Register an existing BuildingPieceData asset; use $Clone when changing the actor or cost."}}},
            {"Properties",{{"type","object"},{"additionalProperties",true},{"description","Reflected BuildingPieceData overrides. PersistenceID, InternalName, BuildingPieceDataIndex and Requirements are managed and rejected here. A BuildableActor override must be a cooked BP_BaseBuilding_BaseActor child."}}},
            {"Requirements",{{"type","array"},{"maxItems",64},{"items",buildingRequirement},{"description","Complete replacement build cost. Every item must resolve before the clone is registered."}}},
            {"Unlock",{{"type","boolean"},{"default",true},{"description","Session-unlock the separate cloned entry. False does not revoke an unlock already persisted by the game."}}},
            {"AddTo",{{"oneOf",{buildingPlacement,json{{"type","array"},{"minItems",1},{"maxItems",32},{"items",buildingPlacement}}}},
                {"description","Optional explicit placement(s). When omitted, RuneSchema appends the clone to every page/collection containing its $Clone source."}}}}}};
    result["buildings"]=many({{"anyOf",{map(buildingDefinition),patch,declarationDocument("Building")}}});
    result["courses"]=many(record);
    result["spawns"]={{"type","array"},{"items",record}};
    const json groundZ={{"anyOf",{json{{"type","number"}},json{{"type","string"},{"pattern","^\\$([+-][0-9]+(?:\\.[0-9]+)?)?$"}}}}};
    const json groundLocation={{"oneOf",{
        json{{"type","array"},{"minItems",3},{"maxItems",3},{"items",json::array({number,number,groundZ})},{"additionalItems",false}},
        json{{"type","object"},{"additionalProperties",false},{"required",{"X","Y","Z"}},{"properties",{{"X",number},{"Y",number},{"Z",groundZ}}}}
    }},{"description","World coordinates as [X,Y,Z] or {X,Y,Z}. Z accepts $, $+offset, or $-offset and resolves to blocking ground."}};
    result["spawns"]["items"]["properties"]["Location"]=groundLocation;
    result["spawns"]["items"]["properties"]["Rotation"]=placementRotator;
    result["spawns"]["items"]["properties"]["Grid"]={{"type","object"},{"additionalProperties",false},{"required",{"Rows","Columns","SpacingMeters"}},
        {"properties",{{"Rows",{{"type","integer"},{"minimum",1},{"maximum",20}}},{"Columns",{{"type","integer"},{"minimum",1},{"maximum",20}}},{"SpacingMeters",{{"type","number"},{"minimum",0.1},{"maximum",1000}}}}}};
    result["spawns"]["items"]["properties"]["AdditionalDrops"]={{"type","array"},{"maxItems",16},{"items",{{"type","object"},{"additionalProperties",false},
        {"required",{"Item","Min","Max","ChancePercent"}},{"properties",{{"Item",{{"type","string"},{"pattern","^/[^\\r\\n\\t]+\\.[^\\r\\n\\t]+$"}}},
        {"Min",{{"type","integer"},{"minimum",1},{"maximum",10000}}},{"Max",{{"type","integer"},{"minimum",1},{"maximum",10000}}},{"ChancePercent",{{"type","number"},{"minimum",0},{"maximum",100}}}}}}}};
    result["spawns"]["items"]["properties"]["PowerLevel"]={{"type","integer"},{"minimum",1},{"maximum",100},{"description","Native Dominion AI spawn power level. Supported on AISpawnPoint entries and validated for every authored ID."}};
    result["spawns"]["items"]["properties"]["SpawnRadiusMeters"]={{"type","number"},{"minimum",0.01},{"maximum",10000},{"description","Ordinary AISpawnPoint native proximity eligibility radius in metres. Minimum distance becomes zero; external activation is disabled. Cannot combine with MinSpawnDistance, MaxSpawnDistance or raw radius/activation Properties. Native streaming, navigation, population and respawn rules still apply; not a guaranteed one-shot ambush trigger."}};
    result["spawns"]["items"]["properties"]["GroundToSurface"]={{"type","boolean"},{"default",true},{"description","Trace the authored X/Y placement onto blocking ground before spawning. Disable only for intentionally airborne actors."}};
    result["spawns"]["items"]["properties"]["GroundOffset"]={{"type","number"},{"minimum",-100000},{"maximum",100000},{"default",0},{"description","Centimetre offset above or below the resolved ground. Do not combine with the Location.Z $+offset shorthand."}};
    result["spawns"]["items"]["properties"]["TimeOfDay"]={{"enum",{"Any","Day","Night"}},{"default","Any"},{"description","Authority-controlled lifecycle gate for AI spawn points, resource actors, and functional BuildingProp entries. Timed actors are transient and cannot use native respawn persistence."}};
    result["spawns"]["items"]["properties"]["QuestCompleted"]={{"type","string"},{"minLength",1},{"maxLength",512},{"description","Activate this placement after a local Id or Mod:Id quest is completed by a connected participant. Combines with TimeOfDay when both are present."}};
    result["spawns"]["items"]["properties"]["PersistAfterCondition"]={{"type","boolean"},{"default",true},{"description","Latch a completed quest condition so an activated placement remains available. Set false for a placement that should exist only while the completion state is observable."}};
    result["spawns"]["items"]["properties"]["DuplicateRadius"]={{"type","number"},{"minimum",0},{"maximum",10000},{"default",100},{"description","For quest-activated Actor and BuildingProp entries, treat the same actor class within this many centimetres as already satisfied. Zero disables the coordinate safeguard."}};
    result["spawns"]["items"]["dependencies"]["PersistAfterCondition"]={{"required",{"QuestCompleted"}}};
    result["spawns"]["items"]["dependencies"]["DuplicateRadius"]={{"required",{"QuestCompleted"}}};
    result["spawns"]["items"]["dependencies"]["SpawnRadiusMeters"]={{"required",{"Type"}},{"properties",{{"Type",{{"const","AISpawnPoint"}}},{"RequiresActivation",{{"const",false}}}}},{"not",{{"anyOf",{json{{"required",{"MinSpawnDistance"}}},json{{"required",{"MaxSpawnDistance"}}},json{{"required",{"EventOnly"}}}}}}}};
    json vendor=record;
    vendor["required"]={"Location"};
    vendor["anyOf"]=json::array();
    for(const auto* field:{"Actor","VisualSource","Mesh","VisualMesh","SkeletalMesh"})
        vendor["anyOf"].push_back(json{{"required",{field}}});
    vendor["properties"]["Id"]=text;
    vendor["properties"]["Stage"]={{"enum",{"Visual","Interaction","Merchant"}},{"default","Visual"}};
    vendor["properties"]["Name"]=text;
    vendor["properties"]["DisplayName"]=text;
    vendor["properties"]["MerchantName"]=text;
    vendor["properties"]["VendorHeaderImage"]={{"anyOf",{
        json{{"const",""}},
        json{{"type","string"},{"minLength",2},{"maxLength",1024},{"pattern","^/"}}
    }},{"default","/Game/Art/UI/Craft/T_DeathShop_Banner.T_DeathShop_Banner"},{"description","Optional cooked banner path. Omit or leave blank to use the vanilla Death banner. RuneSchema custom banners mount below /Game/Mods/RuneSchema/UI/Icons/Banners/. Requires Items; restart after changes."}};
    vendor["properties"]["Repairable"]={{"type","boolean"},{"default",false},{"description","Expose the native Repair tab in the merchant/crafting overlay. When false or omitted, Repair is excluded and E cannot navigate to it."}};
    vendor["properties"]["Masterworkable"]={{"type","boolean"},{"default",false},{"description","Expose the native Ascend/Masterwork tab and its vanilla normal/highlight icons."}};
    vendor["properties"]["CategoryRules"]={{"type","array"},{"maxItems",64},{"uniqueItems",true},
        {"items",{{"type","object"},{"additionalProperties",false},{"required",{"Category"}},
            {"properties",{{"Category",{{"type","string"},{"minLength",1},{"maxLength",128}}},
                {"MinPowerLevel",{{"type","integer"},{"minimum",0},{"maximum",100}}},
                {"MaxPowerLevel",{{"type","integer"},{"minimum",0},{"maximum",100}}},
                {"TimeOfDay",{{"enum",{"Any","Day","Night"}}}},
                {"QuestCompleted",text}}}}},
        {"description","Per-player category visibility. Offers in a matching Category appear only when native power level, time, and completed-quest gates pass. Unlisted categories remain visible."}};
    vendor["properties"]["VendorName"]=text;
    vendor["properties"]["Actor"]=text;
    vendor["properties"]["VisualSource"]=text;
    vendor["properties"]["BaseActor"]=text;
    vendor["properties"]["BaseActorClass"]=text;
    vendor["properties"]["Mesh"]=text;
    vendor["properties"]["VisualMesh"]=text;
    vendor["properties"]["SkeletalMesh"]=text;
    vendor["properties"]["IdleAnimation"]=text;
    vendor["properties"]["IdleAnim"]=text;
    vendor["properties"]["Materials"]=textList;
    vendor["properties"]["DataTable"]=text;
    vendor["properties"]["RowName"]=text;
    vendor["properties"]["Row"]=text;
    vendor["properties"]["Location"]=groundLocation;
    vendor["properties"]["Rotation"]=placementRotator;
    const json positiveNumber={{"type","number"},{"exclusiveMinimum",0}};
    vendor["properties"]["Scale"]={{"anyOf",{positiveNumber,
        json{{"type","array"},{"minItems",3},{"maxItems",3},{"items",positiveNumber}}}}};
    vendor["properties"]["EnableCollision"]={{"type","boolean"}};
    vendor["properties"]["ItemsProperty"]=text;
    vendor["properties"]["Items"]={{"type","array"},{"items",record}};
    vendor["properties"]["Items"]["items"]["properties"]["Order"]={{"type","integer"},{"minimum",0},{"maximum",1000000},{"description","Optional sort position within this offer Category. Lower numbers appear first; equal/missing positions retain source order."}};
    for(const auto& [field,schema]:json{{"MinPowerLevel",{{"type","integer"},{"minimum",0},{"maximum",100}}},
        {"MaxPowerLevel",{{"type","integer"},{"minimum",0},{"maximum",100}}},{"TimeOfDay",{{"enum",{"Any","Day","Night"}}}},{"QuestCompleted",text}}.items())
        vendor["properties"]["Items"]["items"]["properties"][field]=schema;
    vendor["properties"]["InteractionComponentClass"]=text;
    vendor["properties"]["VendorComponentClass"]=text;
    vendor["properties"]["InteractionProperties"]=record;
    vendor["properties"]["VendorProperties"]=record;
    const json vendorCollection={{"anyOf",{vendor,json{{"type","array"},{"items",vendor}},map(vendor)}}};
    result["vendors"]={{"anyOf",{vendorCollection,json{{"type","object"},{"required",{"Vendors"}},
        {"properties",{{"Vendors",vendorCollection}}}}}}};
    json npc={{"type","object"},{"additionalProperties",false},{"required",{"Id","Location"}},
        {"anyOf",{json{{"required",{"Mesh"}}},json{{"required",{"VisualSource"}}},json{{"required",{"Type","Appearance"}},{"properties",{{"Type",{{"const","Human"}}}}}}}},
        {"properties",json::object()}};
    for(const auto* field:{"Id","DisplayName","VisualSource","Mesh","Materials","IdleAnimation","Location","Rotation","Scale","EnableCollision"})
        npc["properties"][field]=vendor["properties"][field];
    npc["properties"]["Enabled"]={{"type","boolean"}};
    npc["properties"]["VendorID"]={{"type","string"},{"minLength",1},{"pattern","^[^:]+(:[^:]+)?$"},{"description","Direct Trade interaction: local store Id or Mod:Id. Omit for a visual-only NPC."}};
    npc["properties"]["Multiplayer"]={{"type","boolean"},{"default",true},{"description","Enabled by default. Set false only for an explicitly local NPC. Matching cooked assets and RuneSchema definitions must be installed on server and clients."}};
    npc["properties"]["MeshCollision"]={{"enum",{"Native","Pawn","None"}},{"description","AI Pawn uses the selected mesh's reinitialized PhysicsAsset and removes capsule pawn blocking; missing physics fails setup. AI Native preserves mesh defaults and capsule blocking; None disables mesh collision but retains capsule blocking. Humans use a body capsule, not equipment collision. Resources use authored static-mesh query collision for Native/Pawn and no capsule pawn blocking. EnableCollision=false disables all actor collision."}};
    npc["properties"]["DialogueID"]={{"type","string"},{"minLength",1},{"pattern","^[^:]+(:[^:]+)?$"},{"description","Native Talk interaction; local dialogue Id or Mod:Id. With VendorID, dialogue choices may open that bound store after closing the conversation."}};
    npc["properties"]["LoreID"]={{"type","string"},{"minLength",1},{"pattern","^[^:]+(:[^:]+)?$"},{"description","Preferred /lore reference: local lore Id or Mod:Id. Creates the direct Examine interaction. Do not combine with LoreEntry, DialogueID or VendorID."}};
    npc["properties"]["LoreEntry"]={{"type","string"},{"minLength",1},{"maxLength",1024},{"description","Direct Examine interaction: /lore entry ID or cooked lore asset path. Do not combine with DialogueID or VendorID."}};
    npc["properties"]["QuestID"]={{"type","string"},{"minLength",1},{"pattern","^[^:]+(:[^:]+)?$"},{"description","Associated /quests definition: local quest Id or Mod:Id. This records the NPC/quest relationship; dialogue choices still control acquisition and completion."}};
    npc["properties"]["Type"]={{"enum",{"AI","Human","Resource","Prop"}},{"default","AI"},{"description","Resource and Prop use an explicit StaticMesh on a neutral NPC, without harvesting, damage, drops or building behavior."}};
    json appearance={{"type","object"},{"additionalProperties",false},{"required",DragonWilds::HumanNpc::AppearanceKeys}};
    const json appearanceDescriptions={
        {"BodyType","Row name from DT_Customization_BodyType."},
        {"Head","Row name from DT_Customization_FaceType (the native FaceDataHandle)."},
        {"HairPreset","Row name from DT_Customization_HairPresets."},
        {"FacialHairPreset","Row name from DT_Customization_FacialHairPresets."},
        {"HairColor","Row name from DT_Customization_HairColor."},
        {"SkinTone","Row name from DT_Customization_SkinTone."},
        {"EyeColor","Row name from DT_Customization_EyeColor."},
        {"EyebrowColor","Row name from DT_Customization_EyebrowColor."}};
    for(const auto* key:DragonWilds::HumanNpc::AppearanceKeys)appearance["properties"][key]={{"type","string"},{"minLength",1},{"maxLength",128},{"description",appearanceDescriptions.at(key)}};
    npc["properties"]["Appearance"]=appearance;
    json equipment={{"type","object"},{"additionalProperties",false}};
    const json equipmentDescriptions={
        {"Head","WearableEquipmentData whose native Slot is Head."},
        {"Body","WearableEquipmentData whose native Slot is Body."},
        {"Legs","WearableEquipmentData whose native Slot is Legs."},
        {"Cape","WearableEquipmentData whose native Slot is Cape."},
        {"Trinket","WearableEquipmentData whose native Slot is Trinket."},
        {"MainHand","HeldEquipmentData with native Slot HeldOnlyRight or HeldTwoHanded."},
        {"OffHand","HeldEquipmentData with native Slot HeldOnlyLeft. Cannot accompany a two-handed MainHand."}};
    const json equipmentReference={{"oneOf",{
        json{{"type","string"},{"pattern","^/[^\\r\\n\\t]+$"},{"maxLength",1024}},
        json{{"type","string"},{"pattern","^[A-Za-z0-9_-]{21}[AQgw]$"}}}}};
    for(const auto* key:DragonWilds::HumanNpc::EquipmentKeys) {
        equipment["properties"][key]=equipmentReference;
        equipment["properties"][key]["description"]=equipmentDescriptions.at(key);
    }
    npc["properties"]["Equipment"]=equipment;
    npc["properties"]["HideWeapon"]={{"type","boolean"},{"default",false},{"description","Human only. Keeps MainHand and OffHand equipment data for pose selection but hides the rendered held weapon visuals."}};
    json poseNames=json::array(),heldPoseNames=json::array();
    for(const auto& preset:DragonWilds::HumanPose::Presets) {
        poseNames.push_back(preset.Name);
        if(preset.Mode==DragonWilds::HumanPose::Playback::Hold)heldPoseNames.push_back(preset.Name);
    }
    npc["properties"]["Pose"]={{"type","object"},{"oneOf",{json{{"required",{"Preset"}},{"not",{{"anyOf",{json{{"required",{"Asset"}}},json{{"required",{"Playback"}}}}}}}},json{{"required",{"Asset"}},{"not",{{"required",{"Preset"}}}}}}},{"additionalProperties",false},
        {"properties",{{"Preset",{{"enum",poseNames}}},{"Asset",{{"type","string"},{"pattern","^/"},{"minLength",2},{"maxLength",1024}}},{"Playback",{{"enum",{"Hold","Loop","Once"}}}},{"WeaponVisibility",{{"enum",{"Auto","Show","Hide"}}}},{"Time",{{"type","number"},{"minimum",0},{"maximum",60}}}}},
        {"if",{{"required",{"Time"}}}},{"then",{{"anyOf",{json{{"required",{"Preset"}},{"properties",{{"Preset",{{"enum",heldPoseNames}}}}}},json{{"required",{"Asset","Playback"}},{"properties",{{"Playback",{{"const","Hold"}}}}}}}}}},
        {"description","Human only. Equipment preserves the held item's pose, or uses Relaxed when empty-handed. Time samples held poses in seconds; must be below the loaded clip duration. IdleAnimation and Pose are mutually exclusive. Full-body idles/emotes do not preserve every weapon grip."}};
    npc["properties"]["DialoguePose"]=npc["properties"]["Pose"];
    npc["properties"]["DialoguePose"]["description"]="Human only. Optional concrete pose applied while dialogue is active; the authored Pose is restored when dialogue closes. Equipment is not supported here.";
    npc["properties"]["Ghost"]={{"type","object"},{"additionalProperties",false},{"properties",{{"Character",{{"type","boolean"}}},{"Equipment",{{"type","boolean"}}},{"Weapons",{{"type","boolean"}}}}}};
    json glowColor={{"type","object"},{"additionalProperties",false},{"required",{"R","G","B","A"}}};
    for(const auto* channel:{"R","G","B","A"})glowColor["properties"][channel]={{"type","number"},{"minimum",0},{"maximum",std::string(channel)=="A"?1:64}};
    npc["properties"]["VisualEffect"]={{"type","object"},{"additionalProperties",false},{"required",{"Type"}},
        {"properties",{{"Type",{{"const","Ghost Glow"}}},{"MainColor",glowColor},{"SecondaryColor",glowColor},
            {"Overlay",{{"type","boolean"},{"default",true}}},{"BodyMaterial",{{"type","boolean"},{"default",false}}}}},
        {"description","All NPC visual types. Applies to meshes, including human equipment/weapons, not nameplate widgets. Enable Overlay or BodyMaterial."}};
    const auto ghostVisual=npc["properties"]["VisualEffect"];
    const json boundedNumber={{"type","number"},{"minimum",-100000},{"maximum",100000}};
    json offset={{"type","object"},{"additionalProperties",false},{"required",{"X","Y","Z"}}};
    for(const auto* key:{"X","Y","Z"})offset["properties"][key]=boundedNumber;
    json parameterColor={{"type","object"},{"additionalProperties",false},{"required",{"R","G","B","A"}}};
    for(const auto* key:{"R","G","B","A"})parameterColor["properties"][key]=boundedNumber;
    const json parameter={{"anyOf",{json{{"type","boolean"}},boundedNumber,offset,parameterColor}}};
    const json npcNiagara={{"type","object"},{"additionalProperties",false},{"required",{"Type","System"}},
        {"properties",{{"Type",{{"const","Niagara"}}},{"System",{{"type","string"},{"minLength",3},{"maxLength",1024},{"pattern","^/[^\\r\\n\\t]+\\.[^\\r\\n\\t]+$"}}},
            {"LocationOffset",offset},{"Parameters",{{"type","object"},{"maxProperties",32},{"propertyNames",{{"maxLength",128},{"pattern","^User\\.[^\\r\\n\\t]*$"}}},{"additionalProperties",parameter}}}}}};
    npc["properties"]["VisualEffect"]={{"anyOf",{ghostVisual,npcNiagara}}};
    npc["properties"]["HideMesh"]={{"type","boolean"},{"description","Prop only: hide visual mesh while retaining collision."}};
    npc["properties"]["NoInteract"]={{"type","boolean"},{"default",false},{"description","Disable interaction and suppress VendorID, DialogueID, LoreID and LoreEntry bindings without changing collision."}};
    npc["properties"]["HideName"]={{"type","boolean"},{"default",false},{"description","Hide the in-world NPC name independently of interaction. Map.ShowName controls map labels."}};
    npc["properties"]["TimeOfDay"]={{"type","string"},{"enum",{"Any","Day","Night"}},{"default","Any"},{"description","Native lifecycle gate. Day/Night NPCs exist only during that phase; authority removes and restores them at the game's dawn/dusk transition while respecting world-cell loading."}};
    json marker={{"type","object"},{"additionalProperties",false},{"properties",{{"Enabled",{{"type","boolean"},{"default",false}}},{"Icon",{{"type","string"},{"pattern","^/"},{"minLength",2},{"maxLength",1024}}}}}};
    marker["properties"]["Size"]={{"type","number"},{"minimum",1},{"maximum",4096}};
    npc["properties"]["Map"]=marker;
    npc["properties"]["Map"]["properties"]["SizeMode"]={{"type","string"},{"enum",{"Pixels","WorldUnits"}},{"default","Pixels"}};
    npc["properties"]["Map"]["properties"]["ShowName"]={{"type","boolean"},{"default",true},{"description","Show the NPC name on its map icon. False keeps the icon without a label; does not change in-world names."}};
    marker["properties"]["Height"]={{"type","number"},{"minimum",0},{"maximum",2000},{"default",220}};
    marker["properties"]["Scale"]={{"type","number"},{"minimum",0.01},{"maximum",5},{"default",0.35}};
    marker["properties"]["Distance"]={{"type","number"},{"minimum",100},{"maximum",100000},{"default",5000}};
    marker["properties"]["SizeMode"]={{"type","string"},{"enum",{"Scale","Pixels"}},{"default","Scale"}};
    npc["properties"]["OverheadIcon"]=marker;
    npc["allOf"]=json::array({json{
        {"if",{{"required",{"Type"}},{"properties",{{"Type",{{"const","Human"}}}}}}},
        {"then",{{"required",{"Appearance"}},{"not",{{"anyOf",{json{{"required",{"Mesh"}}},json{{"required",{"VisualSource"}}},json{{"required",{"Materials"}}}}}}}}},
        {"else",{{"not",{{"anyOf",{json{{"required",{"Appearance"}}},json{{"required",{"Equipment"}}},json{{"required",{"Pose"}}},json{{"required",{"DialoguePose"}}},json{{"required",{"Ghost"}}}}}}}}}
    }});
    npc["allOf"].push_back({{"not",{{"required",{"Pose","IdleAnimation"}}}}});
    npc["allOf"].push_back({{"not",{{"anyOf",{
        json{{"required",{"LoreID","LoreEntry"}}},json{{"required",{"LoreID","DialogueID"}}},
        json{{"required",{"LoreID","VendorID"}}},json{{"required",{"LoreEntry","DialogueID"}}},
        json{{"required",{"LoreEntry","VendorID"}}}}}}}});
    npc["allOf"].push_back({{"if",{{"required",{"Type"}},{"properties",{{"Type",{{"enum",{"Resource","Prop"}}}}}}}},
        {"then",{{"required",{"Mesh"}},{"not",{{"anyOf",{json{{"required",{"VisualSource"}}},json{{"required",{"IdleAnimation"}}}}}}}}}});
    result["npc"]={{"anyOf",{many(npc),json{{"type","object"},{"required",{"Npcs"}},{"properties",{{"Npcs",many(npc)}}}}}}};
    json store={{"type","object"},{"additionalProperties",false},{"required",{"Id","Items"}},
        {"properties",json::object()}};
    for(const auto* field:{"Id","Items","MerchantName","VendorHeaderImage","Repairable","Masterworkable","DataTable","RowName","VendorProperties","CategoryRules"})
        store["properties"][field]=vendor["properties"][field];
    store["properties"]["Enabled"]={{"type","boolean"}};
    result["vendors"]={{"anyOf",json::array({many(store),
        json{{"type","object"},{"required",{"Vendors"}},{"additionalProperties",false},{"properties",{{"Vendors",many(store)}}}}})}};
    result["raw"]=many(map(map(record)));
    json choice={{"type","object"},{"additionalProperties",false},{"required",{"Id","Text"}},
        {"properties",{{"Id",text},{"Text",{{"type","string"},{"minLength",1},{"maxLength",512}}},{"Next",text},{"End",{{"const",true}}},{"Complete",{{"const",true}}}}},
        {"dependencies",{{"Complete",{"Next"}}}},
        {"oneOf",{json{{"required",{"Next"}}},json{{"required",{"End"}}}}}};
    choice["properties"]["Quest"]={{"type","object"},{"additionalProperties",false},{"required",{"Id","Action"}},
        {"properties",{{"Id",text},{"Action",{{"enum",{"Accept","TurnIn","Abandon"}}}}}}};
    choice["properties"]["Quest"]["properties"]["EntryID"]=text;
    choice["properties"]["Quest"]["dependencies"]["EntryID"]["properties"]["Action"]={{"const","Accept"}};
    choice["allOf"]=json::array({{{"not",{{"required",{"Quest","Complete"}}}}}});
    choice["properties"]["Event"]={{"type","object"},{"additionalProperties",false},{"required",{"Id","Action"}},
        {"properties",{{"Id",text},{"Action",{{"enum",{"Start","Cancel"}}}}}}};
    choice["allOf"].push_back({{"not",{{"required",{"Event","Complete"}}}}});
    json combined={{"if",{{"required",{"Event","Quest"}}}},{"then",{{"anyOf",json::array()}}}};
    const auto paired=[](const char* questAction,const char* eventAction) {
        json rule={{"properties",json::object()}};
        rule["properties"]["Quest"]["properties"]["Action"]={{"const",questAction}};
        rule["properties"]["Event"]["properties"]["Action"]={{"const",eventAction}};
        return rule;
    };
    combined["then"]["anyOf"].push_back(paired("Accept","Start"));
    combined["then"]["anyOf"].push_back(paired("Abandon","Cancel"));
    choice["allOf"].push_back(std::move(combined));
    choice["properties"]["VendorID"]=text;
    choice["properties"]["NpcGoAway"]={{"type","string"},{"minLength",1},{"maxLength",257},
        {"description","Retire this mod NPC for the current server session after the choice executes. Use its Id or ModId:Id."}};
    const json dialogueNiagara={{"type","object"},{"additionalProperties",false},{"required",{"Type","System"}},
        {"properties",{{"Type",{{"const","Niagara"}}},{"System",{{"type","string"},{"pattern","^/.+\\..+$"}}},
            {"LocationOffset",{{"type","object"},{"additionalProperties",false},{"required",{"X","Y","Z"}},
                {"properties",{{"X",{{"type","number"}}},{"Y",{{"type","number"}}},{"Z",{{"type","number"}}}}}}},
            {"Parameters",{{"type","object"},{"maxProperties",32}}}}}};
    choice["properties"]["VisualEffect"]={{"type","object"},{"additionalProperties",false},{"required",{"Action"}},
        {"properties",{{"Action",{{"enum",{"Activate","Deactivate"}}}},{"Effect",dialogueNiagara}}},
        {"allOf",json::array({
            json{{"if",{{"properties",{{"Action",{{"const","Activate"}}}}}}},{"then",{{"required",{"Effect"}}}}},
            json{{"if",{{"properties",{{"Action",{{"const","Deactivate"}}}}}}},{"then",{{"not",{{"required",{"Effect"}}}}}}}
        })},
        {"description","Server-authoritative Niagara side effect. Activate attaches the effect after the choice action succeeds; Deactivate removes the prior dialogue effect. The revision is presented locally by every client."}};
    choice["properties"]["WhenQuest"]={{"type","object"},{"additionalProperties",false},{"required",{"Id","States"}},
        {"properties",{{"Id",{{"type","string"},{"minLength",1},{"maxLength",257}}},
            {"States",{{"type","array"},{"minItems",1},{"maxItems",3},{"uniqueItems",true},{"items",{{"enum",{"NotStarted","Active","Completed"}}}}}},
            {"Stage",{{"type","string"},{"minLength",1},{"maxLength",128}}},{"ObjectivesComplete",{{"type","boolean"}}},{"RepeatReady",{{"type","boolean"}}}}}};
    choice["properties"]["WhenTimeOfDay"]={{"enum",{"Day","Night"}},{"description","Hide this branch unless the game's native time state matches."}};
    choice["properties"]["RequirementUnlock"]={{"type","object"},{"additionalProperties",false},{"minProperties",1},
        {"properties",{{"Quest",choice["properties"]["WhenQuest"]},{"TimeOfDay",choice["properties"]["WhenTimeOfDay"]}}},
        {"description","Player-specific unlock requirements evaluated before native conversation choices are populated. Unmet or unreadable requirements omit the branch completely; they never create a blank option."}};
    choice["allOf"].push_back({{"not",{{"anyOf",{
        json{{"required",{"RequirementUnlock","WhenQuest"}}},json{{"required",{"RequirementUnlock","WhenTimeOfDay"}}}}}}}});
    choice["oneOf"].push_back(json{{"required",{"VendorID"}}});
    const json questItem={{"type","string"},{"pattern","^/"},{"minLength",1},{"maxLength",1024}};
    const json questCount={{"type","integer"},{"minimum",1},{"maximum",999}};
    const json questLocation={{"type","array"},{"minItems",3},{"maxItems",3},
        {"items",{{"type","number"},{"minimum",-100000000},{"maximum",100000000}}},
        {"description","Optional native quest objective position [X,Y,Z]. Marker lifecycle is experimental."}};
    auto eventLocation=questLocation;
    eventLocation["items"]={{"anyOf",json::array({questLocation["items"],json{{"type","string"},{"pattern","^\\$([+-][0-9]+(\\.[0-9]+)?)?$"}}})}};
    const json eventMember={{"type","object"},{"additionalProperties",false},{"required",{"SpawnID","Location"}},
        {"properties",{{"SpawnID",text},{"Location",eventLocation},
            {"GroundToSurface",{{"type","boolean"},{"default",true},{"description","Trace this event member onto blocking ground. Disable only for an intentionally airborne actor and use a numeric Z."}}},
            {"GroundOffset",{{"type","number"},{"minimum",-100000},{"maximum",100000},{"default",0},{"description","Additional centimetres above or below RuneSchema's standard 10 cm ground clearance. Do not combine with the Location.Z $+offset shorthand."}}}}}};
    const json eventArea={{"type","object"},{"additionalProperties",false},{"required",{"Location","RadiusMeters"}},
        {"properties",{{"Location",questLocation},{"RadiusMeters",{{"type","number"},{"minimum",0.01},{"maximum",10000}}},
            {"Visibility",{{"enum",{"Auto","Always","Never"}}}}}}};
    json eventMessages={{"type","object"},{"additionalProperties",false},{"properties",json::object()}};
    for(const auto* field:{"Started","WaveStarted","BossStarted","Completed","Failed","Cancelled"})
        eventMessages["properties"][field]={{"type","string"},{"maxLength",512}};
    const json healthStage={{"type","object"},{"additionalProperties",false},{"required",{"Percent","Message"}},
        {"properties",{{"Percent",{{"type","number"},{"exclusiveMinimum",0},{"exclusiveMaximum",100}}},
            {"Message",{{"type","string"},{"minLength",1},{"maxLength",512}}},{"SpawnID",text}}}};
    json eventAction={{"type","object"},{"additionalProperties",false},{"required",{"AfterSeconds","SpawnID"}},
        {"oneOf",json::array({json{{"required",{"Pose"}}},json{{"required",{"Emote"}}}})},
        {"properties",{{"AfterSeconds",{{"type","number"},{"minimum",0},{"maximum",900}}},{"SpawnID",text},
            {"Pose",npc["properties"]["Pose"]},{"Emote",npc["properties"]["Pose"]}}}};
    eventAction["properties"]["Emote"]["description"]="One-shot human NPC animation applied after the event delay.";
    result["events"]=many({{"type","object"},{"additionalProperties",false},{"required",{"Id","Waves"}},
        {"properties",{{"Id",text},{"TimeoutSeconds",{{"type","number"},{"minimum",10},{"maximum",900},{"default",300}}},
            {"Scope",{{"enum",{"Participant","Party","World"}}}},{"TimeOfDay",{{"enum",{"Any","Day","Night"}}}},
            {"SpawnTimeOfDay",{{"enum",{"Any","Day","Night"}},{"description","Native time state required when event actors appear. Replaces legacy TimeOfDay."}}},
            {"DespawnTimeOfDay",{{"enum",{"Day","Night"}},{"description","Retire event actors and restore event weather when this native time state begins."}}},
            {"Weather",{{"enum",{"Sunny","Cloudy","Fog","Rain_Light","Rain_Heavy","Rain_Storm","Rain_LightningStorm","Sandstorm","Velgar_BossFight"}}}},
            {"Area",eventArea},{"Messages",eventMessages},{"HealthStages",{{"type","array"},{"minItems",1},{"maxItems",16},{"items",healthStage},{"description","Health-threshold taunts sent through the native text-notification route. Optional SpawnID limits a threshold to one event spawn template."}}},
            {"Actions",{{"type","array"},{"minItems",1},{"maxItems",32},{"items",eventAction},{"description","Delayed pose or emote actions applied to live event actors matching SpawnID."}}},
            {"Waves",{{"type","array"},{"minItems",1},{"maxItems",8},{"items",{{"type","array"},{"minItems",1},{"maxItems",16},{"items",eventMember}}}}}}}});
    result["events"]["items"]["allOf"]=json::array({{{"not",{{"required",{"TimeOfDay","SpawnTimeOfDay"}}}}}});
    const json eventSpawn={{"type","object"},{"additionalProperties",false},{"required",{"Id","Type","EventOnly","AIClass"}},
        {"properties",{{"Id",text},{"Type",{{"const","AISpawnPoint"}}},{"EventOnly",{{"const",true}}},
            {"AIClass",{{"type","string"},{"pattern","^/"}}},{"DisplayName",{{"type","string"},{"minLength",1},{"maxLength",128}}},{"BossName",{{"type","string"},{"minLength",1},{"maxLength",128}}},{"Scale",{{"type","number"},{"minimum",0.1},{"maximum",5}}}}}};
    result["spawns"]["items"]["allOf"]=json::array({{{"if",{{"required",{"EventOnly"}}}},{"then",eventSpawn}}});
    result["spawns"]["items"]["allOf"][0]["then"]["properties"]["VisualEffect"]=ghostVisual;
    result["spawns"]["items"]["allOf"][0]["then"]["properties"]["PowerLevel"]={{"type","integer"},{"minimum",1},{"maximum",100}};
    result["spawns"]["items"]["allOf"][0]["then"]["properties"]["LootRow"]={{"type","string"},{"minLength",1},{"maxLength",256},{"pattern","^[^\\r\\n\\t]+$"}};
    result["quests"]=many({{"type","object"},{"additionalProperties",false},{"required",{"Id","PersistenceID","Title","Description","Objective","Reward"}},
        {"properties",{{"Id",text},{"PersistenceID",{{"type","string"},{"pattern","^[A-Za-z0-9_-]{21}[AQgw]$"}}},{"Title",text},{"Description",text},
            {"Objective",{{"type","object"},{"additionalProperties",false},{"required",{"Id","Text","Item","Count"}},
                {"dependencies",{{"RadiusMeters",{"Location"}}}},
                {"properties",{{"Id",text},{"Text",text},{"Item",questItem},{"Count",questCount},{"Location",questLocation},
                    {"RadiusMeters",{{"type","number"},{"minimum",0.01},{"maximum",10000},{"description","Optional search-area radius in meters from Location; omit for the existing point marker."}}}}}}},
            {"Reward",{{"type","object"},{"additionalProperties",false},{"required",{"Item","Count"}},
                {"properties",{{"Item",questItem},{"Count",questCount},{"TimeOfDay",{{"enum",{"Any","Day","Night"}},{"default","Any"},{"description","Hold delivery until the authoritative native time phase matches; quest progress and the unclaimed receipt remain intact."}}}}}}}}}});
    auto killObjective=result["quests"]["anyOf"][0]["properties"]["Objective"];
    killObjective["required"]={"Id","Text","Count","Type","AIClasses"};
    killObjective["properties"].erase("Item");
    killObjective["properties"]["Type"]={{"const","Kill"}};
    killObjective["properties"]["AIClasses"]={{"type","array"},{"minItems",1},{"maxItems",32},{"uniqueItems",true},{"items",{{"type","string"},{"pattern","^/"},{"maxLength",1024}}}};
    killObjective["properties"]["IncludeDerived"]={{"type","boolean"},{"default",true}};
    killObjective["properties"]["EventID"]=text;
    killObjective["properties"]["SpawnID"]=text;
    killObjective["dependencies"]["SpawnID"]={"EventID"};
    killObjective["description"]="Kill objective: native counter keyed by objective Id; credited local authoritative player only. Candidate requires live death and reload verification.";
    auto collectObjective=result["quests"]["anyOf"][0]["properties"]["Objective"];
    collectObjective["properties"]["Type"]={{"const","Collect"}};
    for(auto* objective:{&collectObjective,&killObjective}) {
        (*objective)["properties"]["Hidden"]={{"type","boolean"},{"default",false}};
        (*objective)["properties"]["Optional"]={{"type","boolean"},{"default",false}};
        (*objective)["properties"]["ProgressText"]={{"type","string"},{"minLength",1},{"maxLength",1024},{"description","Live objective template. Supports {current}, {required}, {remaining}, and repeat-safe {run}; for example: Run {run}: kill {remaining} more goblins."}};
        (*objective)["properties"]["CompleteText"]={{"type","string"},{"minLength",1},{"maxLength",1024},{"description","Optional text shown when this objective reaches its required count."}};
        (*objective)["properties"]["AnnounceProgress"]={{"type","boolean"},{"default",false},{"description","Push the expanded progress text through the native player text-notification route whenever credited progress changes."}};
    }
    auto acquireObjective=collectObjective;
    acquireObjective["properties"]["Type"]={{"const","Acquire"}};
    acquireObjective["required"].push_back("Type");
    acquireObjective["description"]="Positive inventory additions after acceptance. Any source counts; AOR checks player location. No items consumed at turn-in. Candidate requires multiplayer verification.";
    const json objectives={{"oneOf",{collectObjective,killObjective,acquireObjective}}};
    result["quests"]["anyOf"][0]["properties"]["Objective"]=objectives;
    result["quests"]["anyOf"][1]["items"]["properties"]["Objective"]=objectives;
    json repeatReset={{"type","object"},{"additionalProperties",false},{"required",{"Frequency"}}};
    repeatReset["properties"]["Frequency"]={{"enum",{"Daily","Weekly"}}};
    repeatReset["properties"]["HourUTC"]={{"type","integer"},{"minimum",0},{"maximum",23},{"default",0}};
    repeatReset["properties"]["MinuteUTC"]={{"type","integer"},{"minimum",0},{"maximum",59},{"default",0}};
    repeatReset["properties"]["WeekdayUTC"]={{"type","integer"},{"minimum",0},{"maximum",6},{"description","Monday=0; required for Weekly, forbidden for Daily."}};
    repeatReset["if"]["properties"]["Frequency"]={{"const","Weekly"}};
    repeatReset["then"]["required"]={"WeekdayUTC"};
    repeatReset["else"]["not"]["required"]={"WeekdayUTC"};
    for(auto* questSchema:{&result["quests"]["anyOf"][0],&result["quests"]["anyOf"][1]["items"]}) {
        (*questSchema)["required"]={"Id","PersistenceID","Title","Description","Reward"};
        (*questSchema)["oneOf"]=json::array({{{"required",{"Objective"}},{"not",{{"required",{"Stages"}}}}},{{"required",{"Stages"}},{"not",{{"required",{"Objective"}}}}}});
        (*questSchema)["properties"]["Stages"]={{"type","array"},{"minItems",1},{"maxItems",32},
            {"items",{{"type","object"},{"additionalProperties",false},{"required",{"Id","Objectives"}},
                {"properties",{{"Id",{{"type","string"},{"pattern","^[A-Za-z0-9_-]{1,64}$"}}},
                    {"Objectives",{{"type","array"},{"minItems",1},{"maxItems",16},{"items",objectives}}}}}}}};
        (*questSchema)["properties"]["Category"]={{"enum",{"Regular","Story","Task"}},{"default","Regular"},{"description","Task uses the game's green task-quest presentation."}};
        (*questSchema)["properties"]["TimeOfDay"]={{"enum",{"Any","Day","Night"}},{"default","Any"},{"description","Acceptance gate evaluated from the game's native time state. Active quest progress remains intact across later day/night transitions."}};
        (*questSchema)["properties"]["Prerequisites"]={{"type","array"},{"maxItems",32},{"uniqueItems",true},{"items",text}};
        (*questSchema)["properties"]["Repeatable"]={{"type","boolean"},{"default",false}};
        (*questSchema)["properties"]["Completion"]={{"type","string"},{"enum",{"ReturnToNPC","Automatic"}},{"default","ReturnToNPC"}};
        (*questSchema)["properties"]["Repeat"]={{"type","object"},{"additionalProperties",false},
            {"properties",{{"CooldownSeconds",{{"type","integer"},{"minimum",0},{"maximum",31536000}}},{"Reset",repeatReset},
                {"RewardMultiplierPerRun",{{"type","number"},{"minimum",0},{"maximum",10},{"default",0},{"description","Additive multiplier applied for each run after the first."}}},
                {"MaximumRewardMultiplier",{{"type","number"},{"minimum",1},{"maximum",100},{"default",1},{"description","Required cap when RewardMultiplierPerRun is greater than zero."}}}}},
            {"description","Cooldown starts at completion. UTC daily/weekly reset may be used alone or combined; all specified gates must pass. Never starts a new run automatically."}};
        auto costAmount=(*questSchema)["properties"]["Reward"];
        costAmount["properties"].erase("TimeOfDay");
        (*questSchema)["properties"]["StartCost"]=costAmount;
        (*questSchema)["properties"]["RepeatReward"]=(*questSchema)["properties"]["Reward"];
        (*questSchema)["dependencies"]["RepeatReward"]={{"required",{"Repeatable"}},{"properties",{{"Repeatable",{{"const",true}}}}}};
        const auto amount=(*questSchema)["properties"]["Reward"];
        (*questSchema)["properties"]["EntryOptions"]={{"type","array"},{"minItems",1},{"maxItems",8},
            {"items",{{"type","object"},{"additionalProperties",false},{"required",{"Id","Cost"}},{"properties",{{"Id",text},{"Cost",costAmount}}}}}};
        json tier={{"type","object"},{"additionalProperties",false},{"required",{"Id","Priority","Reward"}}};
        tier["properties"]["Id"]=text;tier["properties"]["EntryID"]=text;tier["properties"]["Reward"]=amount;
        tier["properties"]["RequiresObjectives"]={{"type","array"},{"minItems",1},{"maxItems",128},{"uniqueItems",true},
            {"items",{{"type","string"},{"pattern","^[A-Za-z0-9_-]{1,64}:[A-Za-z0-9_-]{1,64}$"}}}};
        tier["properties"]["Priority"]={{"type","integer"},{"minimum",-100000},{"maximum",100000}};
        tier["properties"]["MaxElapsedSeconds"]={{"type","integer"},{"minimum",1},{"maximum",31536000},
            {"description","Accept to confirmed kill-goal completion, or to first eligible fetch hand-in attempt. Includes offline time. Missing timing does not qualify."}};
        (*questSchema)["properties"]["ResultTiers"]={{"type","array"},{"minItems",1},{"maxItems",16},{"items",tier},
            {"description","Highest unique Priority among matching entry/time conditions replaces the base Reward; never stacks."}};
        (*questSchema)["not"]["required"]={"StartCost","EntryOptions"};
        (*questSchema)["dependencies"]["Repeat"]={{"required",{"Repeatable"}},{"properties",{{"Repeatable",{{"const",true}}}}}};
    }
    result["quests"]["anyOf"].push_back(declarationDocument("Quest"));
    json dialogueNode={{"type","object"},{"additionalProperties",false},{"required",{"Text","Choices"}},
        {"properties",{{"Text",{{"type","string"},{"minLength",1},{"maxLength",4096}}},{"Choices",{{"type","array"},{"minItems",1},{"maxItems",4},{"items",choice}}}}}};
    dialogueNode["properties"]["Pose"]=npc["properties"]["Pose"];
    dialogueNode["properties"]["Emote"]=npc["properties"]["Pose"];
    dialogueNode["properties"]["Emote"]["description"]="Human NPC animation played once when this node is entered. Asset cues must use Playback Once.";
    result["dialogue"]=many({{"type","object"},{"additionalProperties",false},{"required",{"Id","Entry","Nodes"}},
        {"properties",{{"Id",text},{"Entry",text},{"CompletedEntry",text},
            {"Completion",{{"type","object"},{"additionalProperties",false},{"required",{"Flag","Item","Count"}},
                {"properties",{{"Flag",text},{"Item",{{"type","string"},{"pattern","^/"},{"maxLength",1024}}},{"Count",{{"type","integer"},{"minimum",1},{"maximum",999}}}}}}},
            {"Nodes",{{"type","object"},{"minProperties",1},{"maxProperties",128},{"additionalProperties",dialogueNode}}}}},
        {"dependencies",{{"CompletedEntry",{"Completion"}}}}});
    result["enums"]=map(textList);
    const json replacement={{"anyOf",{text,textList}}};
    result["strings"]={{"type","object"},{"additionalProperties",{{"anyOf",{replacement,
        {{"type","object"},{"additionalProperties",replacement}}}}}}};
    const json equipmentEffectReference={{"type","string"},{"maxLength",1024},
        {"anyOf",json::array({json{{"pattern","^[A-Za-z0-9_-]+:Effects/[A-Za-z0-9_./-]+$"}},json{{"pattern","^/.*\\..*_C$"}}})}};
    json equipmentEffectRule={{"type","object"},{"additionalProperties",false},{"required",{"Mode"}}};
    equipmentEffectRule["properties"]["Mode"]={{"enum",{"Replace","Append","Clear"}}};
    equipmentEffectRule["properties"]["Effects"]={{"type","array"},{"minItems",1},{"maxItems",64},{"items",equipmentEffectReference}};
    equipmentEffectRule["properties"]["$Comment"]=text;
    const json equipmentEffects={{"type","object"},{"maxProperties",256},{"propertyNames",{{"pattern","^/.*\\..+$"}}},
        {"additionalProperties",equipmentEffectRule}};
    result["equipment"]={{"type","object"},{"minProperties",1},{"additionalProperties",false},
        {"properties",{{"SurgeEvadeLegs",{{"type","object"},{"maxProperties",64},{"additionalProperties",{{"type","boolean"}}}}},
            {"ShadowveilWearables",object},{"ShadowveilAttackEvadeWearables",{{"type","object"},{"maxProperties",64},{"additionalProperties",{{"type","boolean"}}}}},
            {"GrantedEffects",equipmentEffects}}},
        {"not",{{"required",{"ShadowveilWearables","ShadowveilAttackEvadeWearables"}}}}};
    const json scalar={{"anyOf",{json{{"type",{"number","boolean"}}},json{{"type","string"},{"minLength",1},{"maxLength",512},{"pattern","^/"}}}}};
    const json condition={{"type","object"},{"additionalProperties",false},{"required",{"Path"}},
        {"properties",{{"Path",{{"type","array"},{"minItems",1},{"maxItems",5},
            {"items",{{"type","string"},{"pattern","^[A-Za-z0-9_]{1,64}$"}}}}},{"Equals",scalar},
            {"NotEquals",scalar},{"NonEmpty",{{"type","boolean"}}},{"Exists",{{"type","boolean"}}},
            {"GreaterThan",number},{"GreaterOrEqual",number},{"LessThan",number},{"LessOrEqual",number}}},
        {"oneOf",json::array({json{{"required",{"Equals"}}},json{{"required",{"NotEquals"}}},json{{"required",{"NonEmpty"}}},
            json{{"required",{"Exists"}}},json{{"required",{"GreaterThan"}}},json{{"required",{"GreaterOrEqual"}}},
            json{{"required",{"LessThan"}}},json{{"required",{"LessOrEqual"}}}})}};
    json event=object;
    event["additionalProperties"]=false;
    event["required"]={"Function","State"};
    // Native and explicitly observed Blueprint paths are both valid. Runtime
    // validation applies the stricter exact-path and tick/update safeguards.
    event["properties"]["Function"]={{"type","string"},{"pattern","^/[^:]+:[^:]+$"}};
    event["properties"]["State"]={{"type","string"},{"pattern","^[A-Za-z0-9_]{1,64}$"},{"not",{{"const","Dead"}}}};
    event["properties"]["Parameters"]={{"type","array"},{"maxItems",8},{"items",condition}};
    event["properties"]["Action"]={{"enum",{"Pulse","Activate","Deactivate"}}};
    const json events={{"type","array"},{"minItems",1},{"maxItems",64},{"items",event}};
    const json nameplateState={{"type","object"},{"additionalProperties",false},{"minProperties",1},{"properties",{
            {"Icon",text},{"Scale",{{"type","number"},{"minimum",0.1},{"maximum",4}}},
            {"InactivitySeconds",{{"type","number"},{"minimum",0},{"maximum",3600}}},
            {"Priority",{{"type","integer"},{"minimum",-10000},{"maximum",10000}}},
            {"While",{{"type","object"},{"additionalProperties",false},{"required",{"GameplayEffect"}},
                {"properties",{{"GameplayEffect",{{"type","string"},{"maxLength",512},{"pattern","^/[^:]+[.][^:]+$"}}}}}}},
            {"Native",{{"type","object"},{"additionalProperties",false},{"properties",{
                {"Component",{{"type","object"},{"maxProperties",64},{"additionalProperties",true}}},
                {"Widget",{{"type","object"},{"maxProperties",64},{"additionalProperties",true}}},
                {"Text",{{"type","object"},{"maxProperties",64},{"additionalProperties",true}}}}}}}}}};
    const json yesNo={{"anyOf",{json{{"type","boolean"}},json{{"enum",{"Yes","No","yes","no"}}}}}};
    const json nameplate={{"type","object"},{"additionalProperties",false},{"properties",{
        {"Definition",{{"type","string"},{"minLength",1},{"maxLength",192},{"description","Reusable /nameplates Id. Local Id or Mod:Id; inline fields override the resolved definition."}}},
        {"Mode",{{"enum",{"Name","Icon","Hidden"}}}}, {"Icon",text},
        {"ActivityTimeoutSeconds",{{"type","number"},{"minimum",0},{"maximum",3600}}},
        {"Scale",{{"type","number"},{"minimum",0.1},{"maximum",4}}},
        {"PixelWidth",{{"type","number"},{"minimum",1},{"maximum",4096}}},
        {"PixelHeight",{{"type","number"},{"minimum",1},{"maximum",4096}}},
        {"Distance",{{"type","number"},{"minimum",0},{"maximum",100000}}},
        {"ShowSelf",{{"type","boolean"}}},{"Client",yesNo},{"Server",yesNo},
        {"States",map(nameplateState)},{"Events",events},
        {"SkillXP",{{"type","object"},{"minProperties",1},{"maxProperties",64},{"additionalProperties",text}}},
        {"Native",{{"type","object"},{"additionalProperties",false},{"properties",{
            {"Component",{{"type","object"},{"maxProperties",64},{"additionalProperties",true}}},
            {"Widget",{{"type","object"},{"maxProperties",64},{"additionalProperties",true}}},
            {"Text",{{"type","object"},{"maxProperties",64},{"additionalProperties",true}}}}}}}}}};
    json archetype=object;
    archetype["required"]={"Name","Icon"};
    archetype["additionalProperties"]=false;
    archetype["properties"]["Name"]={{"type","string"},{"minLength",1},{"maxLength",64}};
    archetype["properties"]["Icon"]=text;
    archetype["properties"]["Scale"]={{"type","number"},{"minimum",0.1},{"maximum",4}};
    const json duration={{"anyOf",{{{"type","number"},{"exclusiveMinimum",0},{"maximum",300}},{{"const","INFINITE"}}}}};
    const auto visualEffect=[&](std::initializer_list<const char*> triggers) {
        json triggerValues=json::array();
        for(const auto* trigger:triggers)triggerValues.push_back(trigger);
        return json{{"type","object"},{"additionalProperties",true},{"properties",{
            {"Trigger",{{"enum",std::move(triggerValues)}}},{"DurationSeconds",duration}}}};
    };
    json player={{"type","object"},{"additionalProperties",false},{"properties",json::object()}};
    player["properties"]["Id"]=text;
    player["properties"]["$Id"]=text;
    player["properties"]["PlayerName"]=text;
    player["properties"]["PlayerNames"]=textList;
    player["properties"]["PlayerGuid"]=text;
    player["properties"]["PlayerGuids"]=textList;
    const json multiplier={{"type","number"},{"minimum",0},{"maximum",100}};
    player["properties"]["Scale"]={{"type","number"},{"minimum",0.25},{"maximum",3}};
    for(const auto* field:{"HealthMultiplier","DefenseMultiplier","DamageMultiplier","StaminaMultiplier",
        "WalkSpeedMultiplier","RunSpeedMultiplier","CarryWeightMultiplier","PoisonResistanceMultiplier",
        "StaminaRecoveryMultiplier","PhysicalAttackMultiplier","MagicalAttackMultiplier","MagicAttackMultiplier",
        "RangedAttackMultiplier","RangeAttackMultiplier","PhysicalDefenseMultiplier","MagicalDefenseMultiplier",
        "MagicDefenseMultiplier","RangedDefenseMultiplier","RangeDefenseMultiplier"})
        player["properties"][field]=multiplier;
    for(const auto* field:{"MaxHealth","BaseHealth","MaxStamina","MaxCarryWeight"})
        player["properties"][field]={{"type","number"},{"minimum",1},{"maximum",1000000}};
    player["properties"]["AttributeMultipliers"]={{"type","object"},{"additionalProperties",multiplier}};
    const json attributeEdit={{"type","object"},{"additionalProperties",false},{"oneOf",{
        json{{"required",{"Set"}}},json{{"required",{"Add"}}},json{{"required",{"Multiply"}}}}},
        {"properties",{{"Set",number},{"Add",number},{"Multiply",multiplier}}}};
    player["properties"]["Attributes"]={{"type","object"},{"additionalProperties",attributeEdit}};
    const json appearanceFallback={{"anyOf",{text,json{{"type","object"},{"additionalProperties",false},
        {"required",{"RowName"}},{"properties",{{"DataTable",text},{"RowName",text}}}}}}};
    const json appearanceSelection={{"anyOf",{text,json{{"type","object"},{"additionalProperties",false},
        {"required",{"RowName"}},{"properties",{{"DataTable",text},{"RowName",text},{"Source",text},{"Fallback",appearanceFallback}}}}}}};
    json playerAppearance={{"type","object"},{"additionalProperties",false},{"properties",json::object()}};
    for(const auto* field:{"BodyType","FaceType","Head","HairPreset","HairStyle","FacialHairPreset",
        "BeardStyle","SkinTone","SkinColor","HairColor","EyeColor","EyebrowColor"})
        playerAppearance["properties"][field]=appearanceSelection;
    player["properties"]["Appearance"]=playerAppearance;
    player["properties"]["Nameplate"]=nameplate;
    player["properties"]["MapIcon"]=npc["properties"]["Map"];
    player["properties"]["Native"]={{"type","object"},{"additionalProperties",false},{"properties",{
        {"Pawn",{{"type","object"},{"maxProperties",128},{"additionalProperties",true}}},
        {"Components",{{"type","object"},{"maxProperties",128},{"additionalProperties",{{"type","object"},{"maxProperties",128},{"additionalProperties",true}}}}}}}};
    player["properties"]["VisualEffect"]=visualEffect({"Load","Respawn"});
    player["properties"]["Archetype"]=archetype;
    result["players"]={{"type","array"},{"items",{{"anyOf",{player,patch}}}}};
    const json assetReferenceObject={{"type","object"},{"additionalProperties",false},
        {"properties",{{"ObjectPath",text},{"AssetPathName",text},{"ObjectName",text},{"SubPathString",text}}}};
    const json assetReference={{"anyOf",{text,assetReferenceObject}}};
    json asset=record;
    asset["description"]="Reflected asset fields are accepted. Unlockable item assets may set RecipesToUnlock, or BuildingPieceToUnlock; $Clone inherits these fields before applying overrides. $DominionSpheres patches validated named sphere subobjects owned by the target asset.";
    asset["properties"]["$Clone"]=text;
    asset["properties"]["InternalName"]=text;
    asset["properties"]["PersistenceID"]=text;
    asset["properties"]["bSoftDeleted"]={{"type","boolean"},{"description","Native item retirement flag, including wearable/cape data. Applies only when the target exposes this reflected Boolean. It is not proof of save exclusion and is not injected into ordinary asset patches."}};
    asset["properties"]["BuildingPieceToUnlock"]=assetReference;
    asset["properties"]["RecipesToUnlock"]={{"type","array"},{"items",assetReference}};
    asset["properties"]["$VisualEffect"]={{"type","object"},{"additionalProperties",true},
        {"properties",{{"Trigger",{{"const","Equip"}}},
            {"DurationSeconds",{{"const","INFINITE"}}}}}};
    asset["properties"]["$DominionSpheres"]={{"type","object"},{"minProperties",1},{"maxProperties",32},
        {"propertyNames",{{"pattern","^[A-Za-z0-9_]+(?:\\.[A-Za-z0-9_]+)*$"}}},
        {"additionalProperties",{{"type","object"},{"additionalProperties",false},{"required",{"Radius"}},
            {"properties",{{"Radius",{{"type","number"},{"exclusiveMinimum",0},{"maximum",100000},
                {"description","Sphere radius in Unreal centimeters."}}}}}}}};
    asset["allOf"].push_back({{"not",{{"required",{"$Clone","$DominionSpheres"}}}}});
    const json authorFlag = {
        {"anyOf", {
            json{{"type", "boolean"}},
            json{
                {"type", "string"},
                {"pattern", "^(?:[Yy][Ee][Ss]|[Nn][Oo]|[Tt][Rr][Uu][Ee]|[Ff][Aa][Ll][Ss][Ee])$"}
            }
        }}
    };
    const json moddedBlock={{"type","object"},{"additionalProperties",false},
        {"properties",{{"RuneSchema",authorFlag},{"Cooked",authorFlag},{"SafeToClone",authorFlag}}},
        {"description","Author declarations consumed by RuneSchema, not Unreal properties. SafeToClone is permission; actual source type, package evidence and dependencies are still checked."}};
    const json moddedValue={{"anyOf",{authorFlag,moddedBlock}}};
    asset["properties"]["RuneSchema"]=authorFlag;
    asset["properties"]["Modded"]=moddedValue;
    result["assets"]={{"type","object"},{"patternProperties",{{"^(?!(?:RuneSchema|Modded)$)[^$]",asset}}},
        {"properties",{{"RuneSchema",authorFlag},{"Modded",moddedValue}}},{"additionalProperties",true}};
    result["assets"]["properties"]["$declaration"]={{"oneOf",{cookedDeclaration,
        json{{"type","array"},{"minItems",1},{"maxItems",4096},{"items",cookedDeclaration}}}},
        {"description","Declare PAK-cooked ItemData or RecipeData without cloning or patching it. Verified declarations enter the scoped ownership ledger."}};

    json flatNameplate=nameplate;
    flatNameplate["required"]={"Id"};
    flatNameplate["properties"]["Id"]={{"type","string"},{"minLength",1},{"maxLength",96}};
    json wrappedNameplate={{"type","object"},{"additionalProperties",false},{"required",{"Id","Nameplate"}},
        {"properties",{{"Id",flatNameplate["properties"]["Id"]},{"Nameplate",nameplate}}}};
    result["nameplates"]={{"type","array"},{"items",{{"oneOf",{flatNameplate,wrappedNameplate}}}}};
    const json vector3={{"type","object"},{"additionalProperties",false},{"required",{"X","Y","Z"}},
        {"properties",{{"X",{{"type","number"}}},{"Y",{{"type","number"}}},{"Z",{{"type","number"}}}}}};
    const json rotator={{"type","object"},{"additionalProperties",false},{"required",{"Pitch","Yaw","Roll"}},
        {"properties",{{"Pitch",{{"type","number"}}},{"Yaw",{{"type","number"}}},{"Roll",{{"type","number"}}}}}};
    const json color={{"type","object"},{"additionalProperties",false},{"required",{"R","G","B","A"}},
        {"properties",{{"R",{{"type","number"}}},{"G",{{"type","number"}}},{"B",{{"type","number"}}},{"A",{{"type","number"}}}}}};
    const json niagaraParameter={{"anyOf",{{{"type","boolean"}},{{"type","number"}},vector3,color}}};
    const json niagara={{"type","object"},{"additionalProperties",false},{"required",{"System"}},
        {"properties",{{"System",{{"type","string"},{"pattern","^/[^.]+\\.[^/]+$"}}},
            {"Socket",text},{"AutoActivate",{{"type","boolean"}}},{"LocationOffset",vector3},
            {"RotationOffset",rotator},{"Emitters",{{"type","object"},{"maxProperties",16},{"additionalProperties",{{"type","boolean"}}}}},
            {"Parameters",{{"type","object"},{"maxProperties",32},{"patternProperties",{{"^User\\.",niagaraParameter}}},{"additionalProperties",false}}}}}};
    result["niagara"]=map(niagara);
    const json effect={{"type","object"},{"additionalProperties",false},{"required",{"Class"}},
        {"properties",{{"Class",{{"type","string"},{"pattern","^/[^.]+\\.[^/]+_C$"}}},{"$Comment",text}}}};
    result["effects"]={{"type","object"},{"patternProperties",{{"^[^$]",effect}}},{"additionalProperties",false},
        {"description","Gameplay-effect definitions alias verified cooked Blueprint classes under virtual IDs. They do not clone or patch class defaults."},
        {"x-runeschema-definition-id","ModName:Category/Id"},
        {"x-runeschema-reference-forms",{"ModName:Category/Id","/Game/.../Effect.Effect_C"}}};
    json registryPresentation={{"type","object"},{"additionalProperties",false}};
    registryPresentation["required"]={"Phase","Class","Classification"};
    registryPresentation["properties"]={
        {"Phase",{{"type","string"},{"pattern","^[A-Za-z0-9_.-]+$"},{"maxLength",96}}},
        {"Class",{{"type","string"},{"pattern","^/(?:Game|DowdunReach|FutureMajorVersion)/"}}},
        {"Classification",{{"enum",{"PureVFX","CosmeticWrapper","NativeReplicated","Unsupported"}}}},
        {"Socket",{{"type","string"},{"maxLength",128}}},
        {"Parameters",{{"type","object"},{"maxProperties",32}}}
    };
    json registryEntry={{"type","object"},{"additionalProperties",false}};
    registryEntry["required"]={"Id","Kind"};
    const json registryAuthority={{"type","object"},{"additionalProperties",false},
        {"required",{"Action","GraphClass"}},
        {"properties",{{"Action",{{"enum",{"SpawnFollower","ExecuteGraph"}}}},
            {"GraphClass",{{"type","string"},{"pattern","^/[A-Za-z0-9_.-]+/.+_C$"}}},
            {"Function",{{"type","string"},{"pattern","^[A-Za-z0-9_.-]+$"},{"default","Trigger"}}},
            {"DataAsset",{{"type","string"},{"pattern","^/(?:Game|DowdunReach|FutureMajorVersion)/"}}},
            {"Bindings",{{"type","object"},{"maxProperties",32},{"additionalProperties",{{"type","string"},{"pattern","^/(?:Game|DowdunReach|FutureMajorVersion)/"}}}}}}}};
    registryEntry["anyOf"]={json{{"required",{"Presentation"}}},json{{"required",{"Authority"}}}};
    registryEntry["properties"]={
        {"Id",{{"type","string"},{"pattern","^[A-Za-z0-9_.-]+$"},{"maxLength",96}}},
        {"Kind",{{"enum",{"SpellPresentation","PersistentEffect","WeatherPresentation","AudioPresentation","CosmeticWrapper"}}}},
        {"Spell",{{"type","string"},{"pattern","^/(?:Game|DowdunReach|FutureMajorVersion)/"}}},
        {"Presentation",{{"type","array"},{"minItems",1},{"maxItems",64},{"items",registryPresentation}}},
        {"Authority",registryAuthority},
        {"Metadata",{{"type","object"},{"maxProperties",32}}}
    };
    result["registry"]={{"type","object"},{"additionalProperties",false},{"required",{"SchemaVersion","Entries"}},
        {"properties",{{"SchemaVersion",{{"const",1}}},
            {"Entries",{{"type","array"},{"maxItems",256},{"items",registryEntry}}}}},
        {"description","Client presentation declarations shipped by a mod. RuneSchema namespaces every Id with the owning ModId and merges accepted entries into the replicated registry manifest."}};
    for(auto& [name,schema]:result.items()) {
        if(name=="registry") {
            schema["$schema"]="http://json-schema.org/draft-07/schema#";
            schema["title"]="RuneSchema registry manifest";
            schema["x-runeschema-coverage"]="structural";
            continue;
        }
        const auto& capability=Capability(name);
        schema["x-runeschema-starter-capabilities"]={{"patch",capability.StarterPatch},
            {"clone",capability.Clone},{"append",capability.Append},
            {"search",capability.AuthoredSearch?"authored files":"loaded records"}};
        schema["$schema"]="http://json-schema.org/draft-07/schema#";
        schema["title"]="RuneSchema /"+name;
        if(!schema.contains("description"))schema["description"]="Structural authoring hints, not an exhaustive runtime validator. Asset paths, identities, reflected properties and event compatibility must be validated in game.";
        schema["x-runeschema-coverage"]="structural";
    }
    return result;
}
}
