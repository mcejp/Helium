#include <Helium/Runtime/BindingHelpers.hpp>
#include <Helium/Runtime/NativeObjectFunctions.hpp>
#include <Helium/Runtime/VM.hpp>

#include <SDL.h>

#undef CreateWindow

namespace Helium
{
    using std::move;

    class SDL2 {
    public:
        int Init(unsigned int flags) {
            return SDL_Init(flags);
        }

        int BlitSurface(SDL_Surface* src, int srcx, int srcy, int srcw, int srch, SDL_Surface* dst, int dstx, int dsty) {
            const SDL_Rect srcRect{ srcx, srcy, srcw, srch };
            SDL_Rect dstRect{ dstx, dsty };
            return SDL_BlitSurface(src, &srcRect, dst, &dstRect);
        }

        SDL_Renderer* CreateRenderer(SDL_Window* window, int index, unsigned int flags) {
            return SDL_CreateRenderer(window, index, flags);
        }

        SDL_Window* CreateWindow(StringPtr title, int x, int y, int w, int h, unsigned int flags) {
            return SDL_CreateWindow(title, x, y, w, h, flags);
        }

        int FillRect(SDL_Surface* dst, int x, int y, int w, int h, unsigned int color) {
            const SDL_Rect rect{ x, y, w, h };
            return SDL_FillRect(dst, &rect, color);
        }

        void FreeSurface(SDL_Surface* surface) {
            SDL_FreeSurface(surface);
        }

        SDL_Surface* GetWindowSurface(SDL_Window* window) {
            return SDL_GetWindowSurface(window);
        }

        static void PollEvent(NativeFunctionContext& ctx, SDL2* sdl2) {
            SDL_Event ev;

            while (SDL_PollEvent(&ev)) {
                switch (ev.type) {
                case SDL_MOUSEBUTTONDOWN: {
                    ValueRef event;
                    NativeObjectFunctions::newObject(&event);
                    // TODO: setManyProperties
                    NativeObjectFunctions::setProperty(event, "type", ValueRef::makeString("mousebuttondown"));
                    NativeObjectFunctions::setProperty(event, "x", ValueRef::makeInteger(ev.button.x));
                    NativeObjectFunctions::setProperty(event, "y", ValueRef::makeInteger(ev.button.y));
                    ctx.setReturnValue(move(event));
                    return;
                }

                case SDL_MOUSEMOTION: {
                    ValueRef event;
                    NativeObjectFunctions::newObject(&event);
                    NativeObjectFunctions::setProperty(event, "type", ValueRef::makeString("mousemotion"));
                    NativeObjectFunctions::setProperty(event, "x", ValueRef::makeInteger(ev.motion.x));
                    NativeObjectFunctions::setProperty(event, "y", ValueRef::makeInteger(ev.motion.y));
                    ctx.setReturnValue(move(event));
                    return;
                }

                case SDL_QUIT: {
                    ValueRef event;
                    NativeObjectFunctions::newObject(&event);
                    NativeObjectFunctions::setProperty(event, "type", ValueRef::makeString("quit"));
                    ctx.setReturnValue(move(event));
                    return;
                }

                default:
                    continue;
                }
            }
        }

        int RenderClear(SDL_Renderer* renderer) {
            return SDL_RenderClear(renderer);
        }

        int RenderDrawRect(SDL_Renderer* renderer, int x, int y, int w, int h) {
            const SDL_Rect rect{ x, y, w, h };
            return SDL_RenderDrawRect(renderer, &rect);
        }

        int RenderFillRect(SDL_Renderer* renderer, int x, int y, int w, int h) {
            const SDL_Rect rect{ x, y, w, h };
            return SDL_RenderFillRect(renderer, &rect);
        }

        void RenderPresent(SDL_Renderer* renderer) {
            SDL_RenderPresent(renderer);
        }

        int SetRenderDrawColor(SDL_Renderer* renderer, int r, int g, int b, int a) {
            return SDL_SetRenderDrawColor(renderer, r, g, b, a);
        }

        void UpdateWindowSurface(SDL_Window* window) {
            SDL_UpdateWindowSurface(window);
        }
    };

