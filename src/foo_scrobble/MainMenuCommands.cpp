#include "ScrobbleConfig.h"

namespace foo_scrobble
{
namespace
{

class ScrobbleCommands : public mainmenu_commands
{
public:
    virtual ~ScrobbleCommands() = default;

    enum
    {
        ToggleScrobblingCommand = 0,
        CommandCount
    };

    virtual t_uint32 get_command_count() override { return CommandCount; }

    virtual GUID get_command(t_uint32 p_index) override
    {
        switch (p_index) {
        case ToggleScrobblingCommand:
            // {7C90ACEF-2858-49AD-B614-F3FD4C848149}
            return {0x7C90ACEF,
                    0x2858,
                    0x49AD,
                    {0xB6, 0x14, 0xF3, 0xFD, 0x4C, 0x84, 0x81, 0x49}};
        default:
            uBugCheck();
        }
    }

    virtual void get_name(t_uint32 p_index, pfc::string_base& p_out) override
    {
        switch (p_index) {
        case ToggleScrobblingCommand:
            p_out = "Scrobble tracks";
            break;
        default:
            uBugCheck();
        }
    }

    virtual bool get_description(t_uint32 p_index, pfc::string_base& p_out) override
    {
        switch (p_index) {
        case ToggleScrobblingCommand:
            p_out = "Enables or disables scrobble tracks to last.fm";
            return true;
        default:
            uBugCheck();
        }
    }

    virtual GUID get_parent() override { return mainmenu_groups::playback; }

    virtual bool get_display(t_uint32 p_index, pfc::string_base& p_text,
                             t_uint32& p_flags) override
    {
        switch (p_index) {
        case ToggleScrobblingCommand:
            get_name(p_index, p_text);
            p_flags = Config.EnableScrobbling ? flag_checked : 0;
            return true;
        default:
            uBugCheck();
        }
    }

    virtual void execute(t_uint32 p_index,
                         service_ptr_t<service_base> p_callback) override
    {
        switch (p_index) {
        case ToggleScrobblingCommand:
            Config.EnableScrobbling = !Config.EnableScrobbling;
            break;
        default:
            uBugCheck();
        }
    }
};

mainmenu_commands_factory_t<ScrobbleCommands> g_MainmenuCommandsFactory;

} // namespace
} // namespace foo_scrobble
