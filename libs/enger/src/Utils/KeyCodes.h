#pragma once

#include <cstdint>
#include <ostream>
#include <format>
#include <fmt/format.h>

namespace enger
{
    typedef enum class KeyCode : uint16_t
    {
        // From glfw3.h
        Space               = 32,
        Apostrophe          = 39, /* ' */
        Comma               = 44, /* , */
        Minus               = 45, /* - */
        Period              = 46, /* . */
        Slash               = 47, /* / */

        D0                  = 48, /* 0 */
        D1                  = 49, /* 1 */
        D2                  = 50, /* 2 */
        D3                  = 51, /* 3 */
        D4                  = 52, /* 4 */
        D5                  = 53, /* 5 */
        D6                  = 54, /* 6 */
        D7                  = 55, /* 7 */
        D8                  = 56, /* 8 */
        D9                  = 57, /* 9 */

        Semicolon           = 59, /* ; */
        Equal               = 61, /* = */

        A                   = 65,
        B                   = 66,
        C                   = 67,
        D                   = 68,
        E                   = 69,
        F                   = 70,
        G                   = 71,
        H                   = 72,
        I                   = 73,
        J                   = 74,
        K                   = 75,
        L                   = 76,
        M                   = 77,
        N                   = 78,
        O                   = 79,
        P                   = 80,
        Q                   = 81,
        R                   = 82,
        S                   = 83,
        T                   = 84,
        U                   = 85,
        V                   = 86,
        W                   = 87,
        X                   = 88,
        Y                   = 89,
        Z                   = 90,

        LeftBracket         = 91,  /* [ */
        Backslash           = 92,  /* \ */
        RightBracket        = 93,  /* ] */
        GraveAccent         = 96,  /* ` */

        World1              = 161, /* non-US #1 */
        World2              = 162, /* non-US #2 */

        /* Function keys */
        Escape              = 256,
        Enter               = 257,
        Tab                 = 258,
        Backspace           = 259,
        Insert              = 260,
        Delete              = 261,
        Right               = 262,
        Left                = 263,
        Down                = 264,
        Up                  = 265,
        PageUp              = 266,
        PageDown            = 267,
        Home                = 268,
        End                 = 269,
        CapsLock            = 280,
        ScrollLock          = 281,
        NumLock             = 282,
        PrintScreen         = 283,
        Pause               = 284,
        F1                  = 290,
        F2                  = 291,
        F3                  = 292,
        F4                  = 293,
        F5                  = 294,
        F6                  = 295,
        F7                  = 296,
        F8                  = 297,
        F9                  = 298,
        F10                 = 299,
        F11                 = 300,
        F12                 = 301,
        F13                 = 302,
        F14                 = 303,
        F15                 = 304,
        F16                 = 305,
        F17                 = 306,
        F18                 = 307,
        F19                 = 308,
        F20                 = 309,
        F21                 = 310,
        F22                 = 311,
        F23                 = 312,
        F24                 = 313,
        F25                 = 314,

        /* Keypad */
        KP0                 = 320,
        KP1                 = 321,
        KP2                 = 322,
        KP3                 = 323,
        KP4                 = 324,
        KP5                 = 325,
        KP6                 = 326,
        KP7                 = 327,
        KP8                 = 328,
        KP9                 = 329,
        KPDecimal           = 330,
        KPDivide            = 331,
        KPMultiply          = 332,
        KPSubtract          = 333,
        KPAdd               = 334,
        KPEnter             = 335,
        KPEqual             = 336,

        LeftShift           = 340,
        LeftControl         = 341,
        LeftAlt             = 342,
        LeftSuper           = 343,
        RightShift          = 344,
        RightControl        = 345,
        RightAlt            = 346,
        RightSuper          = 347,
        Menu                = 348
    } Key;

    inline std::ostream& operator<<(std::ostream& os, KeyCode keyCode)
    {
        os << static_cast<int32_t>(keyCode);
        return os;
    }
}

template<>
struct std::formatter<enger::KeyCode> : std::formatter<std::string>
{
    auto format(const enger::KeyCode& keyCode, std::format_context& ctx) const
    {
        return std::formatter<std::string>::format(std::to_string(static_cast<int32_t>(keyCode)), ctx);
    }
};

template<>
struct fmt::formatter<enger::KeyCode>
{
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const enger::KeyCode& keyCode, FormatContext& ctx) const
    {
        return fmt::format_to(ctx.out(), "{}", std::to_string(static_cast<int32_t>(keyCode)));
    }
};