    template <>
    std::pair<const std::pair<const char*, NativeFunction>*, size_t> getMethods<SDL2>() {
        static constexpr std::pair<const char*, NativeFunction> methods[]{
            { "blitSurface",            wrapMethod<int, SDL2, SDL_Surface*, int, int, int, int, SDL_Surface*, int, int, &SDL2::BlitSurface> },
            { "createRenderer",         wrapMethod<SDL_Renderer*, SDL2, SDL_Window*, int, unsigned int, &SDL2::CreateRenderer> },
            { "createWindow",           wrapMethod<SDL_Window*, SDL2, StringPtr, int, int, int, int, unsigned int, &SDL2::CreateWindow> },
            { "init",                   wrapMethod<int, SDL2, unsigned int, &SDL2::Init> },
            { "fillRect",               wrapMethod<int, SDL2, SDL_Surface*, int, int, int, int, unsigned int, &SDL2::FillRect> },
            { "freeSurface",            wrapMethodVoid<SDL2, SDL_Surface*, &SDL2::FreeSurface> },
            { "getWindowSurface",       wrapMethod<SDL_Surface*, SDL2, SDL_Window*, &SDL2::GetWindowSurface> },
            { "pollEvent",              wrapFunctionVoid<SDL2*, &SDL2::PollEvent> },
            { "renderClear",            wrapMethod<int, SDL2, SDL_Renderer*, &SDL2::RenderClear> },
            { "renderDrawRect",         wrapMethod<int, SDL2, SDL_Renderer*, int, int, int, int, &SDL2::RenderDrawRect> },
            { "renderFillRect",         wrapMethod<int, SDL2, SDL_Renderer*, int, int, int, int, &SDL2::RenderFillRect> },
            { "renderPresent",          wrapMethodVoid<SDL2, SDL_Renderer*, &SDL2::RenderPresent> },
            { "setRenderDrawColor",     wrapMethod<int, SDL2, SDL_Renderer*, int, int, int, int, &SDL2::SetRenderDrawColor> },
            { "updateWindowSurface",    wrapMethodVoid<SDL2, SDL_Window*, &SDL2::UpdateWindowSurface> },
        };

        return std::make_pair(methods, std::size(methods));
    }

    template <>
    std::pair<const std::pair<const char*, NativeFunction>*, size_t> getMethods<SDL_Surface>() {
        return {nullptr, 0};
    }

    template <>
    bool wrap(SDL_Renderer*&& value, ValueRef* value_out) { return wrapPointer(value, value_out); }

    template <>
    bool wrap(SDL_Surface*&& value, ValueRef* value_out) {
        if (wrapNonOwning(value, value_out)) {
            NativeObjectFunctions::setProperty(*value_out, "w", ValueRef::makeInteger(value->w));
            NativeObjectFunctions::setProperty(*value_out, "h", ValueRef::makeInteger(value->h));
            return true;
        }
        else
            return false;
    }

    template <>
    bool wrap(SDL_Window*&& value, ValueRef* value_out) { return wrapPointer(value, value_out); }

    template <>
    bool unwrap(Value var, SDL_Renderer** value_out) { return unwrapPointer(var, value_out); }

    template <>
    bool unwrap(Value var, SDL_Surface** value_out) { return unwrapClass(var, value_out); }

    template <>
    bool unwrap(Value var, SDL_Window** value_out) { return unwrapPointer(var, value_out); }

    template <>
    bool unwrap(Value var, SDL2** value_out) { return unwrapClass(var, value_out); }

    // SDL2()
    static void new_SDL2(NativeFunctionContext& ctx) {
        ValueRef object;

        if (!wrapNewDelete(new SDL2(), &object))
            return;

        NativeObjectFunctions::setProperty(object, "INIT_VIDEO",
                                           ValueRef::makeInteger(SDL_INIT_VIDEO));
        NativeObjectFunctions::setProperty(object, "WINDOWPOS_UNDEFINED",
                                           ValueRef::makeInteger(SDL_WINDOWPOS_UNDEFINED));
        ctx.setReturnValue(move(object));
    }

    void registerSDL2(VM* vm) {
        vm->registerCallback("SDL2", &new_SDL2);
    }
}
