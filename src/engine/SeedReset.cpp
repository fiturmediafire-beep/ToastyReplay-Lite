// Seed reset handling is centralized in EngineHooks.cpp.
//
// The previous implementation applied the seed AFTER
// PlayLayer::resetLevel(), which could be too late for
// random selection performed during the reset process.
//
// Do not add another resetLevel hook here.
