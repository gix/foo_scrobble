#include "AsyncHelper.h"
#include "Authorizer.h"
#include "Bindings.h"
#include "Resources.h"
#include "ScrobbleConfig.h"
#include "fb2ksdk.h"

#include <pplawait.h>

namespace foo_scrobble
{
namespace
{

int const WM_EXECUTE_TASK = WM_USER;

LRESULT ExecuteTaskShim(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    bHandled = true;
    reinterpret_cast<concurrency::TaskProc_t>(wParam)(reinterpret_cast<void*>(lParam));
    return 0;
}

class ScrobblerPreferencesDialog
    : public CDialogImpl<ScrobblerPreferencesDialog>
    , public preferences_page_instance
    , public concurrency::scheduler_interface
{
public:
    ScrobblerPreferencesDialog(preferences_page_callback::ptr callback)
        : callback_(callback)
        , config_(Config)
        , authorizer_(config_.SessionKey)
        , savedAuthorizerState_(authorizer_.GetState())
    {
    }

    static int const IDD = IDD_PREFERENCES;

    // #pragma region preferences_page_instance
    virtual t_uint32 get_state() override;
    virtual void apply() override;
    virtual void reset() override;
    // #pragma endregion

    // #pragma region concurrency::scheduler_interface
    virtual void schedule(concurrency::TaskProc_t proc, void* arg) override
    {
        PostMessageW(WM_EXECUTE_TASK, reinterpret_cast<WPARAM>(proc),
                     reinterpret_cast<LPARAM>(arg));
    }
    // #pragma endregion

    BEGIN_MSG_MAP(PreferencesDialog)
    MSG_WM_INITDIALOG(OnInitDialog)
    COMMAND_HANDLER_EX(IDB_AUTH, BN_CLICKED, OnAuthButtonClicked)
    NOTIFY_HANDLER_EX(IDB_AUTH, BCN_DROPDOWN, OnAuthButtonDropDown)
    COMMAND_HANDLER_EX(IDC_CANCEL_AUTH, BN_CLICKED, OnAuthCancelClicked)
    COMMAND_HANDLER_EX(IDC_ENABLE_SCROBBLING, BN_CLICKED, OnEditChange)
    COMMAND_HANDLER_EX(IDC_ENABLE_NOW_PLAYING, BN_CLICKED, OnEditChange)
    COMMAND_HANDLER_EX(IDC_SUBMIT_ONLY_IN_LIBRARY, BN_CLICKED, OnEditChange)
    COMMAND_HANDLER_EX(IDC_SUBMIT_DYNAMIC_SOURCES, BN_CLICKED, OnEditChange)
    COMMAND_HANDLER_EX(IDC_ALBUM_ARTIST_MAPPING_EDIT, EN_CHANGE, OnEditChange)
    COMMAND_HANDLER_EX(IDC_ALBUM_MAPPING_EDIT, EN_CHANGE, OnEditChange)
    COMMAND_HANDLER_EX(IDC_ARTIST_MAPPING_EDIT, EN_CHANGE, OnEditChange)
    COMMAND_HANDLER_EX(IDC_TITLE_MAPPING_EDIT, EN_CHANGE, OnEditChange)
    COMMAND_HANDLER_EX(IDC_MBTRACKID_MAPPING_EDIT, EN_CHANGE, OnEditChange)
    COMMAND_HANDLER_EX(IDC_SKIP_SUBMISSION_FORMAT_EDIT, EN_CHANGE, OnEditChange)
    MESSAGE_HANDLER(WM_EXECUTE_TASK, ExecuteTaskShim)
    END_MSG_MAP()

private:
    BOOL OnInitDialog(CWindow, LPARAM);
    concurrency::task<void> OnAuthButtonClicked(UINT uNotifyCode, int nID,
                                                CWindow wndCtl);
    void OnAuthCancelClicked(UINT uNotifyCode, int nID, CWindow wndCtl);
    LONG OnAuthButtonDropDown(LPNMHDR pnmh) const;
    void OnEditChange(UINT uNotifyCode, int nID, CWindow wndCtl);
    bool HasChanged() const;
    void OnChanged();

    void UpdateAuthButton();
    void UpdateAuthButton(Authorizer::State state);

    preferences_page_callback::ptr const callback_;
    ScrobbleConfig& config_;
    BindingCollection bindings_;
    CToolTipCtrl authTooltip_;
    Authorizer authorizer_;
    Authorizer::State savedAuthorizerState_;
};

BOOL ScrobblerPreferencesDialog::OnInitDialog(CWindow /*wndFocus*/, LPARAM /*lInitParam*/)
{
    bindings_.Bind(config_.EnableScrobbling, m_hWnd, IDC_ENABLE_SCROBBLING);
    bindings_.Bind(config_.EnableNowPlaying, m_hWnd, IDC_ENABLE_NOW_PLAYING);
    bindings_.Bind(config_.SubmitOnlyInLibrary, m_hWnd, IDC_SUBMIT_ONLY_IN_LIBRARY);
    bindings_.Bind(config_.SubmitDynamicSources, m_hWnd, IDC_SUBMIT_DYNAMIC_SOURCES);
    bindings_.Bind(config_.ArtistMapping, m_hWnd, IDC_ARTIST_MAPPING_EDIT);
    bindings_.Bind(config_.AlbumArtistMapping, m_hWnd, IDC_ALBUM_ARTIST_MAPPING_EDIT);
    bindings_.Bind(config_.AlbumMapping, m_hWnd, IDC_ALBUM_MAPPING_EDIT);
    bindings_.Bind(config_.TitleMapping, m_hWnd, IDC_TITLE_MAPPING_EDIT);
    bindings_.Bind(config_.MBTrackIdMapping, m_hWnd, IDC_MBTRACKID_MAPPING_EDIT);
    bindings_.Bind(config_.SkipSubmissionFormat, m_hWnd, IDC_SKIP_SUBMISSION_FORMAT_EDIT);
    bindings_.FlowToControl();

    if (authTooltip_.Create(m_hWnd, nullptr, nullptr, TTS_NOPREFIX | TTS_BALLOON)) {
        CToolInfo toolInfo(TTF_IDISHWND | TTF_SUBCLASS | TTF_CENTERTIP,
                           GetDlgItem(IDB_AUTH), 0, nullptr, nullptr);
        authTooltip_.AddTool(&toolInfo);

        authTooltip_.SetTitle(0, L"last.fm Authorization");
        authTooltip_.UpdateTipText(
            L"Scrobbling requires authorization from last.fm."
            L"\n\n"
            L"\"Request authorization\": Starts the authorization process. You "
            L"will be redirected to the last.fm website to grant access.\n"
            L"\n"
            L"\"Complete authorization\": After granting access, click to "
            L"complete the authorization.\n"
            L"\n"
            L"\"Clear authorization\": foobar2000 is successfully authorized. "
            L"Click to forget the authorization credentials. This does not "
            L"revoke permissions on last.fm directly.",
            GetDlgItem(IDB_AUTH));
        authTooltip_.SetMaxTipWidth(250);
        authTooltip_.SetDelayTime(TTDT_INITIAL, 0);
        authTooltip_.SetDelayTime(TTDT_AUTOPOP, 32000);
        authTooltip_.Activate(TRUE);
    }

    UpdateAuthButton();

    return FALSE;
}

concurrency::task<void>
ScrobblerPreferencesDialog::OnAuthButtonClicked(UINT /*uNotifyCode*/, int /*nID*/,
                                                CWindow /*wndCtl*/)
{
    switch (authorizer_.GetState()) {
    case Authorizer::State::Unauthorized: {
        UpdateAuthButton(Authorizer::State::RequestingAuth);
        co_await with_scheduler(authorizer_.RequestAuthAsync(), *this);
        break;
    }

    case Authorizer::State::WaitingForApproval: {
        UpdateAuthButton(Authorizer::State::CompletingAuth);
        co_await with_scheduler(authorizer_.CompleteAuthAsync(), *this);
        break;
    }

    case Authorizer::State::Authorized:
        authorizer_.ClearAuth();
        break;

    case Authorizer::State::RequestingAuth:
    case Authorizer::State::CompletingAuth:
        break;
    }

    UpdateAuthButton();
    OnChanged();
}

void ScrobblerPreferencesDialog::OnAuthCancelClicked(UINT /*uNotifyCode*/, int /*nID*/,
                                                     CWindow /*wndCtl*/)
{
    authorizer_.CancelAuth();
    UpdateAuthButton();
}

LONG ScrobblerPreferencesDialog::OnAuthButtonDropDown(LPNMHDR pnmh) const
{
    auto const dropDown = reinterpret_cast<NMBCDROPDOWN*>(pnmh);

    POINT pt = {dropDown->rcButton.left, dropDown->rcButton.bottom};

    CWindow button = dropDown->hdr.hwndFrom;
    button.ClientToScreen(&pt);

    CMenu menu;
    if (menu.CreatePopupMenu()) {
        menu.AppendMenuW(MF_BYPOSITION, IDC_CANCEL_AUTH, L"&Cancel");
        menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, m_hWnd, nullptr);
    }

