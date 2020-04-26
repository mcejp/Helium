#include <Helium/Runtime/BindingHelpers.hpp>
#include <Helium/Runtime/RuntimeFunctions.hpp>
#include <Helium/Runtime/VM.hpp>

#include <SDL_ttf.h>

#undef CreateWindow

namespace Helium
{
    using std::move;

    class SDL2_ttf {
    public:
        int Init() {
            return TTF_Init();
        }

        TTF_Font* OpenFont(StringPtr file, int ptsize) {
            return TTF_OpenFont(file, ptsize);
        }

        SDL_Surface* RenderGlyph_Blended(TTF_Font* font, int ch, int r, int g, int b, int a) {
            return TTF_RenderGlyph_Blended(font, ch, SDL_Color{ (Uint8) r, (Uint8)g, (Uint8)b, (Uint8)a });
        }
    };

    template <>
    std::pair<const std::pair<const char*, NativeFunction>*, size_t> getMethods<SDL2_ttf>() {
        static constexpr std::pair<const char*, NativeFunction> methods[]{
            { "init",               wrapMethod<int, SDL2_ttf, &SDL2_ttf::Init> },
            { "openFont",           wrapMethod<TTF_Font*, SDL2_ttf, StringPtr, int, &SDL2_ttf::OpenFont> },
            { "renderGlyphBlended", wrapMethod<SDL_Surface*, SDL2_ttf, TTF_Font*, int, int, int, int, int, &SDL2_ttf::RenderGlyph_Blended> },
        };

        return std::make_pair(methods, std::size(methods));
    }

    template <>
    bool wrap(TTF_Font*&& value, ValueRef* value_out) { return wrapPointer(value, value_out); }

    template <>
    bool unwrap(Value var, TTF_Font** value_out) { return unwrapPointer(var, value_out); }

    template <>
    bool unwrap(Value var, SDL2_ttf** value_out) { return unwrapClass(var, value_out); }

    // SDL2_ttf()
    static void new_SDL2_ttf(NativeFunctionContext& ctx) {
        ValueRef object;

        if (!wrapNewDelete(new SDL2_ttf(), &object))
            return;

        ctx.setReturnValue(move(object));
    }

    void registerSDL2_ttf(VM* vm) {
        vm->registerCallback("SDL2_ttf", &new_SDL2_ttf);
    }
}
