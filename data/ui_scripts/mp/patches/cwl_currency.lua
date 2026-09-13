-- CWL tiles use the first SKU price, just like their Cost model and detail screen.
local function patch_preview( self, properties )
	if not self or self.s2xCurrencyPatched or not self.CodPoints then return self end
	if CONDITIONS and CONDITIONS.IsZombiesMode and CONDITIONS.IsZombiesMode() then return self end
	self.s2xCurrencyPatched = true
	local function update()
		if CONDITIONS and CONDITIONS.IsZombiesMode and CONDITIONS.IsZombiesMode() then return end
		local icon = self.CodPoints
		if not icon or not icon.setImage or not self.GetDataSource or not RegisterMaterial then return end
		local source, controller = self:GetDataSource()
		controller = controller or (properties and properties.controllerIndex)
		local id = source and source.id
		id = id and id.GetValue and id:GetValue( controller )
		local info = id and Engine and Engine.Inventory_GetSKUInfo and Engine.Inventory_GetSKUInfo( id )
		local price = type( info ) == "table" and type( info.prices ) == "table" and info.prices[1]
		local currency = type( price ) == "table" and tonumber( price.currency )
		icon:setImage( RegisterMaterial( currency == 6 and "s2_armory_credits_icon" or "cod_points" ), 0 )
	end
	local function refresh()
		-- Models can be temporarily absent during focus changes or teardown.
		pcall( update )
	end
	if self.SubscribeToModelThroughElement then
		self:SubscribeToModelThroughElement( self, "id", refresh )
		self:SubscribeToModelThroughElement( self, "CoDPointsPrice", refresh )
	end
	refresh()
	return self
end

local builder = LUI and LUI.MenuBuilder
if builder then
	local types = builder.m_types_build or m_types_build
	local original = types and types["cwl_preview"]
	if type( original ) == "function" then
		types["cwl_preview"] = function ( menu, properties )
			return patch_preview( original( menu, properties ), properties )
		end
	end
	local build = builder.BuildRegisteredType
	if type( build ) == "function" then
		builder.BuildRegisteredType = function ( name, properties )
			local element = build( name, properties )
			if name == "cwl_preview" then return patch_preview( element, properties ) end
			return element
		end
	end
end