    return 0;
}

void ScrobblerPreferencesDialog::OnEditChange(UINT /*uNotifyCode*/, int /*nID*/,
                                              CWindow /*wndCtl*/)
{
    OnChanged();
}

t_uint32 ScrobblerPreferencesDialog::get_state()
{
    t_uint32 state = preferences_state::resettable;
    if (HasChanged())
        state |= preferences_state::changed;
    return state;
}

void ScrobblerPreferencesDialog::reset()
{
    CheckDlgButton(IDC_ENABLE_SCROBBLING, BST_CHECKED);
    CheckDlgButton(IDC_ENABLE_NOW_PLAYING, BST_CHECKED);
    CheckDlgButton(IDC_SUBMIT_ONLY_IN_LIBRARY, BST_UNCHECKED);
    CheckDlgButton(IDC_SUBMIT_DYNAMIC_SOURCES, BST_CHECKED);
    uSetDlgItemText(m_hWnd, IDC_ARTIST_MAPPING_EDIT, DefaultArtistMapping);
    uSetDlgItemText(m_hWnd, IDC_ALBUM_ARTIST_MAPPING_EDIT, DefaultAlbumArtistMapping);
    uSetDlgItemText(m_hWnd, IDC_ALBUM_MAPPING_EDIT, DefaultAlbumMapping);
    uSetDlgItemText(m_hWnd, IDC_TITLE_MAPPING_EDIT, DefaultTitleMapping);
    uSetDlgItemText(m_hWnd, IDC_MBTRACKID_MAPPING_EDIT, DefaultMBTrackIdMapping);
    uSetDlgItemText(m_hWnd, IDC_SKIP_SUBMISSION_FORMAT_EDIT, "");
    authorizer_ = Authorizer(Config.SessionKey);
    savedAuthorizerState_ = authorizer_.GetState();

    UpdateAuthButton();
    OnChanged();
}

void ScrobblerPreferencesDialog::apply()
{
    bindings_.FlowToVar();
    savedAuthorizerState_ = authorizer_.GetState();
    Config.SessionKey = authorizer_.GetSessionKey();
    OnChanged();

    ScrobbleConfigNotify::NotifyChanged();
}

bool ScrobblerPreferencesDialog::HasChanged() const
{
    return bindings_.HasChanged() || authorizer_.GetState() != savedAuthorizerState_;
}

void ScrobblerPreferencesDialog::OnChanged() { callback_->on_state_changed(); }

void ScrobblerPreferencesDialog::UpdateAuthButton()
{
    UpdateAuthButton(authorizer_.GetState());
}

void ScrobblerPreferencesDialog::UpdateAuthButton(Authorizer::State state)
{
    CWindow button = GetDlgItem(IDB_AUTH);

    switch (state) {
    case Authorizer::State::Unauthorized:
        button.ModifyStyle(BS_SPLITBUTTON, 0);
        button.SetWindowTextW(L"Request authorization");
        break;
    case Authorizer::State::RequestingAuth:
        button.ModifyStyle(0, BS_SPLITBUTTON);
        button.SetWindowTextW(L"Requesting authorization…");
        break;
    case Authorizer::State::WaitingForApproval:
        button.ModifyStyle(0, BS_SPLITBUTTON);
        button.SetWindowTextW(L"Complete authorization");
        break;
    case Authorizer::State::CompletingAuth:
        button.ModifyStyle(0, BS_SPLITBUTTON);
        button.SetWindowTextW(L"Completing authorization…");
        break;
    case Authorizer::State::Authorized:
        button.ModifyStyle(BS_SPLITBUTTON, 0);
        button.SetWindowTextW(L"Clear authorization");
        break;
    }
}

class ScrobblerPreferencesPage : public preferences_page_impl<ScrobblerPreferencesDialog>
{
public:
    virtual ~ScrobblerPreferencesPage() = default;

    virtual const char* get_name() override { return "Last.fm Scrobbling"; }

    virtual GUID get_guid() override
    {
        // {B94D4B57-0080-4FAA-AE64-0AC515A1B37C}
        static GUID const guid = {
            0xB94D4B57, 0x80, 0x4FAA, {0xAE, 0x64, 0xA, 0xC5, 0x15, 0xA1, 0xB3, 0x7C}};
        return guid;
    }

    virtual GUID get_parent_guid() override { return guid_tools; }
};

preferences_page_factory_t<ScrobblerPreferencesPage> g_PageFactory;

} // namespace
} // namespace foo_scrobble
