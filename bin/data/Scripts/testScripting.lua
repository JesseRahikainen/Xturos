local testing = {}

local testImgID = loadImage("Images/gear.png")

local groupID = 2


function createTestEntity()
	print("Creating test entity")
	local entityID = createEntity()
	addGroupIDToEntity(entityID, groupID)

	-- add some components to the entity

end

function destroyAllTestEntities()
	print("Destroing all test entities")
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