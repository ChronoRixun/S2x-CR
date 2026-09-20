if game:issingleplayer() or not Engine.InFrontend() then
	return
end

local function get_controller_index( element, properties )
	if properties and properties.controllerIndex then
		return properties.controllerIndex
	end

	local scoped_data = LUI.FlowManager.GetScopedData( element )
	if scoped_data and scoped_data.exclusiveControllerIndex then
		return scoped_data.exclusiveControllerIndex
	end

	return Engine.GetFirstActiveController()
end

local function get_toggle_text( dvar_name )
	return function ()
		if Engine.GetDvarBool( dvar_name ) then
			return Engine.Localize( "@LUA_MENU_ENABLED" )
		end

		return Engine.Localize( "@LUA_MENU_DISABLED" )
	end
end

local function toggle_dvar( dvar_name )
	return function ()
		Engine.SetDvarBool( dvar_name, not Engine.GetDvarBool( dvar_name ) )
	end
end

local function open_command_confirmation( element, controller, command, warning )
	LUI.FlowManager.RequestAddMenu( element, "notification_modal", true, controller, false, {
		titleText = Engine.Localize( "@MENU_WARNING" ),
		descText = warning,
		icon = nil,
		modalType = ModalUtils.NotificationModalType.GeneralNotifications,
		accept_func = function ()
			Engine.Exec( command )
		end,
		cancel_func = function ()
		end,
		choices = {}
	} )
end

local function open_unlock_confirmation( element, controller, command, warning )
	open_command_confirmation( element, controller, command .. " confirm", warning )
end

