local testing = {}

local testImgID = loadImage("Images/gear.png")

local groupID = 2


function createTestEntity()
	print("Creating test entity")
	local entityID = createEntity()

	-- add some components to the entity
	local groupIDData = { ["groupID"] = groupID };
	addComponentToEntity( entityID, "GRP", groupIDData )

	local xPos = 400.0 + ( ( math.random() * 2.0 ) - 1.0 ) * 200.0
	local yPos = 300.0 + ( ( math.random() * 2.0 ) - 1.0 ) * 150.0
	local rot = math.random() * 2.0 * 3.14
	local scale = math.random() + 0.5
	local transformData = {
		["currPos"] = { ["x"] = xPos, ["y"] = yPos },
		["futurePos"] = { ["x"] = xPos, ["y"] = yPos },
		["currRotRad"] = rot,
		["futureRotRad"] = rot,
		["currScale"] = { ["x"] = scale, ["y"] = scale },
		["futureScale"] = { ["x"] = scale, ["y"] = scale },
		["parentID"] = 0, ["firstChildID"] = 0, ["nextSiblingID"] = 0,
	}
	addComponentToEntity( entityID, "GC_TF", transformData )

	local spriteData = {
		["camFlags"] = 1,
		["depth"] = 0,
		["img"] = testImgID
	}
	addComponentToEntity( entityID, "GC_SPRT", spriteData )

end

function destroyAllTestEntities()
	print("Destroying all test entities")
	destroyEntitiesByGroupID(groupID)
end

function example()
	print("--- What we're expecting ---'")
	local test = { ["string"] = "Testing", ["testInteger"] = 42, ["vec"] = { ["x"] = 1.0, ["y"] = 2.0 } }
	print(table_print(test))
	print("---")
end

function testStructSerialize()
	example()
	local test = testGetStructure( )
	print("Testing Serialization")
	print(to_string(test))
end

function testStructureDeserialize()
	print("--- What we're expecting ---'")
	local test = { ["string"] = "What", ["testInteger"] = 23, ["vec"] = { ["x"] = 3.0, ["y"] = -4.0 } }
	print(table_print(test))
	print("---")

	sendTestStructure(test)
end

-- don't reload this
package.loaded["testing"] = testing

return testing