#include <Helium/Platform/ScriptContainer.hpp>

#include <memory>

namespace Helium
{
    ScriptContainer::ScriptContainer(const char* scriptName, const char* scriptSource) : ctx(&vm) {
        std::unique_ptr<Module> script(compiler.compileString(scriptName, scriptSource));

        moduleIndex = vm.loadModule(script.get());
    }

    void ScriptContainer::run() {
        if (ctx.callMainFunction(moduleIndex)) {
            vm.execute(ctx);
        }
    }
}