-- Rank chooser (issue #48): prestige and level steppers backed by the setrank command.
local rank_steps = { 1, 5, 10, 50, 100 }

-- Caps from the current mode's rank table, or nil when the table cannot be
-- used; callers treat nil as "not ready" rather than substituting defaults.
local function get_rank_caps()
	if not ( S2xStats and S2xStats.GetRankCaps ) then
		return nil
	end

	local ok, max_prestige, max_level, max_level_final = pcall( S2xStats.GetRankCaps )
	if not ok or type( max_prestige ) ~= "number" or type( max_level ) ~= "number" or
		type( max_level_final ) ~= "number" then
		return nil
	end

	return {
		maxPrestige = max_prestige,
		maxLevel = max_level,
		maxLevelFinalPrestige = max_level_final
	}
end

local function try_player_data( controller, group, field )
	if not group then
		return nil
	end

	local ok, value = pcall( function ()
		return Engine.GetPlayerData( controller, group, field )
	end )
	if ok and type( value ) == "number" then
		return value
	end

	return nil
end

-- Reads the prestige and level the setrank and setprestige commands would
-- change: the C++ side names the stats group and fields it writes, so the
-- chooser can never seed itself from another mode's progression. Returns ok,
-- prestige, level; ok is false when the stats are not loaded, a field could not
-- be read or the rank table could not convert the experience, so callers never
-- mistake defaults for progress.
local function read_current_progression( controller )
	if not ( S2xStats and S2xStats.GetProgressionSource and S2xStats.GetLevelForExperience ) then
		return false, 0, 1
	end

	local ok, group, prestige_field, experience_field = pcall( S2xStats.GetProgressionSource )
	if not ok or type( group ) ~= "number" or type( prestige_field ) ~= "string" or
		type( experience_field ) ~= "string" then
		return false, 0, 1
	end

	local prestige = try_player_data( controller, group, prestige_field )
	local experience = try_player_data( controller, group, experience_field )
	if not ( prestige and experience ) then
		return false, 0, 1
	end

	local converted, level = pcall( S2xStats.GetLevelForExperience, experience )
	if not converted or type( level ) ~= "number" or level < 1 then
		return false, 0, 1
	end

	return true, prestige, level
end

local function progression_options( controller )
	local caps = get_rank_caps()
	local caps_known = caps ~= nil
	local ready, current_prestige, current_level = read_current_progression( controller )
	ready = ready and caps_known
	caps = caps or { maxPrestige = 0, maxLevel = 1, maxLevelFinalPrestige = 1 }
	local state = { ready = ready, prestige = current_prestige, level = current_level, stepIndex = 1 }

	local function level_cap()
		if state.prestige >= caps.maxPrestige then
			return caps.maxLevelFinalPrestige
		end
		return caps.maxLevel
	end

	local function clamp_level()
		state.level = math.max( 1, math.min( state.level, level_cap() ) )
	end

	local function adjust_prestige( delta )
		state.prestige = math.max( 0, math.min( state.prestige + delta, caps.maxPrestige ) )
		clamp_level()
	end

	local function adjust_level( delta )
		state.level = state.level + delta * rank_steps[state.stepIndex]
		clamp_level()
	end

	local function adjust_step( delta )
		state.stepIndex = ( ( state.stepIndex - 1 + delta ) % #rank_steps ) + 1
	end

	local function notify( element, text )
		LUI.FlowManager.RequestAddMenu( element, "notification_modal", true, controller, false, {
			titleText = Engine.Localize( "@MENU_NOTICE" ),
			descText = Engine.Localize( text ),
			icon = nil,
			modalType = ModalUtils.NotificationModalType.GeneralNotifications,
			accept_func = function ()
			end,
			cancel_func = function ()
			end,
			choices = {}
		} )
	end

	-- Retries the seed the menu could not complete when it opened. Only a menu
	-- that is not ready is re-seeded, so a selection made on a seeded menu is
	-- never replaced.
	local function reseed( element )
		local ok, prestige, level = read_current_progression( controller )
		local read_caps = get_rank_caps()
		if not read_caps then
			notify( element, "The rank table for this mode could not be read, so nothing can be written." )
			return
		end
		if not ok then
			notify( element, "Your current rank could not be read yet. Try again in a moment." )
			return
		end

		caps = read_caps
		state.ready = true
		state.prestige = prestige
		state.level = level
		clamp_level()
		notify( element, "Your current rank has loaded and the rows above now show it. Adjust them, then press Apply again." )
	end

	-- The caps come from the rank table, not from the player's stats, so this
	-- text is right whether or not the stats have loaded yet. When the table
	-- itself could not be read, the sentence stays true for any table a retry
	-- may load later.
	local rank_help = "The level to write within that prestige."
	if not caps_known then
		rank_help = rank_help .. " Levels past the regular cap need the final prestige."
	elseif caps.maxLevel < caps.maxLevelFinalPrestige then
		rank_help = string.format( "%s Levels above %d need the final prestige.", rank_help, caps.maxLevel )
	end

	return {
		{
			buttonType = "GenericHeader",
			buttonText = Engine.Localize( "Prestige and Rank" ),
			isHeader = true
		},
		{
			buttonType = "GenericButtonScrollable",
			buttonText = Engine.Localize( "Prestige" ),
			buttonDesc = Engine.Localize( "The prestige to write to your profile. Nothing changes until you press Apply." ),
			buttonDisplayFunc = function ()
				return tostring( state.prestige ) .. " / " .. tostring( caps.maxPrestige )
			end,
			buttonLeftFunc = function () adjust_prestige( -1 ) end,
			buttonRightFunc = function () adjust_prestige( 1 ) end
		},
		{
			buttonType = "GenericButtonScrollable",
			buttonText = Engine.Localize( "Rank" ),
			buttonDesc = Engine.Localize( rank_help ),
			buttonDisplayFunc = function ()
				return tostring( state.level ) .. " / " .. tostring( level_cap() )
			end,
			buttonLeftFunc = function () adjust_level( -1 ) end,
			buttonRightFunc = function () adjust_level( 1 ) end
		},
		{
			buttonType = "GenericButtonScrollable",
			buttonText = Engine.Localize( "Rank Step" ),
			buttonDesc = Engine.Localize( "How far one arrow press moves the Rank row." ),
			buttonDisplayFunc = function ()
				return tostring( rank_steps[state.stepIndex] )
			end,
			buttonLeftFunc = function () adjust_step( -1 ) end,
			buttonRightFunc = function () adjust_step( 1 ) end
		},
		{
			buttonType = "GenericButton",
			buttonText = Engine.Localize( "Apply Prestige and Rank" ),
			buttonDesc = Engine.Localize( "Writes the prestige and level shown above to your profile after a confirmation. Re-open the Soldier tab to see the change." ),
			buttonActionFunc = function ( element )
				-- Never write the placeholder values a failed read leaves behind:
				-- check again instead, and seed the rows once the stats can be read.
				if not state.ready then
					reseed( element )
					return
				end
				open_command_confirmation( element, controller,
					string.format( "setrank %d %d", state.level, state.prestige ),
					string.format( "Set prestige %d and rank %d? This overwrites your current rank progression.",
						state.prestige, state.level ) )
			end
		}
	}
end

local function append_options( options, extra )
	for _, row in ipairs( extra ) do
		table.insert( options, row )
	end
	return options
end

local bot_names_labels = { "Default", "Modern", "Nostalgia" }
local bot_names_count = #bot_names_labels
local bot_names_current = nil

local function read_bot_names_index()
	local ok, val = pcall( Engine.GetDvarString, "bot_names" )
	if ok and val then
		for i, label in ipairs( bot_names_labels ) do
			if val:lower() == label:lower() then
				return i
			end
		end

		local num = tonumber( val )
		if num and num >= 0 and num < bot_names_count then
			return num + 1
		end
	end

	return 1
end

local function get_bot_names_index()
	if not bot_names_current then
		bot_names_current = read_bot_names_index()
	end

	return bot_names_current
end

local function set_bot_names( index )
	index = ( ( index - 1 ) % bot_names_count ) + 1
	bot_names_current = index
	Engine.Exec( "set bot_names " .. ( index - 1 ) )
end

local function cycle_bot_names( direction )
	return function ()
		set_bot_names( get_bot_names_index() + direction )
	end
end

local bot_fill_current = 0
local bot_fill_max = 18

local function get_bot_fill()
	return bot_fill_current
end

local function set_bot_fill( value )
	value = math.max( 0, math.min( bot_fill_max, value ) )
	bot_fill_current = value
	Engine.Exec( "set bot_fill " .. value )
end

local function bot_name_options()
	return {
		{
			buttonType = "GenericButtonScrollable",
			buttonText = Engine.Localize( "Bot Fill" ),
			buttonDesc = Engine.Localize( "Number of bots added automatically on every map start. Set to 0 to disable." ),
			buttonDisplayFunc = function ()
				local count = get_bot_fill()
				if count == 0 then
					return "Off"
				end

				return tostring( count )
			end,
			buttonLeftFunc = function () set_bot_fill( get_bot_fill() - 1 ) end,
			buttonRightFunc = function () set_bot_fill( get_bot_fill() + 1 ) end
		},
		{
			buttonType = "GenericButtonScrollable",
			buttonText = Engine.Localize( "Bot Names" ),
			buttonDesc = Engine.Localize( "Choose a name style for bots. Takes effect on the next map." ),
			buttonDisplayFunc = function ()
				return bot_names_labels[get_bot_names_index()]
			end,
			buttonLeftFunc = cycle_bot_names( -1 ),
			buttonRightFunc = cycle_bot_names( 1 )
		}
	}
end

local function multiplayer_options( controller )
	local items_toggle = toggle_dvar( "cg_unlockall_items" )
	local loot_toggle = toggle_dvar( "cg_unlockall_loot" )

	return append_options( {
		{
			buttonType = "GenericButton",
			buttonText = Engine.Localize( "Unlock Multiplayer Progression" ),
			buttonDesc = Engine.Localize(
				"Permanently unlock Multiplayer progression, stats, and challenges." ),
			buttonActionFunc = function ( element )
				open_unlock_confirmation( element, controller, "unlockstatsmp",
					"WARNING: This permanently changes Multiplayer progression and stats. " ..
					"It cannot automatically be undone." )
			end
		},
		{
			buttonType = "GenericButtonScrollable",
			buttonText = Engine.Localize( "Unlock All Items" ),
			buttonDesc = Engine.Localize( "Override normal item availability." ),
			buttonDisplayFunc = get_toggle_text( "cg_unlockall_items" ),
			buttonLeftFunc = items_toggle,
			buttonRightFunc = items_toggle
		},
		{
			buttonType = "GenericButtonScrollable",
			buttonText = Engine.Localize( "Unlock All Loot" ),
			buttonDesc = Engine.Localize( "Override loot item availability." ),
			buttonDisplayFunc = get_toggle_text( "cg_unlockall_loot" ),
			buttonLeftFunc = loot_toggle,
			buttonRightFunc = loot_toggle
		}
	}, append_options( bot_name_options(), progression_options( controller ) ) )
end

local function zombies_options( controller )
	local loot_toggle = toggle_dvar( "cg_unlockall_loot" )
	local consumables_toggle = toggle_dvar( "cg_unlimited_zm_consumables" )
	local progression_toggle = toggle_dvar( "cg_unlock_zm_progression" )

	return append_options( {
		{
			buttonType = "GenericButton",
			buttonText = Engine.Localize( "Unlock Zombies Progression" ),
			buttonDesc = Engine.Localize(
				"Permanently unlock Zombies rank and Hidden Challenges." ),
			buttonActionFunc = function ( element )
				open_unlock_confirmation( element, controller, "unlockstatszm",
					"WARNING: This permanently changes Zombies progression, including rank " ..
					"and Hidden Challenges. It cannot automatically be undone." )
			end
		},
		{
			buttonType = "GenericButton",
			buttonText = Engine.Localize( "Unlock All Easter Eggs" ),
			buttonDesc = Engine.Localize(
				"Permanently mark the Zombies main quests as completed: Tortured Path chapters, " ..
				"their Easter eggs, the red skull and the DLC3 survival maps." ),
			buttonActionFunc = function ( element )
				open_unlock_confirmation( element, controller, "unlockzmeastereggs",
					"WARNING: This permanently marks the Zombies main quests as completed. " ..
					"It cannot automatically be undone." )
			end
		},
		{
			buttonType = "GenericButtonScrollable",
			buttonText = Engine.Localize( "Unlock Groesten Haus" ),
			buttonDesc = Engine.Localize(
				"Treat the Zombies tutorial progression item as owned so Groesten Haus is " ..
				"playable without finishing The Final Reich." ),
			buttonDisplayFunc = get_toggle_text( "cg_unlock_zm_progression" ),
			buttonLeftFunc = progression_toggle,
			buttonRightFunc = progression_toggle
		},
		{
			buttonType = "GenericButtonScrollable",
			buttonText = Engine.Localize( "Unlock All Loot" ),
			buttonDesc = Engine.Localize( "Override loot item availability." ),
			buttonDisplayFunc = get_toggle_text( "cg_unlockall_loot" ),
			buttonLeftFunc = loot_toggle,
			buttonRightFunc = loot_toggle
		},
		{
			buttonType = "GenericButtonScrollable",
			buttonText = Engine.Localize( "Unlimited Zombies Consumables" ),
			buttonDesc = Engine.Localize( "Override Zombies consumable quantities." ),
			buttonDisplayFunc = get_toggle_text( "cg_unlimited_zm_consumables" ),
			buttonLeftFunc = consumables_toggle,
			buttonRightFunc = consumables_toggle
		}
	}, progression_options( controller ) )
end

local function build_unlocks_menu( menu_name, properties, options_factory )
	local self = LUI.UIElement.new( {
		left = 0,
		right = 0,
		top = 0,
		bottom = 0,
		leftAnchor = true,
		rightAnchor = true,
		topAnchor = true,
		bottomAnchor = true
	} )
	self.id = menu_name
	self:playSound( "menu_open" )

	properties = properties or {}
	local controller = get_controller_index( self, properties )
	local scoped_data = LUI.FlowManager.GetScopedData( self )
	scoped_data.gridData = options_factory( controller )

	local background = LUI.MenuBuilder.BuildRegisteredType( "GenericMenuBackground", {
		controllerIndex = controller,
		fontIconSet = properties.fontIconSet
	} )
	background.id = "S2xUnlocksBackground"
	background:setAnchors( 0, 0, 0, 0, 0 )
	background:setBottom( 0, 0 )
	background:setLeft( 0, 0 )
	background:setRight( 0, 0 )
	background:setTop( 0, 0 )
	self:addElement( background )

	local helper_bar = LUI.MenuBuilder.BuildRegisteredType( "button_helper_bar", {
		controllerIndex = controller,
		fontIconSet = properties.fontIconSet
	} )
	helper_bar.id = "S2xUnlocksButtonHelperBar"
	helper_bar:setAnchors( 0, 0, 1, 0, 0 )
	helper_bar:setBottom( _1080p * -55, 0 )
	helper_bar:setLeft( 0, 0 )
	helper_bar:setRight( 0, 0 )
	helper_bar:setTop( _1080p * -105, 0 )
	self:addElement( helper_bar )

	local options = LUI.MenuBuilder.BuildRegisteredType( "OptionButtonsGrid", {
		controllerIndex = controller,
		fontIconSet = properties.fontIconSet,
		OptionsGrid_maxVisibleRows = 10,
		OptionsGrid_verticalAlignment = LUI.Alignment.Top
	} )
	options.id = "S2xUnlocksOptions"
	options:setAnchors( 0, 1, 0, 1, 0 )
	options:setBottom( _1080p * 952.08, 0 )
	options:setLeft( 0, 0 )
	options:setRight( _1080p * 900, 0 )
	options:setTop( _1080p * 200, 0 )
	self:addElement( options )

	local title = LUI.MenuBuilder.BuildRegisteredType( "GenericMenuTitle", {
		controllerIndex = controller,
		fontIconSet = properties.fontIconSet
	} )
	title.id = "S2xUnlocksTitle"
	title:setAnchors( 0, 0, 0, 1, 0 )
	title:setBottom( _1080p * 173, 0 )
	title:setLeft( _1080p * 100, 0 )
	title:setRight( _1080p * -100, 0 )
	title:setTop( _1080p * 125, 0 )
	if title.Title then
		title.Title:setFont( FONTS.BodyBoldFont.Font )
		title.Title:setHorizontalAlignment( LUI.HorizontalAlignment.Left )
		title.Title:setText( Engine.Localize( "UNLOCKS" ), 0 )
	end
	if title.zm_title_divider0 then
		title.zm_title_divider0:setRight( _1080p * 1727, 0 )
	end
	self:addElement( title )

	local description = LUI.MenuBuilder.BuildRegisteredType( "GenericMenuDescription", {
		controllerIndex = controller,
		fontIconSet = properties.fontIconSet
	} )
	description.id = "S2xUnlocksDescription"
	description:setAnchors( 0, 1, 0, 1, 0 )
	description:setBottom( _1080p * 303, 0 )
	description:setLeft( _1080p * 924, 0 )
	description:setRight( _1080p * 1820, 0 )
	description:setTop( _1080p * 200, 0 )
	if description.DescriptionText then
		description.DescriptionText:setText( "", 0 )
	end
	if description.DescriptionTitle then
		description.DescriptionTitle:setText( "", 0 )
	end
	self:addElement( description )

	local helper = helper_bar:BeginSet()
	helper = helper:AddBackButton()
	helper = helper:AddLeft( LuaButton.primary, "LUA_MENU_SELECT", nil )
	helper:Finish()

	if title.SetTitle then
		title:SetTitle( "UNLOCKS", "LUA_MENU_SOLDIER" )
	end

	return self
end

LUI.MenuBuilder.registerType( "s2x_unlocks_mp_menu", function ( menu, properties )
	return build_unlocks_menu( "s2x_unlocks_mp_menu", properties, multiplayer_options )
end )

LUI.MenuBuilder.registerType( "s2x_unlocks_zm_menu", function ( menu, properties )
	return build_unlocks_menu( "s2x_unlocks_zm_menu", properties, zombies_options )
end )

local menu_builders = LUI.MenuBuilder.m_types_build or m_types_build
assert( type( menu_builders ) == "table", "Missing LUI menu builder registry" )

local function patch_soldier_menu( soldier_menu, properties, requested_menu_name )
	if not soldier_menu then
		return soldier_menu
	end

	local menu_name = requested_menu_name or soldier_menu.id
	if menu_name ~= "soldierscreen_menu" and menu_name ~= "zm_soldier_menu" then
		return soldier_menu
	end

	if soldier_menu.S2xUnlocksTab then
		return soldier_menu
	end

	properties = properties or {}
	local controller = get_controller_index( soldier_menu, properties )
	local is_zombies = menu_name == "zm_soldier_menu"
	local left = 100
	local right = is_zombies and 600 or 440
	local top = is_zombies and 593 or 386
	local bottom = top + 62
	local unlocks_tab = LUI.MenuBuilder.BuildRegisteredType( "soldierscreen_tab_button", {
		controllerIndex = controller,
		fontIconSet = properties.fontIconSet
	} )
	unlocks_tab.id = "S2xUnlocksTab"
	unlocks_tab:setAnchors( 0, 1, 0, 1, 0 )
	unlocks_tab:setBottom( _1080p * bottom, 0 )
	unlocks_tab:setLeft( _1080p * left, 0 )
	unlocks_tab:setRight( _1080p * right, 0 )
	unlocks_tab:setTop( _1080p * top, 0 )
	if unlocks_tab.Icon then
		unlocks_tab.Icon:setImage( RegisterMaterial( "menu_soldier_dossier" ), 0 )
	end
	if unlocks_tab.Name then
		unlocks_tab.Name:setText( Engine.Localize( "UNLOCKS" ), 0 )
	end
	soldier_menu:addElement( unlocks_tab )
	soldier_menu.S2xUnlocksTab = unlocks_tab

	if not is_zombies and soldier_menu.ActiveBoostTab then
		soldier_menu.ActiveBoostTab:setBottom( _1080p * 510, 0 )
		soldier_menu.ActiveBoostTab:setTop( _1080p * 448, 0 )
	end
	if not is_zombies and soldier_menu.ActiveXPBoosts then
		soldier_menu.ActiveXPBoosts:setBottom( _1080p * 992.83, 0 )
		soldier_menu.ActiveXPBoosts:setTop( _1080p * 541.92, 0 )
	end

	local function open_unlocks_menu( event )
		local unlocks_menu = is_zombies and "s2x_unlocks_zm_menu" or "s2x_unlocks_mp_menu"
		ACTIONS.OpenMenu( unlocks_menu, true, event.controller or controller )
		local scoped_data = LUI.FlowManager.GetScopedData( soldier_menu )
		if scoped_data then
			scoped_data.subMenu = true
		end
	end

	unlocks_tab:addEventHandler( "button_action", function ( element, event )
		open_unlocks_menu( event )
	end )
	unlocks_tab:addEventHandler( "gamepad_button", function ( element, event )
		if CONDITIONS.ButtonRight( soldier_menu, event ) and
			CONDITIONS.IsInFocus( element ) and CONDITIONS.IsButtonDown( soldier_menu, event ) then
			open_unlocks_menu( event )
			ACTIONS.PlaySelectSound()
		end
	end )

	soldier_menu:updateNavigation()
	return soldier_menu
end

local stock_soldier_builder = menu_builders["soldierscreen_menu"]
assert( type( stock_soldier_builder ) == "function", "Missing Soldier menu builder" )

menu_builders["soldierscreen_menu"] = function ( menu, properties )
	return patch_soldier_menu( stock_soldier_builder( menu, properties ), properties )
end

local stock_zombies_soldier_builder = menu_builders["zm_soldier_menu"]
if type( stock_zombies_soldier_builder ) == "function" then
	menu_builders["zm_soldier_menu"] = function ( menu, properties )
		return patch_soldier_menu( stock_zombies_soldier_builder( menu, properties ), properties,
			"zm_soldier_menu" )
	end
end

local stock_build_registered_type = LUI.MenuBuilder.BuildRegisteredType
LUI.MenuBuilder.BuildRegisteredType = function ( menu_name, properties )
	local built_menu = stock_build_registered_type( menu_name, properties )
	if menu_name == "soldierscreen_menu" or menu_name == "zm_soldier_menu" then
		return patch_soldier_menu( built_menu, properties, menu_name )
	end

	return built_menu
end

local stock_change_page = TabMenuBase.ChangePage
TabMenuBase.ChangePage = function ( tab_menu, controller, previous_page, previous_index, next_page,
	next_index, properties_func )
	stock_change_page( tab_menu, controller, previous_page, previous_index, next_page, next_index,
		properties_func )
	local requested_menu_name = next_page and next_page.GetMenuName and
		next_page.GetMenuName( controller ) or nil
	patch_soldier_menu( tab_menu.CurrentMenuPage, {
		controllerIndex = controller
	}, requested_menu_name )
end

local root = Engine.GetLuiRoot()
local menu_info = root and root.flowManager and
	LUI.FlowManager.GetTopMenuInfo( root.flowManager.menuInfoStack ) or nil
local current_page = menu_info and menu_info.menu and menu_info.menu.CurrentMenuPage or nil
patch_soldier_menu( current_page, {
	controllerIndex = Engine.GetFirstActiveController()
} )