// from glfw3.h
/* Printable keys */
#define ENGER_KEY_SPACE           ::enger::Key::Space
#define ENGER_KEY_APOSTROPHE      ::enger::Key::Apostrophe    /* ' */
#define ENGER_KEY_COMMA           ::enger::Key::Comma         /* , */
#define ENGER_KEY_MINUS           ::enger::Key::Minus         /* - */
#define ENGER_KEY_PERIOD          ::enger::Key::Period        /* . */
#define ENGER_KEY_SLASH           ::enger::Key::Slash         /* / */
#define ENGER_KEY_0               ::enger::Key::D0
#define ENGER_KEY_1               ::enger::Key::D1
#define ENGER_KEY_2               ::enger::Key::D2
#define ENGER_KEY_3               ::enger::Key::D3
#define ENGER_KEY_4               ::enger::Key::D4
#define ENGER_KEY_5               ::enger::Key::D5
#define ENGER_KEY_6               ::enger::Key::D6
#define ENGER_KEY_7               ::enger::Key::D7
#define ENGER_KEY_8               ::enger::Key::D8
#define ENGER_KEY_9               ::enger::Key::D9
#define ENGER_KEY_SEMICOLON       ::enger::Key::Semicolon     /* ; */
#define ENGER_KEY_EQUAL           ::enger::Key::Equal         /* = */
#define ENGER_KEY_A               ::enger::Key::A
#define ENGER_KEY_B               ::enger::Key::B
#define ENGER_KEY_C               ::enger::Key::C
#define ENGER_KEY_D               ::enger::Key::D
#define ENGER_KEY_E               ::enger::Key::E
#define ENGER_KEY_F               ::enger::Key::F
#define ENGER_KEY_G               ::enger::Key::G
#define ENGER_KEY_H               ::enger::Key::H
#define ENGER_KEY_I               ::enger::Key::I
#define ENGER_KEY_J               ::enger::Key::J
#define ENGER_KEY_K               ::enger::Key::K
#define ENGER_KEY_L               ::enger::Key::L
#define ENGER_KEY_M               ::enger::Key::M
#define ENGER_KEY_N               ::enger::Key::N
#define ENGER_KEY_O               ::enger::Key::O
#define ENGER_KEY_P               ::enger::Key::P
#define ENGER_KEY_Q               ::enger::Key::Q
#define ENGER_KEY_R               ::enger::Key::R
#define ENGER_KEY_S               ::enger::Key::S
#define ENGER_KEY_T               ::enger::Key::T
#define ENGER_KEY_U               ::enger::Key::U
#define ENGER_KEY_V               ::enger::Key::V
#define ENGER_KEY_W               ::enger::Key::W
#define ENGER_KEY_X               ::enger::Key::X
#define ENGER_KEY_Y               ::enger::Key::Y
#define ENGER_KEY_Z               ::enger::Key::Z
#define ENGER_KEY_LEFT_BRACKET    ::enger::Key::LeftBracket   /* [ */
#define ENGER_KEY_BACKSLASH       ::enger::Key::Backslash     /* \ */
#define ENGER_KEY_RIGHT_BRACKET   ::enger::Key::RightBracket  /* ] */
#define ENGER_KEY_GRAVE_ACCENT    ::enger::Key::GraveAccent   /* ` */
#define ENGER_KEY_WORLD_1         ::enger::Key::World1        /* non-US #1 */
#define ENGER_KEY_WORLD_2         ::enger::Key::World2        /* non-US #2 */

