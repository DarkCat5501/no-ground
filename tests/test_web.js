import * as binary_encoder from "./binary_encoder";

function runUnit(name, unit) {
  let status = false;
  try {
    status = unit.call(globalThis);
  } catch (err) {
    console.error(`Test Failed: ${name}:`, err);
    status = false;
  }

  return status;
}

function runModule(moduleName, module) {
  //running tests for
  const { beforeAll, ...testUnits } = module;

  if (runUnit(`${moduleName} setup`, beforeAll)) {
    const testStatus = new Map();
    for (const [name, unit] of Object.entries(testUnits).reverse()) {
      testStatus.set(name, runUnit(name, unit));
    }

    //show results
    console.info(`\x1b[92mModule ${moduleName}:\x1b[0m`);
    for (const [name, status] of testStatus.entries()) {
      console.info(
        `${name}: ${status ? "\x1b[92m[] succeed\x1b[0m" : "\x1b[91m[󰚌] failed\x1b[0m"}`,
      );
    }
  } else {
    console.info(
      `\x1b[91mtest module ${moduleName} failed to initialize\x1b[0m`,
    );
  }
}


//run all tests
runModule("Binary encoder", binary_encoder);
