#include "CoreGameplayTags.h"

namespace CoreGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Input, "Input", "Base Input tag. Add subcomponents to map Enhanced Input Actions to Input Tags. See: DaInputConfig");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Input_Move, "Input.Move");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Input_LookMouse, "Input.LookMouse");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Input_LookStick, "Input.LookStick");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Input_PrimaryAttack, "Input.PrimaryAttack");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Input_SecondaryAttack, "Input.SecondaryAttack");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Input_PrimaryInteract, "Input.PrimaryInteract");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Input_SecondaryInteract, "Input.SecondaryInteract");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Input_Use, "Input.Use");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Input_Jump, "Input.Jump");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Input_Run, "Input.Run");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Input_Dash, "Input.Dash");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Input_Crouch, "Input.Crouch");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Input_InspectZoom, "Input.InspectZoom");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Input_InspectRotate, "Input.InspectRotate");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Character, "Character", "Base character tag");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Character_ID, "Character.ID", "Base Tag for Character ID, add a new component to identify different characters to the subsystem");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Character_Type, "Character.Type", "Base Tag for Character Type, add a new component to identify different character types to the subsystem");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Character_Type_AI, "Character.Type.AI", "Base AI Character Type.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Character_Type_Player, "Character.Type.Player", "Base Player Character Type");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Character_Type_Player_ThirdPerson, "Character.Type.Player.ThirdPerson", "Third person perspective player character type");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(AbilitySet, "AbilitySet", "Base tag for ability sets");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(AbilitySetPlayer, "AbilitySet.Player", "Base tag for player ability sets");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(AbilitySetNPC, "AbilitySet.NPC", "Base tag for NPC ability sets");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(AbilitySetItem, "AbilitySet.Item", "Base tag for ability sets granted to item actors with ability components");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(AbilitySetGameState, "AbilitySet.GameState", "Base tag for ability sets granted to DaGameStateBase subclasses which have an ability component");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Attributes, "Attributes", "Core Attributes Group");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(AttributesVital, "Attributes.Vital", "Vital Attributes base group for handling things like Health, Mana, death, damage, etc");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(AttributesVitalCore, "Attributes.Vital.Core", "Vital Attributes like Health or Mana.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(AttributesVitalCoreHealth, "Attributes.Vital.Core.Health", "Health Attribute");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(AttributesVitalCoreMaxHealth, "Attributes.Vital.Core.MaxHealth", "Max Health Attribute");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(AttributesVitalCoreMana, "Attributes.Vital.Core.Mana", "Mana Attribute");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(AttributesVitalCoreMaxMana, "Attributes.Vital.Core.MaxMana", "Max Mana Attribute");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(AttributesVitalMeta, "Attributes.Vital.Meta", "Meta attribute set for dealing with damage or other things that may not be stored directly but applied to core vitals like health");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(AttributesVitalMetaDamage, "Attributes.Vital.Meta.Damage", "Damage Meta Attribute");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(AttributesVitalMetaHealing, "Attributes.Vital.Meta.Healing", "healing Meta Attribute");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(AttributesStats, "Attributes.Stats", "Game specific Stats defined in game code. Should have Primary and Secondary");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(AttributesStatsPrimary, "Attributes.Stats.Primary", "Primary attributes specific player stats/buff attributes that can be modified directly");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(AttributesStatsSecondary, "Attributes.Stats.Secondary", "Secondary Attributes adjust or modified via calculations or modifications based on primary attribute values");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Inventory, "Inventory", "Base Inventory Tag");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Inventory_Master, "Inventory.Master", "Base Inventory Master Tag");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Inventory_Sub, "Inventory.Sub", "Base Inventory Sub Tag");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Inventory_Sub_Equipped, "Inventory.Sub.Equipped", "Tag for equipped inventory items.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Inventory_ID, "Inventory.ID", "Base Inventory ID Tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InventoryItem, "InventoryItem", "Base tag for inventory item.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InventoryItem_Type, "InventoryItem.Type", "Tag for inventory item types.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InventoryItem_Stackable, "InventoryItem.Stackable", "Tag for stackable inventory item types.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InventoryItem_Consumable, "InventoryItem.Consumable", "Tag for a consumable item");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InventoryItem_Equipable, "InventoryItem.Equipable", "Tag for equipable item");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InventoryItem_ID, "InventoryItem.ID", "Tag for inventory item IDs.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InventoryItem_EquipAbility, "InventoryItem.EquipAbility", "Base EquipAbility tag");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InventoryItem_Slot, "InventoryItem.Slot", "Base Tag for inventory item slot.");
		
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Message, "Message", "Base Message Type");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Message_Info, "Message.Info", "Base Info Message Type");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Message_StatusUpdate, "Message.StatusUpdate", "Base Status Update Message Type");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Message_Notification, "Message.Notification", "Base Notification Message Type");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Message_Warning, "Message.Warning", "Base Warning Message Type");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_UI, "UI", "Parent UI Group Tag");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_UI_Layer, "UI.Layer", "Parent UI.Layer Group Tag");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_UI_Layer_Game, "UI.Layer.Game", "Game HUD UI Layer");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_UI_Layer_GameMenu, "UI.Layer.GameMenu", "In game menu UI Layer");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_UI_Layer_Menu, "UI.Layer.Menu", "Main menu UI Layer");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_UI_Layer_Modal, "UI.Layer.Modal", "Modal UI Layer");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Status, "Status", "Base Status Tag");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Status_Death, "Status.Death", "Target has the death status.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Status_Death_Dying, "Status.Death.Dying", "Target has begun the death process.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Status_Death_Dead, "Status.Death.Dead", "Target has finished the death process.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Status_Stunned, "Status.Stunned", "Target is stunned");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Status_Jumping, "Status.Jumping", "Target is jumping");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Status_Attacking, "Status.Attacking", "Target is attacking");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Status_Sprinting, "Status.Sprinting", "Target is sprinting");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Status_Parrying, "Status.Parrying", "Target is parrying");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Status_Healing, "Status.Healing", "Target is healing");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Action, "Action", "Base Action Tag");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Action_Jump, "Action.Jump");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Action_Sprint, "Action.Sprint");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Action_PrimaryAttack, "Action.PrimaryAttack");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Action_SecondaryAttack, "Action.SecondaryAttack");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Action_PrimaryInteract, "Action.PrimaryInteract");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Action_SecondaryInteract, "Action.SecondaryInteract");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Action_Dash, "Action.Dash");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Action_Parry, "Action.Parry");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Action_SelectItem, "Action.SelectItem");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Action_UseItem, "Action.UseItem");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Action_Heal, "Action.Heal");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Action_GainMana, "Action.GainMana");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Action_Buff, "Action.Buff", "Base Action Buff Tag");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Pickup, "Pickup", "Base Tag for Pickup Items");
	UE_DEFINE_GAMEPLAY_TAG(TAG_Pickup_Health, "Pickup.Health")
	UE_DEFINE_GAMEPLAY_TAG(TAG_Pickup_Credits, "Pickup.Credits")
	UE_DEFINE_GAMEPLAY_TAG(TAG_Pickup_Mana, "Pickup.Mana")
	UE_DEFINE_GAMEPLAY_TAG(TAG_Pickup_Ability, "Pickup.Ability")
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Behavior_SurvivesDeath, "Ability.Behavior.SurvivesDeath", "An ability with this type tag should not be canceled due to death.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Event, "Event", "Base tag for gameplay events");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Event_Damage, "Event.Damage", "Damage Gameplay Event");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Event_Heal, "Event.Heal", "Dealing Gameplay Event");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Event_Death, "Event.Dying", "Dying Gameplay Event");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Event_Montage, "Event.Montage", "Base tag for montage gameplay events");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Event_Montage_Projectile, "Event.Montage.Projectile", "Projectile launch montage gameplay events");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Gameplay_Damage, "Gameplay.Damage", "Tag to identify damage values dealt to damage gameplay attribute");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Gameplay_DamageImmunity, "Gameplay.DamageImmunity", "Give this tag to actors with Attribute components to not take damage");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Gameplay_DamageSelfDestruct, "Gameplay.Damage.SelfDestruct", "Set this to kill an actor manually, like with a cheat");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Gameplay_FellOutOfWorld, "Gameplay.Damage.FellOutOfWorld", "Set if character falls out of the world and dies");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_DA_Damage_Message, "DA.Damage.Message", "Message sent to messaging system for other modules to respond to. Currently Unused");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Cheat_GodMode, "Cheat.GodMode", "Tag set if Stat God mode is enabled");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Cheat_UnlimitedHealth, "Cheat.UnlimitedHealth", "Tag set if unlimiteed heatlh is enabled");
}