/* Function keys */
#define ENGER_KEY_ESCAPE          ::enger::Key::Escape
#define ENGER_KEY_ENTER           ::enger::Key::Enter
#define ENGER_KEY_TAB             ::enger::Key::Tab
#define ENGER_KEY_BACKSPACE       ::enger::Key::Backspace
#define ENGER_KEY_INSERT          ::enger::Key::Insert
#define ENGER_KEY_DELETE          ::enger::Key::Delete
#define ENGER_KEY_RIGHT           ::enger::Key::Right
#define ENGER_KEY_LEFT            ::enger::Key::Left
#define ENGER_KEY_DOWN            ::enger::Key::Down
#define ENGER_KEY_UP              ::enger::Key::Up
#define ENGER_KEY_PAGE_UP         ::enger::Key::PageUp
#define ENGER_KEY_PAGE_DOWN       ::enger::Key::PageDown
#define ENGER_KEY_HOME            ::enger::Key::Home
#define ENGER_KEY_END             ::enger::Key::End
#define ENGER_KEY_CAPS_LOCK       ::enger::Key::CapsLock
#define ENGER_KEY_SCROLL_LOCK     ::enger::Key::ScrollLock
#define ENGER_KEY_NUM_LOCK        ::enger::Key::NumLock
#define ENGER_KEY_PRINT_SCREEN    ::enger::Key::PrintScreen
#define ENGER_KEY_PAUSE           ::enger::Key::Pause
#define ENGER_KEY_F1              ::enger::Key::F1
#define ENGER_KEY_F2              ::enger::Key::F2
#define ENGER_KEY_F3              ::enger::Key::F3
#define ENGER_KEY_F4              ::enger::Key::F4
#define ENGER_KEY_F5              ::enger::Key::F5
#define ENGER_KEY_F6              ::enger::Key::F6
#define ENGER_KEY_F7              ::enger::Key::F7
#define ENGER_KEY_F8              ::enger::Key::F8
#define ENGER_KEY_F9              ::enger::Key::F9
#define ENGER_KEY_F10             ::enger::Key::F10
#define ENGER_KEY_F11             ::enger::Key::F11
#define ENGER_KEY_F12             ::enger::Key::F12
#define ENGER_KEY_F13             ::enger::Key::F13
#define ENGER_KEY_F14             ::enger::Key::F14
#define ENGER_KEY_F15             ::enger::Key::F15
#define ENGER_KEY_F16             ::enger::Key::F16
#define ENGER_KEY_F17             ::enger::Key::F17
#define ENGER_KEY_F18             ::enger::Key::F18
#define ENGER_KEY_F19             ::enger::Key::F19
#define ENGER_KEY_F20             ::enger::Key::F20
#define ENGER_KEY_F21             ::enger::Key::F21
#define ENGER_KEY_F22             ::enger::Key::F22
#define ENGER_KEY_F23             ::enger::Key::F23
#define ENGER_KEY_F24             ::enger::Key::F24
#define ENGER_KEY_F25             ::enger::Key::F25

/* Keypad */
#define ENGER_KEY_KP_0            ::enger::Key::KP0
#define ENGER_KEY_KP_1            ::enger::Key::KP1
#define ENGER_KEY_KP_2            ::enger::Key::KP2
#define ENGER_KEY_KP_3            ::enger::Key::KP3
#define ENGER_KEY_KP_4            ::enger::Key::KP4
#define ENGER_KEY_KP_5            ::enger::Key::KP5
#define ENGER_KEY_KP_6            ::enger::Key::KP6
#define ENGER_KEY_KP_7            ::enger::Key::KP7
#define ENGER_KEY_KP_8            ::enger::Key::KP8
#define ENGER_KEY_KP_9            ::enger::Key::KP9
#define ENGER_KEY_KP_DECIMAL      ::enger::Key::KPDecimal
#define ENGER_KEY_KP_DIVIDE       ::enger::Key::KPDivide
#define ENGER_KEY_KP_MULTIPLY     ::enger::Key::KPMultiply
#define ENGER_KEY_KP_SUBTRACT     ::enger::Key::KPSubtract
#define ENGER_KEY_KP_ADD          ::enger::Key::KPAdd
#define ENGER_KEY_KP_ENTER        ::enger::Key::KPEnter
#define ENGER_KEY_KP_EQUAL        ::enger::Key::KPEqual

#define ENGER_KEY_LEFT_SHIFT      ::enger::Key::LeftShift
#define ENGER_KEY_LEFT_CONTROL    ::enger::Key::LeftControl
#define ENGER_KEY_LEFT_ALT        ::enger::Key::LeftAlt
#define ENGER_KEY_LEFT_SUPER      ::enger::Key::LeftSuper
#define ENGER_KEY_RIGHT_SHIFT     ::enger::Key::RightShift
#define ENGER_KEY_RIGHT_CONTROL   ::enger::Key::RightControl
#define ENGER_KEY_RIGHT_ALT       ::enger::Key::RightAlt
#define ENGER_KEY_RIGHT_SUPER     ::enger::Key::RightSuper
#define ENGER_KEY_MENU            ::enger::Key::Menu
