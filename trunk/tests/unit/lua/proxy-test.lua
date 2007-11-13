local tests = require("proxy.test")

---
-- unit-test the unit-test

--- 
-- a mock for a broken compile test
TestUnknown = tests.BaseTest:new()
function TestUnknown:testUnknownFunction()
	unknownfunction()
end

TestFailed = tests.BaseTest:new()
function TestFailed:testFailedAssert()
	assertEquals(1, 2)
end

TestPassed = tests.BaseTest:new()
function TestPassed:testPassedAssert()
	assertEquals(1, 1)
end


TestUnit = tests.BaseTest:new()
function TestUnit:testUnknownFunction()
	local unknown = tests.Suite:new({
		result = tests.Result:new()
	})

	unknown:run({ "TestUnknown" })

	assertEquals(unknown.result.passed, 0)
	assertEquals(unknown.result.failed, 1)
end

function TestUnit:testFailedAssert()
	local failed = tests.Suite:new({
		result = tests.Result:new()
	})

	failed:run({ "TestFailed" })

	assertEquals(failed.result.passed, 0)
	assertEquals(failed.result.failed, 1)
end

function TestUnit:testPassedAssert()
	local passed = tests.Suite:new({
		result = tests.Result:new()
	})

	passed:run({ "TestPassed" })

	assertEquals(passed.result.passed, 1)
	assertEquals(passed.result.failed, 0)
end

function TestUnit:testAssertEquals()
	assertEquals(1, 1)
	assertEquals(0, 0)
	assertEquals(nil, nil)
end

function TestUnit:testAssertNotEquals()
	assertNotEquals(1, 0)
	assertNotEquals(0, 1)
	assertNotEquals(nil, 0)
end

TestProxyUnit = tests.ProxyBaseTest:new()
function TestProxyUnit:testDefaultScope()
	assertEquals(_G.proxy.global, nil)
	self:setDefaultScope()
	assertNotEquals(_G.proxy.global, nil)
end

function TestProxyUnit:tearDown() 
	_G.proxy.global = nil
end


local suite = tests.Suite:new({
	result = tests.Result:new()
})

suite:run({"TestUnit", "TestProxyUnit"})
suite.result:print()
suite:exit()
